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

// generic entry points into the backend module

#ifndef RB_BACKENDS_H
#define RB_BACKENDS_H

#include <stddef.h>
#include <stdint.h>

int backends_init();
void backends_fini();
int backends_get_fds(int **fds);
void backends_dispatch(int fd);

void backend_draw_pixel(int x, int y, uint32_t pixel);
void backend_composite_frame();
void *backend_get_fb_info();
int backend_get_fb_fd();

void *backend_get_buttons();

#endif
