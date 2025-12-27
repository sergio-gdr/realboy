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

	return cycles;
}

void cpu_halt() {
	cpu.state.is_halted = true;
}
