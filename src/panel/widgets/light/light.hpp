#include <gtkmm.h>
#ifdef HAVE_DDCUTIL
extern "C" {
  #include "public/ddcutil_c_api.h"
}
#endif
#include "widget.hpp"
#include "animated-scale.hpp"

#define ICON_TARGET_BRIGHTEST "brightest"
#define ICON_TARGET_DIMMEST "dimmest"
#define ICON_TARGET_AVERAGE "average"

enum BrightnessLevel
{
  BRIGHTNESS_LEVEL_LOW,
  BRIGHTNESS_LEVEL_MEDIUM,
  BRIGHTNESS_LEVEL_HIGH,
  BRIGHTNESS_LEVEL_OOR, /* Out of range */
};

const std::map<BrightnessLevel, std::string> brightness_display_icons = {
  {BRIGHTNESS_LEVEL_LOW, "display-brightness-low"},
  {BRIGHTNESS_LEVEL_MEDIUM, "display-brightness-medium"},
  {BRIGHTNESS_LEVEL_HIGH, "display-brightness-high"},
  // this icon seems rare, so probably best to have a generic failure
  {BRIGHTNESS_LEVEL_OOR, "display-brightness-invalid"},
};

class WayfireLight;

class WfLightControl : public Gtk::Box
{
  protected:
    WayfireAnimatedScale scale;
    Gtk::Label label;
    std::map<BrightnessLevel, std::string> icons;
    WayfireLight *parent;

    virtual std::string get_name() = 0;

  public:
    WfLightControl(WayfireLight *parent);

    void set_scale_target_value(double value);
    // a double from 0 to 1 for min to max
    virtual void set_brightness(double brightness) = 0;
    virtual double get_brightness() = 0;

};

class WayfireLight : public WayfireWidget {
  private:
    void init(Gtk::Box *container) override;

    Gtk::Image icon;
    std::unique_ptr<WayfireMenuButton> button;
    Gtk::Popover *popover;
    Gtk::Box box;

    std::vector<WfLightControl*> controls;

    WfOption<std::string> icon_target{"panel/light_icon_target"};
    WfOption<double> scroll_sensitivity{"panel/light_scroll_sensitivity"};
    WfOption<bool> invert_scroll{"panel/light_invert_scroll"};

    void setup_sysfs();
    void setup_ddc();

  public:
    void add_control(WfLightControl *control);

    void update_icon();
};
