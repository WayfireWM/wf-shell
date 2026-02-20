widget_sources = [
  'widgets/battery.cpp',
  'widgets/menu.cpp',
  'widgets/clock.cpp',
  'widgets/command-output.cpp',
  'widgets/language.cpp',
  'widgets/launchers.cpp',
  'widgets/network.cpp',
  'widgets/spacing.cpp',
  'widgets/separator.cpp',
  'widgets/window-list/window-list.cpp',
  'widgets/window-list/toplevel.cpp',
  'widgets/window-list/layout.cpp',
  'widgets/notifications/daemon.cpp',
  'widgets/notifications/single-notification.cpp',
  'widgets/notifications/notification-info.cpp',
  'widgets/notifications/notification-center.cpp',
  'widgets/tray/watcher.cpp',
  'widgets/tray/tray.cpp',
  'widgets/tray/item.cpp',
  'widgets/tray/host.cpp',
  'widgets/tray/dbusmenu.cpp',
  'widgets/workspace-switcher.cpp',
]

deps = [
  gtkmm,
  gtklayershell,
  wayland_client,
  libutil,
  wf_protos,
  wfconfig,
  dbusmenu_gtk,
  xkbregistry,
  json,
]

if libpulse.found()
  widget_sources += [
    'widgets/volume.cpp',
    'widgets/volume-level.cpp',
  ]
  deps += [libpulse, libgvc]
else
  message('Pulse not found, volume widget will not be available.')
endif

if wireplumber.found()
  widget_sources += [
    'widgets/wp-mixer/wp-mixer.cpp',
    'widgets/wp-mixer/wf-wp-control.cpp',
    'widgets/wp-mixer/wp-common.cpp',
    'widgets/volume-level.cpp',
  ]
  deps += wireplumber
else
  message('Wireplumber not found, mixer widget will not be available.')
endif

executable(
  'wf-panel',
  ['panel.cpp'] + widget_sources,
  dependencies: deps,
  install: true,
)
