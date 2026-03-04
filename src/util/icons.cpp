#include "icons.hpp"

std::string icon_for(std::map<double, std::string> icons, double value)
{
    if ((value > ICONS_VMAX) || (value < ICONS_VMIN))
    {
        return icons.begin()->second;
    }

    for (auto pair : icons)
    {
        if (value <= pair.first)
        {
            return pair.second;
        }
    }

    return "";
}
