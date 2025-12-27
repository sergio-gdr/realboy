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

#include "wayland-client-core.h"

typedef struct {
	struct wl_display *display;
} wayland_backend_t;
wayland_backend_t wayland_backend;

int wayland_backend_get_fd() {
	return wl_display_get_fd(wayland_backend.display);
}

void wayland_backend_fini() {
	wl_display_disconnect(wayland_backend.display);
}

int wayland_backend_init() {
	wayland_backend_t *back = &wayland_backend;

	back->display = wl_display_connect(NULL);
	if (!back->display) {
		return -1;
	}

	return 0;
}
