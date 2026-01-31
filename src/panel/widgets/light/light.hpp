#include <gtkmm.h>
#include <sigc++/connection.h>
#include <filesystem>
#include <thread>
#ifdef HAVE_DDCUTIL
extern "C"{
	#include <ddcutil_c_api.h>
	#include <ddcutil_status_codes.h>
}
#endif

#include "wf-shell-app.hpp"
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
    std::vector<sigc::connection> signals;

    void update_parent_icon();

    WfOption<int> slider_length{"panel/light_slider_length"};

  public:
    WfLightControl(WayfireLight *parent);

    virtual std::string get_name() = 0;
    WayfireLight *get_parent();

    void set_scale_target_value(double value);
    double get_scale_target_value();
    // a double from 0 to 1 for min to max
    virtual void set_brightness(double brightness) = 0;
    virtual double get_brightness() = 0;

};

class LightManager {
  protected:
    LightManager(){}
    // managed widgets
    std::vector<WayfireLight*> widgets;

    virtual void catch_up_widget(WayfireLight *widget) = 0;
    virtual void strip_widget(WayfireLight *widget) = 0;

  public:
    void add_widget(WayfireLight *widget);
    void rem_widget(WayfireLight *widget);
};

// singleton that monitors sysfs and calls the necessary functions
// monitors appearance and deletion of backlight devices
// and the brightness of each of them
class SysfsSurveillor : public LightManager {
  private:
    SysfsSurveillor();
    void handle_inotify_events();
    bool check_perms(std::filesystem::path);
    void add_dev(std::filesystem::path);
    void rem_dev(std::filesystem::path);
    void catch_up_widget(WayfireLight *widget);
    void strip_widget(WayfireLight *widget);

    static inline std::unique_ptr<SysfsSurveillor> instance;

    int fd; // inotify file descriptor

    // stores the data that goes with the inotify watch descriptor (the int)
    // the controls are all the controls which represent this device, to be updated
    std::map<
        int,
        std::pair<
            std::filesystem::path,
            std::vector<std::shared_ptr<WfLightControl>>
        >
    > wd_to_path_controls;

    // watch descriptors for files (so, a device) being added or removed
    int wd_additions, wd_removal;

    // managed widgets
    std::vector<WayfireLight*> widgets;

    // thread on which to run handle_inotify_event on loop
    std::thread inotify_thread;

  public:
    static SysfsSurveillor& get();
};

#ifdef HAVE_DDCUTIL
class DdcaSurveillor : public LightManager {
  private:
    DdcaSurveillor();
    void catch_up_widget(WayfireLight *widget);
    void strip_widget(WayfireLight *widget);

    static inline std::unique_ptr<DdcaSurveillor> instance;

    std::vector<DDCA_Display_Info*> displays_info;

  public:
    static DdcaSurveillor& get();
};
#endif

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
    sigc::connection popover_timeout;


    WfOption<bool> popup_on_change{"panel/light_popup_on_change"};
    WfOption<double> popup_timeout{"panel/light_popup_timeout"};

    bool on_popover_timeout(int timer);

  public:
    WayfireLight(WayfireOutput *output);
    ~WayfireLight();

    WfOption<double> scroll_sensitivity{"panel/light_scroll_sensitivity"};
    WfOption<bool> invert_scroll{"panel/light_invert_scroll"};

    std::shared_ptr<WfLightControl> ctrl_this_display;

    std::vector<std::shared_ptr<WfLightControl>> controls;

    void check_set_popover_timeout();
    void cancel_popover_timeout();

    void add_control(std::shared_ptr<WfLightControl> control);

    void update_icon();
};
