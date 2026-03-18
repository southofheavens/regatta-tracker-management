#include <ManagementServer.h>
#include <ManagementFactory.h>

#include <Poco/Data/PostgreSQL/Connector.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/HTTPServer.h>

#include <rgt/devkit/Connections.h>

#include <aws/core/Aws.h>

namespace RGT::Management
{

void ManagementServer::initialize(Poco::Util::Application & self)
{
    loadConfiguration();
    ServerApplication::initialize(self);

    Poco::Data::PostgreSQL::Connector::registerConnector();

    Aws::InitAPI(sdkOptions_);

    const Poco::Util::LayeredConfiguration & cfg = ManagementServer::config();

    sessionPool_ = RGT::Devkit::connectToPsql(cfg.getString("psql.host"), cfg.getString("psql.port"),
        cfg.getString("psql.dbname"), cfg.getString("psql.user"),cfg.getString("psql.password"),
        cfg.getUInt16("psql.min_sessions"), cfg.getUInt16("psql.max_sessions"));

    redisPool_ = RGT::Devkit::connectToRedis(cfg.getString("redis.host"), cfg.getString("redis.port"),
        cfg.getUInt16("redis.min_sessions"), cfg.getUInt16("redis.max_sessions"));

    // // TODO Залогировать в месте, где перехватывается ошибка
    // std::string accessKeyId = Poco::Environment::get("AWS_ACCESS_KEY_ID");
    
    // // TODO залогировать в месте, где перехватываестя ошибка
    // std::string secretKey = Poco::Environment::get("AWS_SECRET_ACCESS_KEY");

    // std::string host; 
    // try {
    //     host = Poco::Environment::get("MINIO_HOST");
    // }
    // catch (...)
    // {
    //     // TODO залогировать в месте, где перехватываестя ошибка
    //     host = cfg.getString("minio.host");
    // }

    /**
     * по поводу переменных окружения:
     * 
     * с докером все понятно, но если я хочу запустить микросервис локально?
     * 1. добавление .env - глупость, потому что приложение из контейнера не будет понимать
     * откуда ему читать данные, из этого .env или из переменных окружения, которые мы через
     * докер компоуз прокинули
     * 
     * если локально хочется запустить, то надо будет определить переменные окружения необходимые
     */

    s3Client_ = RGT::Devkit::connectToS3("admin_admin", "admin_admin", "minio:9000", "", Aws::Http::Scheme::HTTPS, false, false);

    std::cout << "Сервачок запущен" << std::endl;
}

void ManagementServer::uninitialize()
{
    Poco::Data::PostgreSQL::Connector::unregisterConnector();

    Aws::ShutdownAPI(sdkOptions_);

    ServerApplication::uninitialize();
}

int ManagementServer::main(const std::vector<std::string>&)
try
{
    Poco::Util::LayeredConfiguration & cfg = ManagementServer::config();

    Poco::Net::ServerSocket svs(cfg.getUInt16("server.port"));
    
    Poco::Net::HTTPServer srv
    (
        new Management::ManagementFactory(*sessionPool_, *redisPool_, *s3Client_, cfg), 
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
