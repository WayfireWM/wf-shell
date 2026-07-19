/*
 * Unit tests for wf_audio — types, pure parsers, process hooks,
 * FreeBSD/Linux/Null backends, volume UI logic.
 *
 * Target: near-100% line coverage of src/util/audio (except CLI main)
 * and volume-logic.hpp.
 */

#include <gtest/gtest.h>

#include "audio/audio-backend.hpp"
#include "audio/audio-parse.hpp"
#include "audio/audio-process.hpp"
#include "audio/volume-logic.hpp"
#include "platform.hpp"

#include <map>
#include <set>
#include <string>
#include <vector>

using namespace wf_audio;
using namespace wf_audio::detail;

/* ─── test fixture: clean hooks ─────────────────────────────────────────── */

class AudioHooksTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        reset_process_hooks();
    }
    void TearDown() override
    {
        reset_process_hooks();
    }
};

/* ─── types ─────────────────────────────────────────────────────────────── */

TEST(AudioTypes, CanPlayAndRecord)
{
    EXPECT_TRUE(can_play(DeviceCapability::Play));
    EXPECT_TRUE(can_play(DeviceCapability::PlayRecord));
    EXPECT_FALSE(can_play(DeviceCapability::Record));
    EXPECT_TRUE(can_record(DeviceCapability::Record));
    EXPECT_TRUE(can_record(DeviceCapability::PlayRecord));
    EXPECT_FALSE(can_record(DeviceCapability::Play));
}

TEST(AudioTypes, Defaults)
{
    AudioDevice d;
    EXPECT_TRUE(d.present);
    EXPECT_TRUE(d.path_ok);
    EXPECT_EQ(d.jack_connected, -1);
    EXPECT_FALSE(d.is_default);

    AudioStackFeatures f;
    EXPECT_FALSE(f.logical_io);
    EXPECT_FALSE(f.virtual_oss);

    VirtualOssStatus st;
    EXPECT_FALSE(st.available);
    EXPECT_FALSE(st.running);

    OpResult r;
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(r.message.empty());
}

/* ─── split_lines / rtrim ───────────────────────────────────────────────── */

TEST(SplitLines, Empty)
{
    EXPECT_TRUE(split_lines("").empty());
}

TEST(SplitLines, SingleNoNewline)
{
    auto L = split_lines("hello");
    ASSERT_EQ(L.size(), 1u);
    EXPECT_EQ(L[0], "hello");
}

TEST(SplitLines, CRLFAndTrailing)
{
    auto L = split_lines("a\r\nb\nc\r\n");
    ASSERT_EQ(L.size(), 3u);
    EXPECT_EQ(L[0], "a");
    EXPECT_EQ(L[1], "b");
    EXPECT_EQ(L[2], "c");
}

TEST(SplitLines, TrailingCRWithoutNewline)
{
    auto L = split_lines("solo\r");
    ASSERT_EQ(L.size(), 1u);
    EXPECT_EQ(L[0], "solo");
}

TEST(RtrimNewlines, Trims)
{
    EXPECT_EQ(rtrim_newlines("x\n\r\n"), "x");
    EXPECT_EQ(rtrim_newlines("x"), "x");
    EXPECT_EQ(rtrim_newlines(""), "");
}

/* ─── parse_cap / kind_from_desc / friendly ─────────────────────────────── */

TEST(ParseCap, Variants)
{
    EXPECT_EQ(parse_cap("(play)"), DeviceCapability::Play);
    EXPECT_EQ(parse_cap("(rec)"), DeviceCapability::Record);
    EXPECT_EQ(parse_cap("(play/rec)"), DeviceCapability::PlayRecord);
    EXPECT_EQ(parse_cap(""), DeviceCapability::Play);
}

TEST(KindFromDesc, Classification)
{
    EXPECT_EQ(kind_from_desc("NVIDIA HDMI/DP 8ch"), "hdmi");
    EXPECT_EQ(kind_from_desc("USB Audio"), "usb");
    EXPECT_EQ(kind_from_desc("Blue Microphone"), "usb");
    EXPECT_EQ(kind_from_desc("Snowball"), "usb");
    EXPECT_EQ(kind_from_desc("Webcam mic"), "usb");
    EXPECT_EQ(kind_from_desc("uaudio0"), "usb");
    EXPECT_EQ(kind_from_desc("Digital SPDIF"), "digital");
    EXPECT_EQ(kind_from_desc("Realtek ALC"), "analog");
}

