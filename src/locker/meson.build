widget_sources = [
    'plugin/clock.cpp',
    'plugin/battery.cpp',
    'plugin/password.cpp',
    'plugin/instant.cpp',
    'plugin/pin.cpp',
    'plugin/fingerprint.cpp'
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
  openssl
]

# We'll come back here with a 'mute mic' widget and a 'volume' widget
#if libpulse.found()
#  widget_sources += 'widgets/volume.cpp'
#  deps += [libpulse, libgvc]
#endif

executable(
  'wf-locker',
  ['locker.cpp'] + widget_sources,
  dependencies: deps,
  install: true,
)
