#pragma once

/**
 * Panel volume widget — full sound popover (output + input + Virtual OSS).
 * Pulse volume via GVC; device routing via IAudioBackend (no OS #ifdef here).
 */

#include "../widget.hpp"
#include "wf-popover.hpp"
#include "audio/audio-backend.hpp"

#include <gtkmm/button.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <gtkmm/box.h>
#include <gtkmm/scale.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/separator.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/eventcontrollerscroll.h>
#include <gtkmm/gesturelongpress.h>

#include "../../util/animated-scale.hpp"
#include <pulse/pulseaudio.h>
#include <gvc-mixer-control.h>
#include <wayfire/util/duration.hpp>

#include <memory>
#include <string>
#include <vector>

class WayfireVolume : public WayfireWidget
{
    Gtk::Image main_image;
    Gtk::Box icon_box{Gtk::Orientation::HORIZONTAL};

    /* Popover root */
    Gtk::Box popover_root{Gtk::Orientation::VERTICAL};

    /* Output */
    Gtk::Box out_section{Gtk::Orientation::VERTICAL};
    Gtk::Box out_row{Gtk::Orientation::HORIZONTAL};
    Gtk::Button out_mute_btn;
    Gtk::Image out_mute_icon;
    WayfireAnimatedScale volume_scale;
    Gtk::Label out_pct;
    Gtk::Box out_meter_cap{Gtk::Orientation::HORIZONTAL};
    Gtk::Label out_meter_lbl;
    Gtk::ComboBoxText out_ch_combo;
    Gtk::ComboBoxText out_graph_combo;
    Gtk::DrawingArea out_meter;
    Gtk::ComboBoxText play_combo;

    /* Input */
    Gtk::Box in_section{Gtk::Orientation::VERTICAL};
    Gtk::Box in_row{Gtk::Orientation::HORIZONTAL};
    Gtk::Button in_mute_btn;
    Gtk::Image in_mute_icon;
    WayfireAnimatedScale mic_scale;
    Gtk::Label mic_pct;
    Gtk::Box in_meter_cap{Gtk::Orientation::HORIZONTAL};
    Gtk::Label in_meter_lbl;
    Gtk::ComboBoxText in_graph_combo;
    Gtk::DrawingArea in_meter;
    Gtk::ComboBoxText cap_combo;

    /* Virtual OSS strip (hidden when features().virtual_oss is false) */
    Gtk::Box voss_section{Gtk::Orientation::VERTICAL};
    Gtk::Label voss_title;
    Gtk::Label voss_play_lbl;
    Gtk::Label voss_cap_lbl;
    Gtk::Label voss_fmt_lbl;
    Gtk::Box foot{Gtk::Orientation::HORIZONTAL};
    Gtk::Button adv_btn;

    std::unique_ptr<WayfireMenuWidget> button;
    std::unique_ptr<wf_audio::IAudioBackend> audio_backend;

    WfOption<double> timeout{"panel/volume_display_timeout"};
    WfOption<double> scroll_sensitivity{"panel/volume_scroll_sensitivity"};
    WfOption<std::string> graph_style_out{"panel/volume_graph_style"};
    WfOption<std::string> graph_style_in{"panel/volume_graph_style_in"};
    WfOption<int> out_channels{"panel/volume_out_channels"};
    WfOption<bool> prefer_virtual_oss{"panel/volume_prefer_virtual_oss"};
    WfOption<std::string> play_device_opt{"panel/volume_play_device"};
    WfOption<std::string> capture_device_opt{"panel/volume_capture_device"};

    void on_volume_value_changed();
    void on_mic_value_changed();

    GvcMixerControl *gvc_control = nullptr;
    GvcMixerStream *gvc_stream   = nullptr; /* default sink */
    GvcMixerStream *gvc_source   = nullptr; /* default source */
    gdouble max_norm = PA_VOLUME_NORM;     /* 100% reference for % display */
    gdouble max_amp  = PA_VOLUME_UI_MAX;   /* allow overdrive above 100% */
    gdouble max_norm_src = PA_VOLUME_NORM;
    gdouble max_amp_src  = PA_VOLUME_UI_MAX;

    gulong notify_volume_signal = 0;
    gulong notify_is_muted_signal = 0;
    gulong notify_default_sink_changed = 0;
    gulong notify_src_volume_signal = 0;
    gulong notify_src_muted_signal  = 0;
    gulong notify_default_source_changed = 0;

    std::vector<sigc::connection> signals;
    sigc::connection meter_tick;
    bool popover_open = false;
    bool filling_combos = false;

    std::vector<wf_audio::AudioDevice> play_devices;
    std::vector<wf_audio::AudioDevice> cap_devices;

    /* Real levels from Pulse (sink monitor = Virtual OSS path when that is default).
     * Opaque PeakProbe lives in volume.cpp (avoid incomplete unique_ptr in header). */
    void *out_probe = nullptr; /* PeakProbe* */
    void *in_probe  = nullptr;
    void start_level_probes();
    void stop_level_probes();
    void copy_peaks(bool is_output, float *out, int max_n, int *n_out);

    void disconnect_gvc_stream_signals();
    void disconnect_gvc_source_signals();

    enum set_volume_flags_t
    {
        VOLUME_FLAG_NO_ACTION    = 0,
        VOLUME_FLAG_SHOW_POPOVER = 1,
        VOLUME_FLAG_UPDATE_GVC   = 2,
        VOLUME_FLAG_FULL         = 3,
    };

    void set_volume(pa_volume_t volume_level,
        set_volume_flags_t flags = VOLUME_FLAG_FULL);
    void set_mic_volume(pa_volume_t volume_level,
        set_volume_flags_t flags = VOLUME_FLAG_FULL);

    void build_popover_ui();
    void refresh_devices();
    void refresh_voss_strip();
    void on_play_device_changed();
    void on_cap_device_changed();
    void on_graph_out_changed();
    void on_graph_in_changed();
    void on_out_channels_changed();
    void on_advanced_clicked();
    void on_popover_shown();
    void on_popover_hidden();
    bool on_meter_tick();
    void fill_graph_combo(Gtk::ComboBoxText& combo, const std::string& active_id);
    void fill_channel_combo();
    std::string safe_graph_style(const std::string& s) const;
    void draw_meter(const Cairo::RefPtr<Cairo::Context>& cr, int w, int h,
        double level, bool muted, bool is_output, const std::string& style);

  public:
    void init(Gtk::Box *container) override;
    virtual ~WayfireVolume();

    void update_icon();
    void update_mic_badge();
    void on_volume_changed_external();
    void on_mic_changed_external();
    void on_default_sink_changed();
    void on_default_source_changed();
};
