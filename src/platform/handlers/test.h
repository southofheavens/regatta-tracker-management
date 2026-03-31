#ifndef __TEST_HANDLER_H__
#define __TEST_HANDLER_H__

#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Data/SessionPool.h>
#include <Poco/Util/LayeredConfiguration.h>
#include <Poco/JSON/Object.h>
#include <Poco/Net/HTTPServerResponse.h>

#include <rgt/devkit/RGTException.h>
#include <rgt/devkit/subsystems/RabbitMQSubsystem.h>

namespace RGT::Management
{

class TestHandler : public Poco::Net::HTTPRequestHandler
{
public:
    TestHandler(Devkit::Subsystems::RabbitMQSubsystem::AmqpConnection & amqpConnection) 
        : amqpConnection_{amqpConnection}
    {
    }

private:
    virtual void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) final
    try
    {
        const char * message = "Hello from rgt-management!";
        amqp_bytes_t amqpMsg = amqp_cstring_bytes(message);
        amqp_basic_publish(amqpConnection_.connection, amqpConnection_.channel, amqp_empty_bytes,
            amqp_cstring_bytes("queue_analytics"), 0, 0, nullptr, amqpMsg);
        
        amqp_rpc_reply_t publishResult = amqp_get_rpc_reply(amqpConnection_.connection);
        if (publishResult.reply_type != AMQP_RESPONSE_NORMAL) {
            throw RGT::Devkit::RGTException("publish msg failed", Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        }

        // УБЕДИТЬСЯ, ЧТО МЫ СМОГЛИ ОТПРАВИТЬ СООБЩЕНИЕ

        Poco::JSON::Object json;
        json.set("OK", "OK");

        std::ostream & out = response.send();
        json.stringify(out);
    }
    catch (std::exception & e)
    {
        std::cerr << e.what() << '\n';
    }

private:
    Devkit::Subsystems::RabbitMQSubsystem::AmqpConnection & amqpConnection_;
};

} // namespace RGT::Management

#endif // __TEST_HANDLER_H__
