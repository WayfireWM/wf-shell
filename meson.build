project(
	'wf-shell',
    'c',
	'cpp',
	version: '0.3',
	license: 'MIT',
	meson_version: '>=0.51.0',
	default_options: [
		'cpp_std=c++14',
        'c_std=c11',
		'warning_level=2',
		'werror=false',
	],
)

wayfire        = dependency('wayfire')
wayland_client = dependency('wayland-client')
wayland_protos = dependency('wayland-protocols')
gtkmm          = dependency('gtkmm-3.0')
wfconfig       = dependency('wf-config', version: '>=0.3') #TODO fallback submodule
gtklayershell  = dependency('gtk-layer-shell-0', version: '>= 0.1', fallback: ['gtk-layer-shell', 'gtk_layer_shell_dep'])
libpulse       = dependency('libpulse')
libgvc         = subproject('gvc', default_options: ['static=true']).get_variable('libgvc_dep')

needs_libinotify = ['freebsd', 'dragonfly'].contains(host_machine.system())
libinotify       = dependency('libinotify', required: needs_libinotify)

add_project_arguments(['-Wno-pedantic', '-Wno-unused-parameter', '-Wno-parentheses', '-Wno-cast-function-type'], language: 'cpp')

metadata_dir = wayfire.get_variable(pkgconfig: 'metadatadir')

icon_dir = join_paths(get_option('prefix'), 'share', 'wayfire', 'icons')
add_project_arguments('-DICONDIR="' + icon_dir + '"', language : 'cpp')
add_project_arguments('-DMETADATA_DIR="' + metadata_dir + '"', language : 'cpp')

subdir('proto')
subdir('data')
subdir('src')
