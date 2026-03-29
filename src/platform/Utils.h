#ifndef __UTILS_H__
#define __UTILS_H__

#include <iostream>

#include <Poco/Util/Subsystem.h>
#include <Poco/Util/Application.h>

#include <rabbitmq-c/amqp.h>

#include <rgt/devkit/General.h>

namespace RGT::Management
{

class RabbitMQSubsystem : public Poco::Util::Subsystem
{
public:
    virtual const char * name() const final;

    struct AmqpConnection
    {
        amqp_connection_state_t connection;
        amqp_channel_t          channel;
    };
    AmqpConnection & getAmqpConnection(const std::string & connectionId);

private:
    virtual void initialize(Poco::Util::Application & app) final;

	virtual void uninitialize() final;

private:
    std::map<std::string, AmqpConnection> amqpConnections_;
};

} // namespace RGT::Management

#endif // __UTILS_H__
