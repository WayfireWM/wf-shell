#include <map>
#include <string>

#define MIN 0.0
#define MAX 1.0

std::string icon_for(std::map<double, std::string> icons, double value);

/*
  format for the icon tables :
    the number in the first term is the maximal value at which this icon will be shown
    icons are expected to be ordered by the value at which they are desired
    a value not between MIN and MAX will result in the first value being returned
*/

const std::map<double, std::string> volume_icons = {
    {-1, "emblem-unreadable"},
    {0, "audio-volume-muted"},
    {MAX/3, "audio-volume-low"},
    {(MAX/3)*2, "audio-volume-medium"},
    {MAX, "audio-volume-high"},
};

const std::map<double, std::string> brightness_display_icons = {
  {-1, "emblem-unreadable"},
  {MAX/3, "display-brightness-low"},
  {(MAX/3)*2, "display-brightness-medium"},
  {MAX, "display-brightness-high"},
};
