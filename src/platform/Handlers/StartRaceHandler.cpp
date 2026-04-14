#include <Handlers/StartRaceHandler.h>

#include <Poco/Redis/PoolableConnectionFactory.h>
#include <Poco/Redis/Command.h>

#include <RGT/Devkit/General.h>
#include <RGT/Devkit/RaceLookup.h>

#include <Utils.h>

namespace
{

using RedisClientObjectPool = Poco::ObjectPool<Poco::Redis::Client, Poco::Redis::Client::Ptr>;

void createParticipations(RedisClientObjectPool & redisPool, const std::vector<RGT::Devkit::UserId> & ids)
{
    static std::string luaScript = RGT::Devkit::readLuaScript("lua_scripts/rpush_if_exists.lua");

    Poco::Redis::PooledConnection pc(redisPool, 500);
    Poco::Redis::Client::Ptr redisClient = static_cast<Poco::Redis::Client::Ptr>(pc);
    if (redisClient == nullptr) 
    {
        // TODO лог
        throw std::exception{};
    }

    Poco::Redis::Array cmd;
    cmd << "EVAL"
        << luaScript
        << std::to_string(static_cast<Poco::Int64>(ids.size()));

    for (RGT::Devkit::UserId id : ids) {
        cmd << std::format("user_participation:{}", RGT::Devkit::mapUserIdToUint(id));
    }
    cmd << "init";

    [[maybe_unused]] Poco::Redis::Array reply = redisClient->execute<Poco::Redis::Array>(cmd);
}

bool isParticipationsExists
(
    RedisClientObjectPool & redisPool, 
    const std::vector<RGT::Devkit::UserId> & participantsIds
)
{
    if (participantsIds.empty()) {
        return true; 
    }

    Poco::Redis::PooledConnection pc(redisPool, 500);
    Poco::Redis::Client::Ptr redisClient = static_cast<Poco::Redis::Client::Ptr>(pc);
    if (redisClient == nullptr) 
    {
        // TODO лог
        throw std::exception{};
    }

    Poco::Redis::Command cmd("EXISTS");
    for (RGT::Devkit::UserId id : participantsIds) {
        cmd << std::format("user_participation:{}", RGT::Devkit::mapUserIdToUint(id));
    }

    Poco::Int64 reply = redisClient->execute<Poco::Int64>(cmd);
    
    return reply == participantsIds.size();
}

bool startTheRace(Poco::Data::Session & session, RGT::Devkit::RaceId raceId)
{
    Poco::Data::Statement stmt(session);

    stmt << 
        "UPDATE races "
        "SET start_of_the_race = COALESCE(start_of_the_race, NOW()), "
            "status = 'In_progress' "
        "WHERE id = $1 AND status = 'Not_started';",
        Poco::Data::Keywords::bind(RGT::Devkit::mapRaceIdToUint(raceId));

    return stmt.execute() > 0; 
}

// TODO добавить кэширование и перенести в devkit
std::vector<RGT::Devkit::UserId> getParticipantsOfRace(Poco::Data::Session & session, RGT::Devkit::RaceId raceId)
{
    std::vector<uint64_t> rawParticipantsIds;
    std::vector<RGT::Devkit::UserId> participantsIds;
    participantsIds.reserve(rawParticipantsIds.size());

    session << 
        "SELECT user_id "
        "FROM participations "
        "WHERE race_id = $1 AND role = 'Participant';",
        Poco::Data::Keywords::bind(RGT::Devkit::mapRaceIdToUint(raceId)),
        Poco::Data::Keywords::into(rawParticipantsIds),
        Poco::Data::Keywords::now;

    for (uint64_t rawId : rawParticipantsIds) {
        participantsIds.push_back(RGT::Devkit::mapUintToUserId(rawId));
    }
    return participantsIds;
}

} // namespace

/* - */

