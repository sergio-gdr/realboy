/*
 * Copyright (C) 2013-2016, 2025 Sergio Gómez Del Real
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
#include <stdio.h>

#include "backend/evdev.h"
#include "backend/wayland.h"
#include "render.h"

FILE *rom;

// populate at compile time.
// the __attribute__(section) attribute tells the compiler to create a contiguous
// area in the section "fds_to_poll" (file descriptors to poll), and a pair of symbols
// (__start* and __stop*) so we can traverse the fds.
#define POLL_ADD_SOURCE(name) int __attribute__((section("fds_to_poll"))) name##_fd;
POLL_ADD_SOURCE(wayland)
POLL_ADD_SOURCE(evdev)
extern int __start_fds_to_poll, __stop_fds_to_poll;

int main(int argc, char *argv[]) {
	int ret = 0;

	if (argc == 1) {
		return 0;
	}

	if (optind < argc) {
		if ((rom = fopen(argv[optind], "r")) == NULL) {
			perror("fopen()");
			ret = -1;
			goto err1;
		}
	}

	ret = evdev_backend_init();
	if (ret == -1) {
		goto err2;
	}
	evdev_fd = evdev_backend_get_fd();

	if ( (ret = wayland_backend_init()) == -1) {
		goto err3;
	}
	wayland_fd = wayland_backend_get_fd();

	ret = render_init();
	if (ret == -1) {
		goto err4;
	}

err5:
	render_fini();
err4:
	wayland_backend_fini();
err3:
	evdev_backend_fini();
err2:
	fclose(rom);
err1:
	return ret;
}
