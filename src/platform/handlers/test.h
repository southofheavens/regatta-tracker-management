#ifndef __TEST_HANDLER_H__
#define __TEST_HANDLER_H__

#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Data/SessionPool.h>
#include <Poco/Util/LayeredConfiguration.h>

#include <SimpleAmqpClient/SimpleAmqpClient.h>

namespace RGT::Management
{

class TestHandler : public Poco::Net::HTTPRequestHandler
{
public:
    TestHandler(AmqpClient::Channel & channel) : channel_{channel}
    {
    }

private:
    virtual void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) final
    try
    {
        std::string payload = "hello from management";
        auto rabbitRequest = AmqpClient::BasicMessage::Create(payload);

        rabbitRequest->ReplyTo("management_notifications");  // Куда ждём ответ
        rabbitRequest->CorrelationId("req_001");   // ID запроса для сопоставления

        channel_.BasicPublish("", "analytics_tasks", rabbitRequest);

        std::cout << "[SEND] -> " << "analytics_tasks" << ": " << payload << "\n";

        std::string consumer_tag = channel_.BasicConsume(
            "management_notifications", "", false, false, false
        );
        
        std::cout << "⏳ Waiting for analytics response...\n";

        AmqpClient::Envelope::ptr_t envelope;
        if (channel_.BasicConsumeMessage(consumer_tag, envelope, 30000)) {
            // Проверяем correlation_id (на случай, если очередь общая)
            if (envelope->Message()->CorrelationId() == "req_001") {
                std::cout << "[RECV] <- " << "management_notifications" << ": " 
                        << envelope->Message()->Body() << "\n";
                // Ack не нужен, если no_ack=true, но у нас false — подтверждаем
                channel_.BasicAck(envelope);
            }
        } else {
            std::cerr << "[TIMEOUT] No response received\n";
        }

        Poco::JSON::Object json;
        json.set("status", "OK");
        json.set("message", "OK");

        std::ostream & out = response.send();
        json.stringify(out);
    }
    catch (std::exception & e)
    {
        std::cerr << e.what() << '\n';
    }

private:
    AmqpClient::Channel & channel_;
};

} // namespace RGT::Management

#endif // __TEST_HANDLER_H__
