#pragma once

#include <cstddef>
#include <memory>
#include <vector>
#include <wp/proxy-interfaces.h>
extern "C" {
#include <wp/wp.h>
}

#include "wp-mixer.hpp"
class WayfireWpMixer;

class WpCommon
{
  private:
    WpCommon();

    static inline std::unique_ptr<WpCommon> instance;

    WpCore *core = nullptr;
    WpObjectManager *object_manager;
    WpPlugin *mixer_api = nullptr;
    WpPlugin *default_nodes_api = nullptr;

    std::vector<WayfireWpMixer*> widgets;

    void catch_up_to_current_state(WayfireWpMixer *widget);
    static void on_mixer_plugin_loaded(WpCore *core, GAsyncResult *res, gpointer data);
    static void on_default_nodes_plugin_loaded(WpCore *core, GAsyncResult *res, gpointer data);
    static void on_all_plugins_loaded();
    void add_object_to_widget(WpPipewireObject *object, WayfireWpMixer *widget);
    static void on_object_added(WpObjectManager *manager, gpointer object, gpointer data);
    static void on_mixer_changed(gpointer mixer_api, guint id, gpointer data);
    static void on_default_nodes_changed(gpointer default_nodes_api, gpointer data);
    static void on_object_removed(WpObjectManager *manager, gpointer node, gpointer data);

  public:

    void add_widget(WayfireWpMixer *widget);
    void rem_widget(WayfireWpMixer *widget);

    void re_evaluate_def_nodes();

    std::pair<double, bool> get_volume_and_mute(guint32 id);
    gboolean set_volume(guint32 id, double volume);
    gboolean set_mute(guint32 id, bool state);
    gboolean set_default(WpPipewireObject *object);

    static WpCommon& get();
};
