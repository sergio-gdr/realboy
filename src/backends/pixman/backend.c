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

#define _GNU_SOURCE

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include <pixman.h>

#include "backends/backends.h"
#include "backends.h"

typedef struct {
	struct framebuffer_info fbinfo;
	pixman_image_t *pix_dst;
	pixman_image_t *pix_src;
	uint32_t *raw_buf;
	uint32_t *final_buf;
	int framebuffer_fd;
} render_t;

static render_t render;

#define DMG_PIXELS_WIDTH 160
#define DMG_PIXELS_HEIGHT 144

static void draw_pixel(int x, int y, uint32_t pixel) {
	assert(x <= DMG_PIXELS_WIDTH && y <= DMG_PIXELS_HEIGHT);
	*(render.raw_buf+(y*DMG_PIXELS_WIDTH)+(x)) = pixel;
}

static void composite_frame() {
	pixman_image_composite32(PIXMAN_OP_SRC, render.pix_src, nullptr,
		render.pix_dst, 0, 0, 0, 0, 0, 0, render.fbinfo.width,
		render.fbinfo.height);
}

static void fini() {
	if (render.pix_src)
		pixman_image_set_transform(render.pix_src, NULL);
	if (render.final_buf)
		munmap(render.final_buf, render.fbinfo.size * sizeof(uint32_t));
	if (render.framebuffer_fd)
		close(render.framebuffer_fd);
	free(render.pix_src);
	free(render.pix_dst);
	free(render.raw_buf);
}

static int init() {
	render.fbinfo.width = DMG_PIXELS_WIDTH*4;
	render.fbinfo.height = DMG_PIXELS_HEIGHT*4;
	render.fbinfo.stride = render.fbinfo.width * sizeof(uint32_t);
	render.fbinfo.size = render.fbinfo.stride * render.fbinfo.height;
	// create an anonymous file in CPU memory so that we can give an fd with the final rendering to the display backend (Wayland).
	render.framebuffer_fd = memfd_create("realboy-bg", MFD_CLOEXEC);
	if (render.framebuffer_fd == -1) {
		perror("memfd_create()");
		return -1;
	}

	// this makes sure that there is enough disk space to backup the anonymous file
	struct framebuffer_info *framebuf = &render.fbinfo;
	if (posix_fallocate(render.framebuffer_fd, 0, framebuf->size * sizeof(uint32_t)) != 0) {
		perror("posix_fallocate()");
		goto err;
	}

	// this buffer will hold the pixels directly drawn by the PPU
	render.raw_buf = malloc(DMG_PIXELS_HEIGHT * DMG_PIXELS_WIDTH * sizeof(uint32_t));
	if (!render.raw_buf) {
		perror("malloc()");
		goto err;
	}
	render.pix_src = pixman_image_create_bits(PIXMAN_x8r8g8b8, DMG_PIXELS_WIDTH, DMG_PIXELS_HEIGHT, render.raw_buf,
		DMG_PIXELS_WIDTH*sizeof(uint32_t));
	if (!render.pix_src) {
		perror("pixman_image_create_bits()");
		goto err;
	}
	// we apply a transform that will scale by 4x and do 2D bilinear interpolation
	pixman_transform_t scale;
	pixman_transform_init_identity(&scale);
	pixman_transform_init_scale(&scale, pixman_double_to_fixed(0.25), pixman_double_to_fixed(0.25));
	pixman_image_set_transform(render.pix_src, &scale);
	pixman_image_set_filter(render.pix_src, PIXMAN_FILTER_BILINEAR, nullptr, 0);

	// this buffer will hold the final transformation done by pixman to the raw_buf.
	// it is what will ultimately be sent to the display backend (Wayland) through framebuffer_fd.
	render.final_buf = mmap(NULL, framebuf->width * framebuf->height * sizeof(uint32_t), PROT_READ | PROT_WRITE, MAP_SHARED,
		render.framebuffer_fd, 0);
	if (render.final_buf == MAP_FAILED) {
		perror("mmap()");
		goto err;
	}
	render.pix_dst = pixman_image_create_bits(PIXMAN_x8r8g8b8, framebuf->width, framebuf->height, render.final_buf, framebuf->stride);
	if (!render.pix_dst) {
		perror("pixman_image_create_bits()");
		goto err;
	}

	return 0;
err:
	fini();
	return -1;
}

static void *get_fb_info() {
	return &render.fbinfo;
}

static int get_fd() {
	return render.framebuffer_fd;
}

backend_render_ext_t pixman_backend_iface =
{
	.backend.type = BACKEND_RENDER,
	.backend.init = init,
	.backend.fini = fini,
	.backend.get_fd = get_fd,
	.draw_pixel = draw_pixel,
	.composite_frame = composite_frame,
	.get_fb_info = get_fb_info
};
