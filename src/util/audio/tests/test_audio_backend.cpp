/*
 * Red/green tests for wf_audio Factory + Builder + FreeBSD backend.
 *
 * Compile & run (no meson required):
 *   c++ -std=c++17 -DWFS_PLATFORM_FREEBSD -I src/util \
 *     src/util/platform.cpp \
 *     src/util/audio/audio-process.cpp \
 *     src/util/audio/audio-backend-builder.cpp \
 *     src/util/audio/audio-backend-freebsd.cpp \
 *     src/util/audio/audio-backend-linux.cpp \
 *     src/util/audio/tests/test_audio_backend.cpp \
 *     -o /tmp/test_audio_backend && /tmp/test_audio_backend
 *
 * Exit 0 = all green.
 */

#include "audio/audio-backend.hpp"
#include "platform.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

static int g_pass = 0;
static int g_fail = 0;

static void expect(bool cond, const char *name)
{
    if (cond)
    {
        ++g_pass;
        std::cout << "  GREEN  " << name << "\n";
    } else
    {
        ++g_fail;
        std::cout << "  RED    " << name << "\n";
    }
}

int main()
{
    std::cout << "=== RED/GREEN: audio backend (Factory/Builder) ===\n\n";

    /* RED would fail if factory returns null or throws */
    std::unique_ptr<wf_audio::IAudioBackend> backend;
    try
    {
        backend = wf_audio::AudioBackendFactory::create();
    } catch (...)
    {
        backend.reset();
    }
    expect(backend != nullptr, "Factory::create() returns non-null");

    if (!backend)
    {
        std::cout << "\n=== RESULT: abort after factory failure ===\n";
        return 1;
    }

    const char *plat = backend->platform_name();
    expect(plat != nullptr && plat[0] != '\0', "platform_name() non-empty");
    expect(std::string(plat) == "freebsd" || std::string(plat) == "linux" ||
           std::string(plat) == "unknown",
        "platform_name() is known tag");

    /* Builder defaults */
    auto b2 = wf_audio::AudioBackendFactory::builder()
        .control_device("/dev/vdsp.ctl")
        .prefer_virtual_oss(true)
        .build();
    expect(b2 != nullptr, "Builder::build() returns non-null");

    /* Must not throw on list APIs */
    bool lists_ok = true;
    try
    {
        auto play = backend->list_playback_devices();
        auto cap  = backend->list_capture_devices();
        auto outs = backend->list_logical_outputs();
        auto ins  = backend->list_logical_inputs(false);
        (void)play;
        (void)cap;
        (void)outs;
        (void)ins;
        expect(true, "list_* APIs do not throw");
        /* On FreeBSD host with audio, expect some play devices */
        if (std::string(plat) == "freebsd")
        {
            expect(!backend->list_playback_devices().empty(),
                "FreeBSD: at least one playback device from sndstat");
            auto caps = backend->list_capture_devices();
            for (const auto& d : caps)
            {
                if (!wf_audio::can_record(d.capability))
                {
                    lists_ok = false;
                }
            }
            expect(lists_ok, "capture list only includes record-capable devices");
        }
    } catch (...)
    {
        expect(false, "list_* APIs do not throw");
    }

    /* features() autodetection must not throw */
    try
    {
        auto feat = backend->features();
        expect(true, "features() does not throw");
        if (std::string(plat) == "freebsd")
        {
            /* FreeBSD-centric: physical and/or virtual_oss often present */
            expect(feat.hw_default_unit, "FreeBSD: hw_default_unit feature true");
            if (feat.virtual_oss)
            {
                expect(!feat.virtual_oss_label.empty(),
                    "FreeBSD: virtual_oss first-class has label");
            } else
            {
                expect(true, "FreeBSD: virtual_oss absent is valid");
            }
        } else if (std::string(plat) == "linux")
        {
            /* Linux: virtual_oss usually false — must not be required */
            expect(true, "Linux: features() ok without virtual_oss");
        }
    } catch (...)
    {
        expect(false, "features() does not throw");
    }

    /* virtual_oss status must not throw (first-class when present) */
    try
    {
        auto st = backend->virtual_oss_status();
        expect(true, "virtual_oss_status() does not throw");
        if (st.available && st.running)
        {
            expect(st.sample_rate >= 0, "sample_rate non-negative");
        } else
        {
            expect(true, "virtual_oss absent is not an error");
        }
    } catch (...)
    {
        expect(false, "virtual_oss_status() does not throw");
    }

    /* set_hw_default_unit no-throw (may fail permission — still OpResult) */
    try
    {
        auto r = backend->set_hw_default_unit(1);
        expect(true, "set_hw_default_unit returns OpResult without throw");
        (void)r;
    } catch (...)
    {
        expect(false, "set_hw_default_unit returns OpResult without throw");
    }

    /* Invalid / unplugged device set should not throw; prefer ok=false */
    try
    {
        auto r = backend->set_playback_device("/dev/dsp_does_not_exist_zzz");
        expect(true, "set_playback_device bad path does not throw");
        expect(!r.ok, "missing device returns ok=false (hotplug fail-soft)");
        expect(!r.message.empty(), "missing device sets OpResult.message");
    } catch (...)
    {
        expect(false, "set_playback_device bad path does not throw");
    }

    /* path_ok / play_path_ok fields populated without throw */
    try
    {
        auto st = backend->virtual_oss_status();
        if (st.running && !st.play_path.empty())
        {
            expect(st.play_path_ok, "running virtual_oss play path currently ok");
        } else
        {
            expect(true, "virtual_oss path_ok optional when not running");
        }
        for (const auto& d : backend->list_playback_devices())
        {
            expect(d.present, "listed playback device marked present");
        }
    } catch (...)
    {
        expect(false, "hotplug presence fields do not throw");
    }

    std::cout << "\n=== RESULT: " << g_pass << " green, " << g_fail << " red ===\n";
    return g_fail ? 1 : 0;
}
