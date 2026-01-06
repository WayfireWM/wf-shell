#include <map>

#include "volume-level.hpp"

enum VolumeLevel
{
    VOLUME_LEVEL_MUTE = 0,
    VOLUME_LEVEL_LOW,
    VOLUME_LEVEL_MED,
    VOLUME_LEVEL_HIGH,
    VOLUME_LEVEL_OOR, /* Out of range */
};

const std::map<VolumeLevel, std::string> volume_icons = {
    {VOLUME_LEVEL_MUTE, "audio-volume-muted"},
    {VOLUME_LEVEL_LOW, "audio-volume-low"},
    {VOLUME_LEVEL_MED, "audio-volume-medium"},
    {VOLUME_LEVEL_HIGH, "audio-volume-high"},
    {VOLUME_LEVEL_OOR, "audio-volume-muted"},
};

// volume is expected to be from 0 to 1
std::string volume_icon_for(double volume)
{
    double max = 1.0;
    auto third = max / 3;
    if (volume == 0)
    {
        return volume_icons.at(VOLUME_LEVEL_MUTE);
    } else if ((volume > 0) && (volume <= third))
    {
        return volume_icons.at(VOLUME_LEVEL_LOW);
    } else if ((volume > third) && (volume <= (third * 2)))
    {
        return volume_icons.at(VOLUME_LEVEL_MED);
    } else if ((volume > (third * 2)) && (volume <= max))
    {
        return volume_icons.at(VOLUME_LEVEL_HIGH);
    }

    return volume_icons.at(VOLUME_LEVEL_OOR);
}
