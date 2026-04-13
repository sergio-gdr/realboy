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
#include <stdio.h>

#include "../include/backend/backends.h"

#include "config.h"

struct backend *backends[NUM_BACKENDS] =
{
#ifdef HAVE_WAYLAND_BACKEND
	&wayland_backend_iface.backend,
#endif
#ifdef HAVE_PIPEWIRE_BACKEND
	&pipewire_backend_iface.backend,
#endif
#ifdef HAVE_EVDEV_BACKEND
	&evdev_backend_iface,
#endif
};
int backend_fds[NUM_BACKENDS];

void backends_dispatch(int fd) {
	for (int i = 0; i < NUM_BACKENDS; i++) {
		backends[i]->dispatch(fd);
	}
}

int backends_get_fds(int **fds) {
	*fds = backend_fds;
	return NUM_BACKENDS;
}

void backends_fini() {
	for (int i = 0; i < NUM_BACKENDS; i++) {
		backends[i]->fini();
	}
}

int backends_init() {
	int ret;

	for (int i = 0; i < NUM_BACKENDS; i++) {
		ret = backends[i]->init();
		if (ret == -1) {
			goto err_init;
		}
	}

	for (int i = 0; i < NUM_BACKENDS; i++) {
		backend_fds[i] = backends[i]->get_fd();
	}

	return 0;

err_init:
	backends_fini();
	return -1;
}

struct backend *backends_get_backend_by_type(enum backend_type type) {
	for (int i = 0; i < NUM_BACKENDS; i++) {
		if (backends[i]->type == type)
			return backends[i];
	}

	return NULL;
}
