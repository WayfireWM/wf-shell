util = static_library('util', ['display.cpp'],
    dependencies: [wf_protos, wayland_client, gtkmm])

util_includes = include_directories('.')
libutil = declare_dependency(
        link_with: util,
        include_directories: util_includes)

