/*
 * FreeBSD audio backend — OSS + virtual_oss + optional pactl.
 *
 * Application code never includes this file; only the factory builds it.
 */

#include "audio-backend.hpp"
#include "audio-process.hpp"
#include "platform.hpp"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unistd.h>

namespace wf_audio
{
namespace detail
{

namespace
{

DeviceCapability parse_cap(const std::string& paren)
{
    /* "(play)", "(play/rec)", "(rec)" */
    bool play = paren.find("play") != std::string::npos;
    bool rec  = paren.find("rec") != std::string::npos;
    if (play && rec)
    {
        return DeviceCapability::PlayRecord;
    }
    if (rec)
    {
        return DeviceCapability::Record;
    }
    return DeviceCapability::Play;
}

std::string kind_from_desc(const std::string& desc)
{
    std::string d = desc;
    for (char& c : d)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (d.find("hdmi") != std::string::npos || d.find("nvidia") != std::string::npos)
    {
        return "hdmi";
    }
    if (d.find("usb") != std::string::npos || d.find("uaudio") != std::string::npos ||
        d.find("snowball") != std::string::npos || d.find("blue microphone") != std::string::npos ||
        d.find("webcam") != std::string::npos)
    {
        return "usb";
    }
    if (d.find("digital") != std::string::npos || d.find("spdif") != std::string::npos)
    {
        return "digital";
    }
    return "analog";
}

/**
 * Parse /dev/sndstat lines like:
 *   pcm1: <NVIDIA (0x00a0) (HDMI/DP 8ch)> (play) default
 * Skip userspace virtual_oss pseudo-devices for physical lists.
 */
std::vector<AudioDevice> parse_sndstat()
{
    std::vector<AudioDevice> devices;
    std::ifstream in("/dev/sndstat");
    if (!in)
    {
        return devices;
    }

    std::string line;
    bool in_userspace = false;
    while (std::getline(in, line))
    {
        if (line.find("Installed devices from userspace") != std::string::npos)
        {
            in_userspace = true;
            continue;
        }
        if (line.find("Installed devices:") != std::string::npos)
        {
            in_userspace = false;
            continue;
        }
        if (in_userspace)
        {
            continue;
        }

        /* pcmN: <desc> (play|play/rec|rec) [default] */
        if (line.compare(0, 3, "pcm") != 0)
        {
            continue;
        }
        auto colon = line.find(':');
        if (colon == std::string::npos)
        {
            continue;
        }
        std::string id = line.substr(0, colon); /* pcm1 */
        int unit = -1;
        if (id.size() > 3)
        {
            unit = std::atoi(id.c_str() + 3);
        }

        auto lt = line.find('<');
        auto gt = line.find('>');
        std::string desc;
        if (lt != std::string::npos && gt != std::string::npos && gt > lt)
        {
            desc = line.substr(lt + 1, gt - lt - 1);
        }

        auto lp = line.find('(', gt == std::string::npos ? 0 : gt);
        auto rp = line.find(')', lp == std::string::npos ? 0 : lp);
        std::string paren;
        if (lp != std::string::npos && rp != std::string::npos)
        {
            paren = line.substr(lp, rp - lp + 1);
        }

        AudioDevice d;
        d.id          = id;
        d.path        = "/dev/dsp" + std::to_string(unit >= 0 ? unit : 0);
        d.description = desc.empty() ? id : desc;
        d.capability  = parse_cap(paren);
        d.is_default  = line.find("default") != std::string::npos;
        d.kind        = kind_from_desc(d.description);
        d.present     = true;
        /* Hotplug: node may lag sndstat by a moment — still mark path_ok honestly. */
        d.path_ok     = (access(d.path.c_str(), F_OK) == 0);
        d.jack_connected = -1; /* jack sense not reliable via sndstat alone */
        if (d.kind == "hdmi")
        {
            /* HDMI "plugged" ≠ analog jack; leave unknown unless we add DRM probe later. */
            d.jack_connected = -1;
        }
        if (unit >= 0)
        {
            devices.push_back(std::move(d));
        }
    }
    return devices;
}

std::vector<AudioDevice> filter_role(const std::vector<AudioDevice>& all, DeviceListRole role)
{
    std::vector<AudioDevice> out;
    for (const auto& d : all)
    {
        if (role == DeviceListRole::Playback && can_play(d.capability))
        {
            out.push_back(d);
        } else if (role == DeviceListRole::Capture && can_record(d.capability))
        {
            out.push_back(d);
        }
    }
    return out;
}

/** Parse `pactl list short sinks|sources` lines: index name module … */
std::vector<AudioDevice> parse_pactl_short(const std::string& text, const std::string& kind)
{
    std::vector<AudioDevice> devices;
    for (const auto& line : split_lines(text))
    {
        if (line.empty())
        {
            continue;
        }
        std::istringstream iss(line);
        std::string idx, name;
        if (!(iss >> idx >> name))
        {
            continue;
        }
        /* skip pure monitor names when kind is source — caller filters */
        AudioDevice d;
        d.id          = name;
        d.path        = name;
        d.description = name;
        d.kind        = kind;
        d.capability  = (kind == "pulse-source") ? DeviceCapability::Record :
                        DeviceCapability::Play;
        devices.push_back(std::move(d));
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

    /**
     * Autodetect FreeBSD stack. virtual_oss is first-class when present.
     * Pulse/sndstat absence is fine — flags false / empty lists.
     */
    AudioStackFeatures features() override
    {
        AudioStackFeatures f;
        f.hw_default_unit = true;
        try
        {
            auto phys = list_playback_devices();
            f.physical_devices = !phys.empty() || !list_capture_devices().empty();
        } catch (...)
        {
            f.physical_devices = false;
        }
        try
        {
            auto outs = list_logical_outputs();
            f.logical_io = !outs.empty() || !list_logical_inputs(false).empty();
        } catch (...)
        {
            f.logical_io = false;
        }
        try
        {
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
        } catch (...)
        {
            f.virtual_oss   = false;
            f.mix_channels  = false;
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
            while (!def.empty() && (def.back() == '\n' || def.back() == '\r'))
            {
                def.pop_back();
            }
            for (auto& d : devs)
            {
                d.is_default = (d.id == def);
            }
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
        auto devs = parse_pactl_short(out, "pulse-source");
        std::vector<AudioDevice> filtered;
        for (auto& d : devs)
        {
            bool mon = d.id.find(".monitor") != std::string::npos;
            if (mon && !include_monitors)
            {
                continue;
            }
            filtered.push_back(std::move(d));
        }
        std::string def;
        int dcode = 0;
        if (run_capture({opts_.pactl_binary(), "get-default-source"}, def, dcode) && dcode == 0)
        {
            while (!def.empty() && (def.back() == '\n' || def.back() == '\r'))
            {
                def.pop_back();
            }
            for (auto& d : filtered)
            {
                d.is_default = (d.id == def);
            }
        }
        return filtered;
    }

    VirtualOssStatus virtual_oss_status() override
    {
        VirtualOssStatus st;
        st.control_device = opts_.control_device();
        st.available = (access(st.control_device.c_str(), F_OK) == 0);
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
        /* virtual_oss_cmd prints status even when extra args invalid; exit may be non-zero */
        if (out.find("Sample rate") == std::string::npos &&
            out.find("Output device") == std::string::npos)
        {
            return st;
        }
        st.running = true;
        for (const auto& line : split_lines(out))
        {
            auto grab = [&] (const char *key, std::string& dest)
            {
                auto p = line.find(key);
                if (p == std::string::npos)
                {
                    return;
                }
                dest = line.substr(p + std::strlen(key));
                while (!dest.empty() && dest[0] == ' ')
                {
                    dest.erase(dest.begin());
                }
            };
            if (line.find("Sample rate:") != std::string::npos)
            {
                st.sample_rate = std::atoi(line.c_str() + line.find(':') + 1);
            }
            if (line.find("Sample width:") != std::string::npos)
            {
                st.bits = std::atoi(line.c_str() + line.find(':') + 1);
            }
            if (line.find("Sample channels:") != std::string::npos)
            {
                st.channels = std::atoi(line.c_str() + line.find(':') + 1);
            }
            grab("Output device name:", st.play_path);
            grab("Input device name:", st.record_path);
        }
        /* USB unplug: virtual_oss may still "run" while backends are gone. */
        st.play_path_ok   = !st.play_path.empty() && (access(st.play_path.c_str(), F_OK) == 0);
        st.record_path_ok = !st.record_path.empty() && (access(st.record_path.c_str(), F_OK) == 0);
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
        try
        {
            auto st = virtual_oss_status();
            if (!st.available)
            {
                r.message = "virtual_oss control device not found";
                return r;
            }
            /* Fail soft if the target PCM was unplugged (USB) or never existed. */
            if (access(path.c_str(), F_OK) != 0)
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
        } catch (...)
        {
            r.ok      = false;
            r.message = "virtual_oss set failed unexpectedly";
            return r;
        }
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

/* Linux stub also lives here when not building linux file — see linux cpp */

} // namespace detail
} // namespace wf_audio
