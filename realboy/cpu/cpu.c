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

#include "cpu.h"

#include "monitor.h"

// main cpu structure
// holds registers and other state
cpu_t cpu;

void cpu_disable_intr() {
	cpu.state.ime_enabled = false;
}

void cpu_enable_intr() {
	cpu.state.ime_enabled = true;
}

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

void cpu_request_intr(enum interrupt_mask im) {
	cpu.state.if_reg |= im;
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
