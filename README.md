# wf-shell

wf-shell is a repository which contains the various components needed to built a fully functional DE based around wayfire.
Currently it has only a GTK-based panel and background client.

# Dependencies

wf-shell needs the core wayland libraries and protocols (`wayland-devel` and `wayland-protocols-devel` for Fedora), gtkmm-3.0 and [wf-config](https://github.com/WayfireWM/wf-config)

# Build

Just like any meson project:
```
git clone https://github.com/WayfireWM/wf-shell && cd wf-shell
meson build --prefix=/usr --buildtype=release
ninja -C build && sudo ninja -C build install
```

# Configuration

To configure the panel and the dock, wf-shell uses a config file located (by default) in `~/.config/wf-shell.ini`
An example configuration can be found in the file `wf-shell.ini.example`, alongside with comments what each option does.

# Screenshots

![Panel & Background demo](/screenshot.png)
