#pragma once

#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/ObjectPool.h>
#include <Poco/Data/SessionPool.h>
#include <Poco/Util/LayeredConfiguration.h>
#include <Poco/Redis/Client.h>
#include <Poco/Net/HTTPServerRequest.h>

#include <Utils.h>

#include <RGT/Devkit/Subsystems/RabbitMQSubsystem.h>

namespace RGT::Management
{

class ManagementFactory : public Poco::Net::HTTPRequestHandlerFactory
{
public:
    using RedisClientObjectPool = Poco::ObjectPool<Poco::Redis::Client, Poco::Redis::Client::Ptr>;

    ManagementFactory
    (
        Poco::Data::SessionPool & sessionPool,
        RedisClientObjectPool & redisPool,
        AmqpClient::Channel & amqpChannel,
        Poco::Util::LayeredConfiguration & cfg
    )
        : sessionPool_{sessionPool}
        , redisPool_{redisPool}
        , amqpChannel_{amqpChannel}
        , cfg_{cfg}
    {
    }

    Poco::Net::HTTPRequestHandler * createRequestHandler(const Poco::Net::HTTPServerRequest & request) final;

private:
    Poco::Data::SessionPool & sessionPool_;
    RedisClientObjectPool   & redisPool_;
    AmqpClient::Channel     & amqpChannel_;

    Poco::Util::LayeredConfiguration & cfg_;
};

} // namespace RGT::Management
