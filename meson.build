project(
	'wf-shell',
    'c',
	'cpp',
	version: '0.3',
	license: 'MIT',
	meson_version: '>=0.47.0',
	default_options: [
		'cpp_std=c++14',
        'c_std=c11',
		'warning_level=2',
		'werror=false',
	],
)

wayland_client = dependency('wayland-client')
wayland_protos = dependency('wayland-protocols')
gtkmm          = dependency('gtkmm-3.0')
wfconfig       = dependency('wf-config', version: '>=0.3') #TODO fallback submodule
gtklayershell  = dependency('gtk-layer-shell-0', version: '>= 0.1', fallback: ['gtk-layer-shell', 'gtk_layer_shell_dep'])
libpulse       = dependency('libpulse', required : get_option('pulse'))
libgvc         = subproject('gvc', default_options: ['static=true'], required : get_option('pulse'))

conf_data = configuration_data()

if libpulse.found()
  libgvc = libgvc.get_variable('libgvc_dep')
  conf_data.set('HAVE_PULSE', true)
endif

configure_file(input: 'config.h.in',
               output: 'config.h',
               configuration: conf_data)

needs_libinotify = ['freebsd', 'dragonfly'].contains(host_machine.system())
libinotify       = dependency('libinotify', required: needs_libinotify)

add_project_arguments(['-Wno-pedantic', '-Wno-unused-parameter', '-Wno-parentheses', '-Wno-cast-function-type'], language: 'cpp')

icon_dir = join_paths(get_option('prefix'), 'share', 'wayfire', 'icons')
add_project_arguments('-DICONDIR="' + icon_dir + '"', language : 'cpp')

subdir('proto')
subdir('data')
subdir('src')
