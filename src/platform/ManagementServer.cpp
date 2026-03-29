#include <ManagementServer.h>
#include <ManagementFactory.h>
#include <Utils.h>

#include <Poco/Data/PostgreSQL/Connector.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPResponse.h>

#include <rgt/devkit/subsystems/S3Subsystem.h>
#include <rgt/devkit/RGTException.h>
#include <rgt/devkit/subsystems/PsqlSubsystem.h>
#include <rgt/devkit/subsystems/RedisSubsystem.h>
#include <rgt/devkit/General.h>

#include <aws/core/Aws.h>

#include <Poco/Util/JSONConfiguration.h>

void Consume(RGT::Management::RabbitMQSubsystem::AmqpConnection & connection)
{
    amqp_basic_consume(connection.connection, connection.channel, amqp_cstring_bytes("queue_management_finish"), 
        amqp_empty_bytes, 0, 0, 0, amqp_empty_table);
    amqp_rpc_reply_t consumeResult = amqp_get_rpc_reply(connection.connection);
    if (consumeResult.reply_type != AMQP_RESPONSE_NORMAL) {
        throw RGT::Devkit::RGTException("consume failed", Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }

    while (true)
    {
        amqp_envelope_t env;
        amqp_maybe_release_buffers(connection.connection);
        amqp_rpc_reply_t consumeMsgResult = amqp_consume_message(connection.connection, &env, nullptr, 0);
        if (consumeMsgResult.reply_type != AMQP_RESPONSE_NORMAL) {
            throw RGT::Devkit::RGTException("consume message failed", Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        }

        std::string receivedMessage((char*)env.message.body.bytes, env.message.body.len);
        std::cout << "ПОЛУЧЕНО СООБЩЕНИЕ: " << receivedMessage << std::endl;
        uint64_t tag = env.delivery_tag;
        amqp_basic_ack(connection.connection, connection.channel, tag, 0);
    }
}

namespace RGT::Management
{

void ManagementServer::initialize(Poco::Util::Application & self)
{
    try
    {
        Poco::Util::JSONConfiguration::Ptr cfg = new Poco::Util::JSONConfiguration("rgt-management.config");
        self.config().add(cfg, PRIO_APPLICATION);
    }
    catch (const Poco::Exception & e) {
        throw std::runtime_error(std::format("Error loading JSON config: {}", e.displayText()));
    }

    RGT::Devkit::readDotEnv();

    Poco::Util::Application::addSubsystem(new RGT::Devkit::Subsystems::PsqlSubsystem());
    Poco::Util::Application::addSubsystem(new RGT::Devkit::Subsystems::S3Subsystem());
    Poco::Util::Application::addSubsystem(new RGT::Devkit::Subsystems::RedisSubsystem());
    Poco::Util::Application::addSubsystem(new RGT::Management::RabbitMQSubsystem());

    ServerApplication::initialize(self);
}

void ManagementServer::uninitialize()
{ ServerApplication::uninitialize(); }

int ManagementServer::main(const std::vector<std::string>&)
try
{
    Poco::Util::LayeredConfiguration & cfg = ManagementServer::config();

    Poco::Net::ServerSocket svs(cfg.getUInt16("server.port"));

    auto & psqlSubsystem = Poco::Util::Application::getSubsystem<Devkit::Subsystems::PsqlSubsystem>();
    auto & s3Subsystem = Poco::Util::Application::getSubsystem<Devkit::Subsystems::S3Subsystem>();
    auto & redisSubsystem = Poco::Util::Application::getSubsystem<Devkit::Subsystems::RedisSubsystem>();
    auto & rabbitmqSubsystem = Poco::Util::Application::getSubsystem<Management::RabbitMQSubsystem>();
    
    Poco::Net::HTTPServer srv
    (
        new Management::ManagementFactory
        (
            psqlSubsystem.getPool(), redisSubsystem.getPool(), s3Subsystem.getS3Client(), cfg, rabbitmqSubsystem.getAmqpConnection("main")
        ), 
        svs, 
        new Poco::Net::HTTPServerParams
    );

    std::thread t(Consume, std::ref(rabbitmqSubsystem.getAmqpConnection("finish")));

    srv.start();
    
    waitForTerminationRequest();
    
    srv.stop();
    
    t.join();

    return Application::EXIT_OK;
}
catch (const Poco::Exception & e) 
{
    std::cerr << e.displayText() << '\n';
    return Application::EXIT_SOFTWARE;
}
catch (const std::exception & e) 
{
    std::cerr << e.what() << '\n';
    return Application::EXIT_SOFTWARE;
}
catch (...) {
    return Application::EXIT_SOFTWARE;
}

} // namespace RGT::Management
