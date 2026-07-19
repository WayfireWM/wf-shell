/*
 * AudioBackendBuilder + AudioBackendFactory.
 *
 * Factory Method: create() / build() select the concrete OS backend.
 * Builder: fluent options without telescoping constructors.
 *
 * Platform #ifdefs are confined to this translation unit (and the
 * per-OS .cpp files), matching power-controller and wayfire platform.
 */

#include "audio-backend.hpp"
#include "platform.hpp"

#include <utility>

namespace wf_audio
{
namespace detail
{
static const char *g_platform_override = nullptr;

void set_platform_override_for_test(const char *name)
{
    g_platform_override = name;
}

static const char *effective_platform()
{
    if (g_platform_override)
    {
        return g_platform_override;
    }
    return wf_platform_name();
}
} // namespace detail

/* --- Builder --- */

AudioBackendBuilder& AudioBackendBuilder::control_device(std::string path)
{
    control_device_ = std::move(path);
    return *this;
}

AudioBackendBuilder& AudioBackendBuilder::prefer_virtual_oss(bool prefer)
{
    prefer_virtual_oss_ = prefer;
    return *this;
}

AudioBackendBuilder& AudioBackendBuilder::pactl_binary(std::string path)
{
    pactl_binary_ = std::move(path);
    return *this;
}

AudioBackendBuilder& AudioBackendBuilder::virtual_oss_cmd_binary(std::string path)
{
    virtual_oss_cmd_binary_ = std::move(path);
    return *this;
}

std::unique_ptr<IAudioBackend> AudioBackendBuilder::build() const
{
    const char *plat = detail::effective_platform();
    if (plat && std::string(plat) == "freebsd")
    {
        return detail::create_freebsd_audio_backend(*this);
    }
    if (plat && std::string(plat) == "linux")
    {
        return detail::create_linux_audio_backend(*this);
    }
    /* OpenBSD/NetBSD/unknown: conservative null backend */
    return detail::create_null_audio_backend(*this);
}

/* --- Factory --- */

std::unique_ptr<IAudioBackend> AudioBackendFactory::create()
{
    return AudioBackendBuilder{}.build();
}

AudioBackendBuilder AudioBackendFactory::builder()
{
    return AudioBackendBuilder{};
}

} // namespace wf_audio
