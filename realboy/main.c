/* RealBoy Emulator
 * Copyright (C) 2013-2025 Sergio Gómez Del Real <sgdr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <getopt.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/epoll.h>

#include "conf.h"
#include "cpu.h"
#include "ipc.h"
#include "ppu.h"
#include "server.h"
#include "wayland.h"

static bool is_debug_mode;

FILE *rom;
struct option options[] = {
	{ "debug", no_argument, 0, 'd' },
	{ NULL, no_argument, 0, 0 }
};

enum {
	EVENT_WAYLAND,
};

int epoll_fd;
int wl_display_fd;
static void *io_poll(void *val) {
	while (!wl_display_fd)
		;

	int fd = epoll_create(10);

	if (epoll_ctl(fd, EPOLL_CTL_ADD, wl_display_fd,
			&(struct epoll_event) {
				.events = EPOLLIN,
				.data.fd = EVENT_WAYLAND
			}) == -1) {
		perror("epoll_ctl");
	}

	epoll_fd = fd;
	struct epoll_event event_list[10];
	while (1) {
		int num_events;
		if ((num_events = epoll_wait(epoll_fd, event_list, 10, -1)) == -1) {
			perror("epoll_wait");
		}
		for (int i = 0; i < num_events; i++) {
			switch (event_list[i].data.fd) {
				case EVENT_WAYLAND:
					backend_wayland_dispatch();
					break;
				default:
					perror("epoll_wait: unknown event");
			}
		}
	}
	pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
	if (argc==1) {
		return 0;
	}

	int option;
	do {
		option = getopt_long(argc, argv, "r:1234fhdbvDCS", options, NULL);
		switch (option) {
			case 'd':
				is_debug_mode = true;
				break;
			default:
				break;
		}
	} while (option != -1);

	// initialize the ipc server
	if (is_debug_mode) {
		if (server_init() == -1)
			goto out;
	}

	if (optind < argc) {
		if ((rom = fopen(argv[optind], "r")) == NULL) {
			perror(argv[optind]);
			goto err1;
		}
	}

	pthread_t epoll_thread;
	pthread_create(&epoll_thread, NULL, io_poll, NULL);

	// XXX abstract the wayland backend
	wl_display_fd = backend_wayland_get_fd();
	while (!epoll_fd)
		;
	backend_wayland_init();

	if (conf_init() == -1)
		goto err2;
	cpu_init();
	ppu_init();

	while (1) {
		while (is_debug_mode) {
			int ret;
			if ((ret = server_recv_request_and_dispatch()) == -1)
				goto fini;

			// this return value indicates a request to continue execution
			if (ret == 0)
				break;
		}
		int cycles = cpu_exec_next();
		if (ppu_rd(PPU_REG_LCDC) & LCDC_BITMASK_PPU_ENABLE) {
			ppu_refresh(cycles);
	}
	}

fini:
	//conf_fini();
err2:
	//backend_wayland_fini();
	fclose(rom);
err1:
	ipc_fini();
out:
	return 0;
}
