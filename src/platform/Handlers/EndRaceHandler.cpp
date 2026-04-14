#include <Handlers/EndRaceHandler.h>
#include <RGT/Devkit/RaceLookup.h>

#include <Utils.h>

namespace RGT::Management::Handlers
{

void EndRaceHandler::requestPreprocessing(Poco::Net::HTTPServerRequest & request)
{
    HTTPRequestHandler::checkContentLength(request, 1024 /* 1 килобайт */);
    HTTPRequestHandler::checkContentLengthIsNull(request);
    HTTPRequestHandler::checkContentType(request, "application/json");
}

void EndRaceHandler::extractPayloadFromRequest(Poco::Net::HTTPServerRequest & request)
{
    const std::string & accessToken = HTTPRequestHandler::extractTokenFromRequest(request);
    RGT::Devkit::JWTPayload tokenPayload = HTTPRequestHandler::extractPayload(accessToken);

    Poco::JSON::Object::Ptr json = HTTPRequestHandler::extractJsonObjectFromRequest(request);

    Poco::Dynamic::Var dvRaceId = json->get("race_id");
    if (dvRaceId.isEmpty()) {
        throw RGT::Devkit::RGTException("Expected race_id field", Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }

    uint64_t rawRaceId;
    Devkit::RaceId raceId;
    try 
    {
        rawRaceId = dvRaceId.convert<uint64_t>();
        raceId = Devkit::mapUintToRaceId(rawRaceId);
    }
    catch (...) 
    {
        throw RGT::Devkit::RGTException("The value for the key \"race_id\" must be of the unsigned integer type", 
            Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }

    requestPayload_.tokenPayload = tokenPayload;
    requestPayload_.raceId = raceId;
}

void EndRaceHandler::requestProcessing(Poco::Net::HTTPServerRequest & request, Poco::Net::HTTPServerResponse & response)
{
    if (requestPayload_.tokenPayload.role != RGT::Devkit::UserRole::Judge) {
        throw RGT::Devkit::RGTException("Only Judge can stop a race", Poco::Net::HTTPResponse::HTTP_FORBIDDEN);
    }

    {
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
                Poco::Net::HTTPResponse::HTTP_BAD_REQUEST
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
                Poco::Net::HTTPResponse::HTTP_BAD_REQUEST
            );
        } 

        RGT::Devkit::RaceStatus raceStatus = RGT::Devkit::getRaceStatus(session, pc, requestPayload_.raceId);
        if (raceStatus != RGT::Devkit::RaceStatus::InProgress)
        {
            if (raceStatus == RGT::Devkit::RaceStatus::NotStarted)
            {
                throw RGT::Devkit::RGTException
                (
                    std::format
                    (
                        "The race {} is not started", RGT::Devkit::mapRaceIdToUint(requestPayload_.raceId)
                    ),
                    Poco::Net::HTTPResponse::HTTP_BAD_REQUEST
                );
            }
            else 
            {
                throw RGT::Devkit::RGTException
                (
                    std::format
                    (
                        "The race {} is already over", RGT::Devkit::mapRaceIdToUint(requestPayload_.raceId)
                    ), 
                    Poco::Net::HTTPResponse::HTTP_BAD_REQUEST
                );
            }
        }
    }

    AmqpClient::BasicMessage::ptr_t msg = AmqpClient::BasicMessage::Create
    (
        std::to_string
        (
            RGT::Devkit::mapRaceIdToUint(requestPayload_.raceId)
        )
    );
    msg->DeliveryMode(AmqpClient::BasicMessage::dm_persistent);
    amqpChannel_.BasicPublish("", "postprocessor_tasks", msg);

    // если сервер упадет в этом месте, то что?

    // TODO по-моему в БД нужно перевести статус в finished именно здесь, а не в постпроцессоре + добавить идемпотентность 
    
    HTTPRequestHandler::sendJsonResponse(response, "OK", "OK");
}

} // namespace RGT::Management::Handlers
