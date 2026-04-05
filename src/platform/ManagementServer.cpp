#include <ManagementServer.h>
#include <ManagementFactory.h>
#include <Utils.h>

#include <Poco/Data/PostgreSQL/Connector.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPResponse.h>

#include <RGT/Devkit/RGTException.h>
#include <RGT/Devkit/General.h>
#include <RGT/Devkit/Subsystems/S3Subsystem.h>
#include <RGT/Devkit/Subsystems/PsqlSubsystem.h>
#include <RGT/Devkit/Subsystems/RedisSubsystem.h>
#include <RGT/Devkit/Subsystems/RabbitMQSubsystem.h>

#include <aws/core/Aws.h>

#include <Poco/Util/JSONConfiguration.h>

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
    Poco::Util::Application::addSubsystem(new RGT::Devkit::Subsystems::RabbitMQSubsystem());

    ServerApplication::initialize(self);
}

void ManagementServer::uninitialize()
{ ServerApplication::uninitialize(); }

int ManagementServer::main(const std::vector<std::string>&)
{
    Poco::Util::LayeredConfiguration & cfg = ManagementServer::config();

    Poco::Net::ServerSocket svs(cfg.getUInt16("server.port"));

    auto & psqlSubsystem = Poco::Util::Application::getSubsystem<Devkit::Subsystems::PsqlSubsystem>();
    auto & s3Subsystem = Poco::Util::Application::getSubsystem<Devkit::Subsystems::S3Subsystem>();
    auto & redisSubsystem = Poco::Util::Application::getSubsystem<Devkit::Subsystems::RedisSubsystem>();
    auto & rabbitmqSubsystem = Poco::Util::Application::getSubsystem<Devkit::Subsystems::RabbitMQSubsystem>();
    
    Poco::Net::HTTPServer srv
    (
        new Management::ManagementFactory
        (
            psqlSubsystem.getPool(), redisSubsystem.getPool(), s3Subsystem.getS3Client(), 
            rabbitmqSubsystem.getChannel(), cfg
        ), 
        svs, 
        new Poco::Net::HTTPServerParams
    );

    srv.start();
    
    waitForTerminationRequest();
    
    srv.stop();
    
    return Application::EXIT_OK;
}

} // namespace RGT::Management
