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

#ifndef RB_RENDER_H
#define RB_RENDER_H

#include <stddef.h>
#include <stdint.h>

#include <pixman.h>

struct framebuffer {
	pixman_image_t *image;
	size_t width;
	size_t height;
	size_t stride;
	size_t size;
};

int render_init();
void render_draw_pixel(int x, int y, uint32_t pixel);
void render_draw_framebuffer();
int render_get_framebuffer_fd();
void render_get_framebuffer_dimensions(struct framebuffer *fb);
#endif
