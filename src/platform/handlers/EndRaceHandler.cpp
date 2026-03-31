#include <handlers/EndRaceHandler.h>

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

    {
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
    }

    const char * message = std::to_string(requiredPayload.raceId).c_str();
    amqp_bytes_t amqpMsg = amqp_cstring_bytes(message);
    amqp_basic_publish(amqpConnection_.connection, amqpConnection_.channel, amqp_empty_bytes,
        amqp_cstring_bytes("postprocessor_tasks"), 0, 0, nullptr, amqpMsg);
    
    amqp_rpc_reply_t publishResult = amqp_get_rpc_reply(amqpConnection_.connection);
    if (publishResult.reply_type != AMQP_RESPONSE_NORMAL) 
    {
        // TODO лог
        throw std::runtime_error("publish msg failed");
    }

    // УБЕДИТЬСЯ, ЧТО МЫ СМОГЛИ ОТПРАВИТЬ СООБЩЕНИЕ

    
    HTTPRequestHandler::sendJsonResponse(response, "OK", "OK");
}

} // namespace RGT::Management::Handlers
