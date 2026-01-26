widget_sources = [
    'plugin/clock.cpp',
    'plugin/weather.cpp',
    'plugin/battery.cpp',
    'plugin/password.cpp',
    'plugin/instant.cpp',
    'plugin/pin.cpp',
    'plugin/fingerprint.cpp',
    'plugin/mpris.cpp',
    'plugin/user.cpp',
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
  yyjson
]

if libpulse.found()
  widget_sources += 'plugin/volume.cpp'
  deps += [libpulse, libgvc]
endif

executable(
  'wf-locker',
  ['locker.cpp'] + widget_sources,
  dependencies: deps,
  install: true,
)
