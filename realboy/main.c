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

#include "backend/wayland.h"
#include "backend/evdev.h"
#include "conf.h"
#include "cpu.h"
#include "monitor.h"
#include "ppu.h"
#include "render.h"

FILE *rom;

static bool is_server_mode; // start realboy as a libemu server
static int epoll_fd;
static struct option options[] = {
	{ "server", no_argument, 0, 's' },
	{ NULL, no_argument, 0, 0 }
};

// populate at compile time
#define POLL_ADD_SOURCE(name) \
	int __attribute__ ((section("fds_to_poll"))) name##_fd;
POLL_ADD_SOURCE(wayland)
//POLL_ADD_SOURCE(evdev)

// through the __attribute__(section) above, the compiler creates a contiguous area in the section "fds_to_poll",
// and a pair of symbols (__start* and __stop*) so we can traverse the fds.
extern int __start_fds_to_poll, __stop_fds_to_poll;

static void *io_poll(void *val) {

	for (int *fd_to_poll = &__start_fds_to_poll; fd_to_poll < &__stop_fds_to_poll; fd_to_poll++) {
		while (!*fd_to_poll) {
			;
		}
	}

	int fd = epoll_create1(0);

	if (epoll_ctl(fd, EPOLL_CTL_ADD, wayland_fd,
			&(struct epoll_event) {
				.events = EPOLLIN,
				.data.fd = wayland_fd
			}) == -1) {
		perror("epoll_ctl");
	}
//if (epoll_ctl(fd, EPOLL_CTL_ADD, evdev_fd,
//&(struct epoll_event) {
//.events = EPOLLIN,
//.data.fd = evdev_fd
//}) == -1) {
//perror("epoll_ctl");
//}


	epoll_fd = fd;
	struct epoll_event event_list[10];
	while (1) {
		int num_events;
		if ((num_events = epoll_wait(epoll_fd, event_list, 10, -1)) == -1) {
			perror("epoll_wait");
		}
		for (int i = 0; i < num_events; i++) {
			if (event_list[i].data.fd == wayland_fd) {
				wayland_backend_dispatch();
			}
//else if (event_list[i].data.fd == evdev_fd) {
//struct input_event *ev;
//evdev_backend_dispatch(&ev);
//if (ev) {
//mon_set_key(ev);
//free(ev);
//}
//}
		}
	}
	pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
	int ret;

	if (argc==1) {
		return 0;
	}

	int option;
	do {
		option = getopt_long(argc, argv, "r:s", options, NULL);
		switch (option) {
			case 's':
				is_server_mode = true;
				break;
			default:
				break;
		}
	} while (option != -1);

	if (optind < argc) {
		if ((rom = fopen(argv[optind], "r")) == NULL) {
			perror(argv[optind]);
			goto err;
		}
	}

	pthread_t epoll_thread;
	ret = pthread_create(&epoll_thread, NULL, io_poll, NULL);
	if (ret)
		goto err2;

	//evdev_fd = evdev_backend_init();
	wayland_fd = wayland_backend_get_fd();
	while (!epoll_fd)
		;
	if (render_init() == -1) {
		goto err3;
	}
	struct framebuffer fb;
	render_get_framebuffer_dimensions(&fb);

	wayland_backend_init();
	int framebuffer_fd = render_get_framebuffer_fd();
	wayland_backend_set_framebuffer(framebuffer_fd, &fb);

	if (conf_init() == -1)
		goto err4;
	cpu_init();
	mon_init();
	ppu_init();

	ret = mon_run();

err4:
	//render_deinit();
err3:
	//pthread_cancel
err2:
	fclose(rom);
err:
	return -1;
}
