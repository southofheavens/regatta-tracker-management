#pragma once

#include <Poco/Util/ServerApplication.h>

namespace RGT::Management
{

class ManagementServer : public Poco::Util::ServerApplication
{
public:
    void initialize(Application & self) final;

    void uninitialize() final;

    int main(const std::vector<std::string> &) final;
};

} // namespace RGT::Management
