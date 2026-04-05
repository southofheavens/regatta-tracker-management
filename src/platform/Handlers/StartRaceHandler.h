#pragma once

#include <RGT/Devkit/HTTPRequestHandler.h>

#include <Poco/Data/SessionPool.h>
#include <Poco/Util/LayeredConfiguration.h>
#include <Poco/ObjectPool.h>
#include <Poco/Redis/Client.h>

#include <any>

namespace RGT::Management::Handlers
{

class StartRaceHandler : public RGT::Devkit::HTTPRequestHandler
{
public:
    using RedisClientObjectPool = Poco::ObjectPool<Poco::Redis::Client, Poco::Redis::Client::Ptr>;

    StartRaceHandler(Poco::Data::SessionPool & sessionPool, RedisClientObjectPool & redisPool) 
        : sessionPool_{sessionPool}
        , redisPool_{redisPool}
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
};

} // namespace RGT::Management::Handlers
