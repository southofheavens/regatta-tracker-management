#pragma once

#include <RGT/Devkit/HTTPRequestHandler.h>

#include <Poco/Data/SessionPool.h>
#include <Poco/Util/LayeredConfiguration.h>

#include <any>

namespace RGT::Management::Handlers
{

class CreateRaceHandler : public RGT::Devkit::HTTPRequestHandler
{
public:
    CreateRaceHandler(Poco::Data::SessionPool & sessionPool) 
        : sessionPool_{sessionPool}
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
};

} // namespace RGT::Management::Handlers