namespace RGT::Management::Handlers
{

void StartRaceHandler::requestPreprocessing(Poco::Net::HTTPServerRequest & request)
{
    HTTPRequestHandler::checkContentLength(request, 1024 /* 1 килобайт */);
    HTTPRequestHandler::checkContentLengthIsNull(request);
    HTTPRequestHandler::checkContentType(request, "application/json");
}

void StartRaceHandler::extractPayloadFromRequest(Poco::Net::HTTPServerRequest & request)
{
    const std::string & accessToken = HTTPRequestHandler::extractTokenFromRequest(request);
    RGT::Devkit::JWTPayload tokenPayload = HTTPRequestHandler::extractPayload(accessToken);

    Poco::JSON::Object::Ptr json = HTTPRequestHandler::extractJsonObjectFromRequest(request);

    Poco::Dynamic::Var dvRaceId = json->get("race_id");
    if (dvRaceId.isEmpty()) {
        throw RGT::Devkit::RGTException("Expected race_id field", Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }

    uint64_t rawRaceId;
    RGT::Devkit::RaceId raceId;
    try 
    {
        rawRaceId = dvRaceId.convert<uint64_t>();
        raceId = RGT::Devkit::mapUintToRaceId(rawRaceId);
    }
    catch (...) 
    {
        throw RGT::Devkit::RGTException("The value for the key \"race_id\" must be of the unsigned integer type", 
            Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }

    requestPayload_.tokenPayload = tokenPayload;
    requestPayload_.raceId = raceId;
}

void StartRaceHandler::requestProcessing(Poco::Net::HTTPServerRequest & request, Poco::Net::HTTPServerResponse & response)
{
    if (requestPayload_.tokenPayload.role != RGT::Devkit::UserRole::Judge) {
        throw RGT::Devkit::RGTException("Only Judge can start a race", Poco::Net::HTTPResponse::HTTP_FORBIDDEN);
    }

    Poco::Data::Session session = sessionPool_.get();
    Poco::Redis::PooledConnection pc(redisPool_, 500);

    if (not RGT::Devkit::isRaceExists(session, pc, requestPayload_.raceId))
    {
        throw RGT::Devkit::RGTException
        (
            std::format
            (
                "The race with id {} is not exists", RGT::Devkit::mapRaceIdToUint(requestPayload_.raceId)
            ), 
            Poco::Net::HTTPResponse::HTTP_NOT_FOUND
        );
    }
    
    if (not RGT::Devkit::isParticipationExists(session, pc, requestPayload_.raceId, requestPayload_.tokenPayload.sub))
    {
        throw RGT::Devkit::RGTException
        (
            std::format
            (
                "The judge is not part of the judging panel for the race with ID {}",
                RGT::Devkit::mapRaceIdToUint(requestPayload_.raceId)
            ),
            Poco::Net::HTTPResponse::HTTP_FORBIDDEN
        );
    } 
    
    if (not startTheRace(session, requestPayload_.raceId))
    // Гонка уже не в статусе 'Not_started'
    {
        if (RGT::Devkit::getRaceStatus(session, pc, requestPayload_.raceId) == RGT::Devkit::RaceStatus::Finished)
        {
            throw RGT::Devkit::RGTException
            (
                std::format
                (
                    "The race {} is already finished", RGT::Devkit::mapRaceIdToUint(requestPayload_.raceId)
                ), 
                Poco::Net::HTTPResponse::HTTP_CONFLICT
            );   
        }

        std::vector<RGT::Devkit::UserId> participantsIds = getParticipantsOfRace(session, requestPayload_.raceId);

        if (isParticipationsExists(redisPool_, participantsIds))
        {
            throw RGT::Devkit::RGTException
            (
                std::format
                (
                    "The race {} is already started", RGT::Devkit::mapRaceIdToUint(requestPayload_.raceId)
                ), 
                Poco::Net::HTTPResponse::HTTP_CONFLICT
            ); 
        }

        session.close();

        // Создаём в Redis ключи user_participation:{user_id}
        createParticipations(redisPool_, participantsIds);

        HTTPRequestHandler::sendJsonResponse(response, "OK", "OK");
    }

    std::vector<RGT::Devkit::UserId> participantsIds = getParticipantsOfRace(session, requestPayload_.raceId);

    session.close();

    // Создаём в Redis ключи user_participation:{user_id}
    createParticipations(redisPool_, participantsIds);

    HTTPRequestHandler::sendJsonResponse(response, "OK", "OK");
}

} // namespace RGT::Management::Handlers
