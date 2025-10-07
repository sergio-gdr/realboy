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

#ifndef RB_CPU_H
#define RB_CPU_H 1

#include <stddef.h>
#include <stdint.h>

#include "ppu.h"

enum interrupt_mask {
	INTR_VBLANK=0x1,
	INTR_LCD=0x2,
	INTR_TIMER=0x4,
	INTR_JOYPAD=0x8
};

enum cpu_reg {
	CPU_REG_AF,
	CPU_REG_BC,
	CPU_REG_DE,
	CPU_REG_HL,
	CPU_REG_SP,
	CPU_REG_PC
};

enum peek_type {
	PEEK_CPU_REG,
	PEEK_PPU_REG,
	PEEK_ADDR,
	PEEK_INSTR_AT_ADDR,
	PEEK_OP_LEN
};

struct cpu_peek_reply {
	size_t size;
	union {
		void *ptr_pay;
		uint32_t ui32_pay;
	} payload;
};

struct cpu_peek {
	enum peek_type type;
	union {
		enum cpu_reg cpu_reg;
		enum ppu_reg ppu_reg;
		uint16_t addr;
		uint8_t opcode;
	};
};

void cpu_request_intr(enum interrupt_mask);
void cpu_enable_intr();
void cpu_disable_intr();
int cpu_exec_next();
void cpu_init();
void cpu_peek(const struct cpu_peek *peek, struct cpu_peek_reply *reply);
#endif
