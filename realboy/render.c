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

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include <pixman.h>

#include "render.h"

typedef struct {
	void *data;
	struct framebuffer framebuffer;
	pixman_image_t *dst;
	pixman_image_t *src;
	int framebuffer_fd;
} render_t;
render_t render;

void render_fini() {
	munmap(render.data, render.framebuffer.size * sizeof(uint32_t));
	close(render.framebuffer_fd);
}

int render_init() {
	render.framebuffer.width = 160*4;
	render.framebuffer.height = 144*4;
	render.framebuffer.stride = render.framebuffer.width * (32 / 8);
	render.framebuffer.size = render.framebuffer.stride * render.framebuffer.height;
	render.framebuffer_fd = memfd_create("realboy-bg", MFD_CLOEXEC);
	if (render.framebuffer_fd == -1) {
		perror("memfd_create()");
		return -1;
	}

	struct framebuffer *framebuf = &render.framebuffer;
	if (posix_fallocate(render.framebuffer_fd, 0, framebuf->size * sizeof(uint32_t)) != 0) {
		perror("posix_fallocate()");
		goto err;
	}
	render.data = mmap(NULL, framebuf->size * sizeof(uint32_t), PROT_READ | PROT_WRITE, MAP_SHARED,
		render.framebuffer_fd, 0);
	if (render.data == MAP_FAILED) {
		perror("mmap()");
		goto err;
	}
	void *src = malloc(framebuf->size * sizeof(uint32_t));
	if (!src) {
		perror("malloc()");
		goto err;
	}

	return 0;
err:
	render_fini();
	return -1;
}