TEST(FriendlyDescription, HdmiBrandsAndNid)
{
    auto n = friendly_description(1, "NVIDIA (0x00a0) (HDMI/DP 8ch)", "hdmi", "nid=5");
    EXPECT_NE(n.find("HDMI"), std::string::npos);
    EXPECT_NE(n.find("pcm1"), std::string::npos);
    EXPECT_NE(n.find("nid 5"), std::string::npos);
    EXPECT_NE(n.find("NVIDIA"), std::string::npos);

    auto a = friendly_description(2, "AMD Radeon HDMI", "hdmi", "nid=3");
    EXPECT_NE(a.find("AMD"), std::string::npos);

    auto i = friendly_description(0, "Intel HD Audio", "hdmi", "");
    EXPECT_NE(i.find("Intel"), std::string::npos);

    auto raw = friendly_description(4, "Some HDMI", "hdmi", "slot=1");
    EXPECT_NE(raw.find("nid slot=1"), std::string::npos);

    auto plain = friendly_description(3, "Realtek", "analog", "");
    EXPECT_EQ(plain, "Realtek · pcm3");

    auto empty = friendly_description(7, "", "analog", "");
    EXPECT_EQ(empty, "pcm7");
}

/* ─── parse_sndstat_text ────────────────────────────────────────────────── */

static const char *SAMPLE_SNDSTAT = R"(FreeBSD Audio Driver (64bit 2009061500/amd64)
Installed devices:
pcm0: <Realtek ALC1220 (Analog)> (play/rec) default
pcm1: <NVIDIA (0x00a0) (HDMI/DP 8ch)> (play)
pcm2: <USB Audio> (rec)
pcm3: <Digital SPDIF> (play)
Installed devices from userspace:
pcm4: <Virtual OSS> (play/rec)
No devices installed from userspace.
garbage line
pcmX: no colon stuff
pcm99: <Broken
)";

TEST(ParseSndstat, PhysicalOnlySkipsUserspace)
{
    auto devs = parse_sndstat_text(SAMPLE_SNDSTAT);
    ASSERT_GE(devs.size(), 4u);
    std::set<std::string> ids;
    for (const auto& d : devs)
    {
        ids.insert(d.id);
    }
    EXPECT_TRUE(ids.count("pcm0"));
    EXPECT_TRUE(ids.count("pcm1"));
    EXPECT_TRUE(ids.count("pcm2"));
    EXPECT_TRUE(ids.count("pcm3"));
    EXPECT_FALSE(ids.count("pcm4")); /* userspace virtual_oss skipped */

    auto play = filter_role(devs, DeviceListRole::Playback);
    auto cap  = filter_role(devs, DeviceListRole::Capture);
    EXPECT_GE(play.size(), 3u);
    EXPECT_GE(cap.size(), 2u); /* pcm0 play/rec + pcm2 rec */

    bool found_default = false;
    for (const auto& d : devs)
    {
        if (d.id == "pcm0")
        {
            EXPECT_TRUE(d.is_default);
            EXPECT_NE(d.description.find("default"), std::string::npos);
            found_default = true;
        }
        if (d.id == "pcm1")
        {
            EXPECT_EQ(d.kind, "hdmi");
            EXPECT_EQ(d.path, "/dev/dsp1");
            EXPECT_TRUE(can_play(d.capability));
        }
        if (d.id == "pcm2")
        {
            EXPECT_EQ(d.kind, "usb");
            EXPECT_TRUE(can_record(d.capability));
            EXPECT_FALSE(can_play(d.capability));
        }
    }
    EXPECT_TRUE(found_default);
}

TEST(ParseSndstat, EmptyAndInstalledReset)
{
    EXPECT_TRUE(parse_sndstat_text("").empty());
    std::string t =
        "Installed devices from userspace:\n"
        "pcm9: <x> (play)\n"
        "Installed devices:\n"
        "pcm0: <Analog> (play)\n";
    auto d = parse_sndstat_text(t);
    ASSERT_EQ(d.size(), 1u);
    EXPECT_EQ(d[0].id, "pcm0");
}

TEST(ParseSndstat, SkipMalformedAndBarePcm)
{
    /* bare pcm without colon; pcm: no unit digits; valid pcm5 */
    std::string t =
        "Installed devices:\n"
        "pcmNoColonHere (play)\n"
        "pcm: <no unit> (play)\n"
        "pcm5: <OK Chip> (play)\n"
        "notpcm0: <x> (play)\n";
    auto d = parse_sndstat_text(t);
    ASSERT_EQ(d.size(), 1u);
    EXPECT_EQ(d[0].id, "pcm5");
}

/* ─── pactl short / monitors ────────────────────────────────────────────── */

