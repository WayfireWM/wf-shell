util = static_library(
    'util',
    [
        'gtk-utils.cpp',
        'wf-shell-app.cpp',
        'wf-autohide-window.cpp',
        'wf-popover.cpp',
        'css-config.cpp',
        'wf-ipc.cpp',
        'animated-scale.cpp',
        'network/manager.cpp',
        'network/wifi.cpp',
        'network/wired.cpp',
        'network/network-widget.cpp',
        'network/wifi-ap.cpp',
        'network/network.cpp',
        'network/modem.cpp',
        'network/settings.cpp',
        'network/connection.cpp',
    ],
    dependencies: [
        wf_protos,
        gtklayershell,
        wayland_client,
        gtkmm,
        wfconfig,
        libinotify,
        json,
    ],
)

util_includes = include_directories('.')
libutil = declare_dependency(link_with: util, include_directories: util_includes)