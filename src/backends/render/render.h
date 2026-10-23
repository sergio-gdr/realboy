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

#ifndef RENDER_BACKEND_H
#define RENDER_BACKEND_H

#include "config.h"

#include <stddef.h>

#include "backends/backends.h"

struct framebuffer_info {
	int fd;
	size_t width;
	size_t height;
	size_t stride;
	size_t size;
};

typedef struct {
	backend_t backend;
	void (*draw_pixel)(int x, int y, uint32_t pixel);
	void (*composite_frame)();
	struct framebuffer_info *(*get_fb_info)();
} backend_render_ext_t;

#ifdef HAVE_PIXMAN_BACKEND
extern backend_render_ext_t pixman_backend_iface;
#endif

#endif