TEST(ParsePactlShort, SinksAndSources)
{
    const char *sinks =
        "0\tvirtual_oss\tmodule-oss.c\ts16le 2ch 48000Hz\tSUSPENDED\n"
        "1\talso_sink\tmodule-null\ts16le 2ch 44100Hz\tIDLE\n"
        "\n"
        "badline\n";
    auto devs = parse_pactl_short(sinks, "pulse");
    ASSERT_EQ(devs.size(), 2u);
    EXPECT_EQ(devs[0].id, "virtual_oss");
    EXPECT_EQ(devs[0].capability, DeviceCapability::Play);
    EXPECT_EQ(devs[1].id, "also_sink");

    const char *sources =
        "0\tvirtual_oss_rec\tmodule-oss.c\ts16le 2ch 48000Hz\tRUNNING\n"
        "1\tvirtual_oss.monitor\tmodule-oss.c\ts16le 2ch 48000Hz\tIDLE\n"
        "2\talso_mic\tmodule-null\ts16le 1ch 48000Hz\tSUSPENDED\n";
    auto src = parse_pactl_short(sources, "pulse-source");
    ASSERT_EQ(src.size(), 3u);
    EXPECT_EQ(src[0].capability, DeviceCapability::Record);

    auto no_mon = filter_monitors(src, false);
    ASSERT_EQ(no_mon.size(), 2u);
    for (const auto& d : no_mon)
    {
        EXPECT_EQ(d.id.find(".monitor"), std::string::npos);
    }
    auto with_mon = filter_monitors(parse_pactl_short(sources, "pulse-source"), true);
    EXPECT_EQ(with_mon.size(), 3u);

    mark_default_device(devs, "also_sink");
    EXPECT_FALSE(devs[0].is_default);
    EXPECT_TRUE(devs[1].is_default);
}

/* ─── virtual_oss status parse ──────────────────────────────────────────── */

TEST(VirtualOssParse, ValidStatus)
{
    const char *txt =
        "Sample rate: 48000\n"
        "Sample width: 16\n"
        "Sample channels: 2\n"
        "Output device name: /dev/dsp1\n"
        "Input device name: /dev/dsp3\n";
    EXPECT_TRUE(virtual_oss_status_looks_valid(txt));
    VirtualOssStatus st;
    parse_virtual_oss_status_text(txt, st);
    EXPECT_TRUE(st.running);
    EXPECT_EQ(st.sample_rate, 48000);
    EXPECT_EQ(st.bits, 16);
    EXPECT_EQ(st.channels, 2);
    EXPECT_EQ(st.play_path, "/dev/dsp1");
    EXPECT_EQ(st.record_path, "/dev/dsp3");
}

TEST(VirtualOssParse, Invalid)
{
    EXPECT_FALSE(virtual_oss_status_looks_valid("error: no control"));
    VirtualOssStatus st;
    parse_virtual_oss_status_text("garbage", st);
    EXPECT_FALSE(st.running);
}

/* ─── process hooks + run_capture real empty ────────────────────────────── */

TEST_F(AudioHooksTest, EmptyArgvFails)
{
    std::string out;
    int code = 0;
    EXPECT_FALSE(run_capture({}, out, code));
}

TEST_F(AudioHooksTest, HookOverrides)
{
    process_hooks().run_capture = [] (const std::vector<std::string>& argv,
                                       std::string& out, int& code) {
        out = "hooked:" + (argv.empty() ? "" : argv[0]);
        code = 0;
        return true;
    };
    process_hooks().path_exists = [] (const std::string& p) {
        return p == "/dev/vdsp.ctl";
    };
    process_hooks().read_text_file = [] (const std::string&) {
        return std::string(SAMPLE_SNDSTAT);
    };

    std::string out;
    int code = -1;
    EXPECT_TRUE(run_capture({"pactl"}, out, code));
    EXPECT_EQ(out, "hooked:pactl");
    EXPECT_EQ(code, 0);
    EXPECT_TRUE(path_exists("/dev/vdsp.ctl"));
    EXPECT_FALSE(path_exists("/nope"));
    EXPECT_FALSE(read_text_file("/dev/sndstat").empty());
}

TEST_F(AudioHooksTest, RealCaptureEcho)
{
    /* Integration: real fork of a portable command */
    std::string out;
    int code = 0;
    ASSERT_TRUE(run_capture_real({"/bin/echo", "hello-audio"}, out, code));
    EXPECT_EQ(code, 0);
    EXPECT_NE(out.find("hello-audio"), std::string::npos);
}

TEST_F(AudioHooksTest, RealCaptureMissingBinary)
{
    std::string out;
    int code = 0;
    /* exec fails → child exits 127; run still returns true */
    bool ok = run_capture_real({"/nonexistent/binary_xyz_audio_test"}, out, code);
    EXPECT_TRUE(ok);
    EXPECT_EQ(code, 127);
}

TEST_F(AudioHooksTest, RealPathAndFileIo)
{
    EXPECT_TRUE(path_exists_real("/"));
    EXPECT_FALSE(path_exists_real("/nonexistent/audio_cov_path_xyz"));
    /* Without hooks, wrappers use real implementations */
    EXPECT_TRUE(path_exists("/"));
    EXPECT_FALSE(path_exists("/nonexistent/audio_cov_path_xyz"));

    auto text = read_text_file_real("/etc/hosts");
    /* FreeBSD always has /etc/hosts */
    EXPECT_FALSE(text.empty());
    EXPECT_TRUE(read_text_file_real("/nonexistent/audio_cov_file_xyz").empty());
    EXPECT_FALSE(read_text_file("/etc/hosts").empty());
}

