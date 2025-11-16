#pragma once

#include <string>
#include <map>

enum VolumeLevel
{
    VOLUME_LEVEL_MUTE = 0,
    VOLUME_LEVEL_LOW,
    VOLUME_LEVEL_MED,
    VOLUME_LEVEL_HIGH,
    VOLUME_LEVEL_OOR, /* Out of range */
};

const std::map<VolumeLevel, std::string> icon_name_from_state = {
    {VOLUME_LEVEL_MUTE, "audio-volume-muted"},
    {VOLUME_LEVEL_LOW, "audio-volume-low"},
    {VOLUME_LEVEL_MED, "audio-volume-medium"},
    {VOLUME_LEVEL_HIGH, "audio-volume-high"},
    {VOLUME_LEVEL_OOR, "audio-volume-muted"},
};
