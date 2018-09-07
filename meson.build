project(
	'wf-shell',
    'c',
	'cpp',
	version: '0.0',
	license: 'MIT',
	meson_version: '>=0.43.0',
	default_options: [
		'cpp_std=c++11',
        'c_std=c11',
		'warning_level=2',
		'werror=false',
	],
)

wayland_client = dependency('wayland-client')
wayland_protos = dependency('wayland-protocols')
gtkmm          = dependency('gtkmm-3.0')
wfconfig       = dependency('wf-config') #TODO fallback submodule

add_project_arguments(['-Wno-pedantic', '-Wno-unused-parameter', '-Wno-parentheses', '-Wno-cast-function-type'], language: 'cpp')

subdir('proto')
subdir('src')