/* ─── Null backend ──────────────────────────────────────────────────────── */

TEST(NullBackend, FailSoftApi)
{
    auto b = create_null_audio_backend(AudioBackendBuilder{});
    ASSERT_NE(b, nullptr);
    EXPECT_NE(b->platform_name(), nullptr);
    EXPECT_FALSE(b->features().virtual_oss);
    EXPECT_TRUE(b->list_playback_devices().empty());
    EXPECT_TRUE(b->list_capture_devices().empty());
    EXPECT_TRUE(b->list_logical_outputs().empty());
    EXPECT_TRUE(b->list_logical_inputs(true).empty());
    EXPECT_FALSE(b->virtual_oss_status().available);
    auto r = b->set_playback_device("/dev/dsp0");
    EXPECT_FALSE(r.ok);
    EXPECT_FALSE(r.message.empty());
    EXPECT_FALSE(b->set_capture_device("/x").ok);
    EXPECT_TRUE(b->set_hw_default_unit(1).ok);
    EXPECT_FALSE(b->set_default_logical_output("x").ok);
    EXPECT_FALSE(b->set_default_logical_input("x").ok);
}

/* ─── FreeBSD backend with full mock stack ──────────────────────────────── */

class FreeBSDBackendMock : public AudioHooksTest
{
  protected:
    std::map<std::string, std::string> files;
    std::set<std::string> paths;
    std::map<std::string, std::pair<int, std::string>> cmds; /* key=joined argv */

    void install()
    {
        process_hooks().path_exists = [this] (const std::string& p) {
            return paths.count(p) > 0;
        };
        process_hooks().read_text_file = [this] (const std::string& p) {
            auto it = files.find(p);
            return it == files.end() ? std::string{} : it->second;
        };
        process_hooks().run_capture = [this] (const std::vector<std::string>& argv,
                                               std::string& out, int& code) {
            std::string key;
            for (size_t i = 0; i < argv.size(); i++)
            {
                if (i)
                {
                    key += '\x1f';
                }
                key += argv[i];
            }
            auto it = cmds.find(key);
            if (it == cmds.end())
            {
                /* prefix match: first two args */
                if (argv.size() >= 3 && argv[0] == "sysctl" && argv[1] == "-n")
                {
                    auto k2 = argv[0] + std::string("\x1f") + argv[1] + "\x1f" + argv[2];
                    it = cmds.find(k2);
                }
            }
            if (it == cmds.end())
            {
                out.clear();
                code = 1;
                return true;
            }
            out  = it->second.second;
            code = it->second.first;
            return true;
        };
    }

    void put_cmd(std::vector<std::string> argv, int code, std::string out)
    {
        std::string key;
        for (size_t i = 0; i < argv.size(); i++)
        {
            if (i)
            {
                key += '\x1f';
            }
            key += argv[i];
        }
        cmds[key] = {code, std::move(out)};
    }
};

