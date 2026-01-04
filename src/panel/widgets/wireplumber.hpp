#ifndef WIDGETS_PIPEWIRE_HPP
#define WIDGETS_PIPEWIRE_HPP

#include <gtkmm.h>
#include <wp/proxy-interfaces.h>
extern "C" {
#include <wp/wp.h>
}
#include <map>

#include "widget.hpp"
#include "wf-popover.hpp"
#include "animated-scale.hpp"

enum class FaceChoice; // config

class WayfireWireplumber;

class WfWpControl : public Gtk::Grid
{
    // Custom grid to facilitate handling

  protected:
    WayfireAnimatedScale scale;
    Gtk::Label label;
    Gtk::Image volume_icon;
    sigc::connection mute_conn;
    WayfireWireplumber *parent;
    std::shared_ptr<Gtk::GestureClick> middle_click_mute, right_click_mute;
    sigc::connection middle_conn, right_conn;
    bool gestures_initialised = false;
    void update_gestures();

  public:
    WfWpControl(WpPipewireObject *obj, WayfireWireplumber *parent_widget);
    WpPipewireObject *object;
    Gtk::ToggleButton button;
    void set_btn_status_no_callbk(bool state);
    void set_scale_target_value(double volume);
    double get_scale_target_value();
    void update_icon();

    void handle_config_reload();

    std::unique_ptr<WfWpControl> copy();
};

// idea: would be neat to have a WfWpControlStream class that presents a dropdown to select which sink a stream goes to

// sinks and sources: a control with a button to set itself as default for it’s category
class WfWpControlDevice : public WfWpControl
{
  private:
    sigc::connection def_conn;
    Gtk::Image is_def_icon;

  public:
    WfWpControlDevice(WpPipewireObject *obj, WayfireWireplumber *parent_widget);
    Gtk::ToggleButton default_btn;
    void set_def_status_no_callbk(bool state);
    std::unique_ptr<WfWpControlDevice> copy();
};

class WayfireWireplumber : public WayfireWidget
{
  private:
    void init(Gtk::Box *container) override;

    Gtk::Image main_image;

    WfOption<double> timeout{"panel/wp_display_timeout"};

    void on_volume_value_changed();
    bool on_popover_timeout(int timer);

    // signals and gestures for the configurable actions on click
    gulong notify_volume_signal   = 0;
    gulong notify_is_muted_signal = 0;
    gulong notify_default_sink_changed = 0;
    sigc::connection popover_timeout;
    sigc::connection volume_changed_signal;
    bool gestures_initialised = false;
    std::shared_ptr<Gtk::GestureClick> left_click_gesture, middle_click_gesture, right_click_gesture;
    sigc::connection left_conn, middle_conn, right_conn;

    // widgets for the mixer itself
    Gtk::Label output, input, streams;
    Gtk::Separator out_in_wall, in_streams_wall, out_sep, in_sep, streams_sep;

    void reload_config();

  public:

    WfOption<double> scroll_sensitivity{"panel/wp_scroll_sensitivity"};
    WfOption<bool> invert_scroll{"panel/wp_invert_scroll"};
    WfOption<bool> popup_on_change{"panel/wp_popup_on_change"};

    FaceChoice face_choice;

    std::unique_ptr<WayfireMenuButton> button;
    Gtk::Popover *popover;

    /*
     * the "face" is the representation of the audio channel that shows it’s volume
     * level on the widget icon and is concerned by the quick actions.
     * configured by panel/wp_face_choice. idea: add pinning?
     */
    std::unique_ptr<WfWpControl> face;

    Gtk::Box master_box, sinks_box, sources_box, streams_box;
    // idea: add a category for stuff that listens to an audio source

    std::map<WpPipewireObject*, std::unique_ptr<WfWpControl>> objects_to_controls;

    /** Update the icon based on volume and muted state of the face widget */
    void update_icon();

    /**
     * Check whether the popover should be auto-hidden, and if yes, start a timer to hide it
     */
    void check_set_popover_timeout();

    void handle_config_reload() override;

    virtual ~WayfireWireplumber();
};

namespace WpCommon{
  void init_wp();
  void catch_up_to_current_state(WayfireWireplumber *widget);
  void on_mixer_plugin_loaded(WpCore *core, GAsyncResult *res, gpointer data);
  void on_default_nodes_plugin_loaded(WpCore *core, GAsyncResult *res, gpointer data);
  void on_all_plugins_loaded();
  void on_om_installed(WpObjectManager *manager, gpointer data);
  void add_object_to_widget(WpPipewireObject *object, WayfireWireplumber *widget);
  void on_object_added(WpObjectManager *manager, gpointer object, gpointer data);
  void on_mixer_changed(gpointer mixer_api, guint id, gpointer data);
  void on_default_nodes_changed(gpointer default_nodes_api, gpointer data);
  void on_object_removed(WpObjectManager *manager, gpointer node, gpointer data);
}

#endif // WIDGETS_PIPEWIRE_HPP
