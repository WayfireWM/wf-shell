#pragma once
#include "network.hpp"

class NullNetwork : public Network
{
  public:
    NullNetwork() : Network("/", nullptr)
    {}

    std::string get_name() override
    {
        return "No connection";
    }

    std::string get_icon_name() override
    {
        return "network-offline";
    }

    std::vector<std::string> get_css_classes() override
    {
        return {"none"};
    }
};