TEST_F(FreeBSDBackendMock, FullHappyPath)
{
    files["/dev/sndstat"] = SAMPLE_SNDSTAT;
    paths.insert("/dev/dsp0");
    paths.insert("/dev/dsp1");
    paths.insert("/dev/dsp2");
    paths.insert("/dev/dsp3");
    paths.insert("/dev/vdsp.ctl");
    paths.insert("/dev/dsp1"); /* play path */

    put_cmd({"sysctl", "-n", "dev.pcm.1.%location"}, 0, "nid=5\n");
    put_cmd({"sysctl", "-n", "dev.pcm.1.%desc"}, 0, "NVIDIA HDMI\n");
    put_cmd({"sysctl", "-n", "dev.pcm.0.%location"}, 0, "\n");
    put_cmd({"pactl", "list", "short", "sinks"}, 0,
        "0\tvirtual_oss\tmodule-oss.c\ts16le 2ch 48000Hz\tRUNNING\n");
    put_cmd({"pactl", "get-default-sink"}, 0, "virtual_oss\n");
    put_cmd({"pactl", "list", "short", "sources"}, 0,
        "0\tvirtual_oss_rec\tmodule-oss.c\ts16le 2ch 48000Hz\tRUNNING\n"
        "1\tvirtual_oss.monitor\tmodule-oss.c\ts16le 2ch 48000Hz\tIDLE\n");
    put_cmd({"pactl", "get-default-source"}, 0, "virtual_oss_rec\n");
    put_cmd({"virtual_oss_cmd", "/dev/vdsp.ctl"}, 0,
        "Sample rate: 48000\nSample width: 16\nSample channels: 2\n"
        "Output device name: /dev/dsp1\nInput device name: /dev/dsp3\n");
    put_cmd({"virtual_oss_cmd", "/dev/vdsp.ctl", "-P", "/dev/dsp1"}, 0, "ok\n");
    put_cmd({"virtual_oss_cmd", "/dev/vdsp.ctl", "-R", "/dev/dsp3"}, 0, "ok\n");
    put_cmd({"sysctl", "hw.snd.default_unit=1"}, 0, "hw.snd.default_unit: 0 -> 1\n");
    put_cmd({"pactl", "set-default-sink", "virtual_oss"}, 0, "");
    put_cmd({"pactl", "set-default-source", "virtual_oss_rec"}, 0, "");
    install();

    AudioBackendBuilder opts;
    opts.control_device("/dev/vdsp.ctl")
        .prefer_virtual_oss(true)
        .pactl_binary("pactl")
        .virtual_oss_cmd_binary("virtual_oss_cmd");
    auto b = create_freebsd_audio_backend(opts);

    EXPECT_STREQ(b->platform_name(), "freebsd");

    auto feat = b->features();
    EXPECT_TRUE(feat.hw_default_unit);
    EXPECT_TRUE(feat.physical_devices);
    EXPECT_TRUE(feat.logical_io);
    EXPECT_TRUE(feat.virtual_oss);
    EXPECT_EQ(feat.virtual_oss_label, "Virtual OSS");
    EXPECT_TRUE(feat.mix_channels);

    auto play = b->list_playback_devices();
    EXPECT_FALSE(play.empty());
    auto cap = b->list_capture_devices();
    EXPECT_FALSE(cap.empty());

    auto outs = b->list_logical_outputs();
    ASSERT_FALSE(outs.empty());
    EXPECT_TRUE(outs[0].is_default);

    auto ins = b->list_logical_inputs(false);
    for (const auto& d : ins)
    {
        EXPECT_EQ(d.id.find(".monitor"), std::string::npos);
    }
    EXPECT_FALSE(b->list_logical_inputs(true).empty());

    auto st = b->virtual_oss_status();
    EXPECT_TRUE(st.available);
    EXPECT_TRUE(st.running);
    EXPECT_EQ(st.sample_rate, 48000);
    EXPECT_TRUE(st.play_path_ok);

    EXPECT_TRUE(b->set_playback_device("/dev/dsp1").ok);
    EXPECT_TRUE(b->set_capture_device("/dev/dsp3").ok);
    EXPECT_TRUE(b->set_hw_default_unit(1).ok);
    EXPECT_TRUE(b->set_default_logical_output("virtual_oss").ok);
    EXPECT_TRUE(b->set_default_logical_input("virtual_oss_rec").ok);
}

TEST_F(FreeBSDBackendMock, FailSoftMissingPaths)
{
    files["/dev/sndstat"] = "";
    /* no control device */
    install();
    AudioBackendBuilder opts;
    opts.prefer_virtual_oss(true).control_device("/dev/vdsp.ctl");
    auto b = create_freebsd_audio_backend(opts);

    EXPECT_TRUE(b->list_playback_devices().empty());
    EXPECT_TRUE(b->list_logical_outputs().empty());
    auto st = b->virtual_oss_status();
    EXPECT_FALSE(st.available);

    auto r = b->set_playback_device("/dev/dsp99");
    EXPECT_FALSE(r.ok);
    EXPECT_NE(r.message.find("virtual_oss"), std::string::npos);

    EXPECT_FALSE(b->set_hw_default_unit(0).ok);
    EXPECT_FALSE(b->set_default_logical_output("x").ok);
}

TEST_F(FreeBSDBackendMock, PreferVirtualOssFalse)
{
    paths.insert("/dev/vdsp.ctl");
    install();
    AudioBackendBuilder opts;
    opts.prefer_virtual_oss(false).control_device("/dev/vdsp.ctl");
    auto b = create_freebsd_audio_backend(opts);
    auto feat = b->features();
    EXPECT_FALSE(feat.virtual_oss);
    auto st = b->virtual_oss_status();
    EXPECT_TRUE(st.available); /* path exists */
    /* but prefer false short-circuits running probe after available */
}

TEST_F(FreeBSDBackendMock, VossSetDeviceMissing)
{
    paths.insert("/dev/vdsp.ctl");
    /* device path not present */
    put_cmd({"virtual_oss_cmd", "/dev/vdsp.ctl"}, 0,
        "Sample rate: 48000\nOutput device name: /dev/dsp1\n");
    install();
    AudioBackendBuilder opts;
    opts.prefer_virtual_oss(true).control_device("/dev/vdsp.ctl");
    auto b = create_freebsd_audio_backend(opts);
    auto r = b->set_playback_device("/dev/dsp_missing");
    EXPECT_FALSE(r.ok);
    EXPECT_NE(r.message.find("not present"), std::string::npos);
}

