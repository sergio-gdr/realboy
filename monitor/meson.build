sources = files(
	'main.c',
	'client.c',
	'disasm.c',
	'../realboy/list.c',
	'tui/cli.c',
	'tui/tui.c',
	'../ipc/ipc.c',
)

executable(
	'monitor',
	sources,
	include_directories: [includes],
	dependencies: [dependency('ncurses')],
	install: false
)
