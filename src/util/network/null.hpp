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

    std::string get_color_name() override
    {
        return "none";
    }
};
