#include <memory>
#include <glibmm.h>
#include <gtkmm/box.h>

#include "lockergrid.hpp"
#include "volume.hpp"
#include "plugin.hpp"
#include "timedrevealer.hpp"

static void default_sink_changed(GvcMixerControl *gvc_control,
    guint id, gpointer user_data)
{
    WayfireLockerVolumePlugin *plugin = (WayfireLockerVolumePlugin*)user_data;
    plugin->on_default_sink_changed();
}

static void default_source_changed(GvcMixerControl *gvc_control,
    guint id, gpointer user_data)
{
    WayfireLockerVolumePlugin *plugin = (WayfireLockerVolumePlugin*)user_data;
    plugin->on_default_source_changed();
}

static void notify_sink_muted(GvcMixerControl *gvc_control,
    guint id, gpointer user_data)
{
    WayfireLockerVolumePlugin *plugin = (WayfireLockerVolumePlugin*)user_data;
    plugin->update_button_images();
}

static void notify_source_muted(GvcMixerControl *gvc_control,
    guint id, gpointer user_data)
{
    WayfireLockerVolumePlugin *plugin = (WayfireLockerVolumePlugin*)user_data;
    plugin->update_button_images();
}

void WayfireLockerVolumePlugin::update_button_images()
{
    for (auto &it : widgets)
    {
        if (gvc_sink_stream)
        {
            it.second->sink_button.set_icon_name(gvc_mixer_stream_get_is_muted(
                gvc_sink_stream) ? "audio-volume-muted-symbolic" : "audio-volume-high-symbolic");
        }
        if (gvc_source_stream)
        {
            it.second->source_button.set_icon_name(gvc_mixer_stream_get_is_muted(
                gvc_source_stream) ? "microphone-sensitivity-muted-symbolic" : "microphone-sensitivity-high-symbolic");
        }
    }
}

WayfireLockerVolumePlugin::WayfireLockerVolumePlugin():
    WayfireLockerPlugin("locker/volume")
{
    
}

WayfireLockerVolumePluginWidget::WayfireLockerVolumePluginWidget():
    WayfireLockerTimedRevealer("locker/volume_always")
{
    set_child(box);
    sink_button.add_css_class("volume-button");
    source_button.add_css_class("mic-button");

    box.append(source_button);
    box.append(sink_button);
}


void WayfireLockerVolumePlugin::add_output(int id, std::shared_ptr<WayfireLockerGrid> grid)
{
    widgets.emplace(id, new WayfireLockerVolumePluginWidget());
    auto widget = widgets[id];
    grid->attach(*widget, position);

    widget->sink_button.signal_clicked().connect(
        [=] ()
    {
        if (!gvc_sink_stream)
        {
            return;
        }

        bool muted = gvc_mixer_stream_get_is_muted(gvc_sink_stream);
        gvc_mixer_stream_change_is_muted(gvc_sink_stream, !muted);
        gvc_mixer_stream_push_volume(gvc_sink_stream);
    });
    widget->source_button.signal_clicked().connect(
        [=] ()
    {
        if (!gvc_source_stream)
        {
            return;
        }

        bool muted = gvc_mixer_stream_get_is_muted(gvc_source_stream);
        gvc_mixer_stream_change_is_muted(gvc_source_stream, !muted);
        gvc_mixer_stream_push_volume(gvc_source_stream);
    });
    update_button_images();
}

void WayfireLockerVolumePlugin::remove_output(int id, std::shared_ptr<WayfireLockerGrid> grid)
{
    grid->remove(*widgets[id]);
    widgets.erase(id);
}

void WayfireLockerVolumePlugin::init()
{
    /* Setup gvc control */
    gvc_control = gvc_mixer_control_new("Wayfire Volume Control");
    notify_sink_changed = g_signal_connect(gvc_control,
        "default-sink-changed", G_CALLBACK(default_sink_changed), this);
    notify_source_changed = g_signal_connect(gvc_control,
        "default-source-changed", G_CALLBACK(default_source_changed), this);
    gvc_mixer_control_open(gvc_control);
}

void WayfireLockerVolumePlugin::deinit()
{
    disconnect_gvc_stream_sink_signals();
    disconnect_gvc_stream_source_signals();
    if (notify_sink_muted_signal)
    {
        g_signal_handler_disconnect(gvc_sink_stream, notify_sink_muted_signal);
    }
    notify_sink_changed = 0;
    if (notify_sink_muted_signal)
    {
        g_signal_handler_disconnect(gvc_sink_stream, notify_sink_muted_signal);
    }
    notify_source_changed = 0;
}

void WayfireLockerVolumePlugin::disconnect_gvc_stream_sink_signals()
{
    if (notify_sink_muted_signal)
    {
        g_signal_handler_disconnect(gvc_sink_stream, notify_sink_muted_signal);
    }

    notify_sink_muted_signal = 0;
}

void WayfireLockerVolumePlugin::disconnect_gvc_stream_source_signals()
{
    if (notify_source_muted_signal)
    {
        g_signal_handler_disconnect(gvc_source_stream, notify_source_muted_signal);
    }

    notify_source_muted_signal = 0;
}

void WayfireLockerVolumePlugin::on_default_sink_changed()
{
    gvc_sink_stream = gvc_mixer_control_get_default_sink(gvc_control);
    if (!gvc_sink_stream)
    {
        printf("GVC: Failed to get default sink\n");
        return;
    }

    /* Reconnect signals to new sink */
    disconnect_gvc_stream_sink_signals();
    notify_sink_muted_signal = g_signal_connect(gvc_sink_stream, "notify::is-muted",
        G_CALLBACK(notify_sink_muted), this);
    update_button_images();
}

void WayfireLockerVolumePlugin::on_default_source_changed()
{
    gvc_source_stream = gvc_mixer_control_get_default_source(gvc_control);
    if (!gvc_source_stream)
    {
        printf("GVC: Failed to get default source\n");
        return;
    }

    /* Reconnect signals to new source */
    disconnect_gvc_stream_source_signals();
    notify_source_muted_signal = g_signal_connect(gvc_source_stream, "notify::is-muted",
        G_CALLBACK(notify_source_muted), this);
    update_button_images();
}
