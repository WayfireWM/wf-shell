#pragma once

#include <memory>
#include <map>
#include <vector>
#include <glib.h>

#include <pulse/pulseaudio.h>
#include <gvc-mixer-control.h>

#include "control.hpp"

enum class StreamRole
{
    Sink,
    Source,
};

class GvcControl;

class GvcCommon
{
  private:
    GvcCommon();

    static inline std::unique_ptr<GvcCommon> instance;

    GvcMixerControl *gvc_control    = NULL;
    GvcMixerStream *gvc_sink_stream = NULL;
    GvcMixerStream *gvc_source_stream = NULL;
    std::map<StreamRole, GvcMixerStream*> role_to_stream;

    gulong notify_default_sink_changed   = 0;
    gulong notify_default_source_changed = 0;
    gulong notify_sink_muted_signal    = 0;
    gulong notify_source_muted_signal  = 0;
    gulong notify_sink_volume_signal   = 0;
    gulong notify_source_volume_signal = 0;

    static void default_stream_changed(GvcMixerControl *vc_control, guint id, gpointer data);
    static void default_stream_muted(GvcMixerControl *gvc_control, guint id, gpointer data);
    static void default_stream_volume(GvcMixerControl *gvc_control, guint id, gpointer data);

    std::vector<GvcControl*> sink_controls, source_controls;
    std::map<StreamRole, std::vector<GvcControl*>*> role_to_controls =
    {
        {StreamRole::Sink, &sink_controls},
        {StreamRole::Source, &source_controls}
    };

    void disconnect_stream_signals(StreamRole role);
    void on_default_stream_changed(StreamRole role);

  public:
    void reg_ctrl(GvcControl *ctrl, StreamRole role);
    void unreg_ctrl(GvcControl *ctrl, StreamRole role);

    bool get_muted(StreamRole role);
    pa_volume_t get_volume(StreamRole role);
    pa_volume_t get_max(StreamRole role);

    void set_muted(StreamRole role, bool muted);
    void set_volume(StreamRole role, pa_volume_t volume);

    static GvcCommon& get();
};
