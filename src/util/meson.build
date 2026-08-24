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

git_command = [
  'sh',
  '-c', 'printf "@0@ version @1@-%s (%s) branch %s gtk @2@" "$(git rev-parse --short HEAD)" "$(git log -1 --format=%as)" "$(git rev-parse --abbrev-ref HEAD)"'.format(meson.project_name(), meson.project_version(), gtkmm.version()),
]

fallback_string = '@0@ version @1@-unknown-commit (unknown-date) branch unknown-branch gtk @2@'.format(meson.project_name(), meson.project_version(), gtkmm.version())

version_h = vcs_tag(
  input: 'version.h.in',
  output: 'version.h',
  command: git_command,
  fallback: fallback_string,
)

util_includes = include_directories('.')
libutil = declare_dependency(link_with: util, include_directories: util_includes)
