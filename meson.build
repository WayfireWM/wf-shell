project(
	'wf-shell',
    'c',
	'cpp',
	version: '0.7.0',
	license: 'MIT',
	meson_version: '>=0.51.0',
	default_options: [
		'cpp_std=c++17',
        'c_std=c11',
		'warning_level=2',
		'werror=false',
	],
)

wayfire        = dependency('wayfire')
wayland_client = dependency('wayland-client')
wayland_protos = dependency('wayland-protocols')
gtkmm          = dependency('gtkmm-3.0', version: '>=3.24')
wfconfig       = dependency('wf-config', version: '>=0.7.0') #TODO fallback submodule
gtklayershell  = dependency('gtk-layer-shell-0', version: '>= 0.1', fallback: ['gtk-layer-shell', 'gtk_layer_shell_dep'])
libpulse       = dependency('libpulse', required : get_option('pulse'))
libgvc         = subproject('gvc', default_options: ['static=true'], required : get_option('pulse'))

if get_option('wayland-logout') == true
  wayland_logout = subproject('wayland-logout')
endif

if libpulse.found()
  libgvc = libgvc.get_variable('libgvc_dep')
  add_project_arguments('-DHAVE_PULSE=1', language : 'cpp')
endif

needs_libinotify = ['freebsd', 'dragonfly'].contains(host_machine.system())
libinotify       = dependency('libinotify', required: needs_libinotify)

add_project_arguments(['-Wno-pedantic', '-Wno-unused-parameter', '-Wno-parentheses'], language: 'cpp')

resource_dir = join_paths(get_option('prefix'), 'share', 'wayfire')
metadata_dir = join_paths(resource_dir, 'metadata', 'wf-shell')
sysconf_dir = join_paths(get_option('prefix'), get_option('sysconfdir'))

icon_dir = join_paths(get_option('prefix'), 'share', 'wayfire', 'icons')
add_project_arguments('-DICONDIR="' + icon_dir + '"', language : 'cpp')
add_project_arguments('-DMETADATA_DIR="' + metadata_dir + '"', language : 'cpp')
add_project_arguments('-DSYSCONF_DIR="' + sysconf_dir + '"', language : 'cpp')

subdir('metadata')
subdir('proto')
subdir('data')
subdir('src')