TEST_F(FreeBSDBackendMock, VossCmdFails)
{
    paths.insert("/dev/vdsp.ctl");
    paths.insert("/dev/dsp1");
    put_cmd({"virtual_oss_cmd", "/dev/vdsp.ctl"}, 0,
        "Sample rate: 48000\nOutput device name: /dev/dsp1\n");
    put_cmd({"virtual_oss_cmd", "/dev/vdsp.ctl", "-P", "/dev/dsp1"}, 1, "err\n");
    install();
    AudioBackendBuilder opts;
    opts.prefer_virtual_oss(true).control_device("/dev/vdsp.ctl");
    auto b = create_freebsd_audio_backend(opts);
    auto r = b->set_playback_device("/dev/dsp1");
    EXPECT_FALSE(r.ok);
    EXPECT_NE(r.message.find("err"), std::string::npos);
}

TEST_F(FreeBSDBackendMock, InvalidVossStatusText)
{
    paths.insert("/dev/vdsp.ctl");
    put_cmd({"virtual_oss_cmd", "/dev/vdsp.ctl"}, 0, "not a status dump\n");
    install();
    AudioBackendBuilder opts;
    opts.prefer_virtual_oss(true).control_device("/dev/vdsp.ctl");
    auto b = create_freebsd_audio_backend(opts);
    auto st = b->virtual_oss_status();
    EXPECT_TRUE(st.available);
    EXPECT_FALSE(st.running);
}

TEST_F(FreeBSDBackendMock, RunCaptureFalse)
{
    paths.insert("/dev/vdsp.ctl");
    process_hooks().path_exists = [this] (const std::string& p) {
        return paths.count(p) > 0;
    };
    process_hooks().run_capture = [] (const std::vector<std::string>&, std::string&, int&) {
        return false;
    };
    AudioBackendBuilder opts;
    opts.prefer_virtual_oss(true).control_device("/dev/vdsp.ctl");
    auto b = create_freebsd_audio_backend(opts);
    auto st = b->virtual_oss_status();
    EXPECT_TRUE(st.available);
    EXPECT_FALSE(st.running);
}

TEST_F(FreeBSDBackendMock, HdmiWithoutSysctlDesc)
{
    files["/dev/sndstat"] =
        "Installed devices:\n"
        "pcm1: <Acme HDMI Port> (play) default\n";
    paths.insert("/dev/dsp1");
    /* No %desc / %location cmds → empty sysctl → chip fallback from description */
    install();
    AudioBackendBuilder opts;
    auto b = create_freebsd_audio_backend(opts);
    auto play = b->list_playback_devices();
    ASSERT_FALSE(play.empty());
    EXPECT_EQ(play[0].kind, "hdmi");
    EXPECT_NE(play[0].description.find("default"), std::string::npos);
}

TEST_F(FreeBSDBackendMock, HdmiAlreadyLabeledFallback)
{
    files["/dev/sndstat"] =
        "Installed devices:\n"
        "pcm2: <HDMI/DP 8ch> (play)\n";
    paths.insert("/dev/dsp2");
    install();
    AudioBackendBuilder opts;
    auto b = create_freebsd_audio_backend(opts);
    auto play = b->list_playback_devices();
    ASSERT_FALSE(play.empty());
    EXPECT_EQ(play[0].kind, "hdmi");
}

TEST_F(FreeBSDBackendMock, VossCmdEmptyErrorMessage)
{
    paths.insert("/dev/vdsp.ctl");
    paths.insert("/dev/dsp1");
    put_cmd({"virtual_oss_cmd", "/dev/vdsp.ctl"}, 0,
        "Sample rate: 48000\nOutput device name: /dev/dsp1\n");
    put_cmd({"virtual_oss_cmd", "/dev/vdsp.ctl", "-P", "/dev/dsp1"}, 1, "");
    install();
    AudioBackendBuilder opts;
    opts.prefer_virtual_oss(true).control_device("/dev/vdsp.ctl");
    auto b = create_freebsd_audio_backend(opts);
    auto r = b->set_playback_device("/dev/dsp1");
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.message, "virtual_oss_cmd failed");
}

TEST_F(FreeBSDBackendMock, PactlEmptyError)
{
    install();
    AudioBackendBuilder opts;
    auto b = create_freebsd_audio_backend(opts);
    auto r = b->set_default_logical_output("x");
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.message, "pactl failed");
}

/* ─── Linux backend mock ────────────────────────────────────────────────── */

class LinuxBackendMock : public FreeBSDBackendMock
{};

