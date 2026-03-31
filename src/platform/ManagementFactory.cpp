#include <ManagementFactory.h>

#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/StreamCopier.h>
#include <Poco/UUIDGenerator.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/s3/S3Client.h>
#include <fstream>

#include <handlers/CreateRaceHandler.h>
#include <handlers/StartRaceHandler.h>
#include <handlers/EndRaceHandler.h>
#include <handlers/test.h>

namespace RGT::Management
{

Poco::Net::HTTPRequestHandler * ManagementFactory::createRequestHandler(const Poco::Net::HTTPServerRequest & request)
{
    const std::string & uri = request.getURI();
    const std::string & method = request.getMethod();

    if (method == "POST")
    {
        if (uri == "/create_race") {
            return new CreateRaceHandler(sessionPool_);
        }
        else if (uri == "/start_race") {
            return new StartRaceHandler(sessionPool_, redisPool_);
        }
        else if (uri == "/end_race") {
            return new EndRaceHandler(sessionPool_, redisPool_, s3Client_, amqpConnection_);
        }
    }
}

} // namespace RGT::Management
