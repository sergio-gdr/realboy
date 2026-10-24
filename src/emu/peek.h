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

#ifndef PEEK_H
#define PEEK_H

#include <stddef.h>
#include <stdint.h>

enum peek_type {
	PEEK_CPU,
	PEEK_PPU,
};

// types of 'peek' provided by the cpu module.
enum cpu_peek_type {
	CPU_PEEK_REG,
	CPU_PEEK_ADDR,
	CPU_PEEK_INSTR_AT_ADDR,
	CPU_PEEK_OP_LEN
};

// types of 'peek' provided by the ppu module.
enum ppu_peek_type {
	PPU_PEEK_REG
};

struct peek {
	enum peek_type type;
	uint32_t subtype;
	uint32_t req;
};
struct peek_reply {
	size_t size;
	void *payload;
};

void cpu_peek(const struct peek *peek, struct peek_reply *reply);
void ppu_peek(const struct peek *peek, struct peek_reply *reply);

#endif
