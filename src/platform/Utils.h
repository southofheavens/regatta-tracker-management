#ifndef __UTILS_H__
#define __UTILS_H__

#include <iostream>

#include <Poco/Util/Subsystem.h>
#include <Poco/Util/Application.h>

#include <SimpleAmqpClient/SimpleAmqpClient.h>

#include <rgt/devkit/General.h>

namespace RGT::Management
{

class RabbitMQSubsystem : public Poco::Util::Subsystem
{
public:
    virtual const char * name() const final
    { return "RabbitMQSubsystem"; }

    static inline std::string configKey = "rabbitmq_connections";

    AmqpClient::Channel & getChannel(const std::string & channelName)
    {
        try {
            return *channels_.at(channelName);
        }
        catch (const std::exception & e) {
            throw std::runtime_error(std::format("Error while calling method RabbitMQSubsystem::getChannel: {}", e.what()));
        }
    }

private:
    virtual void initialize(Poco::Util::Application & app) final
    {
        Poco::Util::LayeredConfiguration & cfg = app.config();

        Poco::Util::AbstractConfiguration::Keys keys;
        uint16_t connIndex = 0;
        std::string connPrefix = std::format("{}[{}]", RabbitMQSubsystem::configKey, connIndex);
        cfg.keys(connPrefix, keys);
        while (not keys.empty())
        {
            Connection conn = getConnection(cfg, connPrefix);

            for (const Channel & channel : conn.channels)
            {
                AmqpClient::Channel::ptr_t channelPtr = AmqpClient::Channel::Create(
                    conn.host, conn.port, conn.username, conn.password, conn.vhost
                );
                
                for (const std::string & queue : channel.queues) {
                    channelPtr->DeclareQueue(queue); // добавить в json параметры для каждой очереди (бульки из этой функции)
                }

                // мб добавить проверку на то, есть ли уже канал с таким именем, но мне лень
                channels_[channel.name] = channelPtr;
            }

            connIndex++;
            connPrefix = std::format("{}[{}]", RabbitMQSubsystem::configKey, connIndex);
            cfg.keys(connPrefix, keys);
        }

        std::cout << "успешно\n";
    }

	virtual void uninitialize() final
    {
    }

private:
    struct Channel
    {
        std::string name;
        std::vector<std::string> queues;
        uint16_t qos;
        bool noAck;
    };

    struct Connection
    {
        std::string name;
        std::vector<Channel> channels;
        std::string host;
        std::string vhost;
        std::string username;
        std::string password;
        uint16_t port;
    };

    static std::string getUpperString(const std::string & str)
    {
        std::string result = str;
        for (char & c : result) 
        {
            unsigned char uc = static_cast<unsigned char>(c);
            if (isalpha(uc)) {
                c = static_cast<char>(toupper(uc));
            }
        }
        return result;
    }

    Channel getChannel(Poco::Util::LayeredConfiguration & cfg, const std::string & channelPrefix)
    {
        std::string name;
        try {
            name = cfg.getString(channelPrefix + ".name");
        }
        catch (const Poco::NotFoundException & e) {
            throw std::runtime_error(std::format("No name was set for the channel '{}' via the 'name' key", channelPrefix));
        }

        uint16_t qos;
        std::string cfgQosKey = channelPrefix + ".qos";
        try {
            qos = cfg.getUInt16(cfgQosKey);
        }
        catch (const Poco::NotFoundException & e) 
        {
            throw std::runtime_error(std::format("The rabbitmq '{}' channel qos is not set via config ({})",
                name, cfgQosKey));        
        }
        catch (const Poco::Exception & e)
        {
            throw std::runtime_error(std::format("The rabbitmq '{}' channel qos is invalid: qos is a number "
                "in the range 0 – 65535", name));
        }

        bool noAck;
        std::string cfgNoAckKey = channelPrefix + ".no_ack";
        try {
            noAck = cfg.getBool(cfgNoAckKey);
        }
        catch (const Poco::NotFoundException & e) 
        {
            throw std::runtime_error(std::format("The rabbitmq '{}' channel no_ack is not set via config ({})",
                name, cfgNoAckKey));        
        }
        catch (const Poco::Exception & e)
        {
            throw std::runtime_error(std::format("The rabbitmq '{}' channel no_ack is invalid: no_ack is either "
                "true or false ", name));
        }

        Poco::Util::AbstractConfiguration::Keys keys;
        uint16_t queueIndex = 0;
        // Переменная queuePrefix будет иметь значение: "{configKey}[n].channels[p].queues[k]",
        // где n, p, k - целые неотрицательные числа, а {configKey} - статическая переменная класса RabbitMQSubsystem
        std::string queuePrefix = std::format("{}.channels[{}].queues[]", channelPrefix, queueIndex);

        std::vector<std::string> queues;
        while (cfg.has(queuePrefix))
        {
            std::string queue = cfg.getString(queuePrefix);
            queues.push_back(std::move(queue));
            queueIndex++;
            queuePrefix = std::format("{}.channels[{}].queues[]", channelPrefix, queueIndex);
        }

        return Channel
        {
            .name = name,
            .queues = queues,
            .qos = qos,
            .noAck = noAck
        };
    }

