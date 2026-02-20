widget_sources = [
    'plugin/clock.cpp',
    'plugin/battery.cpp',
    'plugin/password.cpp',
    'plugin/instant.cpp',
    'plugin/pin.cpp',
    'plugin/fingerprint.cpp',
    'plugin/mpris.cpp',
    'plugin/user.cpp',
    'plugin/network.cpp',
]
deps = [
  gtklayershell,
  gtkmm,
  wayland_client,
  libutil,
  wf_protos,
  wfconfig,
  xkbregistry,
  json,
  pam,
  openssl,
  epoxy,
]

if libpulse.found()
  widget_sources += 'plugin/volume.cpp'
  deps += [libpulse, libgvc]
endif

if get_option('weather') == true
  widget_sources += [
    'plugin/weather.cpp',
  ]
endif

executable(
  'wf-locker',
  ['locker.cpp', 'lockscreen.cpp', 'timedrevealer.cpp'] + widget_sources,
  dependencies: deps,
  install: true,
)
