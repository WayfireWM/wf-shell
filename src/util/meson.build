util = static_library(
    'util',
    [
        'gtk-utils.cpp',
        'icon-select.cpp',
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
        'network/freebsd-network.cpp',
        'network/freebsd-backend.cpp',
        'background-gl.cpp',
        'platform.cpp',
        'power-controller.cpp',
        'power-controller-linux.cpp',
        'power-controller-bsd.cpp',
        # Audio backend (Factory + Builder, OS-specific products)
        'audio/audio-process.cpp',
        'audio/audio-backend-builder.cpp',
        'audio/audio-backend-freebsd.cpp',
        'audio/audio-backend-linux.cpp',
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
    cpp_args: platform_cpp_args,
)

util_includes = include_directories('.')
libutil = declare_dependency(link_with: util, include_directories: util_includes)

# CLI to exercise audio Factory/Builder without GTK
wf_audio_info = executable(
    'wf-audio-info',
    'audio/audio-backend-cli.cpp',
    dependencies: [libutil],
    cpp_args: platform_cpp_args,
    install: true,
)
