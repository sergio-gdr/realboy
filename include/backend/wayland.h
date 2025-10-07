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

#ifndef RB_BACKEND_WAYLAND_H
#define RB_BACKEND_WAYLAND_H

#include <stddef.h>
#include <stdint.h>

#include "render.h"

void wayland_backend_update_frame();
void wayland_backend_draw_pixel(int x, int y, uint32_t rgb);

void wayland_backend_init();
void wayland_backend_set_framebuffer(int fd, struct framebuffer *fb);

// returns fd to integrate to epoll
int wayland_backend_get_fd();

void wayland_backend_dispatch();
bool wayland_backend_is_focus();
#endif
