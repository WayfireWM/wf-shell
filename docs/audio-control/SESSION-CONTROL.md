# Session control — byobu + Wayfire (for agents & UI tests)

## Why

Grok / tools run inside **byobu (tmux)**. That session **survives** a Wayfire
crash. The graphical seat does **not**. UI validation (`grim`, popover clicks)
needs a live `WAYLAND_DISPLAY`.

Plan: **monitor from byobu → detect death → clean sockets → restart → wait healthy → resume tests.**

```text
┌─────────────────────────────────────────────────────────┐
│  byobu (tmux) — always on                               │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────────┐  │
│  │ agent / zsh  │  │ wayfire-     │  │ optional logs │  │
│  │              │  │ session      │  │ tail -f       │  │
│  │              │  │ monitor      │  │ session.log   │  │
│  └──────┬───────┘  └──────┬───────┘  └───────────────┘  │
└─────────┼─────────────────┼─────────────────────────────┘
          │ ensure/restart  │ crash → restart
          ▼                 ▼
   ┌──────────────────────────────────┐
   │  Wayfire + wf-panel + seatd      │
   │  WAYLAND_DISPLAY=wayland-N       │
   │  grim / ydotool UI tests         │
   └──────────────────────────────────┘
```

## Tool: `~/bin/wayfire-session`

| Command | Purpose |
|---------|---------|
| `wayfire-session status` | healthy?, pids, display, seatd, wf-panel |
| `wayfire-session wait [sec]` | block until healthy (default 60s) |
| `wayfire-session monitor [sec]` | loop; print state changes (byobu pane) |
| `wayfire-session stop` | TERM/KILL wayfire; remove stale sockets |
| `wayfire-session restart` | stop + `start-wayfire` detached + wait |
| `wayfire-session ensure` | restart **only if** unhealthy |

Health = **wayfire process** + **working Wayland socket** (probed with `grim` when available).

Logs:

- `~/.local/state/wayfire/control.log` — monitor/restart
- `~/.local/state/wayfire/session.log` — compositor (from `start-wayfire`)
- `~/.local/state/wayfire/restart.stderr` — last restart spawn errors
- `~/.local/state/wayfire/wayland-display` — last known good `WAYLAND_DISPLAY`

## Byobu layout (recommended)

```sh
# Pane A — agent work
# Pane B — session watchdog (auto-restart on crash):
WAYFIRE_MONITOR_AUTO_RESTART=1 wayfire-session monitor 3

# Or manual:
wayfire-session monitor 5
```

Create a dedicated window once:

```sh
byobu new-window -n wayfire-watch 'WAYFIRE_MONITOR_AUTO_RESTART=1 wayfire-session monitor 3'
```

## Agent / test harness contract

Before any UI action (`grim`, open popover, ydotool):

```sh
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/var/run/xdg/$USER}"
wayfire-session ensure || exit 1
export WAYLAND_DISPLAY="$(cat ~/.local/state/wayfire/wayland-display)"
# sanity
grim -t ppm /tmp/wf-alive.ppm >/dev/null
```

After a crash mid-test:

```sh
wayfire-session restart
export WAYLAND_DISPLAY="$(cat ~/.local/state/wayfire/wayland-display)"
# re-open popover; do not assume old coordinates still valid
```

## Restart semantics

1. Kill `wayfire` (not `seatd`, not byobu).
2. Remove stale `$XDG_RUNTIME_DIR/wayland-*` if no compositor.
3. Unset `WAYLAND_DISPLAY` so `start-wayfire` does not refuse “nested” session.
4. Set `START_WAYFIRE_FORCE=1` and `nohup start-wayfire`.
5. Wait until `grim` succeeds.

**DRM / seat notes (FreeBSD):**

- `seatd` must be up (`/var/run/seatd.sock`). Restart does not start seatd as root; enable via rc if needed.
- Claiming the GPU from a tmux pane usually works when the same user already owns the seat; if restart fails, check `session.log` / `restart.stderr` and that nothing else holds the VT/DRM master.
- Nested second Wayfire is avoided unless forced; `ensure` only restarts when dead.

## Integration with audio UI tests

| Layer | Depends on session? |
|-------|---------------------|
| Backend C++ / `wf-audio-info` | No |
| `grim` popover crops | **Yes** → `ensure` first |
| ydotool / wtype clicks | **Yes** → `ensure` + focus |

Suggested runner prologue:

```sh
#!/bin/sh
set -eu
wayfire-session ensure
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/var/run/xdg/$USER}"
export WAYLAND_DISPLAY="$(cat "$HOME/.local/state/wayfire/wayland-display")"
# … open sound popover, grim, assert …
```

## Failure playbook

| Symptom | Action |
|---------|--------|
| `healthy=0` wayfire_pids empty | `wayfire-session restart` |
| pids set, grim fails | stale socket → `stop` then `restart` |
| restart loops | check NVIDIA/seatd; disable auto-restart; read `session.log` |
| byobu dead too | user problem — start byobu on tty, then `ensure` |

## Safety

- Default **monitor does not restart** unless `WAYFIRE_MONITOR_AUTO_RESTART=1`.
- `stop` / `restart` are destructive to the graphical session (apps on that compositor die).
- Never `kill -9 seatd` from this tool.

## Documentation phase

When session-control behavior changes, update:

1. This file (`SESSION-CONTROL.md`)
2. `man/wf-shell-audio.7` diagnostics section if UI-test prerequisites change
3. `PLAN.md` acceptance if UI tests become required CI steps
