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

#ifndef RB_CPU_H
#define RB_CPU_H

#include <stdint.h>

#include "monitor.h"

#include "config.h"

// types of 'peek' provided by the cpu module.
enum cpu_peek_type {
	CPU_PEEK_REG,
	CPU_PEEK_ADDR,
	CPU_PEEK_INSTR_AT_ADDR,
	CPU_PEEK_OP_LEN
};

// for requesting interrupts when calling cpu_request_intr().
enum interrupt_mask {
	REQUEST_INTR_VBLANK = 0x1,
	REQUEST_INTR_LCD = 0x2,
	REQUEST_INTR_TIMER = 0x4,
	REQUEST_INTR_JOYPAD = 0x8
};

// initialiaze the cpu module.
void cpu_init();

// access cpu's internal state.
void cpu_wr(uint16_t addr, uint8_t value);
uint8_t cpu_rd(uint16_t addr);

#ifdef HAVE_LIBEMU
void cpu_peek(struct peek *peek, struct peek_reply *reply);
#endif

// execute the next instruction.
int cpu_exec_next();

// request a cpu interrupt.
void cpu_request_intr(enum interrupt_mask);

#endif
