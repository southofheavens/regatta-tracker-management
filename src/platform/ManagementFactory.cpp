#include <ManagementFactory.h>

#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/StreamCopier.h>
#include <Poco/UUIDGenerator.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/s3/S3Client.h>
#include <fstream>

#include <Handlers/CreateRaceHandler.h>
#include <Handlers/StartRaceHandler.h>
#include <Handlers/EndRaceHandler.h>

namespace RGT::Management
{

Poco::Net::HTTPRequestHandler * ManagementFactory::createRequestHandler(const Poco::Net::HTTPServerRequest & request)
{
    const std::string & uri = request.getURI();
    const std::string & method = request.getMethod();

    if (method == "POST")
    {
        if (uri == "/create_race") {
            return new Handlers::CreateRaceHandler(sessionPool_);
        }
        else if (uri == "/start_race") {
            return new Handlers::StartRaceHandler(sessionPool_, redisPool_);
        }
        else if (uri == "/end_race") {
            return new Handlers::EndRaceHandler(sessionPool_, redisPool_, s3Client_, amqpChannel_);
        }
    }
}

} // namespace RGT::Management
