# Platform detection — compile-time OS identification.
# Each platform macro is passed to all translation units so that the ONE #ifdef lives
# in platform.cpp and nowhere else.
platform_cpp_args = []
if host_machine.system() == 'linux'
    platform_cpp_args += ['-DWFS_PLATFORM_LINUX']
elif host_machine.system() == 'freebsd'
    platform_cpp_args += ['-DWFS_PLATFORM_FREEBSD']
elif host_machine.system() == 'openbsd'
    platform_cpp_args += ['-DWFS_PLATFORM_OPENBSD']
elif host_machine.system() == 'netbsd'
    platform_cpp_args += ['-DWFS_PLATFORM_NETBSD']
endif

subdir('util')
subdir('panel')
subdir('background')
subdir('dock')
if pam.found()
    subdir('locker')
    subdir('locker-pin')
endif
subdir('stream-chooser')

pkgconfig = import('pkgconfig')
pkgconfig.generate(
  version: meson.project_version(),
  name: meson.project_name(),
  description: 'GTK4 shell for Wayfire',
  variables: [
    'metadatadir=' + metadata_dir,
    'sysconfdir=' + sysconf_dir,
  ],
)
