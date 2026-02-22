# wf-shell

![Default look](/gallery/default.png)

wf-shell is a repository which contains various components that can be used to build a fully functional desktop based around wayfire:
- wf-panel, with widgets with various functionalities you would expect a desktop to have on quick access
- wf-dock, to show opened windows, navigate to them, and (un)collapse them
- wf-locker, a stylish and feature-full screen locker
- wf-background, a simple background that can cycle through images

## Installation

[![Distribution packages](https://repology.org/badge/vertical-allrepos/wf-shell.svg)](https://repology.org/project/wf-shell/versions)

### Building

wf-shell needs the core wayland libraries and protocols (`wayland-devel` and `wayland-protocols-devel` for Fedora), gtkmm-4.0 and [wf-config](https://github.com/WayfireWM/wf-config)

Certain functionality is optional:
- panel wp-mixer widget, built if pipewire and wireplumber libraries are found
- panel/locker pulseaudio volume widgets, built if libpulse is found
- panel/locker weather widgets, built only if specified

To build and install, like any meson project:

```
git clone https://github.com/WayfireWM/wf-shell && cd wf-shell
meson build --prefix=/usr --buildtype=release
ninja -C build
sudo ninja -C build install
```

## Configuration

The wf-shell components use a common config file located (by default) at `XDG_CONFIG_HOME/wf-shell.ini` (so, most often resolves to `~/.config/wf-shell.ini`, which is a fallback),
and the configuration is hot reloaded when the configuration file changes.

An example configuration can be found in the file `wf-shell.ini.example`, alongside with comments what each option does.
For an exhaustive breakdown of every option, check out the [github wiki](https://github.com/WayfireWM/wf-shell/wiki)

The GUI [WCM](https://github.com/WayfireWM/wcm) can edit the configuration and show descriptions for the options.

## Style & Theme

Style and theme can be altered with CSS.
Find [here](/data/css/) examples of styles that can be applied to customise the looks.
A full tree of the css classes is available [here](https://github.com/WayfireWM/wf-shell/wiki/Style:-overview#widget-trees).

## Gallery

Past default look:
![Old look](/gallery/legacy.png)
