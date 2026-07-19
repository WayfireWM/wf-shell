/*
 * Linux audio backend — Pulse-oriented; virtual_oss optional if present.
 * Parsing is pure (audio-parse); I/O goes through process hooks.
 */

#include "audio-backend.hpp"
#include "audio-parse.hpp"
#include "audio-process.hpp"

#include <memory>
#include <utility>

namespace wf_audio
{
namespace detail
{

namespace
{

class LinuxAudioBackend : public IAudioBackend
{
  public:
    explicit LinuxAudioBackend(AudioBackendBuilder opts) : opts_(std::move(opts))
    {}

    const char *platform_name() const override
    {
        return "linux";
    }

    AudioStackFeatures features() override
    {
        /* Fail-soft via empty lists / available=false — never throw. */
        AudioStackFeatures f;
        f.hw_default_unit = false;
        auto outs = list_logical_outputs();
        auto ins  = list_logical_inputs(false);
        f.logical_io       = !outs.empty() || !ins.empty();
        f.physical_devices = f.logical_io;
        if (opts_.prefer_virtual_oss())
        {
            auto st = virtual_oss_status();
            f.virtual_oss = st.available;
            if (f.virtual_oss)
            {
                f.virtual_oss_label = "Virtual OSS";
                f.mix_channels      = st.running;
            }
        }
        return f;
    }

    std::vector<AudioDevice> list_playback_devices() override
    {
        return list_logical_outputs();
    }

    std::vector<AudioDevice> list_capture_devices() override
    {
        return list_logical_inputs(false);
    }

    std::vector<AudioDevice> list_logical_outputs() override
    {
        std::string out;
        int code = 0;
        if (!run_capture({opts_.pactl_binary(), "list", "short", "sinks"}, out, code) || code != 0)
        {
            return {};
        }
        return parse_pactl_short(out, "pulse");
    }

    std::vector<AudioDevice> list_logical_inputs(bool include_monitors) override
    {
        std::string out;
        int code = 0;
        if (!run_capture({opts_.pactl_binary(), "list", "short", "sources"}, out, code) || code != 0)
        {
            return {};
        }
        return filter_monitors(parse_pactl_short(out, "pulse-source"), include_monitors);
    }

    VirtualOssStatus virtual_oss_status() override
    {
        VirtualOssStatus st;
        st.control_device = opts_.control_device();
        st.available = path_exists(st.control_device);
        if (!st.available)
        {
            return st;
        }
        std::string out;
        int code = 0;
        if (run_capture({opts_.virtual_oss_cmd_binary(), st.control_device}, out, code) &&
            virtual_oss_status_looks_valid(out))
        {
            st.running = true;
        }
        return st;
    }

    OpResult set_playback_device(const std::string& device_path) override
    {
        return set_default_logical_output(device_path);
    }

    OpResult set_capture_device(const std::string& device_path) override
    {
        return set_default_logical_input(device_path);
    }

    OpResult set_hw_default_unit(int) override
    {
        return {true, "no-op on linux"};
    }

    OpResult set_default_logical_output(const std::string& id) override
    {
        return pactl({"set-default-sink", id});
    }

    OpResult set_default_logical_input(const std::string& id) override
    {
        return pactl({"set-default-source", id});
    }

  private:
    OpResult pactl(std::vector<std::string> args)
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

} // namespace

std::unique_ptr<IAudioBackend> create_linux_audio_backend(const AudioBackendBuilder& b)
{
    return std::make_unique<LinuxAudioBackend>(b);
}

} // namespace detail
} // namespace wf_audio
