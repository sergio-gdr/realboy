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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpu.h"

#include "monitor.h"

#include "config.h"

#ifdef HAVE_LIBEMU
	#include <libemu.h>
#endif

// main cpu structure
// holds registers and other state
cpu_t cpu;

// defined in cpu_ops.c
extern const uint8_t op_len[256];

enum tac_bitmask {
	TAC_BITMASK_CLOCK_SELECT = 0x3,
	TAC_BITMASK_TIMA_ENABLE = 0x4
};

static void process_intr(uint8_t interrupt_bit) {
	enum interrupts {
		VBLANK_INTERRUPT = 0x01,
		LCD_INTERRUPT = 0x02,
		TIMER_INTERRUPT = 0x04,
		SERIAL_INTERRUPT = 0x08,
		JOYPAD_INTERRUPT = 0x10
	};

	uint16_t interrupt_handler_addr;
	switch (interrupt_bit) {
		case VBLANK_INTERRUPT:
			interrupt_handler_addr = 0x40;
			break;
		case LCD_INTERRUPT:
			interrupt_handler_addr = 0x48;
			break;
		case TIMER_INTERRUPT:
			interrupt_handler_addr = 0x50;
			break;
		case SERIAL_INTERRUPT:
			interrupt_handler_addr = 0x58;
			break;
		case JOYPAD_INTERRUPT:
			interrupt_handler_addr = 0x60;
			break;
	}
	REG_SP -= 2;
	WR_WORD(REG_SP, REG_PC);
	cpu.state.registers.pc = interrupt_handler_addr;
}

static void handle_intrs(uint8_t cycles) {
	struct cpu_state *state = &cpu.state;

	if (state->ime_enabled || state->is_halted) {
		uint8_t intr_mask = 1;
		while (intr_mask) {
			if (state->if_reg & state->ie_reg & intr_mask) {
				state->is_halted = false;
				if (state->ime_enabled) {
					state->ime_enabled = false;
					state->if_reg &= ~intr_mask; // disable the corresponding interrupt request
					state->is_halted = false;
					process_intr(intr_mask);
				}
			}
			intr_mask <<=1; // this is guaranteed to be 0 after 0b10000000 << 1
		}
	}
}

void cpu_disable_intr() {
	cpu.state.ime_enabled = false;
}

void cpu_enable_intr() {
	cpu.state.ime_enabled = true;
}

void cpu_request_intr(enum interrupt_mask im) {
	cpu.state.if_reg |= im;
}

uint8_t cpu_rd(uint16_t addr) {
	struct cpu_state *state = &cpu.state;

	if (addr >= 0xc000 && addr <= 0xdfff) {
		return cpu.wram[addr-0xc000];
	}
	if (addr >= 0xff80 && addr <=	0xfffe) {
		return cpu.hram[addr-0xff80];
	}

	switch (addr) {
		case 0xffff:
			return state->ie_reg;
		case 0xff0f:
			return state->if_reg;
		case 0xff04:
			return state->div_reg;
		case 0xff05:
			return state->tima_reg;
		case 0xff06:
			return state->tma_reg;
		case 0xff07:
			return state->tac_reg;
		case 0xff50:
			return state->disable_bootrom;
		default:
			fprintf(stderr, "error: cpu_rd()");
			return 0;
	}
}

#ifdef HAVE_LIBEMU
static uint16_t peek_get_cpu_reg(uintptr_t cpu_reg) {
	enum cpu_reg reg = cpu_reg;
	switch (reg) {
		case CPU_REG_AF:
			return REG_AF;
		case CPU_REG_BC:
			return REG_BC;
		case CPU_REG_DE:
			return REG_DE;
		case CPU_REG_HL:
			return REG_HL;
		case CPU_REG_SP:
			return REG_SP;
		case CPU_REG_PC:
			return REG_PC;
		default:
			fprintf(stderr, "error: peek_get_cpu_reg()");
			return -1;
	}
}

