#include <handlers/EndRaceHandler.h>

#include <Poco/Redis/PoolableConnectionFactory.h>
#include <Poco/Redis/Type.h>

namespace
{

using RedisClientObjectPool = Poco::ObjectPool<Poco::Redis::Client, Poco::Redis::Client::Ptr>;

/// @brief Извлекает из Redis списки с координатами участников
/// @param participantsIds Вектор с id участников
/// @param redisPool 
/// @return Вектор с парами вида user_id - список координат
std::vector<std::pair<uint64_t, Poco::Redis::Array>>
extractListsWithCoordinates
(
    const std::vector<uint64_t> & participantsIds,
    RedisClientObjectPool & redisPool
)
{
    Poco::Redis::PooledConnection pc(redisPool, 500);
    if (static_cast<Poco::Redis::Client::Ptr>(pc) == nullptr) 
    {
        // TODO лог
        throw std::exception{};
    }

    std::vector<std::pair<uint64_t, Poco::Redis::Array>> result;
    for (const uint64_t & id : participantsIds)
    {
        Poco::Redis::Array cmd;
        cmd << "LRANGE" << std::format("user_participation:{}", id) 
            << "1" /* начинаем с 1 потому, что элемент с индексом 0 содержит строку "init" */
            << "-1";
        Poco::Redis::Array coordinates = static_cast<Poco::Redis::Client::Ptr>(pc)->execute<Poco::Redis::Array>(cmd);
        result.push_back({id,coordinates});
    }
}

std::string generateGpxFromCoordinates(const Poco::Redis::Array & coordinates)
{
    
}

} // namespace 

namespace RGT::Management
{

void EndRaceHandler::requestPreprocessing(Poco::Net::HTTPServerRequest & request)
{
    HTTPRequestHandler::checkContentLength(request, 1024 /* 1 килобайт */);
    HTTPRequestHandler::checkContentLengthIsNull(request);
    HTTPRequestHandler::checkContentType(request, "application/json");
}

std::any EndRaceHandler::extractPayloadFromRequest(Poco::Net::HTTPServerRequest & request)
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

void EndRaceHandler::requestProcessing(Poco::Net::HTTPServerRequest & request, Poco::Net::HTTPServerResponse & response)
{
    RequiredPayload requiredPayload = std::any_cast<RequiredPayload>(payload_);

    if (requiredPayload.tokenPayload.role != "Judge") {
        throw RGT::Devkit::RGTException("Only Judge can stop a race", Poco::Net::HTTPResponse::HTTP_FORBIDDEN);
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

    if (raceStatus != "In_progress")
    {
        if (raceStatus == "Not_started")
        {
            throw RGT::Devkit::RGTException(std::format("The race {} is not started", requiredPayload.raceId),
                Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        }
        else 
        {
            throw RGT::Devkit::RGTException(std::format("The race {} is already over", requiredPayload.raceId), 
                Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        }
    }

    // Проверки окончены. Добавляем в запись текущее время по UTC и меняем статус гонки

    session <<
        "UPDATE races "
        "SET end_of_the_race = NOW() " /* TODO переименовать в end_time */
        "WHERE id = $1;",
        Poco::Data::Keywords::use(requiredPayload.raceId),
        Poco::Data::Keywords::now;

    session <<
        "UPDATE races "
        "SET status = 'Finished' "
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

    // Извлекаем из Redis координаты каждого участника гонки
    std::vector<std::pair<uint64_t, Poco::Redis::Array>> usersCoordinates = 
        extractListsWithCoordinates(participantsIds, redisPool_);

    // for (auto id : participantsIds)
    // {
    //     std::cout << "ID: " << id << std::endl;

    //     Poco::Redis::Array cmd;
    //     cmd << "LRANGE" << std::format("user_participation:{}", id) << "1" << "-1";
    //     Poco::Redis::Array resultOfCmd = static_cast<Poco::Redis::Client::Ptr>(pc)->execute<Poco::Redis::Array>(cmd);
        
    //     for (auto it = resultOfCmd.begin(); it != resultOfCmd.end(); ++it)
    //     {
            
    //         const Poco::Redis::RedisType & rt = *(*it);
    //         if (rt.isBulkString())
    //         {
    //             const auto & t = dynamic_cast<const Poco::Redis::Type<Poco::Redis::BulkString>&>(rt);
    //             std::cout << t.value() << '\n';
    //         }

    //         std::cout << std::endl;
    //     }

    //     std::cout << std::endl;
    // }

    
    HTTPRequestHandler::sendJsonResponse(response, "OK", "OK");
}

} // namespace RGT::Management::Handlers
