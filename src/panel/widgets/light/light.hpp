#include "wf-shell-app.hpp"
#include <gtkmm.h>
#ifdef HAVE_DDCUTIL
extern "C" {
  #include "public/ddcutil_c_api.h"
}
#endif
#include "widget.hpp"
#include "animated-scale.hpp"

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

  public:
    WfLightControl(WayfireLight *parent);

    virtual std::string get_name() = 0;

    void set_scale_target_value(double value);
    double get_scale_target_value();
    // a double from 0 to 1 for min to max
    virtual void set_brightness(double brightness) = 0;
    virtual double get_brightness() = 0;

};

class WayfireLight : public WayfireWidget {
  private:
    void init(Gtk::Box *container) override;

    WayfireOutput *output;

    Gtk::Image icon;
    std::unique_ptr<WayfireMenuButton> button;
    Gtk::Popover *popover;
    Gtk::Box box, display_box, other_box;
    Gtk::Label display_label, other_label;
    Gtk::Separator disp_othr_sep;

    WfLightControl *ctrl_this_display = nullptr;
    std::vector<WfLightControl*> controls;

    WfOption<double> scroll_sensitivity{"panel/light_scroll_sensitivity"};
    WfOption<bool> invert_scroll{"panel/light_invert_scroll"};

    void setup_sysfs();
    void setup_ddc();

  public:
    WayfireLight(WayfireOutput *output);

    void add_control(WfLightControl *control);

    void update_icon();
};
