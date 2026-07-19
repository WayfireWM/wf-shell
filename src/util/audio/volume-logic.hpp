#pragma once

/**
 * Pure volume UI helpers (no GTK, no Pulse).
 * Used by the panel volume widget and fully unit-tested.
 */

#include "audio-types.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace wf_audio
{
namespace volume_logic
{

/** Allowed graph style ids (null-terminated). */
inline constexpr const char *GRAPH_STYLES[] = {
    "bars", "wave", "wave-fill", "mirror", "scope", "spectrum", "dots", "ribbon", nullptr
};

/** Return @s if known, else "wave-fill". */
inline std::string safe_graph_style(const std::string& s)
{
    for (int i = 0; GRAPH_STYLES[i]; i++)
    {
        if (s == GRAPH_STYLES[i])
        {
            return s;
        }
    }
    return "wave-fill";
}

/** Fingerprint for combo rebuild skip (path + desc + path_ok). */
inline std::string device_list_fingerprint(const std::vector<AudioDevice>& devs)
{
    std::string fp;
    for (const auto& d : devs)
    {
        fp += d.path;
        fp += '\x1f';
        fp += d.description;
        fp += '\x1f';
        fp += d.path_ok ? '1' : '0';
        fp += '\x1e';
    }
    return fp;
}

/** Clamp channel count to supported set {2,6,8}; default 8. */
inline int safe_out_channels(int ch)
{
    if (ch == 2 || ch == 6 || ch == 8)
    {
        return ch;
    }
    return 8;
}

/** Volume fraction 0..max_overdrive (e.g. 1.5 for 150%) from PA volumes. */
inline double volume_fraction(double volume, double max_norm)
{
    if (max_norm <= 0.0)
    {
        return 0.0;
    }
    return volume / max_norm;
}

/** Display percent string without trailing % sign (e.g. "100", "150"). */
inline std::string format_volume_percent(double fraction)
{
    int pct = static_cast<int>(std::lround(fraction * 100.0));
    if (pct < 0)
    {
        pct = 0;
    }
    return std::to_string(pct);
}

/** Map peak channel for display channel index. */
inline double peak_for_channel(const float *peaks, int n_peaks, int ch)
{
    if (!peaks || n_peaks <= 0)
    {
        return 0.0;
    }
    int src = ch % n_peaks;
    double p = std::clamp(static_cast<double>(peaks[src]), 0.0, 1.0);
    return std::min(1.0, p * 1.35);
}

/** Meter color by amplitude (r,g,b out params). */
inline void level_color(double amp, bool is_output, double& r, double& g, double& b)
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
}

/** How many display traces for style/channel settings. */
inline int meter_trace_count(const std::string& style, bool is_output, int out_channels)
{
    int n = is_output ? std::clamp(safe_out_channels(out_channels), 2, 8) : 2;
    std::string s = safe_graph_style(style);
    if (s == "spectrum" && is_output)
    {
        return std::max(n * 3, 16);
    }
    if (s == "spectrum")
    {
        return 12;
    }
    return n;
}

/** VOSS strip fingerprint (for anti-flicker). */
inline std::string voss_strip_fingerprint(bool show, const VirtualOssStatus& st,
    const std::string& label)
{
    if (!show)
    {
        return {};
    }
    return label + '\x1f' + st.play_path + '\x1f' + st.record_path + '\x1f' +
           std::to_string(st.sample_rate) + '\x1f' + std::to_string(st.bits) + '\x1f' +
           std::to_string(st.channels) + '\x1f' + (st.play_path_ok ? '1' : '0') +
           (st.record_path_ok ? '1' : '0') + (st.running ? '1' : '0');
}

/** Combo active selection fingerprint. */
inline std::string selection_fingerprint(const std::string& path)
{
    return path;
}

} // namespace volume_logic
} // namespace wf_audio
