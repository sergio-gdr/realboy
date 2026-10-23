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

#include <stdint.h>

enum backend_type {
	BACKEND_AUDIO,
	BACKEND_DISPLAY,
	BACKEND_INPUT,
	BACKEND_RENDER
};

struct backend;
typedef struct backend {
	enum backend_type type;
	struct backend *(*init)();
	void (*fini)();
	void (*dispatch)(int fd);
	int (*get_fd)();
} backend_t;

#endif
