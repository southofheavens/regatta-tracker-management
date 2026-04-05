#include <Handlers/CreateRaceHandler.h>

namespace
{

constexpr uint8_t min_participants_count = 3;
constexpr uint8_t min_judges_count = 1;

} // namespace

namespace RGT::Management::Handlers
{

void CreateRaceHandler::requestPreprocessing(Poco::Net::HTTPServerRequest & request)
{
    HTTPRequestHandler::checkContentLength(request, 1024 * 1024 /* 1 мегабайт */);
    HTTPRequestHandler::checkContentLengthIsNull(request);
    HTTPRequestHandler::checkContentType(request, "application/json");
}

std::any CreateRaceHandler::extractPayloadFromRequest(Poco::Net::HTTPServerRequest & request)
{
    const std::string & accessToken = HTTPRequestHandler::extractTokenFromRequest(request);
    RGT::Devkit::JWTPayload tokenPayload = HTTPRequestHandler::extractPayload(accessToken);

    Poco::JSON::Object::Ptr json = HTTPRequestHandler::extractJsonObjectFromRequest(request);

    auto extractIdsFromJson = [](Poco::JSON::Object::Ptr json, const std::string & key) -> std::vector<uint64_t>
    {
        Poco::JSON::Array::Ptr array = json->getArray(key);

        if (array.isNull()) 
        {
            throw RGT::Devkit::RGTException
            (
                std::format("The key '{}' is missing or the value is not equal to the array", key),
                Poco::Net::HTTPResponse::HTTP_BAD_REQUEST
            );
        }

        size_t arrSize = array->size();

        std::vector<uint64_t> ids;
        ids.reserve(arrSize);

        for (size_t i = 0; i < arrSize; ++i)
        {
            try {
                ids.push_back(array->getElement<uint64_t>(i));
            }
            catch (...) 
            {
                throw RGT::Devkit::RGTException
                (
                    std::format("The elements of the '{}' array must be of the uint64_t type", key),
                    Poco::Net::HTTPResponse::HTTP_BAD_REQUEST
                );
            }
        }

        return ids;
    };

    std::vector<uint64_t> participantsVector = extractIdsFromJson(json, "participants");
    std::vector<uint64_t> judgesVector = extractIdsFromJson(json, "judges");

    return RequiredPayload
    {
        .tokenPayload = tokenPayload,
        .participants = participantsVector,
        .judges = judgesVector
    };
}

void CreateRaceHandler::requestProcessing(Poco::Net::HTTPServerRequest & request, Poco::Net::HTTPServerResponse & response)
{
    RequiredPayload requiredPayload = std::any_cast<RequiredPayload>(payload_);

    if (requiredPayload.tokenPayload.role != "Judge") {
        throw RGT::Devkit::RGTException("Only judge can create a race", Poco::Net::HTTPResponse::HTTP_FORBIDDEN);
    }

    size_t participantsSize = requiredPayload.participants.size();
    if (participantsSize < min_participants_count) 
    {
        throw RGT::Devkit::RGTException("There must be a minimum of 3 participants in the race",
            Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }

    size_t judgesSize = requiredPayload.judges.size();
    if (judgesSize < min_judges_count) 
    {
        throw RGT::Devkit::RGTException("There must be at least 1 judge in the race",
            Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }

    std::set<uint64_t> checkingUniqueIds;

    for (const uint64_t & id : requiredPayload.participants) {
        checkingUniqueIds.insert(id);
    }
    for (const uint64_t & id : requiredPayload.judges) {
        checkingUniqueIds.insert(id);
    }

    if (checkingUniqueIds.size() != participantsSize + judgesSize)
    {
        throw RGT::Devkit::RGTException
        (
            "All IDs must be unique. Also, a user cannot be both a judge and a participant at the same time",
            Poco::Net::HTTPResponse::HTTP_BAD_REQUEST
        );
    }

    // TODO Нужно подумать есть ли какой-нибудь простой способ, чтобы тренер не мог наплодить 
    // кучу одинаковых гонок
    // ключ в редис с TTL создавать?

    uint64_t raceId;
    {
        Poco::Data::Session session = sessionPool_.get();

        // Проверяем, что пользователи с полученными ID существуют и что все участники действительно участники, 
        // а судьи действительно судьи

        auto checkExistenceAndRole = []
        (
            Poco::Data::Session & session, 
            const std::vector<uint64_t> & ids, 
            const std::string & expectingRole
        )
        {
            for (uint64_t id : ids) 
            {
                std::string role;
                session << "SELECT role FROM users WHERE id = $1",
                    Poco::Data::Keywords::use(id),
                    Poco::Data::Keywords::into(role),
                    Poco::Data::Keywords::now;

                if (role.empty())
                {
                    throw RGT::Devkit::RGTException(std::format("User with id {} doesnt exists", id),
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
                }

                if (role != expectingRole) 
                {
                    throw RGT::Devkit::RGTException
                    (
                        std::format
                        (
                            "User with ID {0} is not a {1}, but an attempt was made to register "
                            "him in the race as a {1}",
                            id, expectingRole
                        ),
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST
                    );
                };
            }
        };

        checkExistenceAndRole(session, requiredPayload.participants, "Participant");
        checkExistenceAndRole(session, requiredPayload.judges, "Judge");

        // Вставляем значения в базу данных

        // TODO тут вылетает exception. корректно ли он перехватится и залогируется?
        session << "INSERT INTO races DEFAULT VALUES RETURNING id", 
            Poco::Data::Keywords::into(raceId), Poco::Data::Keywords::now;

        static const std::string participantString = "Participant";
        static const std::string judgeString = "Judge";

        for (uint64_t id : requiredPayload.participants) 
        {
            session << "INSERT INTO participations (user_id, race_id, role) VALUES ($1, $2, $3)",
                Poco::Data::Keywords::use(id), 
                Poco::Data::Keywords::use(raceId),
                Poco::Data::Keywords::use(const_cast<std::string&>(participantString) /* осуждаю себя за const_cast */),
                Poco::Data::Keywords::now;
        }

        for (uint64_t id : requiredPayload.judges) 
        {
            session << "INSERT INTO participations (user_id, race_id, role) VALUES ($1, $2, $3)",
                Poco::Data::Keywords::use(id), 
                Poco::Data::Keywords::use(raceId),
                Poco::Data::Keywords::use(const_cast<std::string&>(judgeString) /* осуждаю себя за const_cast */),
                Poco::Data::Keywords::now;
        }
    }
    
    Poco::JSON::Object json;
    json.set("id", raceId);

    response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_CREATED);
    std::ostream & out = response.send();
    json.stringify(out);
}

} // namespace RGT::Management::Handlers
