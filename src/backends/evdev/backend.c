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

#include "../include/backends/backends.h"
#include "backends/evdev/evdev.h"

int evdev_fd;

static int backend_get_fd() {
	return evdev_fd;
}

static int backend_init() {
	evdev_fd = evdev_init();

	if (evdev_fd == -1)
		return -1;

	return 0;
}

struct backend evdev_backend_iface =
{
	.type = BACKEND_INPUT,
	.init = backend_init,
	.fini = evdev_fini,
	.get_fd = backend_get_fd,
	.dispatch = evdev_dispatch
};