TEST_F(LinuxBackendMock, PulsePrimary)
{
    put_cmd({"pactl", "list", "short", "sinks"}, 0, "0\tsinkA\tm\ts16le\tIDLE\n");
    put_cmd({"pactl", "list", "short", "sources"}, 0,
        "0\tmicA\tm\ts16le\tIDLE\n1\tsinkA.monitor\tm\ts16le\tIDLE\n");
    put_cmd({"pactl", "set-default-sink", "sinkA"}, 0, "");
    put_cmd({"pactl", "set-default-source", "micA"}, 0, "");
    install();

    AudioBackendBuilder opts;
    opts.prefer_virtual_oss(true).control_device("/dev/vdsp.ctl");
    auto b = create_linux_audio_backend(opts);
    EXPECT_STREQ(b->platform_name(), "linux");

    auto feat = b->features();
    EXPECT_FALSE(feat.hw_default_unit);
    EXPECT_TRUE(feat.logical_io);
    EXPECT_TRUE(feat.physical_devices);
    EXPECT_FALSE(feat.virtual_oss);

    EXPECT_EQ(b->list_playback_devices().size(), 1u);
    EXPECT_EQ(b->list_capture_devices().size(), 1u);
    EXPECT_EQ(b->list_logical_inputs(true).size(), 2u);

    EXPECT_TRUE(b->set_playback_device("sinkA").ok);
    EXPECT_TRUE(b->set_capture_device("micA").ok);
    EXPECT_TRUE(b->set_hw_default_unit(0).ok);
    EXPECT_EQ(b->set_hw_default_unit(0).message, "no-op on linux");
}

TEST_F(LinuxBackendMock, OptionalVoss)
{
    paths.insert("/dev/vdsp.ctl");
    put_cmd({"pactl", "list", "short", "sinks"}, 0, "");
    put_cmd({"virtual_oss_cmd", "/dev/vdsp.ctl"}, 0, "Output device name: /dev/dsp0\n");
    /* empty sinks with code 0 → empty list from parse */
    put_cmd({"pactl", "list", "short", "sinks"}, 1, "fail");
    put_cmd({"pactl", "list", "short", "sources"}, 1, "fail");
    install();
    AudioBackendBuilder opts;
    opts.prefer_virtual_oss(true).control_device("/dev/vdsp.ctl");
    auto b = create_linux_audio_backend(opts);
    auto feat = b->features();
    EXPECT_TRUE(feat.virtual_oss);
    EXPECT_TRUE(feat.mix_channels);
    EXPECT_FALSE(feat.logical_io);
}

TEST_F(LinuxBackendMock, PactlFail)
{
    install();
    AudioBackendBuilder opts;
    auto b = create_linux_audio_backend(opts);
    EXPECT_TRUE(b->list_logical_outputs().empty());
    EXPECT_FALSE(b->set_default_logical_output("x").ok);
}

/* ─── Factory / Builder ─────────────────────────────────────────────────── */

TEST(Builder, FluentAccessors)
{
    auto b = AudioBackendFactory::builder()
        .control_device("/tmp/ctl")
        .prefer_virtual_oss(false)
        .pactl_binary("/bin/pactl")
        .virtual_oss_cmd_binary("/bin/voss");
    EXPECT_EQ(b.control_device(), "/tmp/ctl");
    EXPECT_FALSE(b.prefer_virtual_oss());
    EXPECT_EQ(b.pactl_binary(), "/bin/pactl");
    EXPECT_EQ(b.virtual_oss_cmd_binary(), "/bin/voss");

    auto backend = b.build();
    ASSERT_NE(backend, nullptr);
    EXPECT_NE(backend->platform_name(), nullptr);

    auto def = AudioBackendFactory::create();
    ASSERT_NE(def, nullptr);
}

TEST(Factory, PlatformNameKnown)
{
    const char *p = wf_platform_name();
    ASSERT_NE(p, nullptr);
    EXPECT_TRUE(std::string(p) == "freebsd" || std::string(p) == "linux" ||
        std::string(p) == "openbsd" || std::string(p) == "netbsd" ||
        std::string(p) == "unknown");
}

TEST(Factory, PlatformOverrideSelectsProduct)
{
    set_platform_override_for_test("linux");
    auto linux_b = AudioBackendBuilder{}.build();
    ASSERT_NE(linux_b, nullptr);
    EXPECT_STREQ(linux_b->platform_name(), "linux");

    set_platform_override_for_test("unknown");
    auto null_b = AudioBackendBuilder{}.build();
    ASSERT_NE(null_b, nullptr);
    /* Null backend reports host platform name, not "unknown" */
    EXPECT_NE(null_b->platform_name(), nullptr);
    EXPECT_TRUE(null_b->list_playback_devices().empty());

    set_platform_override_for_test("freebsd");
    auto fbsd = AudioBackendBuilder{}.build();
    ASSERT_NE(fbsd, nullptr);
    EXPECT_STREQ(fbsd->platform_name(), "freebsd");

    set_platform_override_for_test(nullptr);
}

/* ─── volume_logic ──────────────────────────────────────────────────────── */

