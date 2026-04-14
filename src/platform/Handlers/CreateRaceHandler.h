#pragma once

#include <RGT/Devkit/Types.h>
#include <RGT/Devkit/HTTPRequestHandler.h>

#include <Poco/Data/SessionPool.h>
#include <Poco/Util/LayeredConfiguration.h>

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

    virtual void extractPayloadFromRequest(Poco::Net::HTTPServerRequest & request) final;

    virtual void requestProcessing
    (
        Poco::Net::HTTPServerRequest  & request, 
        Poco::Net::HTTPServerResponse & response
    ) final;

private:
    struct
    {
        RGT::Devkit::JWTPayload tokenPayload;

        std::vector<Devkit::UserId> participants;
        std::vector<Devkit::UserId> judges;
    } requestPayload_;

    Poco::Data::SessionPool & sessionPool_;
};

} // namespace RGT::Management::Handlers
