#!/usr/bin/env python3
"""
Red/green tests for audio mockup defaults + wf-shell.ini keys.

Run: python3 docs/audio-control/tests/test_defaults.py
Exit 0 = green, non-zero = red.
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]  # docs/audio-control
REPO = ROOT.parents[1]  # wf-shell
MOCK = ROOT / "mockup.html"
META = REPO / "metadata" / "panel.xml"
INI_EXAMPLE = REPO / "wf-shell.ini.example"
HOME_INI = Path.home() / ".config" / "wf-shell.ini"

passed = 0
failed = 0


def ok(cond: bool, name: str, detail: str = "") -> None:
    global passed, failed
    if cond:
        passed += 1
        print(f"  GREEN  {name}")
    else:
        failed += 1
        print(f"  RED    {name}" + (f" — {detail}" if detail else ""))


def main() -> int:
    print("=== RED/GREEN: audio defaults & config ===\n")

    # --- mockup.html hard defaults ---
    print("[mockup.html]")
    html = MOCK.read_text(encoding="utf-8") if MOCK.is_file() else ""
    ok(MOCK.is_file(), "mockup.html exists")

    ok("graphStyleOut: 'wave-fill'" in html or 'graphStyleOut: "wave-fill"' in html,
       "default graphStyleOut is wave-fill")
    ok("graphStyleIn: 'wave-fill'" in html or 'graphStyleIn: "wave-fill"' in html,
       "default graphStyleIn is wave-fill")
    ok("on('selGraph'" in html and "on('selGraphIn'" in html,
       "output and input graph dropdowns have separate handlers")
    ok("state.graphStyleOut" in html and "state.graphStyleIn" in html,
       "graph styles are independent state fields")
    ok("outChannels: 8" in html, "default outChannels is 8")
    ok("playPcm: 1" in html, "default playPcm is 1 (HDMI)")
    ok("capPcm: 3" in html, "default capPcm is 3 (rear mic)")
    ok("const DEFAULTS = Object.freeze" in html, "DEFAULTS object is frozen hard-coded")
    ok("function sanitizeState" in html, "sanitizeState present")
    ok("safeStyle" in html and "safeOutCh" in html, "safeStyle/safeOutCh guards present")
    ok("id=\"selGraph\"" in html, "graph type dropdown present")
    ok("id=\"selGraphIn\"" in html, "input graph type dropdown present (synced)")
    ok("wave-fill" in html and "selected" in html, "wave-fill selectable in UI")
    # graph dropdown lives on meter caption row (not separate Display section)
    ok("Output levels" in html and "selGraph" in html, "graph control near Output levels")
    ok("id=\"selOutCh\"" in html and "graph-dd" in html, "compact channel+graph dropdowns on meter row")
    m = re.search(
        r'id="selGraph"[\s\S]*?<option value="wave-fill"[^>]*>',
        html,
    )
    ok(bool(m), "selGraph has wave-fill option")
    ok("Display" not in html or "section-title\">Display" not in html,
       "no separate Display section (graph is on meter row)")
    ok("try {" in html and "catch (e)" in html, "try/catch fail-soft present")
    ok("localStorage" in html and "saveSettings" in html, "persistence helpers present")
    ok("function on(" in html or "function on (" in html, "safe event binder on() present")

    # Corrupt storage must not break: sanitize rejects unknown style
    ok(
        "DEFAULTS.graphStyleOut" in html or "DEFAULTS.graphStyleIn" in html,
        "invalid graph style falls back to DEFAULTS",
    )

    # --- metadata panel.xml ---
    print("\n[metadata/panel.xml]")
    meta = META.read_text(encoding="utf-8") if META.is_file() else ""
    ok(META.is_file(), "panel.xml exists")
    ok('name="volume_graph_style"' in meta, "volume_graph_style option defined")
    ok('name="volume_graph_style_in"' in meta, "volume_graph_style_in option defined")
    block = re.search(
        r'name="volume_graph_style"[\s\S]*?<default>([^<]+)</default>',
        meta,
    )
    ok(bool(block) and block.group(1).strip() == "wave-fill",
       "volume_graph_style default is wave-fill",
       block.group(1) if block else "missing")
    block_in = re.search(
        r'name="volume_graph_style_in"[\s\S]*?<default>([^<]+)</default>',
        meta,
    )
    ok(bool(block_in) and block_in.group(1).strip() == "wave-fill",
       "volume_graph_style_in default is wave-fill",
       block_in.group(1) if block_in else "missing")
    ok('name="volume_out_channels"' in meta, "volume_out_channels defined")
    block2 = re.search(
        r'name="volume_out_channels"[\s\S]*?<default>([^<]+)</default>',
        meta,
    )
    ok(bool(block2) and block2.group(1).strip() == "8",
       "volume_out_channels default is 8",
       block2.group(1) if block2 else "missing")
    ok('name="volume_play_device"' in meta, "volume_play_device defined")
    ok('name="volume_capture_device"' in meta, "volume_capture_device defined")
    for key, default in (
        ("volume_prefer_virtual_oss", "true"),
        ("volume_auto_switch_headset", "true"),
        ("volume_auto_switch_usb", "true"),
        ("volume_auto_restore_previous", "true"),
        ("volume_auto_switch_capture", "true"),
        ("volume_notify_device_change", "true"),
        ("volume_manual_sticky", "true"),
        ("volume_mix_channels", "2"),
    ):
        ok(f'name="{key}"' in meta, f"{key} defined")
        blk = re.search(
            rf'name="{key}"[\s\S]*?<default>([^<]+)</default>',
            meta,
        )
        ok(bool(blk) and blk.group(1).strip() == default,
           f"{key} default is {default}",
           blk.group(1) if blk else "missing")
    for style in ("bars", "wave", "wave-fill", "mirror", "scope", "spectrum", "dots", "ribbon"):
        ok(f"<value>{style}</value>" in meta, f"style enum includes {style}")

    # --- ini example ---
    print("\n[wf-shell.ini.example]")
    ex = INI_EXAMPLE.read_text(encoding="utf-8") if INI_EXAMPLE.is_file() else ""
    ok(INI_EXAMPLE.is_file(), "wf-shell.ini.example exists")
    ok("volume_graph_style = wave-fill" in ex, "example default wave-fill")
    ok("volume_out_channels = 8" in ex, "example default 8 channels")

    # --- user config if present ---
    print("\n[~/.config/wf-shell.ini]")
    if HOME_INI.is_file():
        hi = HOME_INI.read_text(encoding="utf-8")
        ok("volume_graph_style" in hi, "user ini has volume_graph_style")
        ok("volume_graph_style = wave-fill" in hi or "volume_graph_style=wave-fill" in hi,
           "user ini graph default wave-fill")
    else:
        ok(True, "user ini optional (skipped)")

    # --- UI stays sparse; detail in man pages ---
    print("\n[man pages + no tutorial UI]")
    man7 = REPO / "man" / "wf-shell-audio.7"
    man1 = REPO / "man" / "wf-audio-info.1"
    ok(man7.is_file(), "man/wf-shell-audio.7 exists")
    ok(man1.is_file(), "man/wf-audio-info.1 exists")
    if man7.is_file():
        m7 = man7.read_text(encoding="utf-8")
        ok("Virtual OSS is first-class" in m7 or "first-class" in m7,
           "wf-shell-audio.7 documents Virtual OSS first-class")
        ok("AUTODETECTION" in m7 or "Autodetect" in m7 or "features" in m7,
           "wf-shell-audio.7 documents autodetection")
        ok("wf-shell.ini" in m7, "wf-shell-audio.7 documents config")
    ok("Output/Input dropdowns above" not in html,
       "mockup has no tutorial footnote under Virtual OSS strip")
    ok("Saved preferences go to wf-shell.ini" not in html,
       "mockup does not teach ini path in the popover")
    ok("id=\"vossHint\"" not in html, "vossHint element removed")

    # --- architecture sources exist ---
    print("\n[audio backend sources]")
    audio = REPO / "src" / "util" / "audio"
    for name in (
        "audio-types.hpp",
        "audio-backend.hpp",
        "audio-backend-builder.cpp",
        "audio-backend-freebsd.cpp",
        "audio-backend-linux.cpp",
        "audio-process.cpp",
    ):
        ok((audio / name).is_file(), f"source {name}")

    print(f"\n=== RESULT: {passed} green, {failed} red ===")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
