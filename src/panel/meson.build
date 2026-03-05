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
  'widgets/backlight/backlight.cpp',
  'widgets/backlight/sysfs.cpp',
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
  ]
  deps += [libpulse, libgvc]
else
  message('Pulseaudio not found, volume widget will not be available.')
endif

if wireplumber.found()
  widget_sources += [
    'widgets/wp-mixer/wp-mixer.cpp',
    'widgets/wp-mixer/wf-wp-control.cpp',
    'widgets/wp-mixer/wp-common.cpp',
  ]
  deps += [pipewire, wireplumber]
else
  message('Pipewire/Wireplumber not found, mixer widget will not be available.')
endif

if get_option('weather') == true
  widget_sources += [
    'widgets/weather.cpp',
  ]
else
  message('Weather disabled, weather widgets and owf service not included.')
endif

if ddcutil.found()
  widget_sources += 'widgets/backlight/ddcutil.cpp'
  deps += ddcutil
else
  message('Libddcutil not found, light widget will not support external monitors.')
endif

if gbm.found() and drm.found() and not get_option('live-previews-dmabuf').disabled()
  deps += [gbm, drm]
else
  message('Development packages for gbm and drm are required to enable live window preview tooltips in wf-panel.')
endif

executable(
  'wf-panel',
  ['panel.cpp'] + widget_sources,
  dependencies: deps,
  install: true,
)
