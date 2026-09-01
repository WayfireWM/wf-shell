sources = [
    'gtk-utils.cpp',
    'icon-select.cpp',
    'wf-shell-app.cpp',
    'wf-autohide-window.cpp',
    'wf-popover.cpp',
    'css-config.cpp',
    'wf-ipc.cpp',
    'animated-scale.cpp',
    'widget-utils/battery.cpp',
    'widget-utils/clock.cpp',
    'widget-utils/network/manager.cpp',
    'widget-utils/network/wifi.cpp',
    'widget-utils/network/wired.cpp',
    'widget-utils/network/network-widget.cpp',
    'widget-utils/network/wifi-ap.cpp',
    'widget-utils/network/network.cpp',
    'widget-utils/network/modem.cpp',
    'widget-utils/network/settings.cpp',
    'widget-utils/network/connection.cpp',
    'background-gl.cpp',
    'icon-select.cpp'
]

deps = [
    wf_protos,
    gtklayershell,
    wayland_client,
    gtkmm,
    wfconfig,
    libinotify,
    json,
]

if libpulse.found()
    sources += [
        'widget-utils/pulseaudio/control.cpp',
        'widget-utils/pulseaudio/gvc-proxy.cpp',
    ]
    deps += [libpulse, libgvc]
else
  message('Pulseaudio not found, volume widget will not be available.')
endif

if get_option('weather') == true
    sources += [
        'widget-utils/weather.cpp',
    ]
endif

util = static_library(
    'util',
    sources,
    dependencies: deps
)

util_includes = include_directories('.')
libutil = declare_dependency(link_with: util, include_directories: util_includes)
