/*
 * FreeBSD audio backend — OSS + virtual_oss + optional pactl.
 *
 * Application code never includes this file; only the factory builds it.
 * Parsing is pure (audio-parse); I/O goes through process hooks.
 */

#include "audio-backend.hpp"
#include "audio-parse.hpp"
#include "audio-process.hpp"
#include "platform.hpp"

#include <cstdlib>
#include <utility>

namespace wf_audio
{
namespace detail
{

namespace
{

std::string sysctl_n(const std::string& name)
{
    std::string out;
    int code = 0;
    if (!run_capture({"sysctl", "-n", name}, out, code) || code != 0)
    {
        return {};
    }
    return rtrim_newlines(std::move(out));
}

std::vector<AudioDevice> parse_sndstat()
{
    auto devices = parse_sndstat_text(read_text_file("/dev/sndstat"));
    for (auto& d : devices)
    {
        /* Re-apply location-aware HDMI labels when sysctl is available. */
        int unit = -1;
        if (d.id.size() > 3)
        {
            unit = std::atoi(d.id.c_str() + 3);
        }
        /* parse_sndstat_text only emits unit >= 0 */
        if (d.kind == "hdmi")
        {
            bool was_default = d.is_default;
            std::string loc  = sysctl_n("dev.pcm." + std::to_string(unit) + ".%location");
            std::string chip = sysctl_n("dev.pcm." + std::to_string(unit) + ".%desc");
            if (chip.empty())
            {
                /* Prefer brand from raw sysctl; otherwise leave empty (label still has pcmN/nid). */
                chip.clear();
            }
            d.description = friendly_description(unit, chip, d.kind, loc);
            if (was_default)
            {
                d.description += " · default";
            }
        }
        d.path_ok = path_exists(d.path);
    }
    return devices;
}

class FreeBSDAudioBackend : public IAudioBackend
{
  public:
    explicit FreeBSDAudioBackend(AudioBackendBuilder opts) : opts_(std::move(opts))
    {}

    const char *platform_name() const override
    {
        return "freebsd";
    }

    AudioStackFeatures features() override
    {
        /* Fail-soft via empty lists / available=false — never throw. */
        AudioStackFeatures f;
        f.hw_default_unit = true;
        auto phys = list_playback_devices();
        f.physical_devices = !phys.empty() || !list_capture_devices().empty();
        auto outs = list_logical_outputs();
        f.logical_io = !outs.empty() || !list_logical_inputs(false).empty();
        if (opts_.prefer_virtual_oss())
        {
            auto st = virtual_oss_status();
            f.virtual_oss = st.available;
            if (f.virtual_oss)
            {
                f.virtual_oss_label = "Virtual OSS";
                f.mix_channels     = st.running;
            }
        }
        return f;
    }

    std::vector<AudioDevice> list_playback_devices() override
    {
        return filter_role(parse_sndstat(), DeviceListRole::Playback);
    }

    std::vector<AudioDevice> list_capture_devices() override
    {
        return filter_role(parse_sndstat(), DeviceListRole::Capture);
    }

    std::vector<AudioDevice> list_logical_outputs() override
    {
        std::string out;
        int code = 0;
        if (!run_capture({opts_.pactl_binary(), "list", "short", "sinks"}, out, code) || code != 0)
        {
            return {};
        }
        auto devs = parse_pactl_short(out, "pulse");
        std::string def;
        int dcode = 0;
        if (run_capture({opts_.pactl_binary(), "get-default-sink"}, def, dcode) && dcode == 0)
        {
            mark_default_device(devs, rtrim_newlines(std::move(def)));
        }
        return devs;
    }

    std::vector<AudioDevice> list_logical_inputs(bool include_monitors) override
    {
        std::string out;
        int code = 0;
        if (!run_capture({opts_.pactl_binary(), "list", "short", "sources"}, out, code) || code != 0)
        {
            return {};
        }
        auto filtered = filter_monitors(parse_pactl_short(out, "pulse-source"), include_monitors);
        std::string def;
        int dcode = 0;
        if (run_capture({opts_.pactl_binary(), "get-default-source"}, def, dcode) && dcode == 0)
        {
            mark_default_device(filtered, rtrim_newlines(std::move(def)));
        }
        return filtered;
    }

