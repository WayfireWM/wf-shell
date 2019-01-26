util = static_library('util', ['display.cpp', 'gtk-utils.cpp', 'wf-shell-app.cpp', 'wf-autohide-window.cpp'],
    dependencies: [wf_protos, wayland_client, gtkmm, wfconfig])

util_includes = include_directories('.')
libutil = declare_dependency(
        link_with: util,
        include_directories: util_includes)

