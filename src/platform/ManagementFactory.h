#ifndef __MANAGEMENT_FACTORY_H__
#define __MANAGEMENT_FACTORY_H__

#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/ObjectPool.h>
#include <Poco/Data/SessionPool.h>
#include <Poco/Util/LayeredConfiguration.h>
#include <Poco/Redis/Client.h>
#include <Poco/Net/HTTPServerRequest.h>

#include <SimpleAmqpClient/SimpleAmqpClient.h>

namespace Aws::S3 { class S3Client; } // namespace Aws::S3

namespace RGT::Management
{

class ManagementFactory : public Poco::Net::HTTPRequestHandlerFactory
{
public:
    using RedisClientObjectPool = Poco::ObjectPool<Poco::Redis::Client, Poco::Redis::Client::Ptr>;

    ManagementFactory(Poco::Data::SessionPool & sessionPool, RedisClientObjectPool & redisPool, 
        Aws::S3::S3Client & s3Client, Poco::Util::LayeredConfiguration & cfg, 
        AmqpClient::Channel & channel) 
        : sessionPool_(sessionPool)
        , redisPool_(redisPool)
        , s3Client_{s3Client}
        , cfg_(cfg)
        , myChannel_(channel)
    {
    }

    Poco::Net::HTTPRequestHandler * createRequestHandler(const Poco::Net::HTTPServerRequest & request) final;

private:
    Poco::Data::SessionPool & sessionPool_;
    RedisClientObjectPool   & redisPool_;
    Aws::S3::S3Client       & s3Client_;
    Poco::Util::LayeredConfiguration & cfg_;
    AmqpClient::Channel & myChannel_;
};

} // namespace RGT::Management

#endif // __MANAGEMENT_FACTORY_H__
