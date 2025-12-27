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

#ifndef CPU_H
#define CPU_H

#include <stdbool.h>
#include <stdint.h>

#include "../include/cpu.h"

// for convenience, we want to access registers as either 8-bit values (eg, register B),
// 16-bit pairs (eg, register B and C as BC).
struct registers {
	union {
		struct {
			uint8_t f;
			uint8_t a;
		};
		uint16_t af;
	};
	union {
		struct {
			uint8_t c;
			uint8_t b;
		};
		uint16_t bc;
	};
	union {
		struct {
			uint8_t e;
			uint8_t d;
		};
		uint16_t de;
	};
	union {
		struct {
			uint8_t l;
			uint8_t h;
		};
		uint16_t hl;
	};
	uint16_t sp;
	uint16_t pc;
};

struct cpu_state {
	// internal registers
	struct registers registers;

	// other registers mapped to the IO range
	uint8_t div_reg; // 0xff04
	uint8_t if_reg; // 0xff0f
	uint8_t ie_reg; // 0xffff

	// total cycles since boot
	uint64_t cycles;

	unsigned _BitInt(6) div_counter;
	bool tima_enabled;
	uint8_t tima_reg; // 0xff05
	uint8_t tma_reg; // 0xff06
	uint8_t tac_reg; // 0xff07

	uint8_t tac_counter;
	uint32_t tac_divider;

	bool is_halted;

	// set through ei/di opcodes
	bool ime_enabled;

	bool disable_bootrom;
};

typedef struct {
	struct cpu_state state;

	// internal ram
	uint8_t wram[0x2000]; // 0xc000-0xdfff
	uint8_t hram[0x7f]; // 0xff80-0xfffe
} cpu_t;

#endif
