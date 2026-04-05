#pragma once

#include <RGT/Devkit/HTTPRequestHandler.h>

#include <Poco/Data/SessionPool.h>
#include <Poco/Util/LayeredConfiguration.h>
#include <Poco/ObjectPool.h>
#include <Poco/Redis/Client.h>

#include <aws/s3/S3Client.h>

#include <SimpleAmqpClient/Channel.h>

#include <any>

namespace RGT::Management::Handlers
{

class EndRaceHandler : public RGT::Devkit::HTTPRequestHandler
{
public:
    using RedisClientObjectPool = Poco::ObjectPool<Poco::Redis::Client, Poco::Redis::Client::Ptr>;

    EndRaceHandler(Poco::Data::SessionPool & sessionPool, RedisClientObjectPool & redisPool, 
        Aws::S3::S3Client & s3Client, 
        AmqpClient::Channel & amqpChannel) 
        : sessionPool_{sessionPool}
        , redisPool_{redisPool}
        , s3Client_{s3Client}
        , amqpChannel_{amqpChannel}
    {
    }

private:
    virtual void requestPreprocessing(Poco::Net::HTTPServerRequest & request) final;

    virtual std::any extractPayloadFromRequest(Poco::Net::HTTPServerRequest & request) final;

    virtual void requestProcessing(Poco::Net::HTTPServerRequest & request, Poco::Net::HTTPServerResponse & response) final;

private:
    struct RequiredPayload
    {
        RGT::Devkit::JWTPayload tokenPayload;

        uint64_t raceId;
    };

    Poco::Data::SessionPool & sessionPool_;
    RedisClientObjectPool   & redisPool_;
    Aws::S3::S3Client       & s3Client_;
    AmqpClient::Channel     & amqpChannel_;
};

} // namespace RGT::Management::Handlers
