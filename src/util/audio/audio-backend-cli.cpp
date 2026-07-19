/*
 * wf-audio-info — tiny CLI to exercise IAudioBackend (Factory/Builder).
 * Application-agnostic dump for debugging FreeBSD virtual_oss + Pulse.
 */

#include "audio-backend.hpp"

#include <iostream>

static void print_devices(const char *title, const std::vector<wf_audio::AudioDevice>& devs)
{
    std::cout << title << " (" << devs.size() << ")\n";
    for (const auto& d : devs)
    {
        std::cout << "  " << (d.is_default ? "*" : " ")
                  << " " << d.id
                  << "  path=" << d.path
                  << "  kind=" << d.kind
                  << "  path_ok=" << d.path_ok
                  << "  jack=" << d.jack_connected
                  << "  " << d.description << "\n";
    }
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    auto backend = wf_audio::AudioBackendFactory::builder()
        .control_device("/dev/vdsp.ctl")
        .prefer_virtual_oss(true)
        .build();

    std::cout << "platform: " << backend->platform_name() << "\n";

    auto feat = backend->features();
    std::cout << "features (autodetect):\n"
              << "  logical_io=" << feat.logical_io
              << " physical_devices=" << feat.physical_devices
              << " virtual_oss=" << feat.virtual_oss
              << " hw_default_unit=" << feat.hw_default_unit
              << " mix_channels=" << feat.mix_channels << "\n";
    if (feat.virtual_oss)
    {
        std::cout << "  virtual_oss_label=" << feat.virtual_oss_label << "\n";
    }

    auto voss = backend->virtual_oss_status();
    std::cout << "virtual_oss (first-class when available): available=" << voss.available
              << " running=" << voss.running
              << " ctl=" << voss.control_device << "\n";
    if (voss.running)
    {
        std::cout << "  play=" << voss.play_path
                  << " ok=" << voss.play_path_ok
                  << "  capture=" << voss.record_path
                  << " ok=" << voss.record_path_ok
                  << "  " << voss.sample_rate << "Hz "
                  << voss.bits << "bit ch=" << voss.channels << "\n";
    }

    print_devices("playback devices", backend->list_playback_devices());
    print_devices("capture devices", backend->list_capture_devices());
    print_devices("logical outputs (Pulse sinks)", backend->list_logical_outputs());
    print_devices("logical inputs (Pulse sources)", backend->list_logical_inputs(false));

    return 0;
}
