#include "icon-select.hpp"

std::string icon_from_range(std::map<double, std::string> icons, double value)
{
    for (auto pair : icons)
    {
        if (value <= pair.first)
        {
            return pair.second;
        }
    }

    return "";
}
