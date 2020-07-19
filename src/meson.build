subdir('util')
subdir('panel')
subdir('background')
subdir('dock')

pkgconfig = import('pkgconfig')
pkgconfig.generate(
  version: meson.project_version(),
  name: meson.project_name(),
  description: 'GTK3 shell for Wayfire',
  variables: [
    'metadatadir=' + metadata_dir,
    'sysconfdir=' + sysconf_dir,
  ],
)
