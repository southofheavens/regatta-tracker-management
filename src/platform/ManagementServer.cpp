#include <ManagementServer.h>
#include <ManagementFactory.h>

#include <Poco/Data/PostgreSQL/Connector.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/HTTPServer.h>

#include <rgt/devkit/subsystems/S3Subsystem.h>
#include <rgt/devkit/subsystems/PsqlSubsystem.h>
#include <rgt/devkit/subsystems/RedisSubsystem.h>
#include <rgt/devkit/General.h>

#include <aws/core/Aws.h>

namespace RGT::Management
{

void ManagementServer::initialize(Poco::Util::Application & self)
{
    loadConfiguration();

    RGT::Devkit::readDotEnv();

    Poco::Util::Application::addSubsystem(new RGT::Devkit::Subsystems::PsqlSubsystem());
    Poco::Util::Application::addSubsystem(new RGT::Devkit::Subsystems::S3Subsystem());
    Poco::Util::Application::addSubsystem(new RGT::Devkit::Subsystems::RedisSubsystem());

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
    
    Poco::Net::HTTPServer srv
    (
        new Management::ManagementFactory
        (
            psqlSubsystem.getPool(), redisSubsystem.getPool(), s3Subsystem.getS3Client(), cfg
        ), 
        svs, 
        new Poco::Net::HTTPServerParams
    );

    srv.start();
    
    waitForTerminationRequest();
    
    srv.stop();
    
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
