project(
	'realboy',
	'c',
	version: '0.1',
	license: 'GPL-2.0-or-later',
	default_options: [
		'c_std=c23'
	],
)

subdir('src/backends/wayland/protocols')

includes = include_directories(['include', 'build/src/backends/wayland/protocols'])
subdir('src')
