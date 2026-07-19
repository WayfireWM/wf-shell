#include <gtkmm.h>
#include <glibmm.h>
#include <glibmm/spawn.h>
#include <cmath>
#include <algorithm>
#include <mutex>
#include <cstring>
#include <array>

#include <pulse/pulseaudio.h>
#include <pulse/thread-mainloop.h>

#include "volume.hpp"
#include "icon-select.hpp"
#include "wf-popover.hpp"
#include "wf-shell-app.hpp"

#define ICON(volume) icon_from_range(volume_icons, volume)
#define MIC_ICON(level) icon_from_range(microphone_icons, level)

/**
 * Pulse peak probe — records from a source (sink monitor or mic).
 * On FreeBSD + Virtual OSS: default sink is usually "virtual_oss", so
 * output levels come from "virtual_oss.monitor" (post-app, pre-OSS hardware).
 * Input uses default source, typically "virtual_oss_rec".
 */
struct PeakProbe
{
    pa_threaded_mainloop *ml = nullptr;
    pa_context *ctx = nullptr;
    pa_stream *stream = nullptr;
    std::string source_name;
    std::mutex mu;
    std::array<float, 8> peaks{};
    int nch = 2;
    bool ready = false;

    ~PeakProbe()
    {
        stop();
    }

    void stop()
    {
        if (!ml)
        {
            return;
        }

        pa_threaded_mainloop_lock(ml);
        if (stream)
        {
            pa_stream_disconnect(stream);
            pa_stream_unref(stream);
            stream = nullptr;
        }

        if (ctx)
        {
            pa_context_disconnect(ctx);
            pa_context_unref(ctx);
            ctx = nullptr;
        }

        pa_threaded_mainloop_unlock(ml);
        pa_threaded_mainloop_stop(ml);
        pa_threaded_mainloop_free(ml);
        ml = nullptr;
        ready = false;
        std::lock_guard<std::mutex> lock(mu);
        peaks.fill(0.f);
    }

    static void context_state_cb(pa_context *c, void *userdata)
    {
        auto *self = static_cast<PeakProbe*>(userdata);
        switch (pa_context_get_state(c))
        {
            case PA_CONTEXT_READY:
                self->ready = true;
                pa_threaded_mainloop_signal(self->ml, 0);
                break;
            case PA_CONTEXT_FAILED:
            case PA_CONTEXT_TERMINATED:
                pa_threaded_mainloop_signal(self->ml, 0);
                break;
            default:
                break;
        }
    }

    static void stream_read_cb(pa_stream *s, size_t /*nbytes*/, void *userdata)
    {
        auto *self = static_cast<PeakProbe*>(userdata);
        const void *data = nullptr;
        size_t length = 0;
        if (pa_stream_peek(s, &data, &length) < 0)
        {
            return;
        }

        if (data && (length >= sizeof(float)))
        {
            const float *f = static_cast<const float*>(data);
            int samples = (int)(length / sizeof(float));
            int ch = self->nch > 0 ? self->nch : 2;
            std::lock_guard<std::mutex> lock(self->mu);
            /* PEAK_DETECT: successive floats are channel peaks (or interleaved peaks). */
            for (int c = 0; c < ch && c < 8; c++)
            {
                float p = 0.f;
                if (samples >= ch)
                {
                    /* take max across frames for this channel */
                    for (int i = c; i < samples; i += ch)
                    {
                        p = std::max(p, std::fabs(f[i]));
                    }
                } else if (c < samples)
                {
                    p = std::fabs(f[c]);
                }

                /* attack fast, decay slower so meters track music without flicker */
                float &cur = self->peaks[c];
                if (p > cur)
                {
                    cur = p;
                } else
                {
                    cur = cur * 0.82f + p * 0.18f;
                }

                if (cur < 0.001f)
                {
                    cur = 0.f;
                }
            }
        }

        pa_stream_drop(s);
    }

    static void stream_state_cb(pa_stream *s, void *userdata)
    {
        auto *self = static_cast<PeakProbe*>(userdata);
        switch (pa_stream_get_state(s))
        {
            case PA_STREAM_READY:
            case PA_STREAM_FAILED:
            case PA_STREAM_TERMINATED:
                pa_threaded_mainloop_signal(self->ml, 0);
                break;
            default:
                break;
        }
    }

