#include <algorithm>
#include <iostream>

#include "gvc-proxy.hpp"

GvcCommon::GvcCommon()
{
    gvc_control     = gvc_mixer_control_new("Wf-shell volume control");
    gvc_sink_stream = NULL;
    gvc_source_stream = NULL;

    notify_default_sink_changed = g_signal_connect(
        gvc_control, "default-sink-changed", G_CALLBACK(default_stream_changed),
        GINT_TO_POINTER(static_cast<int>(StreamRole::Sink)));

    notify_default_source_changed = g_signal_connect(
        gvc_control, "default-source-changed", G_CALLBACK(default_stream_changed),
        GINT_TO_POINTER(static_cast<int>(StreamRole::Source)));

    gvc_mixer_control_open(gvc_control);

    if (auto *sink = gvc_mixer_control_get_default_sink(gvc_control))
    {
        default_stream_changed(gvc_control, gvc_mixer_stream_get_id(sink),
            GINT_TO_POINTER(static_cast<int>(StreamRole::Sink)));
    }

    if (auto *source = gvc_mixer_control_get_default_source(gvc_control))
    {
        default_stream_changed(gvc_control, gvc_mixer_stream_get_id(source),
            GINT_TO_POINTER(static_cast<int>(StreamRole::Source)));
    }
}

void GvcCommon::default_stream_muted(GvcMixerControl *gvc_control, guint id, gpointer data)
{
    StreamRole role = static_cast<StreamRole>(GPOINTER_TO_INT(data));
    bool muted = instance->get_muted(role);
    for (auto control : *instance->role_to_controls[role])
    {
        control->on_mute(muted);
    }
}

void GvcCommon::default_stream_volume(GvcMixerControl *gvc_control, guint id, gpointer data)
{
    StreamRole role    = static_cast<StreamRole>(GPOINTER_TO_INT(data));
    pa_volume_t volume = instance->get_volume(role);
    for (auto control : *instance->role_to_controls[role])
    {
        control->on_volume(volume);
    }
}

void GvcCommon::disconnect_stream_signals(StreamRole role)
{
    if ((role == StreamRole::Sink) && gvc_sink_stream)
    {
        if (notify_sink_muted_signal)
        {
            g_signal_handler_disconnect(G_OBJECT(gvc_sink_stream),
                notify_sink_muted_signal);
            notify_sink_muted_signal = 0;
        }

        if (notify_sink_volume_signal)
        {
            g_signal_handler_disconnect(G_OBJECT(gvc_sink_stream),
                notify_sink_volume_signal);
            notify_sink_volume_signal = 0;
        }

        g_object_unref(gvc_sink_stream);
        gvc_sink_stream = nullptr;
    }

    if ((role == StreamRole::Source) && gvc_source_stream)
    {
        if (notify_source_muted_signal)
        {
            g_signal_handler_disconnect(G_OBJECT(gvc_source_stream),
                notify_source_muted_signal);
            notify_source_muted_signal = 0;
        }

        if (notify_source_volume_signal)
        {
            g_signal_handler_disconnect(G_OBJECT(gvc_source_stream),
                notify_source_volume_signal);
            notify_source_volume_signal = 0;
        }

        g_object_unref(gvc_source_stream);
        gvc_source_stream = nullptr;
    }
}

void GvcCommon::default_stream_changed(GvcMixerControl *control, guint id, gpointer data)
{
    StreamRole role = static_cast<StreamRole>(GPOINTER_TO_INT(data));

    GvcMixerStream *stream = GVC_MIXER_STREAM(gvc_mixer_control_lookup_stream_id(control, id));
    if (!stream)
    {
        return;
    }

    g_object_ref(stream);

    instance->disconnect_stream_signals(role);

    if (role == StreamRole::Sink)
    {
        instance->gvc_sink_stream = stream;
        instance->notify_sink_muted_signal =
            g_signal_connect(stream, "notify::is-muted", G_CALLBACK(default_stream_muted),
                GINT_TO_POINTER(static_cast<int>(StreamRole::Sink)));
        instance->notify_sink_volume_signal =
            g_signal_connect(stream, "notify::volume", G_CALLBACK(default_stream_volume),
                GINT_TO_POINTER(static_cast<int>(StreamRole::Sink)));
    } else
    {
        instance->gvc_source_stream = stream;
        instance->notify_source_muted_signal =
            g_signal_connect(stream, "notify::is-muted", G_CALLBACK(default_stream_muted),
                GINT_TO_POINTER(static_cast<int>(StreamRole::Source)));
        instance->notify_source_volume_signal =
            g_signal_connect(stream, "notify::volume", G_CALLBACK(default_stream_volume),
                GINT_TO_POINTER(static_cast<int>(StreamRole::Source)));
    }

    instance->role_to_stream[role] = stream;
}

void GvcCommon::reg_ctrl(GvcControl *ctrl, StreamRole role)
{
    role_to_controls[role]->push_back(ctrl);
}

void GvcCommon::unreg_ctrl(GvcControl *ctrl, StreamRole role)
{
    auto & vec = *role_to_controls[role];
    auto it    = std::find(vec.begin(), vec.end(), ctrl);
    if (it != vec.end())
    {
        vec.erase(it);
    }
}

bool GvcCommon::get_muted(StreamRole role)
{
    GvcMixerStream *stream = role_to_stream[role];
    return stream ? gvc_mixer_stream_get_is_muted(stream) : true;
}

pa_volume_t GvcCommon::get_volume(StreamRole role)
{
    GvcMixerStream *stream = role_to_stream[role];
    return stream ? gvc_mixer_stream_get_volume(stream) : 0;
}

pa_volume_t GvcCommon::get_max(StreamRole role)
{
    return gvc_mixer_control_get_vol_max_norm(gvc_control);
}

void GvcCommon::set_muted(StreamRole role, bool muted)
{
    GvcMixerStream *stream = role_to_stream[role];
    if (!stream)
    {
        return;
    }

    gvc_mixer_stream_change_is_muted(stream, muted);
    gvc_mixer_stream_push_volume(stream);
}

void GvcCommon::set_volume(StreamRole role, pa_volume_t volume)
{
    GvcMixerStream *stream = role_to_stream[role];
    if (!stream)
    {
        return;
    }

    gvc_mixer_stream_set_volume(stream, volume);
    gvc_mixer_stream_push_volume(stream);
}

GvcCommon& GvcCommon::get()
{
    if (!instance)
    {
        instance = std::unique_ptr<GvcCommon>(new GvcCommon());
    }

    return *instance;
}
