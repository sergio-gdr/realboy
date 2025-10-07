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

#ifndef CPU_H
#define CPU_H

#include <stddef.h>
#include <stdint.h>

#include <libemu.h>

struct peek;
struct peek_reply;

enum cpu_peek_type {
	CPU_PEEK_REG,
	CPU_PEEK_ADDR,
	CPU_PEEK_INSTR_AT_ADDR,
	CPU_PEEK_OP_LEN
};

enum interrupt_mask {
	INTR_VBLANK=0x1,
	INTR_LCD=0x2,
	INTR_TIMER=0x4,
	INTR_JOYPAD=0x8
};

int cpu_exec_next();
void cpu_init();
void cpu_peek(struct peek *peek, struct peek_reply *reply);
void cpu_request_intr(enum interrupt_mask);
void cpu_wr(uint16_t addr, uint8_t value);
uint8_t cpu_rd(uint16_t addr);

#endif
