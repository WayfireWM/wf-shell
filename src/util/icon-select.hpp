#include <map>
#include <string>
#include <limits>

std::string icon_from_range(std::map<double, std::string> icons, double value);

// the number in the first term is the maximal value at which this icon will be shown
// selection values of the tables are expected to be ordered from least to greatest

const std::map<double, std::string> volume_icons = {
    {std::numeric_limits<double>::min(), "emblem-unreadable"},
    {0.0, "audio-volume-muted"},
    {0.33, "audio-volume-low"},
    {0.66, "audio-volume-medium"},
    {1.0, "audio-volume-high"},
    {std::numeric_limits<double>::max(), "dialog-warning"}
};
