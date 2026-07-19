#pragma once

/**
 * Pure audio parsing and classification (no process, no filesystem).
 *
 * All functions are deterministic given their inputs — ideal for unit
 * tests and for FreeBSD/Linux backends that feed them live text.
 */

#include "audio-types.hpp"

#include <string>
#include <vector>

namespace wf_audio
{
namespace detail
{

/** Parse sndstat capability token: "(play)", "(rec)", "(play/rec)". */
DeviceCapability parse_cap(const std::string& paren);

/** Classify device kind from description text (hdmi, usb, digital, analog). */
std::string kind_from_desc(const std::string& desc);

/**
 * Human-readable label for pcm unit.
 * @location is typically "nid=5" from sysctl (may be empty).
 */
std::string friendly_description(int unit, const std::string& raw_desc,
    const std::string& kind, const std::string& location = {});

/**
 * Parse /dev/sndstat text. Does not call access(); path_ok defaults true.
 * Callers may fix up path_ok via path_exists after parse.
 */
std::vector<AudioDevice> parse_sndstat_text(const std::string& text);

/** Filter devices by play/record capability. */
std::vector<AudioDevice> filter_role(const std::vector<AudioDevice>& all, DeviceListRole role);

/** Parse `pactl list short sinks|sources` output. */
std::vector<AudioDevice> parse_pactl_short(const std::string& text, const std::string& kind);

/** Mark default by id (after get-default-sink/source). */
void mark_default_device(std::vector<AudioDevice>& devices, const std::string& default_id);

/** Drop *.monitor sources when include_monitors is false. */
std::vector<AudioDevice> filter_monitors(std::vector<AudioDevice> devices, bool include_monitors);

/** Trim trailing CR/LF from a sysctl/pactl one-liner. */
std::string rtrim_newlines(std::string s);

/**
 * Parse virtual_oss_cmd status text into fields on @st.
 * Sets running=true only if status looks valid.
 * Does not set path_ok (caller probes filesystem).
 */
void parse_virtual_oss_status_text(const std::string& text, VirtualOssStatus& st);

/** True if virtual_oss_cmd output looks like a live status dump. */
bool virtual_oss_status_looks_valid(const std::string& text);

} // namespace detail
} // namespace wf_audio