    bool start(const std::string& source)
    {
        stop();
        if (source.empty())
        {
            return false;
        }

        source_name = source;
        ml = pa_threaded_mainloop_new();
        if (!ml)
        {
            return false;
        }

        pa_mainloop_api *api = pa_threaded_mainloop_get_api(ml);
        ctx = pa_context_new(api, "wf-panel-level-meter");
        if (!ctx)
        {
            pa_threaded_mainloop_free(ml);
            ml = nullptr;
            return false;
        }

        pa_context_set_state_callback(ctx, context_state_cb, this);
        if (pa_context_connect(ctx, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0)
        {
            stop();
            return false;
        }

        pa_threaded_mainloop_lock(ml);
        if (pa_threaded_mainloop_start(ml) < 0)
        {
            pa_threaded_mainloop_unlock(ml);
            stop();
            return false;
        }

        while (pa_context_get_state(ctx) != PA_CONTEXT_READY &&
               pa_context_get_state(ctx) != PA_CONTEXT_FAILED &&
               pa_context_get_state(ctx) != PA_CONTEXT_TERMINATED)
        {
            pa_threaded_mainloop_wait(ml);
        }

        if (pa_context_get_state(ctx) != PA_CONTEXT_READY)
        {
            pa_threaded_mainloop_unlock(ml);
            stop();
            return false;
        }

        pa_sample_spec ss;
        ss.format   = PA_SAMPLE_FLOAT32LE;
        ss.rate     = 25; /* peak rate; PEAK_DETECT compresses to peaks */
        ss.channels = (uint8_t)std::clamp(nch, 1, 8);

        pa_channel_map map;
        pa_channel_map_init_auto(&map, ss.channels, PA_CHANNEL_MAP_DEFAULT);

        stream = pa_stream_new(ctx, "wf-panel-peaks", &ss, &map);
        if (!stream)
        {
            pa_threaded_mainloop_unlock(ml);
            stop();
            return false;
        }

        pa_stream_set_state_callback(stream, stream_state_cb, this);
        pa_stream_set_read_callback(stream, stream_read_cb, this);

        pa_buffer_attr attr{};
        attr.fragsize  = (uint32_t)-1;
        attr.maxlength = (uint32_t)-1;

        int flags = PA_STREAM_PEAK_DETECT | PA_STREAM_ADJUST_LATENCY |
            PA_STREAM_DONT_MOVE;
        if (pa_stream_connect_record(stream, source_name.c_str(), &attr,
                (pa_stream_flags_t)flags) < 0)
        {
            pa_threaded_mainloop_unlock(ml);
            stop();
            return false;
        }

        while (pa_stream_get_state(stream) != PA_STREAM_READY &&
               pa_stream_get_state(stream) != PA_STREAM_FAILED &&
               pa_stream_get_state(stream) != PA_STREAM_TERMINATED)
        {
            pa_threaded_mainloop_wait(ml);
        }

        bool ok = pa_stream_get_state(stream) == PA_STREAM_READY;
        pa_threaded_mainloop_unlock(ml);
        if (!ok)
        {
            stop();
        }

        return ok;
    }

    void get_levels(float *out, int max_n, int *n_out)
    {
        std::lock_guard<std::mutex> lock(mu);
        int n = std::min(nch, max_n);
        if (n_out)
        {
            *n_out = n;
        }

        for (int i = 0; i < n; i++)
        {
            out[i] = peaks[i];
        }
    }
};

void WayfireVolume::update_icon()
{
    if (gvc_stream && gvc_mixer_stream_get_is_muted(gvc_stream))
    {
        main_image.set_from_icon_name(ICON(0));
        out_mute_icon.set_from_icon_name(ICON(0));
        return;
    }

    double frac = max_norm > 0 ?
        volume_scale.get_target_value() / (double)max_norm : 0.0;
    main_image.set_from_icon_name(ICON(frac));
    out_mute_icon.set_from_icon_name(ICON(frac));
}

void WayfireVolume::update_mic_badge()
{
    if (!gvc_source)
    {
        mic_image.set_visible(false);
        mic_pct_badge.set_visible(false);
        in_mute_icon.set_from_icon_name(MIC_ICON(0));
        return;
    }

    mic_image.set_visible(true);
    mic_pct_badge.set_visible(true);

    bool muted = gvc_mixer_stream_get_is_muted(gvc_source);
    double frac = max_norm_src > 0 ?
        mic_scale.get_target_value() / max_norm_src : 0.0;
    frac = std::clamp(frac, 0.0, 1.0);

    if (muted)
    {
        mic_image.set_from_icon_name(MIC_ICON(0));
        in_mute_icon.set_from_icon_name(MIC_ICON(0));
        mic_pct_badge.set_text("mute");
        return;
    }

    mic_image.set_from_icon_name(MIC_ICON(frac));
    in_mute_icon.set_from_icon_name(MIC_ICON(frac));
    int pct = (int)std::lround(frac * 100.0);
    mic_pct_badge.set_text(std::to_string(pct) + "%");
}

void WayfireVolume::set_volume(pa_volume_t volume, set_volume_flags_t flags)
{
    volume_scale.set_target_value(volume);
    if (gvc_stream && (flags & VOLUME_FLAG_UPDATE_GVC))
    {
        gvc_mixer_stream_set_volume(gvc_stream, volume);
        gvc_mixer_stream_push_volume(gvc_stream);
    }

    double frac = max_norm > 0 ? volume / max_norm : 0.0;
    out_pct.set_text(std::to_string((int)std::lround(frac * 100)) + "%");
    update_icon();
}

void WayfireVolume::set_mic_volume(pa_volume_t volume, set_volume_flags_t flags)
{
    mic_scale.set_target_value(volume);
    if (gvc_source && (flags & VOLUME_FLAG_UPDATE_GVC))
    {
        gvc_mixer_stream_set_volume(gvc_source, volume);
        gvc_mixer_stream_push_volume(gvc_source);
    }

    double frac = max_norm_src > 0 ? volume / max_norm_src : 0.0;
    mic_pct.set_text(std::to_string((int)std::lround(frac * 100)) + "%");
    update_mic_badge();
}

void WayfireVolume::on_volume_changed_external()
{
    if (!gvc_stream)
    {
        return;
    }

    auto volume = gvc_mixer_stream_get_volume(gvc_stream);
    if (volume != (pa_volume_t)this->volume_scale.get_target_value())
    {
        set_volume(volume, VOLUME_FLAG_SHOW_POPOVER);
    }

    Glib::signal_idle().connect([=] ()
    {
        button->popup_timed(timeout * 1000);
        return G_SOURCE_REMOVE;
    });
}

void WayfireVolume::on_mic_changed_external()
{
    if (!gvc_source)
    {
        return;
    }

    auto volume = gvc_mixer_stream_get_volume(gvc_source);
    if (volume != (pa_volume_t)this->mic_scale.get_target_value())
    {
        set_mic_volume(volume, VOLUME_FLAG_NO_ACTION);
    }

    update_mic_badge();
}

static void notify_volume(GvcMixerControl *, guint, gpointer user_data)
{
    ((WayfireVolume*)user_data)->on_volume_changed_external();
}

static void notify_is_muted(GvcMixerControl *, guint, gpointer user_data)
{
    auto *wf = (WayfireVolume*)user_data;
    wf->update_icon();
    wf->update_mic_badge();
}

static void notify_src_volume(GvcMixerControl *, guint, gpointer user_data)
{
    ((WayfireVolume*)user_data)->on_mic_changed_external();
}

static void notify_src_muted(GvcMixerControl *, guint, gpointer user_data)
{
    ((WayfireVolume*)user_data)->update_mic_badge();
}

void WayfireVolume::disconnect_gvc_stream_signals()
{
    if (notify_volume_signal && gvc_stream)
    {
        g_signal_handler_disconnect(gvc_stream, notify_volume_signal);
    }

    notify_volume_signal = 0;
    if (notify_is_muted_signal && gvc_stream)
    {
        g_signal_handler_disconnect(gvc_stream, notify_is_muted_signal);
    }

    notify_is_muted_signal = 0;
}

void WayfireVolume::disconnect_gvc_source_signals()
{
    if (notify_src_volume_signal && gvc_source)
    {
        g_signal_handler_disconnect(gvc_source, notify_src_volume_signal);
    }

    notify_src_volume_signal = 0;
    if (notify_src_muted_signal && gvc_source)
    {
        g_signal_handler_disconnect(gvc_source, notify_src_muted_signal);
    }

    notify_src_muted_signal = 0;
}

void WayfireVolume::on_default_sink_changed()
{
    gvc_stream = gvc_mixer_control_get_default_sink(gvc_control);
    if (!gvc_stream)
    {
        return;
    }

    disconnect_gvc_stream_signals();
    notify_volume_signal = g_signal_connect(gvc_stream, "notify::volume",
        G_CALLBACK(notify_volume), this);
    notify_is_muted_signal = g_signal_connect(gvc_stream, "notify::is-muted",
        G_CALLBACK(notify_is_muted), this);

    max_norm = gvc_mixer_control_get_vol_max_norm(gvc_control);
    volume_scale.set_range(0.0, max_norm);
    volume_scale.set_increments(max_norm * scroll_sensitivity,
        max_norm * scroll_sensitivity * 2);
    set_volume(gvc_mixer_stream_get_volume(gvc_stream), VOLUME_FLAG_NO_ACTION);
    if (popover_open)
    {
        start_level_probes();
    }
}

void WayfireVolume::on_default_source_changed()
{
    gvc_source = gvc_mixer_control_get_default_source(gvc_control);
    disconnect_gvc_source_signals();
    if (!gvc_source)
    {
        update_mic_badge();
        return;
    }

    notify_src_volume_signal = g_signal_connect(gvc_source, "notify::volume",
        G_CALLBACK(notify_src_volume), this);
    notify_src_muted_signal = g_signal_connect(gvc_source, "notify::is-muted",
        G_CALLBACK(notify_src_muted), this);

    max_norm_src = gvc_mixer_control_get_vol_max_norm(gvc_control);
    mic_scale.set_range(0.0, max_norm_src);
    mic_scale.set_increments(max_norm_src * scroll_sensitivity,
        max_norm_src * scroll_sensitivity * 2);
    set_mic_volume(gvc_mixer_stream_get_volume(gvc_source), VOLUME_FLAG_NO_ACTION);
    if (popover_open)
    {
        start_level_probes();
    }
}

static void default_sink_changed(GvcMixerControl *, guint, gpointer user_data)
{
    ((WayfireVolume*)user_data)->on_default_sink_changed();
}

static void default_source_changed(GvcMixerControl *, guint, gpointer user_data)
{
    ((WayfireVolume*)user_data)->on_default_source_changed();
}

void WayfireVolume::on_volume_value_changed()
{
    set_volume(volume_scale.get_target_value());
}

void WayfireVolume::on_mic_value_changed()
{
    set_mic_volume(mic_scale.get_target_value());
}

std::string WayfireVolume::safe_graph_style(const std::string& s) const
{
    static const char *ok[] = {
        "bars", "wave", "wave-fill", "mirror", "scope", "spectrum", "dots", "ribbon", nullptr
    };
    for (int i = 0; ok[i]; i++)
    {
        if (s == ok[i])
        {
            return s;
        }
    }

    return "wave-fill";
}

void WayfireVolume::fill_graph_combo(Gtk::ComboBoxText& combo, const std::string& active_id)
{
    filling_combos = true;
    combo.remove_all();
    struct
    {
        const char *id;
        const char *label;
    } styles[] = {
        {"bars", "bars"},
        {"wave", "wave"},
        {"wave-fill", "wave-fill"},
        {"mirror", "mirror"},
        {"scope", "scope"},
        {"spectrum", "spectrum"},
        {"dots", "dots"},
        {"ribbon", "ribbon"},
    };
    std::string want = safe_graph_style(active_id);
    int active = 2; /* wave-fill */
    for (size_t i = 0; i < sizeof(styles) / sizeof(styles[0]); i++)
    {
        combo.append(styles[i].id, styles[i].label);
        if (want == styles[i].id)
        {
            active = (int)i;
        }
    }

    combo.set_active(active);
    filling_combos = false;
}

void WayfireVolume::fill_channel_combo()
{
    filling_combos = true;
    out_ch_combo.remove_all();
    out_ch_combo.append("2", "2 ch");
    out_ch_combo.append("6", "6 ch");
    out_ch_combo.append("8", "8 ch");
    int ch = out_channels.value();
    if (ch == 2)
    {
        out_ch_combo.set_active(0);
    } else if (ch == 6)
    {
        out_ch_combo.set_active(1);
    } else
    {
        out_ch_combo.set_active(2);
    }

    filling_combos = false;
}

void WayfireVolume::draw_meter(const Cairo::RefPtr<Cairo::Context>& cr, int w, int h,
    double level, bool muted, bool is_output, const std::string& style_in)
{
    if ((w <= 0) || (h <= 0))
    {
        return;
    }

    const std::string style = safe_graph_style(style_in);

    cr->set_source_rgb(0.07, 0.07, 0.08);
    cr->rectangle(0, 0, w, h);
    cr->fill();

    if (muted || (level <= 0.001))
    {
        cr->set_source_rgba(0.27, 0.28, 0.35, 0.5);
        cr->set_line_width(1.0);
        cr->move_to(0, h / 2.0);
        cr->line_to(w, h / 2.0);
        cr->stroke();
        return;
    }

    /* Real peaks from Pulse (Virtual OSS: virtual_oss.monitor / virtual_oss_rec). */
    float peaks[8] = {};
    int n_peaks = 0;
    copy_peaks(is_output, peaks, 8, &n_peaks);

    int n = is_output ? std::clamp(out_channels.value(), 2, 8) : 2;
    if ((style == "spectrum") && is_output)
    {
        n = std::max(n * 3, 16);
    } else if (style == "spectrum")
    {
        n = 12;
    }

    /* Map peak channels onto display channels (repeat/expand as needed). */
    auto peak_for = [&] (int ch) -> double
    {
        if (n_peaks <= 0)
        {
            /* No probe yet / silent — show flat zero (not fake waves). */
            return 0.0;
        }

        int src = ch % n_peaks;
        double p = std::clamp((double)peaks[src], 0.0, 1.0);
        /* Soft boost so quiet content is still visible */
        p = std::min(1.0, p * 1.35);
        return p;
    };

    double t = g_get_monotonic_time() / 1e6;

    auto level_color = [&] (double amp, double& r, double& g, double& b)
    {
        r = 0.53;
        g = 0.89;
        b = 0.63;
        if (amp > 0.88)
        {
            r = 0.95;
            g = 0.55;
            b = 0.66;
        } else if (amp > 0.65)
        {
            r = 0.98;
            g = 0.89;
            b = 0.69;
        } else if (is_output)
        {
            r = 0.54;
            g = 0.71;
            b = 0.98;
        }
    };

    if ((style == "bars") || (style == "spectrum"))
    {
        double gap = 2.0;
        double bw  = std::max(2.0, (w - gap * (n + 1)) / n);
        for (int ch = 0; ch < n; ch++)
        {
            double amp = peak_for(ch);
            if (amp < 0.01)
            {
                continue;
            }

            double r, g, b;
            level_color(amp, r, g, b);
            double bh = amp * (h - 4);
            double x  = gap + ch * (bw + gap);
            cr->set_source_rgb(r, g, b);
            cr->rectangle(x, h - 2 - bh, bw, bh);
            cr->fill();
        }

        return;
    }

    /* mid line for wave-like styles */
    cr->set_source_rgba(0.27, 0.28, 0.35, 0.5);
    cr->set_line_width(1.0);
    cr->move_to(0, h / 2.0);
    cr->line_to(w, h / 2.0);
    cr->stroke();

    int traces = n;
    for (int ch = 0; ch < traces; ch++)
    {
        double amp = peak_for(ch);
        if (amp < 0.01)
        {
            continue;
        }

        /* Shape is decorative; amplitude is real peak (not volume %). */
        double phase = ch * 0.7 + t * (1.2 + amp * 2.0);
        double r, g, b;
        level_color(amp, r, g, b);
        double alpha = 0.35 + 0.4 * (1.0 - ch / (double)std::max(traces, 1));

        if (style == "dots")
        {
            cr->set_source_rgba(r, g, b, alpha);
            for (int x = 0; x <= w; x += 6)
            {
                double u = x / (double)w;
                double y = std::sin(u * 6.28 * (1.5 + ch * 0.2) + phase) * amp * (h * 0.4);
                cr->arc(x, h / 2.0 - y, 2.0, 0, 2 * G_PI);
                cr->fill();
            }

            continue;
        }

        if ((style == "wave-fill") || (style == "mirror") || (style == "ribbon"))
        {
            cr->set_source_rgba(r, g, b, alpha);
            cr->move_to(0, h / 2.0);
            for (int x = 0; x <= w; x += 2)
            {
                double u = x / (double)w;
                double y = std::sin(u * 6.28 * (1.5 + ch * 0.2) + phase) * amp * (h * 0.4);
                if (style == "mirror")
                {
                    y = std::abs(y);
                }

                cr->line_to(x, h / 2.0 - y);
            }

            if (style == "mirror")
            {
                for (int x = w; x >= 0; x -= 2)
                {
                    double u = x / (double)w;
                    double y = std::abs(std::sin(u * 6.28 * (1.5 + ch * 0.2) + phase) * amp * (h * 0.4));
                    cr->line_to(x, h / 2.0 + y);
                }
            } else
            {
                for (int x = w; x >= 0; x -= 2)
                {
                    double u = x / (double)w;
                    double y = std::sin(u * 6.28 * (1.5 + ch * 0.2) + phase) * amp * (h * 0.4);
                    cr->line_to(x, h / 2.0 + y * 0.85);
                }
            }

            cr->close_path();
            cr->fill();
            continue;
        }

        /* wave / scope — stroke only */
        cr->set_source_rgba(r, g, b, std::min(1.0, alpha + 0.3));
        cr->set_line_width(style == "scope" ? 1.5 : 1.8);
        bool first = true;
        for (int x = 0; x <= w; x += 2)
        {
            double u = x / (double)w;
            double y = std::sin(u * 6.28 * (1.5 + ch * 0.2) + phase) * amp * (h * 0.4);
            if (first)
            {
                cr->move_to(x, h / 2.0 - y);
                first = false;
            } else
            {
                cr->line_to(x, h / 2.0 - y);
            }
        }

        cr->stroke();
    }

    (void)level; /* volume slider level no longer drives fake amplitude */
}

bool WayfireVolume::on_meter_tick()
{
    if (!popover_open)
    {
        return true;
    }

    out_meter.queue_draw();
    in_meter.queue_draw();
    return true;
}

void WayfireVolume::refresh_voss_strip()
{
    if (!audio_backend)
    {
        voss_section.set_visible(false);
        return;
    }

    try
    {
        auto feat = audio_backend->features();
        bool show = feat.virtual_oss && prefer_virtual_oss.value();
        voss_section.set_visible(show);
        if (!show)
        {
            return;
        }

        auto st = audio_backend->virtual_oss_status();
        std::string live = st.running ? "● running" : "○ not running";
        voss_title.set_text("Virtual OSS  " + live);

        std::string play = st.play_path.empty() ? "—" : st.play_path;
        if (!st.play_path_ok && !st.play_path.empty())
        {
            play += " (missing)";
        }

        std::string cap = st.record_path.empty() ? "—" : st.record_path;
        if (!st.record_path_ok && !st.record_path.empty())
        {
            cap += " (missing)";
        }

        voss_play_lbl.set_text("Play  " + play);
        voss_cap_lbl.set_text("Capture  " + cap);
        voss_fmt_lbl.set_text(
            std::to_string(st.sample_rate) + " Hz · " +
            std::to_string(st.bits) + "-bit · " +
            std::to_string(st.channels) + " ch");
    } catch (...)
    {
        voss_section.set_visible(false);
    }
}

void WayfireVolume::refresh_devices()
{
    if (!audio_backend)
    {
        return;
    }

    filling_combos = true;
    try
    {
        auto feat = audio_backend->features();
        play_devices.clear();
        cap_devices.clear();
        play_combo.remove_all();
        cap_combo.remove_all();

        if (feat.virtual_oss && prefer_virtual_oss.value())
        {
            play_devices = audio_backend->list_playback_devices();
            cap_devices  = audio_backend->list_capture_devices();
        } else if (feat.logical_io)
        {
            play_devices = audio_backend->list_logical_outputs();
            cap_devices  = audio_backend->list_logical_inputs(false);
        } else
        {
            play_devices = audio_backend->list_playback_devices();
            cap_devices  = audio_backend->list_capture_devices();
        }

        auto st = audio_backend->virtual_oss_status();
        std::string cur_play = play_device_opt.value();
        std::string cur_cap  = capture_device_opt.value();
        if (cur_play.empty() && st.running)
        {
            cur_play = st.play_path;
        }

        if (cur_cap.empty() && st.running)
        {
            cur_cap = st.record_path;
        }

        int play_active = 0;
        for (size_t i = 0; i < play_devices.size(); i++)
        {
            const auto& d = play_devices[i];
            std::string label = d.description.empty() ? d.id : d.description;
            if (!d.path_ok)
            {
                label += " (missing)";
            }

            play_combo.append(d.path, label);
            if (!cur_play.empty() && (d.path == cur_play || d.id == cur_play))
            {
                play_active = (int)i;
            }
        }

        int cap_active = 0;
        for (size_t i = 0; i < cap_devices.size(); i++)
        {
            const auto& d = cap_devices[i];
            std::string label = d.description.empty() ? d.id : d.description;
            if (!d.path_ok)
            {
                label += " (missing)";
            }

            cap_combo.append(d.path, label);
            if (!cur_cap.empty() && (d.path == cur_cap || d.id == cur_cap))
            {
                cap_active = (int)i;
            }
        }

        if (!play_devices.empty())
        {
            play_combo.set_active(play_active);
        }

        if (!cap_devices.empty())
        {
            cap_combo.set_active(cap_active);
        }
    } catch (...)
    {
        /* keep empty combos */
    }

    filling_combos = false;
    refresh_voss_strip();
}

void WayfireVolume::on_play_device_changed()
{
    if (filling_combos || !audio_backend)
    {
        return;
    }

    auto id = play_combo.get_active_id();
    if (id.empty())
    {
        return;
    }

    try
    {
        auto r = audio_backend->set_playback_device(id);
        if (r.ok)
        {
            if (auto opt = WayfireShellApp::get().config.get_option("panel/volume_play_device"))
            {
                opt->set_value_str(id);
            }
        }

        /* always re-read status; never throw into UI */
        refresh_voss_strip();
    } catch (...)
    {}
}

void WayfireVolume::on_cap_device_changed()
{
    if (filling_combos || !audio_backend)
    {
        return;
    }

    auto id = cap_combo.get_active_id();
    if (id.empty())
    {
        return;
    }

    try
    {
        auto r = audio_backend->set_capture_device(id);
        if (r.ok)
        {
            if (auto opt = WayfireShellApp::get().config.get_option("panel/volume_capture_device"))
            {
                opt->set_value_str(id);
            }
        }

        refresh_voss_strip();
    } catch (...)
    {}
}

void WayfireVolume::on_advanced_clicked()
{
    try
    {
        Glib::spawn_command_line_async("pavucontrol");
    } catch (...)
    {
        try
        {
            Glib::spawn_command_line_async("pavucontrol-qt");
        } catch (...)
        {}
    }
}

void WayfireVolume::start_level_probes()
{
    stop_level_probes();

    /* Output: monitor of default sink → for Virtual OSS this is virtual_oss.monitor */
    std::string mon;
    if (gvc_stream)
    {
        const char *sink = gvc_mixer_stream_get_name(gvc_stream);
        if (sink && sink[0])
        {
            mon = std::string(sink) + ".monitor";
        }
    }

    if (mon.empty())
    {
        mon = "virtual_oss.monitor";
    }

    auto *op = new PeakProbe();
    op->nch = std::clamp(out_channels.value(), 2, 8);
    if (!op->start(mon) && !op->start("virtual_oss.monitor"))
    {
        delete op;
        op = nullptr;
    }

    out_probe = op;

    /* Input: default source → typically virtual_oss_rec */
    std::string src;
    if (gvc_source)
    {
        const char *name = gvc_mixer_stream_get_name(gvc_source);
        if (name && name[0])
        {
            src = name;
        }
    }

    if (src.empty())
    {
        src = "virtual_oss_rec";
    }

    auto *ip = new PeakProbe();
    ip->nch = 2;
    if (!ip->start(src))
    {
        delete ip;
        ip = nullptr;
    }

    in_probe = ip;
}

void WayfireVolume::stop_level_probes()
{
    if (out_probe)
    {
        delete static_cast<PeakProbe*>(out_probe);
        out_probe = nullptr;
    }

    if (in_probe)
    {
        delete static_cast<PeakProbe*>(in_probe);
        in_probe = nullptr;
    }
}

void WayfireVolume::copy_peaks(bool is_output, float *out, int max_n, int *n_out)
{
    auto *p = static_cast<PeakProbe*>(is_output ? out_probe : in_probe);
    if (!p)
    {
        if (n_out)
        {
            *n_out = 0;
        }

        for (int i = 0; i < max_n; i++)
        {
            out[i] = 0.f;
        }

        return;
    }

    p->get_levels(out, max_n, n_out);
}

void WayfireVolume::on_popover_shown()
{
    popover_open = true;
    refresh_devices();
    start_level_probes();
    if (!meter_tick)
    {
        meter_tick = Glib::signal_timeout().connect(
            sigc::mem_fun(*this, &WayfireVolume::on_meter_tick), 40);
    }
}

void WayfireVolume::on_popover_hidden()
{
    popover_open = false;
    if (meter_tick)
    {
        meter_tick.disconnect();
    }

    stop_level_probes();
}

void WayfireVolume::build_popover_ui()
{
    popover_root.set_spacing(8);
    popover_root.set_margin(10);
    popover_root.set_size_request(320, -1);

    auto head = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    auto title = Gtk::make_managed<Gtk::Label>();
    title->set_markup("<b>Sound Settings</b>");
    title->set_halign(Gtk::Align::START);
    title->set_hexpand(true);
    head->append(*title);
    popover_root.append(*head);
    popover_root.append(*Gtk::make_managed<Gtk::Separator>());

    /* Output */
    auto out_title = Gtk::make_managed<Gtk::Label>("OUTPUT");
    out_title->set_halign(Gtk::Align::START);
    out_title->add_css_class("dim-label");
    out_section.set_spacing(6);
    out_section.append(*out_title);

    out_mute_icon.set_from_icon_name("audio-volume-high-symbolic");
    out_mute_btn.set_child(out_mute_icon);
    out_mute_btn.set_tooltip_text("Mute output");
    out_mute_btn.set_has_frame(false);
    volume_scale.set_draw_value(false);
    volume_scale.set_hexpand(true);
    volume_scale.set_size_request(180, 0);
    out_pct.set_width_chars(4);
    out_row.set_spacing(8);
    out_row.append(out_mute_btn);
    out_row.append(volume_scale);
    out_row.append(out_pct);
    out_section.append(out_row);

    /* Meter caption: label + channel count + independent graph style */
    out_meter_lbl.set_text("Output levels");
    out_meter_lbl.set_halign(Gtk::Align::START);
    out_meter_lbl.set_hexpand(true);
    out_meter_lbl.add_css_class("dim-label");
    out_ch_combo.set_tooltip_text("Output meter channels");
    out_graph_combo.set_tooltip_text("Output graph style");
    fill_channel_combo();
    fill_graph_combo(out_graph_combo, graph_style_out.value());
    out_meter_cap.set_spacing(4);
    out_meter_cap.append(out_meter_lbl);
    out_meter_cap.append(out_ch_combo);
    out_meter_cap.append(out_graph_combo);
    out_section.append(out_meter_cap);

    out_meter.set_content_width(300);
    out_meter.set_content_height(48);
    out_meter.set_draw_func([this] (const Cairo::RefPtr<Cairo::Context>& cr, int w, int h)
    {
        double level = max_norm > 0 ? volume_scale.get_target_value() / max_norm : 0.0;
        bool muted = gvc_stream && gvc_mixer_stream_get_is_muted(gvc_stream);
        draw_meter(cr, w, h, level, muted, true, graph_style_out.value());
    });
    out_section.append(out_meter);
    play_combo.set_hexpand(true);
    out_section.append(play_combo);
    popover_root.append(out_section);

    popover_root.append(*Gtk::make_managed<Gtk::Separator>());

    /* Input */
    auto in_title = Gtk::make_managed<Gtk::Label>("INPUT");
    in_title->set_halign(Gtk::Align::START);
    in_title->add_css_class("dim-label");
    in_section.set_spacing(6);
    in_section.append(*in_title);

    in_mute_icon.set_from_icon_name("audio-input-microphone-symbolic");
    in_mute_btn.set_child(in_mute_icon);
    in_mute_btn.set_tooltip_text("Mute microphone");
    in_mute_btn.set_has_frame(false);
    mic_scale.set_draw_value(false);
    mic_scale.set_hexpand(true);
    mic_scale.set_size_request(180, 0);
    mic_pct.set_width_chars(4);
    in_row.set_spacing(8);
    in_row.append(in_mute_btn);
    in_row.append(mic_scale);
    in_row.append(mic_pct);
    in_section.append(in_row);

    in_meter_lbl.set_text("Input levels");
    in_meter_lbl.set_halign(Gtk::Align::START);
    in_meter_lbl.set_hexpand(true);
    in_meter_lbl.add_css_class("dim-label");
    in_graph_combo.set_tooltip_text("Input graph style (independent of output)");
    fill_graph_combo(in_graph_combo, graph_style_in.value());
    in_meter_cap.set_spacing(4);
    in_meter_cap.append(in_meter_lbl);
    in_meter_cap.append(in_graph_combo);
    in_section.append(in_meter_cap);

    in_meter.set_content_width(300);
    in_meter.set_content_height(40);
    in_meter.set_draw_func([this] (const Cairo::RefPtr<Cairo::Context>& cr, int w, int h)
    {
        double level = max_norm_src > 0 ? mic_scale.get_target_value() / max_norm_src : 0.0;
        bool muted = gvc_source && gvc_mixer_stream_get_is_muted(gvc_source);
        draw_meter(cr, w, h, level, muted, false, graph_style_in.value());
    });
    in_section.append(in_meter);
    cap_combo.set_hexpand(true);
    in_section.append(cap_combo);
    popover_root.append(in_section);

    /* Virtual OSS */
    voss_section.set_spacing(4);
    voss_section.set_margin_top(4);
    voss_title.set_halign(Gtk::Align::START);
    voss_play_lbl.set_halign(Gtk::Align::START);
    voss_cap_lbl.set_halign(Gtk::Align::START);
    voss_fmt_lbl.set_halign(Gtk::Align::START);
    voss_fmt_lbl.add_css_class("dim-label");
    voss_section.append(voss_title);
    voss_section.append(voss_play_lbl);
    voss_section.append(voss_cap_lbl);
    voss_section.append(voss_fmt_lbl);
    voss_section.set_visible(false);
    popover_root.append(voss_section);

    /* Footer: Advanced only — Virtual OSS status is already in the strip above. */
    foot.set_margin_top(6);
    foot.set_halign(Gtk::Align::END);
    adv_btn.set_label("Advanced…");
    adv_btn.set_has_frame(false);
    foot.append(adv_btn);
    popover_root.append(foot);

    signals.push_back(out_mute_btn.signal_clicked().connect([this] ()
    {
        if (!gvc_stream)
        {
            return;
        }

        bool muted = !gvc_mixer_stream_get_is_muted(gvc_stream);
        gvc_mixer_stream_change_is_muted(gvc_stream, muted);
        gvc_mixer_stream_push_volume(gvc_stream);
        update_icon();
    }));
    signals.push_back(in_mute_btn.signal_clicked().connect([this] ()
    {
        if (!gvc_source)
        {
            return;
        }

        bool muted = !gvc_mixer_stream_get_is_muted(gvc_source);
        gvc_mixer_stream_change_is_muted(gvc_source, muted);
        gvc_mixer_stream_push_volume(gvc_source);
        update_mic_badge();
    }));
    signals.push_back(play_combo.signal_changed().connect(
        sigc::mem_fun(*this, &WayfireVolume::on_play_device_changed)));
    signals.push_back(cap_combo.signal_changed().connect(
        sigc::mem_fun(*this, &WayfireVolume::on_cap_device_changed)));
    signals.push_back(out_graph_combo.signal_changed().connect(
        sigc::mem_fun(*this, &WayfireVolume::on_graph_out_changed)));
    signals.push_back(in_graph_combo.signal_changed().connect(
        sigc::mem_fun(*this, &WayfireVolume::on_graph_in_changed)));
    signals.push_back(out_ch_combo.signal_changed().connect(
        sigc::mem_fun(*this, &WayfireVolume::on_out_channels_changed)));
    signals.push_back(adv_btn.signal_clicked().connect(
        sigc::mem_fun(*this, &WayfireVolume::on_advanced_clicked)));
}

void WayfireVolume::on_graph_out_changed()
{
    if (filling_combos)
    {
        return;
    }

    auto id = safe_graph_style(out_graph_combo.get_active_id());
    if (auto opt = WayfireShellApp::get().config.get_option("panel/volume_graph_style"))
    {
        opt->set_value_str(id);
    }

    out_meter.queue_draw();
}

void WayfireVolume::on_graph_in_changed()
{
    if (filling_combos)
    {
        return;
    }

    auto id = safe_graph_style(in_graph_combo.get_active_id());
    if (auto opt = WayfireShellApp::get().config.get_option("panel/volume_graph_style_in"))
    {
        opt->set_value_str(id);
    }

    in_meter.queue_draw();
}

void WayfireVolume::on_out_channels_changed()
{
    if (filling_combos)
    {
        return;
    }

    auto id = out_ch_combo.get_active_id();
    if (id.empty())
    {
        return;
    }

    if (auto opt = WayfireShellApp::get().config.get_option("panel/volume_out_channels"))
    {
        opt->set_value_str(id);
    }

    out_meter.queue_draw();
}

void WayfireVolume::init(Gtk::Box *container)
{
    try
    {
        audio_backend = wf_audio::AudioBackendFactory::builder()
            .prefer_virtual_oss(prefer_virtual_oss.value())
            .build();
    } catch (...)
    {
        audio_backend.reset();
    }

    main_image.add_css_class("widget-icon");
    mic_image.add_css_class("widget-icon");
    mic_image.set_margin_start(4);
    mic_pct_badge.add_css_class("dim-label");
    mic_pct_badge.set_margin_start(2);
    icon_box.append(main_image);
    icon_box.append(mic_image);
    icon_box.append(mic_pct_badge);

    button = std::make_unique<WayfireMenuWidget>("panel", "volume");
    button->set_keyboard_interactive(false);

    auto middle_click_gesture = Gtk::GestureClick::create();
    auto long_press     = Gtk::GestureLongPress::create();
    auto scroll_gesture = Gtk::EventControllerScroll::create();
    auto scroll_gesture2 = Gtk::EventControllerScroll::create();

    scroll_gesture->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL);
    scroll_gesture->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    scroll_gesture2->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL);
    scroll_gesture2->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    long_press->set_touch_only(true);
    middle_click_gesture->set_button(2);

    gvc_control = gvc_mixer_control_new("Wayfire Volume Control");
    notify_default_sink_changed = g_signal_connect(gvc_control,
        "default-sink-changed", G_CALLBACK(default_sink_changed), this);
    notify_default_source_changed = g_signal_connect(gvc_control,
        "default-source-changed", G_CALLBACK(default_source_changed), this);
    gvc_mixer_control_open(gvc_control);

    auto apply_scroll = [this] (double dy, Gdk::ScrollUnit unit, bool shift_mic)
    {
        double maxn = shift_mic ? max_norm_src : max_norm;
        double cur  = shift_mic ? mic_scale.get_target_value() :
            volume_scale.get_target_value();
        int change = 0;
        if (unit == Gdk::ScrollUnit::WHEEL)
        {
            change = dy * maxn * scroll_sensitivity;
        } else
        {
            change = (dy / 100.0) * maxn * scroll_sensitivity;
        }

        double nv = std::clamp(cur - change, 0.0, maxn);
        if (shift_mic)
        {
            set_mic_volume(nv);
        } else
        {
            set_volume(nv);
        }
    };

    signals.push_back(scroll_gesture->signal_scroll().connect(
        [=] (double, double dy)
    {
        /* Primary scroll = output volume. Mic via popover / future Shift detect. */
        apply_scroll(dy, scroll_gesture->get_unit(), false);
        return true;
    }, true));

    signals.push_back(scroll_gesture2->signal_scroll().connect(
        [=] (double, double dy)
    {
        apply_scroll(dy, scroll_gesture2->get_unit(), false);
        return true;
    }, false));

    signals.push_back(long_press->signal_pressed().connect(
        [=] (double, double)
    {
        if (!gvc_stream)
        {
            return;
        }

        bool muted = !gvc_mixer_stream_get_is_muted(gvc_stream);
        gvc_mixer_stream_change_is_muted(gvc_stream, muted);
        gvc_mixer_stream_push_volume(gvc_stream);
        long_press->set_state(Gtk::EventSequenceState::CLAIMED);
        middle_click_gesture->set_state(Gtk::EventSequenceState::DENIED);
    }));
    signals.push_back(middle_click_gesture->signal_pressed().connect(
        [=] (int, double, double)
    {
        middle_click_gesture->set_state(Gtk::EventSequenceState::CLAIMED);
    }));
    signals.push_back(middle_click_gesture->signal_released().connect(
        [=] (int, double, double)
    {
        if (!gvc_stream)
        {
            return;
        }

        bool muted = !gvc_mixer_stream_get_is_muted(gvc_stream);
        gvc_mixer_stream_change_is_muted(gvc_stream, muted);
        gvc_mixer_stream_push_volume(gvc_stream);
    }));

    volume_scale.set_user_changed_callback([=] () { on_volume_value_changed(); });
    mic_scale.set_user_changed_callback([=] () { on_mic_value_changed(); });
    volume_scale.add_controller(scroll_gesture2);

    build_popover_ui();

    button->add_controller(scroll_gesture);
    button->add_controller(long_press);
    button->add_controller(middle_click_gesture);
    button->open_on(1);

    container->append(*button);
    button->append(icon_box);
    button->set_popup_child(popover_root);
    button->get_scroll().set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);

    signals.push_back(button->signal_popup().connect(
        sigc::mem_fun(*this, &WayfireVolume::on_popover_shown)));
    signals.push_back(button->signal_popdown().connect(
        sigc::mem_fun(*this, &WayfireVolume::on_popover_hidden)));

    /* Initial sink/source if GVC already ready */
    on_default_sink_changed();
    on_default_source_changed();
    refresh_devices();
}

WayfireVolume::~WayfireVolume()
{
    if (meter_tick)
    {
        meter_tick.disconnect();
    }

    stop_level_probes();
    disconnect_gvc_stream_signals();
    disconnect_gvc_source_signals();

    if (notify_default_sink_changed && gvc_control)
    {
        g_signal_handler_disconnect(gvc_control, notify_default_sink_changed);
    }

    if (notify_default_source_changed && gvc_control)
    {
        g_signal_handler_disconnect(gvc_control, notify_default_source_changed);
    }

    if (gvc_control)
    {
        gvc_mixer_control_close(gvc_control);
        g_object_unref(gvc_control);
    }
}
