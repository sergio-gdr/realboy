sources = files(
	'wayland/wayland.c',
	'wayland/backend.c',
	'pipewire/pipewire.c',
	'pipewire/backend.c',
	'evdev/evdev.c',
	'evdev/backend.c',
	'backends.c',
	'render.c',
	'emu/cpu/cpu.c',
	'emu/cpu/cpu_ops.c',
	'emu/mem/mbc.c',
	'emu/mem/mbc1.c',
	'emu/ppu.c',
	'main.c',
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
    required: false,
)

internal_config = configuration_data()
if libemu.found() and get_option('libemu') == true
    sources += files('server.c')
    realboy_dependencies += libemu
    internal_config.set('HAVE_LIBEMU', '1')
endif

num_backends = 0
if get_option('wayland-backend') == true
    internal_config.set('HAVE_WAYLAND_BACKEND', '1')
	num_backends += 1
endif
if get_option('pipewire-backend') == true
    internal_config.set('HAVE_PIPEWIRE_BACKEND', '1')
	num_backends += 1
endif
if get_option('evdev-backend') == true
    internal_config.set('HAVE_EVDEV_BACKEND', '1')
	num_backends += 1
endif
internal_config.set('NUM_BACKENDS', num_backends)

configure_file(output : 'config.h',
               configuration : internal_config)

executable(
	'realboy',
	sources + wl_protos_src,
	include_directories: [includes],
	dependencies: realboy_dependencies,
	install: true
)