    Connection getConnection(Poco::Util::LayeredConfiguration & cfg, const std::string & connPrefix)
    {
        std::string name;
        try {
            name = cfg.getString(connPrefix + ".name");
        }
        catch (const Poco::NotFoundException & e) {
            throw std::runtime_error(std::format("No name was set for the connection '{}' via the 'name' key", connPrefix));
        }

        std::string vhost;
        std::string cfgVhostKey = connPrefix + ".vhost";
        try {
            vhost = cfg.getString(cfgVhostKey);
        }
        catch (const Poco::NotFoundException & e) 
        {
            throw std::runtime_error(std::format("The rabbitmq '{}' connection vhost is not set via config ({})",
                name, cfgVhostKey));        
        }

        std::string envHostKey = getUpperString(std::format("{}_{}_HOST", configKey, name));
        std::string cfgHostKey = connPrefix + ".host";
        std::optional<std::string> host = RGT::Devkit::getEnvOrCfg(envHostKey, cfgHostKey, cfg);
        if (not host.has_value()) 
        {
            throw std::runtime_error(std::format("The rabbitmq '{}' connection host is not set via environment variable ({}) "
                "or config ({})", name, envHostKey, cfgHostKey));
        }

        std::string envPortKey = getUpperString(std::format("{}_{}_PORT", configKey, name));
        std::string cfgPortKey = connPrefix + ".port";
        std::optional<std::string> strPort = RGT::Devkit::getEnvOrCfg(envPortKey, cfgPortKey, cfg);
        if (not strPort.has_value()) 
        {
            throw std::runtime_error(std::format("The rabbitmq '{}' connection port is not set via environment variable ({}) "
                "or config ({})", name, envPortKey, cfgPortKey));
        }
        uint16_t port;
        try {
            port = std::stoul(*strPort);
        }
        catch (std::exception & e) {
            throw std::runtime_error("The rabbitmq 'connection' port must be unsigned integer variable in range 0-65535");
        }

        std::string envUsernameKey = getUpperString(std::format("{}_{}_USERNAME", configKey, name));
        std::optional<std::string> username = RGT::Devkit::getEnv(envUsernameKey);
        if (not username.has_value()) 
        {
            throw std::runtime_error(std::format("The rabbitmq '{}' connection username is not set via environment "
                "variable ({}) ", name, envUsernameKey));
        }

        std::string envPasswordKey = getUpperString(std::format("{}_{}_PASSWORD", configKey, name));
        std::optional<std::string> password = RGT::Devkit::getEnv(envPasswordKey);
        if (not password.has_value()) 
        {
            throw std::runtime_error(std::format("The rabbitmq '{}' connection password is not set via environment "
                "variable ({}) ", name, envPasswordKey));
        }

        Poco::Util::AbstractConfiguration::Keys keys;
        uint16_t channelIndex = 0;
        // Переменная channelPrefix будет иметь значение: "{configKey}[n].channels[p]",
        // где n, p - целые неотрицательные числа, а {configKey} - статическая переменная класса RabbitMQSubsystem
        std::string channelPrefix = std::format("{}.channels[{}]", connPrefix, channelIndex);
        cfg.keys(channelPrefix, keys);

        std::vector<Channel> channels;
        while (not keys.empty())
        {
            Channel channel = getChannel(cfg, channelPrefix);
            channels.push_back(std::move(channel));
            channelIndex++;
            channelPrefix = std::format("{}.channels[{}]", connPrefix, channelIndex);
            cfg.keys(channelPrefix, keys);
        }

        return Connection
        {
            .name = name,
            .channels = channels,
            .host = *host,
            .vhost = vhost,
            .username = *username, 
            .password = *password,
            .port = port
        };
    }

private:
    std::map<std::string, AmqpClient::Channel::ptr_t> channels_;
};

} // namespace RGT::Management

#endif // __UTILS_H__
