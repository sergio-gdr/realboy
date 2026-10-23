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
#include "config.h"

#include "backends.h"

#include "audio/audio.h"
#include "display/display.h"
#include "input/input.h"
#include "render/render.h"

static bool backends_initialized;
static int backend_fds[NUM_BACKENDS];

backend_t *backends[NUM_BACKENDS] =
{
#ifdef HAVE_EVDEV_BACKEND
	&evdev_backend_iface.backend,
#endif
#ifdef HAVE_PIPEWIRE_BACKEND
	&pipewire_backend_iface.backend,
#endif
#ifdef HAVE_PIXMAN_BACKEND
	&pixman_backend_iface.backend,
#endif
#ifdef HAVE_WAYLAND_BACKEND
	&wayland_backend_iface.backend,
#endif
};

static backend_audio_ext_t *audio;
static backend_display_ext_t *display;
static backend_input_ext_t *input;
static backend_render_ext_t *render;

void backends_dispatch(int fd) {
	for (int i = 0; i < NUM_BACKENDS; i++) {
		if (backend_fds[i] == fd) {
			backends[i]->dispatch(fd);
		}
	}
}

int backends_get_fds(int **fds) {
	*fds = backend_fds;
	return NUM_BACKENDS;
}

void backends_fini() {
	if (!backends_initialized)
		return;

	for (int i = 0; i < NUM_BACKENDS; i++) {
		backends[i]->fini();
	}
}

static inline backend_t *backends_get_backend_by_type(enum backend_type type) {
	for (int i = 0; i < NUM_BACKENDS; i++) {
		if (backends[i]->type == type)
			return backends[i];
	}

	return NULL;
}

int backends_init() {
	for (int i = 0; i < NUM_BACKENDS; i++) {
		backend_t *backend = backends[i]->init();
		if (backend == NULL) {
			while (i) {
				backends[i]->fini();
				i--;
			}
			goto err_init;
		}
		switch (backend->type) {
			case BACKEND_AUDIO:
				audio = (backend_audio_ext_t*)backends_get_backend_by_type(BACKEND_AUDIO);
				break;
			case BACKEND_DISPLAY:
				display = (backend_display_ext_t*)backends_get_backend_by_type(BACKEND_DISPLAY);
				break;
			case BACKEND_INPUT:
				input = (backend_input_ext_t*)backends_get_backend_by_type(BACKEND_INPUT);
				break;
			case BACKEND_RENDER:
				render = (backend_render_ext_t*)backends_get_backend_by_type(BACKEND_RENDER);
				break;
		}
	}

	for (int i = 0; i < NUM_BACKENDS; i++) {
		backend_fds[i] = backends[i]->get_fd();
	}

	backends_initialized = true;

	return 0;

err_init:
	return -1;
}

void backend_composite_frame() {
	render->composite_frame();
}

void backend_draw_pixel(int x, int y, uint32_t pixel) {
	render->draw_pixel(x, y, pixel);
}

void *backend_get_fb_info() {
	return render->get_fb_info();
}

int backend_get_fb_fd() {
	return render->backend.get_fd();
}

void *backend_get_buttons() {
	return input->get_buttons();
}
