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
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"

struct wl_display *display;
struct wl_registry *registry;
struct wl_compositor *compositor;
struct wl_shm *shm;
struct wl_shm_pool *shm_pool;
struct wl_surface *surface;
struct xdg_surface *xdg_surface;
struct xdg_toplevel *toplevel;
struct xdg_wm_base *wm_base;
struct wl_callback *frame_callback;
bool window_configured;

uint32_t (*buf)[256][256];

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
		compositor = wl_registry_bind(registry, id, &wl_compositor_interface, 1);
	} else if (strcmp(interface, "xdg_wm_base") == 0) {
		wm_base = wl_registry_bind(registry, id, &xdg_wm_base_interface, 1);
		xdg_wm_base_add_listener(wm_base, &xdg_wm_base_listener, NULL);
	} else if (strcmp(interface, "wl_shm") == 0) {
		shm = wl_registry_bind(registry, id, &wl_shm_interface, 1);
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

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
	.configure = handle_xdg_toplevel_configure,
	.close = handle_xdg_toplevel_close,
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

static const struct wl_callback_listener frame_listener;
void backend_wayland_update_frame() {
	struct wl_buffer *wl_buf = wl_shm_pool_create_buffer(shm_pool, 0, 256, 256, 256 * 4, WL_SHM_FORMAT_XRGB8888);
	wl_surface_attach(surface, wl_buf, 0, 0);
	wl_surface_damage(surface, 0, 0, 256, 256);
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

void backend_wayland_draw_pixel(int x, int y, uint32_t rgb) {
	(*buf)[y][x] = rgb;
}

int backend_wayland_get_fd() {
	display = wl_display_connect(NULL);
	assert(display);
	return wl_display_get_fd(display);
}

void backend_wayland_dispatch() {
	wl_display_dispatch(display);
}

void backend_wayland_init() {
	registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry,
		&registry_listener, NULL);
	wl_display_roundtrip(display);
	assert(shm);

	int width = 256;
	int height = 256;
	int stride = width * (32 / 8);
	int size = stride * height;

	int fd = memfd_create("realboy-bg", MFD_CLOEXEC);
	posix_fallocate(fd, 0, size * sizeof(uint32_t));
	buf = mmap(NULL, size * sizeof(uint32_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (buf == MAP_FAILED) {
		perror("mmap");
		return;
	}
	shm_pool = wl_shm_create_pool(shm, fd, size);
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