    VirtualOssStatus virtual_oss_status() override
    {
        VirtualOssStatus st;
        st.control_device = opts_.control_device();
        st.available = path_exists(st.control_device);
        if (!st.available || !opts_.prefer_virtual_oss())
        {
            return st;
        }

        std::string out;
        int code = 0;
        if (!run_capture({opts_.virtual_oss_cmd_binary(), st.control_device}, out, code))
        {
            return st;
        }
        parse_virtual_oss_status_text(out, st);
        if (!st.running)
        {
            return st;
        }
        st.play_path_ok   = !st.play_path.empty() && path_exists(st.play_path);
        st.record_path_ok = !st.record_path.empty() && path_exists(st.record_path);
        return st;
    }

    OpResult set_playback_device(const std::string& device_path) override
    {
        return voss_set("-P", device_path);
    }

    OpResult set_capture_device(const std::string& device_path) override
    {
        return voss_set("-R", device_path);
    }

    OpResult set_hw_default_unit(int unit) override
    {
        OpResult r;
        std::string out;
        int code = 0;
        std::string arg = "hw.snd.default_unit=" + std::to_string(unit);
        if (!run_capture({"sysctl", arg}, out, code) || code != 0)
        {
            r.message = out.empty() ? "sysctl failed" : out;
            return r;
        }
        r.ok = true;
        r.message = out;
        return r;
    }

    OpResult set_default_logical_output(const std::string& id) override
    {
        return pactl_set({"set-default-sink", id});
    }

    OpResult set_default_logical_input(const std::string& id) override
    {
        return pactl_set({"set-default-source", id});
    }

  private:
    OpResult voss_set(const char *flag, const std::string& path)
    {
        OpResult r;
        auto st = virtual_oss_status();
        if (!st.available)
        {
            r.message = "virtual_oss control device not found";
            return r;
        }
        if (!path_exists(path))
        {
            r.message = "device not present: " + path;
            return r;
        }
        std::string out;
        int code = 0;
        if (!run_capture({opts_.virtual_oss_cmd_binary(), st.control_device, flag, path}, out,
                code) ||
            code != 0)
        {
            r.message = out.empty() ? "virtual_oss_cmd failed" : out;
            return r;
        }
        r.ok      = true;
        r.message = out;
        return r;
    }

    OpResult pactl_set(std::vector<std::string> args)
    {
        OpResult r;
        args.insert(args.begin(), opts_.pactl_binary());
        std::string out;
        int code = 0;
        if (!run_capture(args, out, code) || code != 0)
        {
            r.message = out.empty() ? "pactl failed" : out;
            return r;
        }
        r.ok = true;
        return r;
    }

    AudioBackendBuilder opts_;
};

class NullAudioBackend : public IAudioBackend
{
  public:
    explicit NullAudioBackend(AudioBackendBuilder) {}
    const char *platform_name() const override { return wf_platform_name(); }
    AudioStackFeatures features() override { return {}; }
    std::vector<AudioDevice> list_playback_devices() override { return {}; }
    std::vector<AudioDevice> list_capture_devices() override { return {}; }
    std::vector<AudioDevice> list_logical_outputs() override { return {}; }
    std::vector<AudioDevice> list_logical_inputs(bool) override { return {}; }
    VirtualOssStatus virtual_oss_status() override { return {}; }
    OpResult set_playback_device(const std::string&) override
    {
        return {false, "audio backend not available on this platform"};
    }
    OpResult set_capture_device(const std::string&) override
    {
        return {false, "audio backend not available on this platform"};
    }
    OpResult set_hw_default_unit(int) override { return {true, "no-op"}; }
    OpResult set_default_logical_output(const std::string&) override
    {
        return {false, "not supported"};
    }
    OpResult set_default_logical_input(const std::string&) override
    {
        return {false, "not supported"};
    }
};

} // namespace

std::unique_ptr<IAudioBackend> create_freebsd_audio_backend(const AudioBackendBuilder& b)
{
    return std::make_unique<FreeBSDAudioBackend>(b);
}

std::unique_ptr<IAudioBackend> create_null_audio_backend(const AudioBackendBuilder& b)
{
    return std::make_unique<NullAudioBackend>(b);
}

} // namespace detail
} // namespace wf_audio
