#pragma once
#include <memory>
#include <gtkmm/button.h>
#include <gtkmm/popover.h>
#include <gtkmm/scale.h>
#include <pulse/pulseaudio.h>

#include "gvc-mixer-control.h"
#include "plugin.hpp"
#include "lockergrid.hpp"
#include "timedrevealer.hpp"

class WayfireLockerVolumePluginWidget : public WayfireLockerTimedRevealer
{
  public:
    Gtk::Box box;
    Gtk::Button sink_button;
    Gtk::Button source_button;
    WayfireLockerVolumePluginWidget();
};

class WayfireLockerVolumePlugin : public WayfireLockerPlugin
{
  private:
    GvcMixerControl *gvc_control;
    GvcMixerStream *gvc_sink_stream   = NULL;
    GvcMixerStream *gvc_source_stream = NULL;
    gulong notify_sink_muted_signal   = 0;
    gulong notify_source_muted_signal = 0;
    gulong notify_sink_changed   = 0;
    gulong notify_source_changed = 0;
    void disconnect_gvc_stream_sink_signals();
    void disconnect_gvc_stream_source_signals();

  public:
    WayfireLockerVolumePlugin();
    void add_output(int id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void remove_output(int id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void init() override;
    void deinit() override;
    void update_button_images();

    /** Called when the default sink changes */
    void on_default_sink_changed();
    void on_default_source_changed();

    std::map<int, std::shared_ptr<WayfireLockerVolumePluginWidget>> widgets;
};
