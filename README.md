# wf-shell

![Default look](/gallery/default.png)

wf-shell is a repository which contains various components that can be used to build a fully functional desktop based around wayfire:
- wf-panel, with widgets with various functionalities you would expect a desktop to have on quick access
- wf-dock, to show opened windows, navigate to them, and (un)collapse them
- wf-locker, a stylish and feature-full screen locker
- wf-background, a simple background that can cycle through images

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

## Installation

### Packages

[![](https://repology.org/badge/vertical-allrepos/wf-shell.svg)](https://repology.org/project/wf-shell/versions)

### Building

wf-shell depends on the wayland and wayfire protocol and libraries, gtkmm4, [wf-config](https://github.com/WayfireWM/wf-config), pam, dbusmenu-gtk, openssl, epoxy, xkbregistry and inotify.

gtk4-layer-shell and wf-json are built as subprojects if they are not found.

Certain features are optionally built :
- panel wp-mixer widget, built if pipewire and wireplumber libraries are found
- panel/locker pulseaudio volume widgets, built if libpulse is found
- live previews for window-list widget, built if gbm and libdrm are found
- panel/locker weather widgets, built only if specified

For the following distributions, these are the names of the packages you will need :

| Distribution | Packages |
| ------------ | -------- |
| Ubuntu | `git gcc pkgconf meson ninja wayire-dev libgtkmm-4.0-dev libgirepository2.0-dev libgirepository1.0-dev vapigen libdbusmenu-glib-dev openssl-dev libyyjson-dev libinotifytools0-dev libepoxy-dev libpam0g-dev libaudit-dev libxkbregistry-dev libpipewire-0.3-dev libwireplumber-0.5-dev libgbm-dev libdrm-dev` |
| Void | `git gcc pkconf meson ninja wayfire-devel gtkmm4-devel libgirepository-devel vala libdbusmenu-glib-devel openssl-dev pam-devel libepoxy-devel yyjson libxkbregistry pipewire-devel wireplumber-devel libgbm-devel libdrm-devel ` |
| Alpine | `git g++ binutils pkgconf meson ninja musl-dev gtkmm4-dev vala gobject-introspection gobject-introspection-dev pulseaudio-dev pipewire-dev wireplumber-dev libdbusmenu-glib-dev alsa-lib-dev yyjson-dev linux-pam-dev util-linux-login openssl-dev` |

To build and install, like any meson project:
```
git clone https://github.com/WayfireWM/wf-shell && cd wf-shell
meson build --prefix=/usr --buildtype=release
ninja -C build
sudo ninja -C build install
```

## Gallery

Past default look:
![Old look](/gallery/legacy.png)
