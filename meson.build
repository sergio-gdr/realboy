project(
	'realboy',
	'c',
	version: '1.0-dev',
	license: 'MIT',
	meson_version: '>=1.3',
	default_options: [
		'c_std=c11',
		'warning_level=2',
		'wrap_mode=nodownload',
	],
)

add_project_arguments(
	[
		'-Wno-unused-parameter',
		'-Wno-unused-result',
		'-Wno-missing-braces',
		'-Wno-format-zero-length',
		'-Wundef',
		'-Wvla',
	],
	language: 'c',
)

subdir('realboy/backend/wayland/protocols')

includes = include_directories(['build/realboy/backend/wayland/protocols'])
subdir('realboy')