TEST(VolumeLogic, SafeGraphStyle)
{
    EXPECT_EQ(volume_logic::safe_graph_style("bars"), "bars");
    EXPECT_EQ(volume_logic::safe_graph_style("wave-fill"), "wave-fill");
    EXPECT_EQ(volume_logic::safe_graph_style("nope"), "wave-fill");
    EXPECT_EQ(volume_logic::safe_graph_style(""), "wave-fill");
    for (int i = 0; volume_logic::GRAPH_STYLES[i]; i++)
    {
        EXPECT_EQ(volume_logic::safe_graph_style(volume_logic::GRAPH_STYLES[i]),
            volume_logic::GRAPH_STYLES[i]);
    }
}

TEST(VolumeLogic, FingerprintsAndChannels)
{
    std::vector<AudioDevice> devs(1);
    devs[0].path = "/dev/dsp1";
    devs[0].description = "HDMI";
    devs[0].path_ok = true;
    auto fp = volume_logic::device_list_fingerprint(devs);
    EXPECT_FALSE(fp.empty());
    devs[0].path_ok = false;
    EXPECT_NE(fp, volume_logic::device_list_fingerprint(devs));

    EXPECT_EQ(volume_logic::safe_out_channels(2), 2);
    EXPECT_EQ(volume_logic::safe_out_channels(6), 6);
    EXPECT_EQ(volume_logic::safe_out_channels(8), 8);
    EXPECT_EQ(volume_logic::safe_out_channels(7), 8);
    EXPECT_EQ(volume_logic::safe_out_channels(0), 8);
}

TEST(VolumeLogic, VolumePercent)
{
    EXPECT_DOUBLE_EQ(volume_logic::volume_fraction(0, 100), 0.0);
    EXPECT_DOUBLE_EQ(volume_logic::volume_fraction(50, 0), 0.0);
    EXPECT_DOUBLE_EQ(volume_logic::volume_fraction(150, 100), 1.5);
    EXPECT_EQ(volume_logic::format_volume_percent(1.0), "100");
    EXPECT_EQ(volume_logic::format_volume_percent(1.5), "150");
    EXPECT_EQ(volume_logic::format_volume_percent(-0.1), "0");
}

TEST(VolumeLogic, PeaksAndColor)
{
    EXPECT_DOUBLE_EQ(volume_logic::peak_for_channel(nullptr, 0, 0), 0.0);
    float peaks[] = {0.5f, 1.0f, -0.1f};
    double p0 = volume_logic::peak_for_channel(peaks, 3, 0);
    EXPECT_NEAR(p0, std::min(1.0, 0.5 * 1.35), 1e-9);
    double p1 = volume_logic::peak_for_channel(peaks, 3, 1);
    EXPECT_NEAR(p1, 1.0, 1e-9); /* clamped then *1.35 capped */
    double p2 = volume_logic::peak_for_channel(peaks, 3, 5); /* wraps to peaks[2] */
    EXPECT_NEAR(p2, 0.0, 1e-9); /* -0.1 clamps to 0 */

    double r, g, b;
    volume_logic::level_color(0.9, true, r, g, b);
    EXPECT_GT(r, 0.9);
    volume_logic::level_color(0.7, true, r, g, b);
    EXPECT_GT(r, 0.9);
    volume_logic::level_color(0.3, true, r, g, b);
    EXPECT_NEAR(r, 0.54, 0.01);
    volume_logic::level_color(0.3, false, r, g, b);
    EXPECT_NEAR(r, 0.53, 0.01);
}

TEST(VolumeLogic, MeterTraceCount)
{
    EXPECT_EQ(volume_logic::meter_trace_count("bars", true, 8), 8);
    EXPECT_EQ(volume_logic::meter_trace_count("bars", false, 8), 2);
    EXPECT_EQ(volume_logic::meter_trace_count("spectrum", true, 8), 24);
    EXPECT_EQ(volume_logic::meter_trace_count("spectrum", false, 8), 12);
    EXPECT_EQ(volume_logic::meter_trace_count("spectrum", true, 2), 16); /* max(6,16) */
}

TEST(VolumeLogic, VossStripFingerprint)
{
    VirtualOssStatus st;
    st.play_path = "/dev/dsp1";
    st.record_path = "/dev/dsp3";
    st.sample_rate = 48000;
    st.running = true;
    st.play_path_ok = true;
    EXPECT_TRUE(volume_logic::voss_strip_fingerprint(false, st, "x").empty());
    auto fp = volume_logic::voss_strip_fingerprint(true, st, "Virtual OSS");
    EXPECT_FALSE(fp.empty());
    st.running = false;
    EXPECT_NE(fp, volume_logic::voss_strip_fingerprint(true, st, "Virtual OSS"));
    EXPECT_EQ(volume_logic::selection_fingerprint("/dev/dsp1"), "/dev/dsp1");
}
