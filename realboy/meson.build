sources = files(
	'../ipc/ipc.c',
	'backend/wayland/wayland.c',
	'conf.c',
	'cpu/cpu.c',
	'cpu/cpu_ops.c',
	'list.c',
	'main.c',
	'mem/mbc.c',
	'mem/mbc1.c',
	'ppu.c',
	'server.c',
)

executable(
	'realboy',
	sources + wl_protos_src,
	include_directories: [includes],
	dependencies: [dependency('wayland-client'), dependency('wayland-protocols')],
	install: false
)
