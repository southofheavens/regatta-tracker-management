#include <handlers/StartRaceHandler.h>

#include <Poco/Redis/PoolableConnectionFactory.h>

namespace
{

using RedisClientObjectPool = Poco::ObjectPool<Poco::Redis::Client, Poco::Redis::Client::Ptr>;

void createParticipations(RedisClientObjectPool & redisPool, const std::vector<uint64_t> & ids)
{
    Poco::Redis::PooledConnection pc(redisPool, 500);
    if (static_cast<Poco::Redis::Client::Ptr>(pc) == nullptr) 
    {
        // TODO лог
        throw std::exception{};
    }
    for (const uint64_t & id : ids)
    {
        Poco::Redis::Array cmd;
        cmd << "RPUSH" << std::format("user_participation:{}", id) << "init";
        Poco::Int64 resultOfCmd = static_cast<Poco::Redis::Client::Ptr>(pc)->execute<Poco::Int64>(cmd);
    }
}

} // namespace

namespace RGT::Management
{

void StartRaceHandler::requestPreprocessing(Poco::Net::HTTPServerRequest & request)
{
    HTTPRequestHandler::checkContentLength(request, 1024 /* 1 килобайт */);
    HTTPRequestHandler::checkContentLengthIsNull(request);
    HTTPRequestHandler::checkContentType(request, "application/json");
}

std::any StartRaceHandler::extractPayloadFromRequest(Poco::Net::HTTPServerRequest & request)
{
    const std::string & accessToken = HTTPRequestHandler::extractTokenFromRequest(request);
    RGT::Devkit::JWTPayload tokenPayload = HTTPRequestHandler::extractPayload(accessToken);

    Poco::JSON::Object::Ptr json = HTTPRequestHandler::extractJsonObjectFromRequest(request);

    Poco::Dynamic::Var dvRaceId = json->get("race_id");
    if (dvRaceId.isEmpty()) {
        throw RGT::Devkit::RGTException("Expected race_id field", Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }

    uint64_t raceId;
    try {
        raceId = dvRaceId.convert<uint64_t>();
    }
    catch (...) 
    {
        throw RGT::Devkit::RGTException("The value for the key \"race_id\" must be of the unsigned integer type", 
            Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }

    return RequiredPayload
    {
        .tokenPayload = tokenPayload,
        .raceId = raceId
    };
}

void StartRaceHandler::requestProcessing(Poco::Net::HTTPServerRequest & request, Poco::Net::HTTPServerResponse & response)
{
    RequiredPayload requiredPayload = std::any_cast<RequiredPayload>(payload_);

    if (requiredPayload.tokenPayload.role != "Judge") {
        throw RGT::Devkit::RGTException("Only Judge can start a race", Poco::Net::HTTPResponse::HTTP_FORBIDDEN);
    }

    Poco::Data::Session session = sessionPool_.get();

    bool isRaceExists = false;
    session <<
        "SELECT EXISTS ("
            "SELECT 1 "
            "FROM races "
            "WHERE id = $1"
        ");",
        Poco::Data::Keywords::use(requiredPayload.raceId),
        Poco::Data::Keywords::into(isRaceExists),
        Poco::Data::Keywords::now;
    if (not isRaceExists)
    {
        throw RGT::Devkit::RGTException(std::format("The race with id {} is not exists", requiredPayload.raceId), 
            Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
    
    bool isParticipationExists = false;
    session <<
        "SELECT EXISTS ("
            "SELECT 1 "
            "FROM participations "
            "WHERE user_id = $1 AND race_id = $2"
        ");",
        Poco::Data::Keywords::use(requiredPayload.tokenPayload.sub),
        Poco::Data::Keywords::use(requiredPayload.raceId),
        Poco::Data::Keywords::into(isParticipationExists),
        Poco::Data::Keywords::now;
    if (not isParticipationExists)
    {
        throw RGT::Devkit::RGTException
        (
            std::format
            (
                "The judge is not part of the judging panel for the race with ID {}",
                requiredPayload.raceId
            ),
            Poco::Net::HTTPResponse::HTTP_BAD_REQUEST
        );
    } 

    std::string raceStatus;
    session <<
        "SELECT status "
        "FROM races "
        "WHERE id = $1",
        Poco::Data::Keywords::use(requiredPayload.raceId),
        Poco::Data::Keywords::into(raceStatus),
        Poco::Data::Keywords::now;

    if (raceStatus != "Not_started")
    {
        if (raceStatus == "In_progress")
        {
            throw RGT::Devkit::RGTException(std::format("The race {} is already underway", requiredPayload.raceId),
                Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        }
        else 
        {
            throw RGT::Devkit::RGTException(std::format("The race {} is already finished", requiredPayload.raceId), 
                Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        }
    }

    // Проверки окончены. Добавляем в запись текущее время по UTC и меняем статус гонки

    session <<
        "UPDATE races "
        "SET start_of_the_race = NOW() " /* TODO переименовать в start_time */
        "WHERE id = $1;",
        Poco::Data::Keywords::use(requiredPayload.raceId),
        Poco::Data::Keywords::now;

    session <<
        "UPDATE races "
        "SET status = 'In_progress' "
        "WHERE id = $1;",
        Poco::Data::Keywords::use(requiredPayload.raceId),
        Poco::Data::Keywords::now;

    std::vector<uint64_t> participantsIds;
    session << 
        "SELECT user_id "
        "FROM participations "
        "WHERE race_id = $1 AND role = 'Participant';",
        Poco::Data::Keywords::use(requiredPayload.raceId),
        Poco::Data::Keywords::into(participantsIds),
        Poco::Data::Keywords::now;

    session.close();

    // Создаём в Redis ключи user_participation:{user_id}
    createParticipations(redisPool_, participantsIds);

    HTTPRequestHandler::sendJsonResponse(response, "OK", "OK");
}

} // namespace RGT::Management
