/*
  abstract light control class, which are handled by a widget
  other .cpp files in this directory are backends for different ways of handling them
*/

#include <gtkmm.h>

#include "widget.hpp"
#include "animated-scale.hpp"

enum BrightnessLevel
{
  BRIGHTNESS_LEVEL_LOW,
  BRIGHTNESS_LEVEL_MEDIUM,
  BRIGHTNESS_LEVEL_HIGH,
  BRIGHTNESS_LEVEL_OOR, /* Out of range */
};

const std::map<BrightnessLevel, std::string> icon_name_from_state = {
  {BRIGHTNESS_LEVEL_LOW, "display-brightness-low"},
  {BRIGHTNESS_LEVEL_MEDIUM, "display-brightness-medium"},
  {BRIGHTNESS_LEVEL_HIGH, "display-brightness-high"},
  // this icon seems rare, so probably best to have a generic failure
  {BRIGHTNESS_LEVEL_OOR, "display-brightness-invalid"},
};

class WfLightControl : public Gtk::Box
{
  protected:
    WayfireAnimatedScale scale;
    Gtk::Label label;

    virtual std::string get_name();

  public:
    WfLightControl();

    // a double from 0 to 1 for min to max
    virtual void set_brightness(double brightness);
    virtual double get_brightness();
};

class WayfireLight : public WayfireWidget {
  private:
    void init(Gtk::Box *container) override;
    Gtk::Image icon;

    std::unique_ptr<WayfireMenuButton> button;
    Gtk::Popover *popover;
    Gtk::Box box;

    std::vector<std::unique_ptr<WfLightControl>> controls;

    WfOption<double> scroll_sensitivity{"panel/light_scroll_sensitivity"};

  public:
    void update_icon();
};
