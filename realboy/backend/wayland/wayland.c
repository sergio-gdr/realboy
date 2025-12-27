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

#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "wayland-client.h"
#include "xdg-shell-client-protocol.h"

typedef struct {
	struct wl_display *display;
	struct wl_compositor *compositor;
	struct wl_keyboard *keyboard;
	struct wl_registry *registry;
	struct wl_shm *shm;
	struct wl_seat *seat;
	struct wl_surface *surface;
	struct xdg_surface *xdg_surface;
	struct xdg_toplevel *toplevel;
	struct xdg_wm_base *wm_base;

	bool window_configured;
	bool is_surface_focused;
} wayland_backend_t;
wayland_backend_t wayland_backend;

static void
handle_xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel,
			      int32_t width, int32_t height,
			      struct wl_array *states)
{
}

static void
handle_xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel)
{
}

static void handle_xdg_configure_bounds(void *data, struct xdg_toplevel *xdg_toplevel,
		int32_t width, int32_t height) {
}

static void handle_xdg_wm_capabilities(void *data, struct xdg_toplevel *xdg_toplevel,
		struct wl_array *capabilities) {
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
	.configure = handle_xdg_toplevel_configure,
	.close = handle_xdg_toplevel_close,
	.configure_bounds = handle_xdg_configure_bounds,
	.wm_capabilities = handle_xdg_wm_capabilities
};
static void
handle_xdg_surface_configure(void *data, struct xdg_surface *surface, uint32_t serial)
{
	static bool configured;
	xdg_surface_ack_configure(surface, serial);
	if (!configured) {
		configured = true;
	}
}

static const struct xdg_surface_listener xdg_surface_listener = {
	handle_xdg_surface_configure,
};

static void handle_keyboard_enter(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t serial, struct wl_surface *surface, struct wl_array *keys) {
	wayland_backend.is_surface_focused = true;
}

static void handle_keyboard_leave(void *data,
		struct wl_keyboard *wl_keyboard, uint32_t serial, struct wl_surface *surface) {
	wayland_backend.is_surface_focused = false;
}

static void handle_key(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t serial, uint32_t time, uint32_t key, uint32_t state) {
}

static void handle_keymap(void *data, struct wl_keyboard *wl_keyboard, uint32_t format,
		int32_t fd, uint32_t size) {
	close(fd);
}

static void handle_modifiers(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial,
		uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group) {
}

static void handle_repeat_info(void *data, struct wl_keyboard *wl_keyboard, int32_t rate,
		int32_t delay) {
}

static const struct wl_keyboard_listener keyboard_listener = {
	.enter = handle_keyboard_enter,
	.leave = handle_keyboard_leave,
	.key = handle_key,
	.keymap = handle_keymap,
	.modifiers = handle_modifiers,
	.repeat_info = handle_repeat_info
};

static void handle_seat_name(void *data, struct wl_seat *wl_seat, const char *name) {
}

static void handle_seat_capabilities(void *data, struct wl_seat *wl_seat,
		uint32_t capabilities) {
	if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) {
		wayland_backend.keyboard = wl_seat_get_keyboard(wayland_backend.seat);
		assert(wayland_backend.keyboard);
		wl_keyboard_add_listener(wayland_backend.keyboard, &keyboard_listener, NULL);
	}
}

static const struct wl_seat_listener seat_listener = {
	.capabilities = handle_seat_capabilities,
	.name = handle_seat_name
};

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *shell, uint32_t serial) {
	xdg_wm_base_pong(shell, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
	xdg_wm_base_ping,
};
static void
registry_handle_global(void *data, struct wl_registry *registry,
	uint32_t id, const char *interface, uint32_t version) {
	if (strcmp(interface, "wl_compositor") == 0) {
		wayland_backend.compositor = wl_registry_bind(registry, id, &wl_compositor_interface, 6);
	} else if (strcmp(interface, "xdg_wm_base") == 0) {
		wayland_backend.wm_base = wl_registry_bind(registry, id, &xdg_wm_base_interface, 5);
		xdg_wm_base_add_listener(wayland_backend.wm_base, &xdg_wm_base_listener, NULL);
	} else if (strcmp(interface, "wl_shm") == 0) {
		wayland_backend.shm = wl_registry_bind(registry, id, &wl_shm_interface, 2);
	} else if (strcmp(interface, "wl_seat") == 0) {
		wayland_backend.seat = wl_registry_bind(registry, id, &wl_seat_interface, 9);
		wl_seat_add_listener(wayland_backend.seat, &seat_listener, NULL);
	}
}

static void registry_handle_global_remove(void *data, struct wl_registry *registry,
	uint32_t name) {
}

static const struct wl_registry_listener registry_listener = {
	registry_handle_global,
	registry_handle_global_remove
};

int wayland_backend_get_fd() {
	return wl_display_get_fd(wayland_backend.display);
}

bool wayland_backend_is_focus() {
	return wayland_backend.is_surface_focused;
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
	back->registry = wl_display_get_registry(back->display);
	wl_registry_add_listener(back->registry,
		&registry_listener, NULL);
	wl_display_roundtrip(back->display);
	assert(back->shm);

	back->surface = wl_compositor_create_surface(back->compositor);

	back->xdg_surface = xdg_wm_base_get_xdg_surface(back->wm_base, back->surface);
	assert(back->xdg_surface);
	xdg_surface_add_listener(back->xdg_surface, &xdg_surface_listener, NULL);

	back->toplevel = xdg_surface_get_toplevel(back->xdg_surface);
	xdg_toplevel_add_listener(back->toplevel, &xdg_toplevel_listener, NULL);
	xdg_toplevel_set_title(back->toplevel, "realboy");
	xdg_toplevel_set_app_id(back->toplevel, "org.freedesktop.realboy");
	assert(back->toplevel);

	wl_surface_commit(back->surface);
	wl_display_flush(back->display);

	return 0;
}
