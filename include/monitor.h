/* RealBoy Emulator
 * Copyright (C) 2013-2025 Sergio Gómez Del Real <sgdr>
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

#ifndef MON_H
#define MON_H

#include <stdint.h>
#include <stdlib.h>

#include <linux/input.h>

enum cpu_peek_type;
enum ppu_peek_type;

enum peek_type {
	PEEK_CPU,
	PEEK_PPU,
};

struct peek_reply {
	size_t size;
	union {
		void *ptr_pay;
		uint32_t ui32_pay;
	} payload;
};

struct peek {
	enum peek_type type;
	union {
		uint32_t subtype;
	};
	uintptr_t req;
};

int mon_recv_request_and_dispatch();
int mon_run();
void mon_set_key(struct input_event *ev);
uint8_t rd(uint16_t addr);
void wr(uint16_t addr, uint8_t value);
int mon_init();
void mon_throttle_fps();

#endif
