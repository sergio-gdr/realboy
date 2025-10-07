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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cpu.h"

#include "../mbc.h"
#include "ppu.h"

// main cpu structure
// holds registers and other state
cpu_t cpu;

// implementation of the mbc interface
mbc_iface_t *mbc_impl;

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

	if (state->ime_enabled) {
		uint8_t intr_mask = 1;
		while (intr_mask) {
			if (state->if_reg & state->ie_reg & intr_mask) {
				state->ime_enabled = false;
				state->if_reg &= ~intr_mask; // disable the corresponding interrupt request
				state->is_halted = false;
				process_intr(intr_mask);
			}
			intr_mask <<=1; // this is guaranteed to be 0 after 0b10000000 << 1
		}
		// refresh screen if enabled
		if (ppu_rd(0xff40) & 0x80) {
			ppu_refresh(cycles);
		}
	}
}

bool cpu_is_halted() {
	return cpu.state.is_halted;
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

int cpu_exec_next() {
	int cycles = 0;
	bool op_is_prefix = false;

	// fetch opcode
	if (!cpu.state.is_halted) {
		uint8_t op = rd(REG_PC);
		REG_PC++;
		//printf("EXECUTING %x\n", REG_PC);
		if (op == OPCODE_PREFIX) {
			op = rd(REG_PC);
			REG_PC++;
			op_is_prefix = true;
		}

		// execute
		cycles = ops_table_dispatch(op, op_is_prefix);
		cpu.state.cycles += cycles;
	}

	if (ppu_rd(0xff40) & LCDC_BITMASK_PPU_ENABLE) {
		ppu_refresh(cycles);
	}
	// handle interrupts
	handle_intrs(cycles);

	return cycles;
}

void cpu_halt() {
	cpu.state.is_halted = true;
}

void cpu_init() {
	mbc_impl = mbc_init();
}

uint8_t tmp_ioregs[0xffff];
uint8_t rd(uint16_t addr) {
	struct cpu_state *state = &cpu.state;

	if (addr == 0xffff) {
		return state->ie_reg;
	}
	else if (addr == 0xff0f) {
		return state->if_reg;
	}
	else if (addr == 0xff04) {
		return state->div_reg;
	}
	else if (addr == 0xff05) {
		return state->tima_reg;
	}
	else if (addr == 0xff06) {
		return state->tma_reg;
	}
	else if (addr == 0xff07) {
		return state->tac_reg;
	}
	else if (addr == 0xff50) {
		return state->disable_bootrom;
	}
	else if ((addr >= 0x8000 && addr <= 0x9fff) ||
			(addr >= 0xfe00 && addr <= 0xfe9f) ||
			(addr >= 0xff40 && addr <= 0xff4b)) {
		return ppu_rd(addr);
	}
	else if (addr >= 0xc000 && addr <= 0xdfff) {
		return cpu.wram[addr-0xc000];
	}
	else if (addr >= 0xff80 && addr <=	0xfffe) {
		printf("reading from addr %x value %d\n", addr, cpu.hram[addr-0xff80]);
		return cpu.hram[addr-0xff80];
	}
	else if (addr <= 0xbfff) {
		return mbc_impl->rd_mem(addr);
	}
	else
		return tmp_ioregs[addr];
}

void wr(uint16_t addr, uint8_t value) {
	struct cpu_state *state = &cpu.state;

	if (addr == 0xffff) {
		state->ie_reg = value;
	}
	else if (addr == 0xff0f) {
		state->if_reg = value;
	}
	else if (addr == 0xff04) {
		state->div_reg = value;
	}
	else if (addr == 0xff05) {
		state->tima_reg = value;
	}
	else if (addr == 0xff06) {
		state->tma_reg = value;
	}
	else if (addr == 0xff07) {
		state->tac_reg = value;
	}
	else if (addr == 0xff50) {
		state->disable_bootrom = value;
		mbc_impl->wr_mem(addr, value);
	}
	else if ((addr >= 0x8000 && addr <= 0x9fff) ||
			(addr >= 0xfe00 && addr <= 0xfe9f) ||
			(addr >= 0xff40 && addr <= 0xff4b)) {
		ppu_wr(addr, value);
	}
	else if (addr >= 0xc000 && addr <= 0xdfff) {
		cpu.wram[addr-0xc000] = value;
	}
	else if (addr >= 0xff80 && addr <=	0xfffe) {
		printf("writing to addr %x value %d\n", addr, value);
		cpu.hram[addr-0xff80] = value;
	}
	else if (addr <= 0xbfff) {
		mbc_impl->wr_mem(addr, value);
	}
	else {
		if (addr == 0xff00)
			tmp_ioregs[addr] = 0xff;
		else
			tmp_ioregs[addr] = value;
	}
}

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
			perror("peek_get_cpu_reg");
			return -1;
	}
}

void cpu_peek(struct peek *peek, struct peek_reply *reply) {
	uint32_t *buf;
	switch (peek->subtype) {
		case CPU_PEEK_REG:
			reply->size = 1 * sizeof(uint32_t);
			reply->payload.ui32_pay = peek_get_cpu_reg(peek->req);
			break;
		case CPU_PEEK_ADDR:
			reply->size = sizeof(uint32_t);
			reply->payload.ui32_pay = rd(peek->req);
			break;
		case CPU_PEEK_INSTR_AT_ADDR:
			{
			uint8_t opcode = rd(peek->req);
			reply->size = opcode == OPCODE_PREFIX ? 2 : op_len[opcode];
			reply->size++; // space for the PC
			reply->size *= sizeof(uint32_t);
			buf = malloc(reply->size); // caller frees
			buf[0] = peek->req;
			for (size_t i=0; i < (reply->size-1)/sizeof(uint32_t); i++) {
				buf[i+1] = rd(peek->req+i);
			}
			reply->payload.ptr_pay = buf; // payload owns this now
			buf = NULL;
			}
			break;
		case CPU_PEEK_OP_LEN:
			reply->size = sizeof(uint32_t);
			uint32_t len = op_len[peek->req];
			reply->payload.ui32_pay = len;
			break;
		default:
			perror("cpu_peek");
	}
}
