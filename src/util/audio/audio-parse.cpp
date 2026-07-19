#include "audio-parse.hpp"
#include "audio-process.hpp"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace wf_audio
{
namespace detail
{

DeviceCapability parse_cap(const std::string& paren)
{
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

std::string friendly_description(int unit, const std::string& raw_desc,
    const std::string& kind, const std::string& location)
{
    const std::string unit_s = "pcm" + std::to_string(unit);
    std::string nid;
    if (location.rfind("nid=", 0) == 0)
    {
        nid = location.substr(4);
    } else if (!location.empty())
    {
        nid = location;
    }

    if (kind == "hdmi")
    {
        std::string brand;
        std::string low = raw_desc;
        for (char& c : low)
        {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (low.find("nvidia") != std::string::npos)
        {
            brand = "NVIDIA";
        } else if (low.find("amd") != std::string::npos || low.find("radeon") != std::string::npos)
        {
            brand = "AMD";
        } else if (low.find("intel") != std::string::npos)
        {
            brand = "Intel";
        }

        std::string label = "HDMI · " + unit_s;
        if (!nid.empty())
        {
            label += " · nid " + nid;
        }
        if (!brand.empty())
        {
            label += " (" + brand + ")";
        } else if (!raw_desc.empty())
        {
            label += " · " + raw_desc;
        }
        return label;
    }

    if (!raw_desc.empty())
    {
        return raw_desc + " · " + unit_s;
    }
    return unit_s;
}

std::vector<AudioDevice> parse_sndstat_text(const std::string& text)
{
    std::vector<AudioDevice> devices;
    bool in_userspace = false;

    for (const auto& line : split_lines(text))
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

        if (line.compare(0, 3, "pcm") != 0)
        {
            continue;
        }
        auto colon = line.find(':');
        if (colon == std::string::npos)
        {
            continue;
        }
        std::string id = line.substr(0, colon);
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
        d.capability  = parse_cap(paren);
        d.is_default  = line.find("default") != std::string::npos;
        d.kind        = kind_from_desc(desc.empty() ? id : desc);
        d.description = (unit >= 0) ?
            friendly_description(unit, desc, d.kind, {}) :
            (desc.empty() ? id : desc);
        if (d.is_default)
        {
            d.description += " · default";
        }
        d.present        = true;
        d.path_ok        = true;
        d.jack_connected = -1;
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

void mark_default_device(std::vector<AudioDevice>& devices, const std::string& default_id)
{
    for (auto& d : devices)
    {
        d.is_default = (d.id == default_id);
    }
}

std::vector<AudioDevice> filter_monitors(std::vector<AudioDevice> devices, bool include_monitors)
{
    if (include_monitors)
    {
        return devices;
    }
    std::vector<AudioDevice> filtered;
    filtered.reserve(devices.size());
    for (auto& d : devices)
    {
        if (d.id.find(".monitor") == std::string::npos)
        {
            filtered.push_back(std::move(d));
        }
    }
    return filtered;
}

std::string rtrim_newlines(std::string s)
{
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
    {
        s.pop_back();
    }
    return s;
}

bool virtual_oss_status_looks_valid(const std::string& text)
{
    return text.find("Sample rate") != std::string::npos ||
           text.find("Output device") != std::string::npos;
}

void parse_virtual_oss_status_text(const std::string& text, VirtualOssStatus& st)
{
    if (!virtual_oss_status_looks_valid(text))
    {
        return;
    }
    st.running = true;
    for (const auto& line : split_lines(text))
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
}

} // namespace detail
} // namespace wf_audio
