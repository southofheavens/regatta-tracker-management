#include <Handlers/CreateRaceHandler.h>

#include <RGT/Devkit/Types.h>

namespace
{

constexpr uint8_t min_participants_count = 3;
constexpr uint8_t min_judges_count = 1;

void checkExistenceAndRole
(
    Poco::Data::Session & session,
    const std::vector<RGT::Devkit::UserId> & uids,
    RGT::Devkit::UserRole expectedRole
)
{
    if (uids.empty()) {
        return;
    }

    std::vector<uint64_t> rawIds;
    rawIds.reserve(uids.size());
    std::string placeholders;

    for (size_t i = 0; i < uids.size(); ++i) 
    {
        rawIds.push_back(RGT::Devkit::mapUserIdToUint(uids[i]));
        placeholders += std::format("${}", i + 1);
        if (i + 1 < uids.size()) { 
            placeholders += ",";
        }
    }

    std::string query = "SELECT id, role FROM users WHERE id IN (" + placeholders + ")";
    Poco::Data::Statement stmt(session);
    stmt << query;
    for (uint64_t id : rawIds) {
        stmt, Poco::Data::Keywords::bind(id);
    }

    std::vector<uint64_t> foundRawIds;
    std::vector<std::string> foundRawRoles;
    stmt,
        Poco::Data::Keywords::into(foundRawIds),
        Poco::Data::Keywords::into(foundRawRoles),
        Poco::Data::Keywords::now;

    std::unordered_map<uint64_t, std::string_view> userRoles;
    userRoles.reserve(foundRawIds.size());
    for (size_t i = 0; i < foundRawIds.size(); ++i) {
        userRoles[foundRawIds[i]] = foundRawRoles[i];
    }

    const std::string_view rawExpectedRole = RGT::Devkit::mapUserRoleToString(expectedRole);

    for (uint64_t id : rawIds) 
    {
        auto it = userRoles.find(id);
        if (it == userRoles.end()) 
        {
            throw RGT::Devkit::RGTException
            (
                std::format("User with ID {} not found", id),
                Poco::Net::HTTPResponse::HTTP_NOT_FOUND
            );
        }
        
        if (it->second != rawExpectedRole) 
        {
            throw RGT::Devkit::RGTException
            (
                std::format
                (
                    "User with ID {} has role '{}', but an attempt was made to register "
                    "him in the race as a {}",
                    id, 
                    it->second,
                    rawExpectedRole
                ),
                Poco::Net::HTTPResponse::HTTP_UNPROCESSABLE_ENTITY
            );
        }
    }
}

std::vector<RGT::Devkit::UserId> extractIdsFromJson(Poco::JSON::Object::Ptr json, const std::string & key)
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

    std::vector<RGT::Devkit::UserId> ids;
    ids.reserve(arrSize);

    for (size_t i = 0; i < arrSize; ++i)
    {
        try 
        {
            uint64_t rawId = array->getElement<uint64_t>(i);
            ids.push_back(RGT::Devkit::mapUintToUserId(rawId));
        }
        catch (...) 
        {
            throw RGT::Devkit::RGTException
            (
                std::format("The elements of the '{}' array must be of the unsigned integer type", key),
                Poco::Net::HTTPResponse::HTTP_BAD_REQUEST
            );
        }
    }

    return ids;
}

void createParticipations
(
    Poco::Data::Session & session,
    RGT::Devkit::RaceId raceId,
    std::vector<RGT::Devkit::UserId> & userIds,
    RGT::Devkit::UserRole userRole
)
try
{
    const uint64_t rawRaceId = RGT::Devkit::mapRaceIdToUint(raceId);
    const std::string_view rawUserRole = RGT::Devkit::mapUserRoleToString(userRole);
    const char * cStrUserRole = rawUserRole.data();

    for (const RGT::Devkit::UserId userId : userIds)
    {
        const uint64_t rawUserId = RGT::Devkit::mapUserIdToUint(userId);

        session << "INSERT INTO participations (user_id, race_id, role) VALUES ($1, $2, $3)",
            Poco::Data::Keywords::bind(rawUserId),   
            Poco::Data::Keywords::bind(rawRaceId),        
            Poco::Data::Keywords::bind(cStrUserRole),
            Poco::Data::Keywords::now;
    }
}
catch (const Poco::Exception & e)
{
    std::cerr << "createParticipations: " << e.displayText() << std::endl;
    throw;
}
catch (const std::exception & e)
{
    std::cerr << "createParticipations: " << e.what() << std::endl;
    throw;
}

} // namespace

