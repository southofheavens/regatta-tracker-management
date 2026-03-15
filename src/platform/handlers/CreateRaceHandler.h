#ifndef __CREATE_RACE_HANDLER_H__
#define __CREATE_RACE_HANDLER_H__

#include <rgt/devkit/HTTPRequestHandler.h>

#include <Poco/Data/SessionPool.h>
#include <Poco/Util/LayeredConfiguration.h>

#include <any>

namespace RGT::Management
{

class CreateRaceHandler : public RGT::Devkit::HTTPRequestHandler
{
public:
    CreateRaceHandler(Poco::Data::SessionPool & sessionPool, Poco::Util::LayeredConfiguration & cfg) 
        : sessionPool_{sessionPool}
        , cfg_{cfg}
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

        std::vector<uint64_t> participants;
        std::vector<uint64_t> judges;
    };

    Poco::Data::SessionPool          & sessionPool_;
    Poco::Util::LayeredConfiguration & cfg_;
};

} // namespace RGT::Management

#endif // __CREATE_RACE_HANDLER_H__
