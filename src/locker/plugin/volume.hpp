#ifndef LOCKER_VOLUME_PLUGIN_HPP
#define LOCKER_VOLUME_PLUGIN_HPP

#include <memory>
#include <unordered_map>
#include <gtkmm/button.h>
#include <gtkmm/popover.h>
#include <gtkmm/scale.h>
#include <pulse/pulseaudio.h>

#include "gvc-mixer-control.h"
#include "../plugin.hpp"
#include "lockergrid.hpp"

class WayfireLockerVolumePlugin : public WayfireLockerPlugin
{
  private:
    GvcMixerControl *gvc_control;
    GvcMixerStream *gvc_sink_stream   = NULL;
    GvcMixerStream *gvc_source_stream = NULL;
    gulong notify_sink_muted_signal   = 0;
    gulong notify_source_muted_signal = 0;
    void disconnect_gvc_stream_sink_signals();
    void disconnect_gvc_stream_source_signals();

  public:
    WayfireLockerVolumePlugin();
    void add_output(int id, WayfireLockerGrid *grid) override;
    void remove_output(int id) override;
    bool should_enable() override;
    void init() override;
    bool enable;
    void update_button_images();

    /** Called when the default sink changes */
    void on_default_sink_changed();
    void on_default_source_changed();

    std::unordered_map<int, std::shared_ptr<Gtk::Button>> sink_buttons;
    std::unordered_map<int, std::shared_ptr<Gtk::Button>> source_buttons;
};

#endif
