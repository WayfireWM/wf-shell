# wf-shell

This fork of wf-shell is attempting to port the project to GTK4.

It is anticipated that a few cosmetic options will be removed, with a subset of those ported to CSS rules

wf-shell is a repository which contains the various components needed to built a fully functional DE based around wayfire.
Currently it has only a GTK-based panel and background client.

# Dependencies

wf-shell needs the core wayland libraries and protocols (`wayland-devel` and `wayland-protocols-devel` for Fedora), gtkmm-4.0 and [wf-config](https://github.com/WayfireWM/wf-config)

# Build

Just like any meson project:
```
git clone https://github.com/trigg/wf-shell && cd wf-shell
meson build --prefix=/usr --buildtype=release
ninja -C build && sudo ninja -C build install
```

# Configuration

To configure the panel and the dock, wf-shell uses a config file located (by default) in `~/.config/wf-shell.ini`
An example configuration can be found in the file `wf-shell.ini.example`, alongside with comments what each option does.

# Style & Theme

Style and theme can be altered with [CSS](/data/css/)

# Screenshots

![Panel & Background demo](/screenshot.png)
