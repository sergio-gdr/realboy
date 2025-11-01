/*
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

#define _GNU_SOURCE

#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include <pixman.h>

#include "render.h"

typedef struct {
	void *data;
	struct framebuffer framebuffer;
	pixman_image_t *src;
	pixman_image_t *dst;
	int framebuffer_fd;
} render_t;

render_t render;

pixman_image_t *mask;
uint8_t *dmg_buf;
void render_draw_pixel(int x, int y, uint32_t pixel) {
	assert(x <= 160 && y <= 144);
	*(uint32_t*)(dmg_buf+(y*160*(32/8))+(x*sizeof(uint32_t))) = pixel;
}

void render_get_framebuffer_dimensions(struct framebuffer *fb) {
	fb->height = render.framebuffer.height;
	fb->width = render.framebuffer.width;
	fb->size = render.framebuffer.size;
	fb->stride = render.framebuffer.stride;
}

int render_get_framebuffer_fd() {
	return render.framebuffer_fd;
}

void render_draw_framebuffer() {
	pixman_image_composite32(PIXMAN_OP_SRC, render.src, nullptr,
		render.framebuffer.image, 0, 0, 0, 0, 0, 0, render.framebuffer.width,
		render.framebuffer.height);
}

int render_init() {
	render.framebuffer.width = 160*4;
	render.framebuffer.height = 144*4;
	render.framebuffer.stride = render.framebuffer.width * (32 / 8);
	render.framebuffer.size = render.framebuffer.stride * render.framebuffer.height;
	render.framebuffer_fd = memfd_create("realboy-bg", MFD_CLOEXEC);

	struct framebuffer *framebuf = &render.framebuffer;
	posix_fallocate(render.framebuffer_fd, 0, framebuf->size * sizeof(uint32_t));
	render.data = mmap(NULL, framebuf->size * sizeof(uint32_t), PROT_READ | PROT_WRITE, MAP_SHARED,
		render.framebuffer_fd, 0);
	if (render.data == MAP_FAILED) {
		perror("mmap");
		close(render.framebuffer_fd);
		goto err;
	}
	void *src = malloc(framebuf->size * sizeof(uint32_t));
	dmg_buf = src;
	render.src = render.framebuffer.image = pixman_image_create_bits(PIXMAN_x8r8g8b8, 160, 144, src, 160*(32/8));
	render.framebuffer.image = pixman_image_create_bits(PIXMAN_x8r8g8b8, framebuf->width, framebuf->height, render.data, framebuf->stride);
	pixman_transform_t scale;
	pixman_transform_init_identity(&scale);
	pixman_transform_init_scale(&scale, (1<<16)/4, (1<<16)/4);
	pixman_image_set_transform(render.src, &scale);
	pixman_image_set_filter(render.src, PIXMAN_FILTER_BILINEAR, nullptr, 0);

	return 0;
err:
	return -1;
}
