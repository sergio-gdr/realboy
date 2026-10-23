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

#ifndef BACKENDS_H
#define BACKENDS_H

#include "config.h"

#include <stdint.h>

enum backend_type {
	BACKEND_AUDIO,
	BACKEND_DISPLAY,
	BACKEND_INPUT,
	BACKEND_RENDER
};

typedef struct {
	enum backend_type type;
	int (*init)();
	void (*fini)();
	void (*dispatch)(int fd);
	int (*get_fd)();
} backend_t;

typedef struct {
	backend_t backend;
	bool (*is_focus)();
} backend_display_ext_t;

typedef struct {
	backend_t backend;
	void *(*get_buttons)();
} backend_input_ext_t;

typedef struct {
	backend_t backend;
} backend_audio_ext_t;

typedef struct {
	backend_t backend;
	void (*draw_pixel)(int x, int y, uint32_t pixel);
	void (*composite_frame)();
	void *(*get_fb_info)();
} backend_render_ext_t;

#ifdef HAVE_WAYLAND_BACKEND
extern backend_display_ext_t wayland_backend_iface;
#endif
#ifdef HAVE_PIPEWIRE_BACKEND
extern backend_audio_ext_t pipewire_backend_iface;
#endif
#ifdef HAVE_PIPEWIRE_BACKEND
extern backend_render_ext_t pixman_backend_iface;
#endif
#ifdef HAVE_EVDEV_BACKEND
extern backend_input_ext_t evdev_backend_iface;
#endif

backend_t *backends_get_backend_by_type(enum backend_type type);

#endif
