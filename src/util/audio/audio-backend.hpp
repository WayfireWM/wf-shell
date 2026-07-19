#pragma once

/**
 * Audio backend — abstract platform interface (Factory product).
 *
 * Application code (panel volume popover) talks only to IAudioBackend.
 * FreeBSD-centric: **virtual_oss is first-class** when autodetected.
 * OS-specific classes also plug in modular extras:
 *   - logical I/O     (PulseAudio / PipeWire via pactl, …)
 *   - physical devices (OSS pcm* on FreeBSD; Pulse-backed on Linux)
 *   - virtual_oss     (FreeBSD primary; usually absent on Linux — OK)
 *
 * Autodetect with features() + empty lists / available=false.
 * Missing modules must not throw — OpResult{ok=false, message=…}.
 *
 * Construction:
 *   auto backend = wf_audio::AudioBackendFactory::create();
 *   auto backend = wf_audio::AudioBackendBuilder()
 *                      .control_device("/dev/vdsp.ctl")
 *                      .prefer_virtual_oss(true)
 *                      .build();
 *
 * Mirrors WFPowerController::create() and wayfire::create_platform_backend().
 */

#include "audio-types.hpp"

#include <memory>
#include <string>
#include <vector>

namespace wf_audio
{

/**
 * Abstract audio operations used by the shell UI.
 * No GTK, no #ifdef in callers. Never throw into UI code.
 */
class IAudioBackend
{
  public:
    virtual ~IAudioBackend() = default;

    /** Compile/runtime platform tag: "freebsd", "linux", "unknown", … */
    virtual const char *platform_name() const = 0;

    /**
     * Autodetect what is installed and configured on this host.
     * FreeBSD with virtual_oss → virtual_oss=true (first-class UI).
     * Linux Pulse-only → logical_io=true, virtual_oss=false.
     * Always safe; never throws.
     */
    virtual AudioStackFeatures features() = 0;

    /**
     * Physical / OS devices eligible for playback routing.
     * FreeBSD: OSS pcm* for virtual_oss play. Linux: often same as logical.
     * Empty if unavailable — not an error.
     */
    virtual std::vector<AudioDevice> list_playback_devices() = 0;

    /**
     * Physical / OS devices eligible for capture routing.
     * Empty if unavailable — not an error.
     */
    virtual std::vector<AudioDevice> list_capture_devices() = 0;

    /**
     * Logical outputs apps use (e.g. Pulse sinks).
     * May be empty if Pulse/PipeWire is not running.
     */
    virtual std::vector<AudioDevice> list_logical_outputs() = 0;

    /** Logical inputs apps use (e.g. Pulse sources; monitors optional). */
    virtual std::vector<AudioDevice> list_logical_inputs(bool include_monitors) = 0;

    /**
     * First-class FreeBSD: live virtual_oss status when the module exists.
     * Linux / missing: available=false, running=false — normal, not an error.
     */
    virtual VirtualOssStatus virtual_oss_status() = 0;

    /**
     * Route playback to a device path/id.
     * FreeBSD + virtual_oss: play backend. Else: logical default sink if any.
     * Returns OpResult; never throws.
     */
    virtual OpResult set_playback_device(const std::string& device_path) = 0;

    /**
     * Route capture to a device path/id.
     * FreeBSD + virtual_oss: capture backend. Else: logical default source.
     */
    virtual OpResult set_capture_device(const std::string& device_path) = 0;

    /**
     * Optional: set hardware default unit (FreeBSD hw.snd.default_unit).
     * Linux / unsupported: ok=true, message no-op (not a failure).
     */
    virtual OpResult set_hw_default_unit(int unit) = 0;

    /** Best-effort: set default logical sink/source by id (Pulse/PipeWire). */
    virtual OpResult set_default_logical_output(const std::string& id) = 0;
    virtual OpResult set_default_logical_input(const std::string& id) = 0;
};

/**
 * Builder for IAudioBackend (GoF Builder).
 * Collects construction parameters, then materializes via Factory.
 */
class AudioBackendBuilder
{
  public:
    AudioBackendBuilder& control_device(std::string path);
    AudioBackendBuilder& prefer_virtual_oss(bool prefer);
    AudioBackendBuilder& pactl_binary(std::string path);
    AudioBackendBuilder& virtual_oss_cmd_binary(std::string path);

    /** Build the OS-appropriate backend with these options. */
    std::unique_ptr<IAudioBackend> build() const;

    const std::string& control_device() const { return control_device_; }
    bool prefer_virtual_oss() const { return prefer_virtual_oss_; }
    const std::string& pactl_binary() const { return pactl_binary_; }
    const std::string& virtual_oss_cmd_binary() const { return virtual_oss_cmd_binary_; }

  private:
    std::string control_device_ = "/dev/vdsp.ctl";
    bool prefer_virtual_oss_ = true;
    std::string pactl_binary_ = "pactl";
    std::string virtual_oss_cmd_binary_ = "virtual_oss_cmd";
};

/**
 * Abstract Factory entry point (GoF Factory Method).
 * Hides OS selection; only place that may branch on platform.
 */
class AudioBackendFactory
{
  public:
    /** Default backend for this OS (uname/platform macros). */
    static std::unique_ptr<IAudioBackend> create();

    /** Start a builder (fluent configuration). */
    static AudioBackendBuilder builder();
};

} // namespace wf_audio