/* - */

namespace RGT::Management::Handlers
{

void CreateRaceHandler::requestPreprocessing(Poco::Net::HTTPServerRequest & request)
{
    HTTPRequestHandler::checkContentLength(request, 1024 * 1024 /* 1 мегабайт */);
    HTTPRequestHandler::checkContentLengthIsNull(request);
    HTTPRequestHandler::checkContentType(request, "application/json");
}

void CreateRaceHandler::extractPayloadFromRequest(Poco::Net::HTTPServerRequest & request)
{
    const std::string & accessToken = HTTPRequestHandler::extractTokenFromRequest(request);
    RGT::Devkit::JWTPayload tokenPayload = HTTPRequestHandler::extractPayload(accessToken);

    Poco::JSON::Object::Ptr json = HTTPRequestHandler::extractJsonObjectFromRequest(request);

    std::vector<RGT::Devkit::UserId> participantsIds = extractIdsFromJson(json, "participants");
    std::vector<RGT::Devkit::UserId> judgesIds = extractIdsFromJson(json, "judges");

    requestPayload_.tokenPayload = tokenPayload;
    requestPayload_.participants = participantsIds;
    requestPayload_.judges = judgesIds;
}

void CreateRaceHandler::requestProcessing(Poco::Net::HTTPServerRequest & request, Poco::Net::HTTPServerResponse & response)
try
{
    // Можно проверить, что в бд уже есть гонка с такими участниками, а поле
    // с началом времени не заполнено. в таком случае пользователю мы и вернем
    // айдишник этой гонки. + надо добавить колонку с примерным временем старта

    if (requestPayload_.tokenPayload.role != "Judge") {
        throw RGT::Devkit::RGTException("Only judge can create a race", Poco::Net::HTTPResponse::HTTP_FORBIDDEN);
    }

    size_t participantsSize = requestPayload_.participants.size();
    if (participantsSize < min_participants_count) 
    {
        throw RGT::Devkit::RGTException("There must be a minimum of 3 participants in the race",
            Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }

    size_t judgesSize = requestPayload_.judges.size();
    if (judgesSize < min_judges_count) 
    {
        throw RGT::Devkit::RGTException("There must be at least 1 judge in the race",
            Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }

    std::set<RGT::Devkit::UserId> checkingUniqueIds;

    for (const RGT::Devkit::UserId & id : requestPayload_.participants) {
        checkingUniqueIds.insert(id);
    }
    for (const RGT::Devkit::UserId & id : requestPayload_.judges) {
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

    uint64_t rawRaceId;
    {
        Poco::Data::Session session = sessionPool_.get();

        // Проверяем, что пользователи с полученными ID существуют и что все участники действительно участники, 
        // а судьи действительно судьи

        checkExistenceAndRole(session, requestPayload_.participants, RGT::Devkit::UserRole::Participant);
        checkExistenceAndRole(session, requestPayload_.judges, RGT::Devkit::UserRole::Judge);

        // Вставляем значения в базу данных

        session << "INSERT INTO races DEFAULT VALUES RETURNING id", 
            Poco::Data::Keywords::into(rawRaceId), Poco::Data::Keywords::now;
        RGT::Devkit::RaceId raceId = RGT::Devkit::mapUintToRaceId(rawRaceId);
        
        createParticipations(session, raceId, requestPayload_.participants, RGT::Devkit::UserRole::Participant);
        createParticipations(session, raceId, requestPayload_.judges, RGT::Devkit::UserRole::Judge);
    }
    
    Poco::JSON::Object json;
    json.set("id", rawRaceId);

    response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_CREATED);
    std::ostream & out = response.send();
    json.stringify(out);
}
catch (Poco::Exception & e)
{
    std::cerr << e.what() << " : " << e.displayText() << '\n';
}
catch (std::exception & e) 
{
    std::cerr << e.what() << std::endl;
}

} // namespace RGT::Management::Handlers
