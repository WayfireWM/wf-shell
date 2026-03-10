#include <map>
#include <vector>
#include <string>
#include <limits>

std::string icon_from_range(std::map<double, std::vector<std::string>> icons, double value);

// the number in the first term is the maximal value at which this icon will be shown
// selection values of the tables are expected to be ordered from least to greatest
// the vector of icons is the different possibilites of icons to show,
// with the prefered one as the first. This is to be able to use non-standard icons
// if they are present in the current theme, and fall back to a standard one if not.

const std::map<double, std::vector<std::string>> volume_icons = {
    {std::numeric_limits<double>::min(), {"emblem-unreadable"}},
    {0.0, {"audio-volume-muted"}},
    {0.33, {"audio-volume-low"}},
    {0.66, {"audio-volume-medium"}},
    {1.0, {"audio-volume-high"}},
    {std::numeric_limits<double>::max(), {"audio-volume-high-danger", "dialog-warning"}}
};
