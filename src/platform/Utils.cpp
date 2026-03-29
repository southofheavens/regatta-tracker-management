#include <Utils.h>

#include <rabbitmq-c/tcp_socket.h>

namespace
{



} // namespace

namespace RGT::Management
{

const char * RabbitMQSubsystem::name() const
{ return "RabbitMQSubsystem"; }

RabbitMQSubsystem::AmqpConnection & RabbitMQSubsystem::getAmqpConnection(const std::string & connectionId)
{
    try {
        return amqpConnections_.at(connectionId);
    }
    catch (const std::exception & e) 
    {
        throw std::runtime_error(std::format("Error while calling method "
            "RabbitMQSubsystem::getAmqpConnection: {}", e.what()));
    }
}

void RabbitMQSubsystem::initialize(Poco::Util::Application & app) 
{
    Poco::Util::LayeredConfiguration & cfg = app.config();

    std::optional<std::string> host = Devkit::getEnvOrCfg("RABBITMQ_HOST", "rabbitmq.host", cfg);
    if (not host.has_value()) {
        throw std::runtime_error("The host must be specified via environment variable or config");
    }

    std::optional<std::string> strPort = Devkit::getEnvOrCfg("RABBITMQ_PORT", "rabbitmq.port", cfg);
    if (not strPort.has_value()) {
        throw std::runtime_error("The port must be specified via environment variable or config");
    }
    uint16_t port;
    try {
        port = std::stoul(*strPort);
    }
    catch (const std::exception & e) {
        throw std::runtime_error("The port must be specified as an unsigned integer in the range 0 - 65535");
    }

    std::string vhost;
    try {
        vhost = cfg.getString("rabbitmq.vhost");
    }
    catch (const Poco::NotFoundException & e) {
        throw std::runtime_error("The vhost must be specified via config");
    }

    std::optional<std::string> username = Devkit::getEnv("RABBITMQ_USERNAME");
    if (not username.has_value()) {
        throw std::runtime_error("The username must be specified via environment variable");
    }

    std::optional<std::string> password = Devkit::getEnv("RABBITMQ_PASSWORD");
    if (not password.has_value()) {
        throw std::runtime_error("The password must be specified via environment variable");
    }

    // лучше это вынести в конфиг, но чтобы отобразить всевозможные настройки в конфиге,
    // уйдёт куча времени. я позволю себе сделать хардкод, но это очень ужасно и так делать не надо

    amqp_connection_state_t conn = amqp_new_connection();
    if (conn == nullptr) {
        throw std::runtime_error("amqp_connection_state initialize error");
    }
    amqp_socket_t * socket = amqp_tcp_socket_new(conn);
    if (socket == nullptr) {
        throw std::runtime_error("amqp_socket initialize error");
    }
    int socketOpenResult = amqp_socket_open(socket, host->c_str(), port);
    if (socketOpenResult != AMQP_STATUS_OK) {
        throw std::runtime_error("amqp_socket_open error");
    }
    amqp_rpc_reply_t amqpLoginResult = amqp_login(conn, vhost.c_str(), AMQP_DEFAULT_MAX_CHANNELS, 
        AMQP_DEFAULT_FRAME_SIZE, 0, AMQP_SASL_METHOD_PLAIN, username->c_str(), password->c_str());
    if (amqpLoginResult.reply_type != AMQP_RESPONSE_NORMAL) {
        throw std::runtime_error("amqp_login error");
    }
    amqp_channel_open(conn, (amqp_channel_t)1);
    amqp_rpc_reply_t amqpChannelOpenResult = amqp_get_rpc_reply(conn);
    if (amqpChannelOpenResult.reply_type != AMQP_RESPONSE_NORMAL) {
        throw std::runtime_error("amqp_channel_open error");
    }
    amqp_channel_open(conn, (amqp_channel_t)2);
    amqp_rpc_reply_t amqpChannelOpenResult2 = amqp_get_rpc_reply(conn);
    if (amqpChannelOpenResult2.reply_type != AMQP_RESPONSE_NORMAL) {
        throw std::runtime_error("amqp_channel_open 2 error");
    }

    amqp_queue_declare(conn, 1, amqp_cstring_bytes("queue_analytics"), 0, 0, 0, 0, amqp_empty_table);
    amqp_rpc_reply_t queueAnalyticsDeclareResult = amqp_get_rpc_reply(conn);
    if (queueAnalyticsDeclareResult.reply_type != AMQP_RESPONSE_NORMAL) {
        throw std::runtime_error("amqp_queue_declare error");
    }
    amqp_queue_declare(conn, 1, amqp_cstring_bytes("queue_management"), 0, 0, 0, 0, amqp_empty_table);
    amqp_rpc_reply_t queueMgmtDeclareResult = amqp_get_rpc_reply(conn);
    if (queueMgmtDeclareResult.reply_type != AMQP_RESPONSE_NORMAL) {
        throw std::runtime_error("amqp_queue_declare error");
    }

    amqp_queue_declare(conn, 2, amqp_cstring_bytes("queue_analytics_finish"), 0, 0, 0, 0, amqp_empty_table);
    amqp_rpc_reply_t queueAnalyticsFinishDeclareResult = amqp_get_rpc_reply(conn);
    if (queueAnalyticsFinishDeclareResult.reply_type != AMQP_RESPONSE_NORMAL) {
        throw std::runtime_error("amqp_queue_declare finish error");
    }
    amqp_queue_declare(conn, 2, amqp_cstring_bytes("queue_management_finish"), 0, 0, 0, 0, amqp_empty_table);
    amqp_rpc_reply_t queueManagementFinishDeclareResult = amqp_get_rpc_reply(conn);
    if (queueManagementFinishDeclareResult.reply_type != AMQP_RESPONSE_NORMAL) {
        throw std::runtime_error("amqp_queue_declare finish error");
    }

    amqpConnections_.insert({"main", AmqpConnection{.connection = conn, .channel = 1}});
    amqpConnections_.insert({"finish", AmqpConnection{.connection = conn, .channel = 2}});
}

void RabbitMQSubsystem::uninitialize()
{
}

} // namespace RGT::Management