void cpu_peek(struct peek *peek, struct peek_reply *reply) {
	switch (peek->subtype) {
		case CPU_PEEK_REG:
			reply->size = sizeof(uint32_t);
			reply->payload = malloc(reply->size); // caller frees
			uint32_t cpu_reg = peek_get_cpu_reg(peek->req);
			memcpy(reply->payload, &cpu_reg, reply->size);
			break;
		case CPU_PEEK_ADDR:
			reply->size = sizeof(uint32_t);
			reply->payload = malloc(reply->size); // caller frees
			uint32_t addr = monitor_rd_mem(peek->req);
			memcpy(reply->payload, &addr, reply->size);
			break;
		case CPU_PEEK_INSTR_AT_ADDR:
			{
				uint32_t opcode = monitor_rd_mem(peek->req);
				reply->size = opcode == OPCODE_PREFIX ? 2 : op_len[opcode];
				reply->size *= sizeof(uint32_t);
				reply->payload = malloc(reply->size); // caller frees
				memcpy(reply->payload, &opcode, sizeof(uint32_t));
				for (size_t i=1; i < (reply->size)/sizeof(uint32_t); i++) {
					uint32_t req = monitor_rd_mem(peek->req+i);
					memcpy((uint32_t*)reply->payload+i, &req, sizeof(uint32_t));
				}
			}
			break;
		case CPU_PEEK_OP_LEN:
			reply->size = sizeof(uint32_t);
			reply->payload = malloc(reply->size); // caller frees
			uint32_t len = op_len[peek->req];
			memcpy(reply->payload, &len, sizeof(uint32_t));
			break;
		default:
			fprintf(stderr, "error: cpu_peek()");
	}
}
#endif

void cpu_wr(uint16_t addr, uint8_t value) {
	struct cpu_state *state = &cpu.state;

	if (addr >= 0xc000 && addr <= 0xdfff) {
		cpu.wram[addr-0xc000] = value;
		return;
	}
	if (addr >= 0xff80 && addr <= 0xfffe) {
		cpu.hram[addr-0xff80] = value;
		return;
	}

	switch (addr) {
		case 0xffff:
			state->ie_reg = value;
			break;
		case 0xff0f:
			state->if_reg = value & 0x1f;
			break;
		case 0xff04:
			state->div_reg = 0;
			break;
		case 0xff05:
			state->tima_reg = value;
			break;
		case 0xff06:
			state->tma_reg = value;
			break;
		case 0xff07:
			state->tac_reg = value & 7;
			state->tima_enabled = value & TAC_BITMASK_TIMA_ENABLE;
			switch (value & TAC_BITMASK_CLOCK_SELECT) {
				case 0:
					state->tac_divider = 256;
					break;
				case 1:
					state->tac_divider = 4;
					break;
				case 2:
					state->tac_divider = 16;
					break;
				case 3:
					state->tac_divider = 64;
					break;
			}
			break;
		case 0xff50:
			state->disable_bootrom = value;
			break;
		default:
			fprintf(stderr, "error: cpu_wr() addr %d value %d", addr, value);
	}
}

int cpu_exec_next() {
	int cycles = 0;
	bool op_is_prefix = false;

	// fetch opcode
	if (!cpu.state.is_halted) {
		uint8_t op = monitor_rd_mem(REG_PC);
		//printf("EXECUTING %x\n", REG_PC);
		REG_PC++;
		if (op == OPCODE_PREFIX) {
			op = monitor_rd_mem(REG_PC);
			REG_PC++;
			op_is_prefix = true;
		}

		// execute
		cycles = ops_table_dispatch(op, op_is_prefix);
		cpu.state.cycles += cycles;
	}
	else {
		cycles = 3;
	}

	struct cpu_state *state = &cpu.state;
	if ((typeof(state->div_counter))(state->div_counter + cycles) < state->div_counter) {
		state->div_reg++;
	}
	cpu.state.div_counter += cycles;

	if (state->tima_enabled) {
		if ((state->tac_counter + cycles)/state->tac_divider) {
			state->tima_reg++;
			if (state->tima_reg == 0) {
				state->tima_reg = state->tma_reg;
				cpu_request_intr(REQUEST_INTR_TIMER);
			}
			state->tac_counter = (state->tac_counter + cycles)%state->tac_divider;
		}
		else {
			state->tac_counter += cycles;
		}
	}

	// handle interrupts
	handle_intrs(cycles);

	return cycles;
}

void cpu_halt() {
	cpu.state.is_halted = true;
}
