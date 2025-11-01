/* RealBoy Emulator
 * Copyright (C) 2025 Sergio Gómez Del Real
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

// Wayland backend

#define _GNU_SOURCE
#include <assert.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"

#include "monitor.h"
#include "render.h"

struct wl_compositor *compositor;
struct wl_display *display;
struct wl_keyboard *keyboard;
struct wl_registry *registry;
struct wl_shm *shm;
struct wl_seat *seat;
struct wl_shm_pool *shm_pool;
struct wl_surface *surface;
struct xdg_surface *xdg_surface;
struct xdg_toplevel *toplevel;
struct xdg_wm_base *wm_base;
struct wl_callback *frame_callback;
bool window_configured;

typedef struct {
	int framebuffer_fd;
	size_t width;
	size_t height;
	size_t stride;
	size_t size;
} wayland_backend_t;
wayland_backend_t wayland_backend;

static bool is_surface_focused;

static void handle_keyboard_enter(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t serial, struct wl_surface *surface, struct wl_array *keys) {
	is_surface_focused = true;
}

static void handle_keyboard_leave(void *data,
		struct wl_keyboard *wl_keyboard, uint32_t serial, struct wl_surface *surface) {
	is_surface_focused = false;
}

static void handle_key(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t serial, uint32_t time, uint32_t key, uint32_t state) {
	switch (key) {
		case KEY_ENTER:
		case KEY_SPACE:
		case KEY_D:
		case KEY_S:
		case KEY_RIGHT:
		case KEY_H:
		case KEY_UP:
		case KEY_K:
		case KEY_DOWN:
		case KEY_J:
		case KEY_L:
		case KEY_LEFT:
			struct input_event ev = { .value = state == WL_KEYBOARD_KEY_STATE_PRESSED ? 1 : 0, .time
				= time, .type = EV_KEY, .code = key };
			mon_set_key(&ev);
			break;
	}
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
		keyboard = wl_seat_get_keyboard(seat);
		assert(keyboard);
		wl_keyboard_add_listener(keyboard, &keyboard_listener, NULL);
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
		compositor = wl_registry_bind(registry, id, &wl_compositor_interface, 6);
	} else if (strcmp(interface, "xdg_wm_base") == 0) {
		wm_base = wl_registry_bind(registry, id, &xdg_wm_base_interface, 5);
		xdg_wm_base_add_listener(wm_base, &xdg_wm_base_listener, NULL);
	} else if (strcmp(interface, "wl_shm") == 0) {
		shm = wl_registry_bind(registry, id, &wl_shm_interface, 2);
	} else if (strcmp(interface, "wl_seat") == 0) {
		seat = wl_registry_bind(registry, id, &wl_seat_interface, 9);
		wl_seat_add_listener(seat, &seat_listener, NULL);
	}
}

static void registry_handle_global_remove(void *data, struct wl_registry *registry,
	uint32_t name) {
}

static const struct wl_registry_listener registry_listener = {
	registry_handle_global,
	registry_handle_global_remove
};

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

void backend_wayland_update_frame();
static void
handle_xdg_surface_configure(void *data, struct xdg_surface *surface, uint32_t serial)
{
	static bool configured;
	xdg_surface_ack_configure(surface, serial);
	if (!configured) {
		configured = true;
		backend_wayland_update_frame();
	}
}

static const struct xdg_surface_listener xdg_surface_listener = {
	handle_xdg_surface_configure,
};

void wayland_backend_set_framebuffer(int fd, struct framebuffer *fb) {
	wayland_backend.framebuffer_fd = fd;
	wayland_backend.width = fb->width;
	wayland_backend.height = fb->height;
	wayland_backend.size = fb->size;
	wayland_backend.stride = fb->stride;

	shm_pool = wl_shm_create_pool(shm, wayland_backend.framebuffer_fd, wayland_backend.size);
}

static const struct wl_callback_listener frame_listener;
void backend_wayland_update_frame() {
	wayland_backend_t *back = &wayland_backend;

	if (!shm_pool)
		return;
	struct wl_buffer *wl_buf = wl_shm_pool_create_buffer(shm_pool, 0, back->width, back->height, back->stride, WL_SHM_FORMAT_XRGB8888);
	wl_surface_attach(surface, wl_buf, 0, 0);
	wl_surface_damage(surface, 0, 0, back->width, back->height);
	if (frame_callback)
		wl_callback_destroy(frame_callback);

	frame_callback = wl_surface_frame(surface);
	wl_callback_add_listener(frame_callback, &frame_listener, NULL);
	wl_surface_commit(surface);
	wl_display_flush(display);
}

static void done(void *data, struct wl_callback *wl_callback,
	uint32_t callback_data) {

	backend_wayland_update_frame();
}

static const struct wl_callback_listener frame_listener = {
        .done = done
};

int wayland_backend_get_fd() {
	display = wl_display_connect(NULL);
	assert(display);
	return wl_display_get_fd(display);
}

bool wayland_backend_is_focus() {
	return is_surface_focused;
}

void wayland_backend_dispatch() {
	wl_display_dispatch(display);
}

void wayland_backend_init(int framebuffer_fd, size_t framebuffer_size) {
	registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry,
		&registry_listener, NULL);
	wl_display_roundtrip(display);
	assert(shm);

	surface = wl_compositor_create_surface(compositor);

	xdg_surface = xdg_wm_base_get_xdg_surface(wm_base, surface);
	assert(xdg_surface);
	xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, NULL);

	toplevel = xdg_surface_get_toplevel(xdg_surface);
	xdg_toplevel_add_listener(toplevel, &xdg_toplevel_listener, NULL);
	xdg_toplevel_set_title(toplevel, "realboy");
	xdg_toplevel_set_app_id(toplevel, "org.freedesktop.realboy");
	assert(toplevel);

	wl_surface_commit(surface);
	wl_display_flush(display);
}
