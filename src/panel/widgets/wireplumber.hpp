#ifndef WIDGETS_PIPEWIRE_HPP
#define WIDGETS_PIPEWIRE_HPP

#include "../widget.hpp"
#include "gtkmm/togglebutton.h"
#include "wf-popover.hpp"
#include "wp/proxy-interfaces.h"
#include "animated-scale.hpp"
#include <gtkmm/image.h>
#include <gtkmm/scale.h>
extern "C" {
#include <wp/wp.h>
}
#include <wayfire/util/duration.hpp>
#include <map>

enum class FaceChoice;
enum class ClickAction;

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

  public:
    WfWpControl(WpPipewireObject *obj, WayfireWireplumber *parent_widget);
    WpPipewireObject *object;
    Glib::ustring get_name();
    Gtk::ToggleButton button;
    void set_btn_status_no_callbk(bool state);
    void set_scale_target_value(double volume);
    double get_scale_target_value();
    void update_icon();
    bool is_muted();

    void update_gestures();
    void handle_config_reload();

    WfWpControl *copy();
};

// todo : add a WfWpControlStream class that presents a dropdown to select which sink a stream goes to

// sinks and sources
class WfWpControlDevice : public WfWpControl
{
  private:
    // todo : add port stuff
    sigc::connection def_conn;
    Gtk::Image is_def_icon;

  public:
    WfWpControlDevice(WpPipewireObject *obj, WayfireWireplumber *parent_widget);
    Gtk::ToggleButton default_btn;
    void set_def_status_no_callbk(bool state);
    WfWpControlDevice *copy();
};

class wayfire_config;
class WayfireWireplumber : public WayfireWidget
{
  private:
    Gtk::Image main_image;

    WfOption<double> timeout{"panel/wp_display_timeout"};

    void show_mixer_action();
    void show_face_action();
    void mute_face_action();

    void on_volume_value_changed();
    bool on_popover_timeout(int timer);

    gulong notify_volume_signal   = 0;
    gulong notify_is_muted_signal = 0;
    gulong notify_default_sink_changed = 0;
    sigc::connection popover_timeout;
    sigc::connection volume_changed_signal;

  public:
    void init(Gtk::Box *container) override;

    WfOption<double> scroll_sensitivity{"panel/wp_scroll_sensitivity"};
    WfOption<bool> invert_scroll{"panel/wp_invert_scroll"};
    WfOption<bool> popup_on_change{"panel/wp_popup_on_change"};

    FaceChoice face_choice;
    ClickAction right_click_action, middle_click_action;

    std::unique_ptr<WayfireMenuButton> button;
    Gtk::Popover *popover;

    /*
     *   the « face » is the representation of the audio channel that shows it’s volume level on the widget
     * icon and is concerned by the quick actions.
     *   currently, it is the last channel to have been updated. TODO : add pinning ?
     */
    WfWpControl *face;

    Gtk::Box master_box, sinks_box, sources_box, streams_box;
    // TODO : add a category for stuff that listens to an audio source

    std::map<WpPipewireObject*, WfWpControl*> objects_to_controls;

    /** Update the icon based on volume and muted state of the face widget */
    void update_icon();

    /** Called when the volume changed from outside of the widget */
    void on_volume_changed_external();

    /**
     * Check whether the popover should be auto-hidden, and if yes, start a timer to hide it
     */
    void check_set_popover_timeout();

    void reload_config();
    void handle_config_reload();

    virtual ~WayfireWireplumber();
};

namespace WpCommon
{
static WpCore *core = nullptr;
static WpObjectManager *object_manager;
static WpPlugin *mixer_api;
static WpPlugin *default_nodes_api;

static std::vector<WayfireWireplumber*> widgets;

static void init_wp();
static void catch_up_to_current_state(WayfireWireplumber *widget);
static void on_mixer_plugin_loaded(WpCore *core, GAsyncResult *res, gpointer data);
static void on_default_nodes_plugin_loaded(WpCore *core, GAsyncResult *res, gpointer data);
static void on_all_plugins_loaded();
static void on_om_installed(WpObjectManager *manager, gpointer data);
static void on_object_added(WpObjectManager *manager, gpointer object, gpointer data);
static void on_mixer_changed(gpointer mixer_api, guint id, gpointer data);
static void on_default_nodes_changed(gpointer default_nodes_api, gpointer data);
static void on_object_removed(WpObjectManager *manager, gpointer node, gpointer data);
}

#endif // WIDGETS_PIPEWIRE_HPP
