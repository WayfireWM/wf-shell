util = static_library('util', ['gtk-utils.cpp', 'wf-shell-app.cpp', 'wf-autohide-window.cpp', 'wf-popover.cpp',
    'json.cpp', 'wf-ipc.cpp'],
    dependencies: [wayfire, wf_protos, wayland_client, gtkmm, wfconfig, libinotify, gtklayershell, json])

util_includes = include_directories('.')
libutil = declare_dependency(
        link_with: util,
        include_directories: util_includes)

