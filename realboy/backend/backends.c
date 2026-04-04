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
#include "backend/wayland.h"
#include "backend/evdev.h"

#define countof(arr) (sizeof(arr) / sizeof((arr)[0]))

struct backend *backends[] = { &evdev_backend_iface, &wayland_backend_iface };
int backend_fds[countof(backends)];

void backends_dispatch(int fd) {
	for (int i = 0; i < countof(backends); i++) {
		backends[i]->dispatch(fd);
	}
}

int *backends_get_fds() {
	return backend_fds;
}

void backends_fini() {
	for (int i = 0; i < countof(backends); i++) {
		backends[i]->fini();
	}
}

int backends_init() {
	int ret;

	for (int i = 0; i < countof(backends); i++) {
		ret = backends[i]->init();
		if (ret == -1) {
			goto err_init;
		}
	}

	int num_fds = 0;
	for (int i = 0; i < countof(backends); i++) {
		backend_fds[i] = backends[i]->get_fd();
		num_fds++;
	}

	return num_fds;

err_init:
	backends_fini();
	return -1;
}
