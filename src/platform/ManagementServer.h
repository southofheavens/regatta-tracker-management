#ifndef __MANAGEMENT_SERVER_H__
#define __MANAGEMENT_SERVER_H__

#include <Poco/Util/ServerApplication.h>
#include <Poco/ObjectPool.h>
#include <Poco/Redis/Client.h>
#include <Poco/Data/SessionPool.h>

#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>

namespace RGT::Management
{

class ManagementServer : public Poco::Util::ServerApplication
{
public:
    using RedisClientObjectPool = Poco::ObjectPool<Poco::Redis::Client, Poco::Redis::Client::Ptr>;

    void initialize(Application & self) final;

    void uninitialize() final;

    int main(const std::vector<std::string> &) final;

private:
    std::unique_ptr<Poco::Data::SessionPool>    sessionPool_;
    std::unique_ptr<RedisClientObjectPool>      redisPool_;
    std::unique_ptr<Aws::S3::S3Client>          s3Client_;
    Aws::SDKOptions sdkOptions_;
};

} // namespace RGT::Management

#endif // __MANAGEMENT_SERVER_H__
