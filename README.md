# wf-shell

**Maintained by [REVYTECH, Inc.](https://github.com/revytechinc)**  
Fork of the Wayfire shell stack, with FreeBSD-first desktop work (audio, network, power, and panel UX).

![Default look](/gallery/default.png)

wf-shell packages the pieces you need for a full desktop around [Wayfire](https://github.com/WayfireWM/wayfire):

| Component | Role |
|-----------|------|
| **wf-panel** | Top/bottom panel with launchers, tray, network, volume (**Sound Settings**), and more |
| **wf-dock** | Open windows, activate, (un)collapse |
| **wf-locker** | Screen locker plugins |
| **wf-background** | Background images (optional cycling) |

Upstream history remains attributed to the original WayfireWM authors; this tree is developed and shipped by **REVYTECH, Inc.**

---

## Repository

| | |
|--|--|
| **Canonical remote** | https://github.com/revytechinc/wf-shell |
| **Clone (SSH)** | `git@github.com:revytechinc/wf-shell.git` |
| **Clone (HTTPS)** | `https://github.com/revytechinc/wf-shell.git` |
| **Upstream (WayfireWM)** | https://github.com/WayfireWM/wf-shell (reference only) |

Related REVYTECH forks used with this stack may include `wf-config` and other Wayfire companions under [github.com/revytechinc](https://github.com/revytechinc).

---

## Configuration

Components share a config file at `$XDG_CONFIG_HOME/wf-shell.ini` (typically `~/.config/wf-shell.ini`). Configuration is hot-reloaded when the file changes.

- Example + comments: [`wf-shell.ini.example`](wf-shell.ini.example)
- Option metadata (for WCM / docs): [`metadata/`](metadata/)

The GUI configurator [WCM](https://github.com/WayfireWM/wcm) can edit many options. FreeBSD-specific audio keys under `[panel]` are documented in **`man 7 wf-shell-audio`**.

---

## Style & theme

Panel look is CSS-driven. Examples live under [`data/css/`](data/css/).

---

## Installation

Build from this repository. Distribution packages are **not** documented here yet (the previous packaging-status badge has been removed until packaging is ready for REVYTECH builds).

### Dependencies

- Wayland client libraries and protocols  
- **gtkmm-4.0**  
- **[wf-config](https://github.com/revytechinc/wf-config)** (or a compatible install)  
- Optional:
  - **libpulse** — panel volume / Sound Settings (recommended on FreeBSD + Pulse)
  - **pipewire** + **wireplumber** — wp-mixer widget when enabled
  - weather stack — only if configured on at build time

On FreeBSD, prefer ports/pkg names for `gtkmm40`, `wayland`, `wayland-protocols`, `pulseaudio`, etc.

### Building

```sh
git clone https://github.com/revytechinc/wf-shell.git
cd wf-shell
meson setup build --prefix="$HOME/.local" --buildtype=release
ninja -C build
ninja -C build install
```

System-wide install (optional):

```sh
meson setup build --prefix=/usr/local --buildtype=release
ninja -C build
sudo ninja -C build install
```

Ensure `$HOME/.local/bin` (or `/usr/local/bin`) is on your `PATH` so `wf-panel` and friends are found by the session.

### FreeBSD session tips

- Start Wayfire from a session script (e.g. `~/bin/start-wayfire` / `wayfire-session`).
- Run the installed panel: `wf-panel` (or `$HOME/.local/bin/wf-panel`).
- Audio model, Virtual OSS, and popover behaviour: **`man 7 wf-shell-audio`**.
- Backend smoke tool: **`wf-audio-info`** · **`man 1 wf-audio-info`**.

---

## FreeBSD audio (REVYTECH work)

This fork treats **Virtual OSS as first-class** when present:

```text
applications → PulseAudio → virtual_oss (/dev/dsp) → OSS PCM
```

- Panel **Sound Settings** popover: output + input volume, device routing, independent meters, VOSS status strip  
- Modular **Factory + Builder** backend (`IAudioBackend`) — FreeBSD, Linux, and null products; fail-soft if modules are missing  
- Design notes and status: [`docs/audio-control/`](docs/audio-control/)  
  - [PLAN.md](docs/audio-control/PLAN.md) — goals + **implementation addendum**  
  - [ARCHITECTURE.md](docs/audio-control/ARCHITECTURE.md) — Factory/Builder, hotplug, tests  
  - [SESSION-CONTROL.md](docs/audio-control/SESSION-CONTROL.md) — byobu + Wayfire recovery for agents  

### Tests

```sh
meson test -C build --suite unit
meson test -C build --suite audio
docs/audio-control/tests/run_all
# optional coverage of src/util/audio:
docs/audio-control/tests/coverage.sh
```

---

## Documentation map

| Doc | Contents |
|-----|----------|
| `man 7 wf-shell-audio` | Sound popover, Virtual OSS, ini keys, diagnostics |
| `man 1 wf-audio-info` | CLI for backend autodetection dump |
| `docs/audio-control/` | Architecture, plan, session control, mockups |
| `wf-shell.ini.example` | Annotated default config |

---

## License

MIT — see [LICENSE](LICENSE).  
Copyright of the original project remains with its authors; REVYTECH, Inc. contributions to this fork are likewise under the MIT terms unless otherwise noted.
