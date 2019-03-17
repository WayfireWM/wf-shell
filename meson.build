project(
	'wf-shell',
    'c',
	'cpp',
	version: '0.1',
	license: 'MIT',
	meson_version: '>=0.43.0',
	default_options: [
		'cpp_std=c++11',
        'c_std=c11',
		'warning_level=2',
		'werror=false',
		'sysconfdir=/etc',
	],
)

wayland_client = dependency('wayland-client')
wayland_protos = dependency('wayland-protocols')
gtkmm          = dependency('gtkmm-3.0')
wfconfig       = dependency('wf-config') #TODO fallback submodule

prefixdir=get_option('prefix')
sysconfdir=join_paths(prefixdir, get_option('sysconfdir'))
conf_data = configuration_data()
conf_data.set('SYSCONFDIR', get_option('sysconfdir'))
install_data('wf-shell.ini.example', rename: 'wf-shell.ini',
		install_dir: join_paths(sysconfdir, 'wf-shell'))

add_project_arguments(['-Wno-pedantic', '-Wno-unused-parameter', '-Wno-parentheses', '-Wno-cast-function-type'], language: 'cpp')

icon_dir = join_paths(get_option('prefix'), 'share', 'wayfire', 'icons')
add_global_arguments('-DICONDIR="' + icon_dir + '"', language : 'cpp')
add_global_arguments('-DSYSCONFDIR="' + get_option('sysconfdir') + '"', language : 'cpp')

subdir('proto')
subdir('data')
subdir('src')
