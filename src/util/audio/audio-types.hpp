#pragma once

/**
 * Application-agnostic audio domain types.
 *
 * No OS headers here. UI and orchestration depend only on these types;
 * OS-specific backends translate them to Pulse / PipeWire / OSS /
 * virtual_oss / etc. Missing modules are normal (all false / empty) —
 * never treat absence as a crash.
 *
 * Patterns: domain model (TAOCP-style clear data) + interface segregation
 * for Factory / Builder products; modular stack features for the UI.
 */

#include <string>
#include <vector>
#include <cstdint>

namespace wf_audio
{

/** What a physical or virtual device can do. */
enum class DeviceCapability
{
    Play,
    Record,
    PlayRecord,
};

/** High-level role when listing for a dropdown. */
enum class DeviceListRole
{
    Playback, /**< hardware or sink candidates for output routing */
    Capture,  /**< hardware or source candidates for input routing */
};

/**
 * One selectable audio endpoint (PCM, sink name, etc.).
 * @id stable key for UI dropdowns (e.g. "pcm1", "alsa_output.pci…").
 * @path OS path or server name ("/dev/dsp1", "virtual_oss", Pulse sink id).
 *
 * Hotplug: list_* always re-probes. A USB device that was unplugged simply
 * disappears from the next list — never a crash. present/path_ok reflect
 * liveness when known; jack_connected is optional and often unknown.
 */
struct AudioDevice
{
    std::string id;
    std::string path;
    std::string description;
    DeviceCapability capability = DeviceCapability::Play;
    bool is_default = false;
    /** Optional tags for UI: "hdmi", "analog", "pulse", "virtual_oss", "usb", "pipewire". */
    std::string kind;
    /** Still enumerated by the OS (in list ⇒ true on FreeBSD sndstat). */
    bool present = true;
    /** Device node / sink openable when last probed (best-effort). */
    bool path_ok = true;
    /**
     * Analog jack sense when the driver exposes it.
     * -1 = unknown (common), 0 = unplugged, 1 = plugged.
     * Do not treat -1 as failure.
     */
    int jack_connected = -1;
};

/**
 * Autodetected stack modules (IAudioBackend::features()).
 *
 * FreeBSD-centric product: **virtual_oss is first-class** when present.
 * Other modules (Pulse logical I/O, raw OSS devices) plug in around it.
 * Linux / hosts without virtual_oss simply leave that flag false — never crash.
 *
 * Defaults are all false. Probe the live system; do not assume packages.
 *
 * | Host example                         | Flags typically true        |
 * |--------------------------------------|-----------------------------|
 * | FreeBSD + virtual_oss + Pulse        | virtual_oss, physical, logical |
 * | FreeBSD OSS only                     | physical_devices            |
 * | Linux Pulse only                     | logical_io                  |
 * | Unknown / null backend               | (none) — quiet UI           |
 */
struct AudioStackFeatures
{
    /** Pulse/PipeWire (or similar) sinks & sources. */
    bool logical_io = false;
    /** Hardware endpoints (OSS pcm* on FreeBSD; often via Pulse on Linux). */
    bool physical_devices = false;
    /**
     * First-class FreeBSD module: virtual_oss control device present
     * (and preferred). When true, UI shows Virtual OSS manage strip + badge.
     * When false (typical Linux), those panels stay hidden — not an error.
     */
    bool virtual_oss = false;
    /** FreeBSD hw.snd.default_unit. */
    bool hw_default_unit = false;
    /** Mix/open channel control (virtual_oss -C/-c when module live). */
    bool mix_channels = false;
    /** User-facing label when virtual_oss is true (default "Virtual OSS"). */
    std::string virtual_oss_label;
};

/**
 * Live virtual_oss snapshot (first-class on FreeBSD when available).
 * available=false is normal when the module is not installed (e.g. Linux).
 *
 * After USB unplug, running may stay true while play_path_ok becomes false —
 * UI should re-list devices and offer fallback without crashing.
 */
struct VirtualOssStatus
{
    bool available = false; /**< control device node exists */
    bool running   = false; /**< status query succeeded */
    std::string control_device;
    std::string play_path;
    std::string record_path;
    int sample_rate = 0;
    int bits        = 0;
    int channels    = 0;
    /** play_path exists and is accessible (false if USB backend unplugged). */
    bool play_path_ok = false;
    /** record_path exists and is accessible. */
    bool record_path_ok = false;
};

/** Volume 0.0–1.0 plus mute (application-agnostic). */
struct VolumeState
{
    double level = 0.0;
    bool muted   = false;
};

/**
 * Result of a mutating operation.
 * Always return this — never throw across the UI boundary.
 * ok=false + message is the failure path (missing tool, permission, …).
 */
struct OpResult
{
    bool ok = false;
    std::string message;
};

inline bool can_play(DeviceCapability c)
{
    return c == DeviceCapability::Play || c == DeviceCapability::PlayRecord;
}

inline bool can_record(DeviceCapability c)
{
    return c == DeviceCapability::Record || c == DeviceCapability::PlayRecord;
}

} // namespace wf_audio
