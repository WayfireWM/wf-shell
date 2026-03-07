#include <map>
#include <string>

#define ICONS_VMIN 0.0
#define ICONS_VMAX 1.0

std::string icon_for(std::map<double, std::string> icons, double value);

/*
 *  format for the icon tables :
 *   the number in the first term is the maximal value at which this icon will be shown icons are expected to
 * be ordered by the value at which they are desired a value not between MIN and MAX will result in the first
 * value being returned
 */

const std::map<double, std::string> volume_icons = {
    {-1, "emblem-unreadable"},
    {0, "audio-volume-muted"},
    {ICONS_VMAX / 3, "audio-volume-low"},
    {(ICONS_VMAX / 3) * 2, "audio-volume-medium"},
    {ICONS_VMAX, "audio-volume-high"},
};

const std::map<double, std::string> brightness_display_icons = {
    {-1, "emblem-unreadable"},
    {ICONS_VMAX / 3, "display-brightness-low"},
    {(ICONS_VMAX / 3) * 2, "display-brightness-medium"},
    {ICONS_VMAX, "display-brightness-high"},
};
