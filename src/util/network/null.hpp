#pragma once
#include "network.hpp"
#include <string>

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

    std::string get_a11y_label() override
    {
        return "Unknown Network Device";
    }
};
