#include <Handlers/EndRaceHandler.h>

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

    uint64_t raceId;
    try {
        raceId = dvRaceId.convert<uint64_t>();
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
    if (requestPayload_.tokenPayload.role != "Judge") {
        throw RGT::Devkit::RGTException("Only Judge can stop a race", Poco::Net::HTTPResponse::HTTP_FORBIDDEN);
    }

    {
        Poco::Data::Session session = sessionPool_.get();

        if (not RGT::Management::isRaceExists(session, requestPayload_.raceId))
        {
            throw RGT::Devkit::RGTException(std::format("The race with id {} is not exists", requestPayload_.raceId), 
                Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        }
        
        if (not RGT::Management::isParticipationExists(session, requestPayload_.raceId, requestPayload_.tokenPayload.sub))
        {
            throw RGT::Devkit::RGTException
            (
                std::format
                (
                    "The judge is not part of the judging panel for the race with ID {}",
                    requestPayload_.raceId
                ),
                Poco::Net::HTTPResponse::HTTP_BAD_REQUEST
            );
        } 

        std::string raceStatus;
        if ((raceStatus = RGT::Management::getRaceStatus(session, requestPayload_.raceId)) != "In_progress")
        {
            if (raceStatus == "Not_started")
            {
                throw RGT::Devkit::RGTException(std::format("The race {} is not started", requestPayload_.raceId),
                    Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
            }
            else 
            {
                throw RGT::Devkit::RGTException(std::format("The race {} is already over", requestPayload_.raceId), 
                    Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
            }
        }
    }

    // TODO по-моему в БД нужно перевести статус в finished именно здесь, а не в постпроцессоре + добавить идемпотентность 

    AmqpClient::BasicMessage::ptr_t msg = AmqpClient::BasicMessage::Create(std::to_string(requestPayload_.raceId));
    msg->DeliveryMode(AmqpClient::BasicMessage::dm_persistent);
    amqpChannel_.BasicPublish("", "postprocessor_tasks", msg);
    
    HTTPRequestHandler::sendJsonResponse(response, "OK", "OK");
}

} // namespace RGT::Management::Handlers
