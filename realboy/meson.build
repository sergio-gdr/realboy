sources = files(
	'backend/wayland/wayland.c',
	'backend/evdev/evdev.c',
	'render.c',
	'cpu/cpu.c',
	'cpu/cpu_ops.c',
	'main.c',
	'mem/mbc.c',
	'mem/mbc1.c',
	'ppu.c',
	'monitor.c',
)

pixman = dependency('pixman-1',
    version: '>=0.43.0',
    fallback: 'pixman',
    default_options: ['werror=false'],
)

realboy_dependencies = [
	pixman,
	dependency('libevdev'),
	dependency('wayland-client'),
	dependency('wayland-protocols')
]

libemu = dependency(
    'libemu',
    fallback: 'libemu',
    required: get_option('libemu'),
)

internal_config = configuration_data()
if libemu.found() and get_option('libemu') == true
    sources += files('server.c')
    realboy_dependencies += libemu
    internal_config.set('HAVE_LIBEMU', '1')
endif
configure_file(output : 'config.h',
               configuration : internal_config)

executable(
	'realboy',
	sources + wl_protos_src,
	include_directories: [includes],
	dependencies: realboy_dependencies,
	install: true
)
