project(
	'realboy',
	'c',
	version: '0.1',
	license: 'GPL-2.0-or-later',
	default_options: [
		'c_std=c23'
	],
)

subdir('realboy/backend/wayland/protocols')

includes = include_directories(['include', 'build/realboy/backend/wayland/protocols'])
subdir('realboy')
