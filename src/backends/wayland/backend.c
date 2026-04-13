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

#include "wayland-client.h"

#include "wayland.h"
#include "../include/backends/backends.h"

struct wl_display *display;

static void backend_dispatch(int fd) {
	if (fd == wl_display_get_fd(display)) {
		wl_display_dispatch(display);
	}
}

static int backend_get_fd() {
	return wl_display_get_fd(display);
}

static void backend_fini() {
	wl_display_disconnect(display);
}

static int backend_init() {
	display = wayland_init();

	return 0;
}

extern bool wayland_is_focus();
struct backend_display_ext wayland_backend_iface =
{
	.backend.type = BACKEND_DISPLAY,
	.backend.init = backend_init,
	.backend.fini = backend_fini,
	.backend.get_fd = backend_get_fd,
	.backend.dispatch = backend_dispatch,
	.is_focus = wayland_is_focus
};

