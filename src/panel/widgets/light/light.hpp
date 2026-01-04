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

class WfLightControl : public Gtk::Box
{
  protected:
    WayfireAnimatedScale scale;
    Gtk::Label label;
    std::map<BrightnessLevel, std::string> icons;

    virtual std::string get_name() = 0;

  public:
    WfLightControl();

    // a double from 0 to 1 for min to max
    virtual void set_brightness(double brightness) = 0;
    virtual double get_brightness() = 0;

};

#ifdef HAVE_DDCUTIL
class WfLightDdcControl : public WfLightControl
{
  protected:
    DDCA_Display_Handle dh;
};
#endif

class WfLightFsControl: public WfLightControl
{
  protected:
    std::string path;
    int get_max();

    std::string get_name();

  public:
    WfLightFsControl(std::string path);
    void set_brightness(double brightness);
    double get_brightness();
};

class WayfireLight : public WayfireWidget {
  private:
    void init(Gtk::Box *container) override;

    Gtk::Image icon;
    std::unique_ptr<WayfireMenuButton> button;
    Gtk::Popover *popover;
    Gtk::Box box;

    std::vector<std::unique_ptr<WfLightControl>> controls;

    // WfOption<double> scroll_sensitivity{"panel/light_scroll_sensitivity"};

    void add_control(std::unique_ptr<WfLightControl> control);

    void setup_fs();
    void setup_ddc();

  public:
    void update_icon();
};
