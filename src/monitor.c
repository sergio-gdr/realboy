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

#include "config.h"

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include "monitor.h"

#include "emu/cpu/cpu.h"
#include "emu/mem/mbc.h"
#include "emu/joy.h"
#include "events.h"
#include "list.h"
#include "emu/peek.h"
#include "ppu.h"
#include "server.h"

static bool monitor_initialized;
static bool emulation_paused;

static void exec_next() {
	int cycles = cpu_exec_next();
	ppu_refresh(cycles);
}

void monitor_throttle_fps() {
	static uint64_t frame_count_last = 0;
	static struct timespec last;
	static int sleep_factor = 60;

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	if (!frame_count_last) {
		frame_count_last = ppu_get_frame_count();
		last = now;
		return;
	}

	if (now.tv_sec > last.tv_sec) {
		uint64_t curr_frame_count = ppu_get_frame_count();
		uint64_t fps = curr_frame_count - frame_count_last;
		printf("FPS %lu sleep_factor %d\n", fps, sleep_factor);
		if (fps > 60)
			sleep_factor--;
		else if (fps < 60)
			sleep_factor++;
		frame_count_last = curr_frame_count;
		last = now;
	}
	struct timespec spec = { .tv_sec = 0, .tv_nsec = 1000000000/sleep_factor };
	nanosleep(&spec, NULL);
}

static list_t *breakpoint_list;
static uint16_t until_addr;
static bool control_flow_continue;
static bool control_flow_until;
static bool control_flow_next;

static bool check_hit_breakpoint() {
	bool hit = false;
	if (!server_is_client_connected()) {
		return hit;
	}
#ifdef HAVE_LIBEMU
	struct peek peek = {};
	struct peek_reply peek_reply = {};
	peek.type = PEEK_CPU;
	peek.subtype = CPU_PEEK_REG;
	peek.req = CPU_REG_PC;
	cpu_peek(&peek, &peek_reply);
	uint32_t addr = *(uint32_t*)peek_reply.payload;
	free(peek_reply.payload);

	enum server_msg msg;
	if (control_flow_continue || control_flow_until) {
		uint32_t breakpoint;
		list_for_each(breakpoint_list, breakpoint) {
			if (breakpoint == addr) {
				msg = SERVER_MSG_BREAK;
				control_flow_continue = 0;
				hit = true;
				break;
			}
		}

		if (control_flow_until) {
			if (until_addr == addr) {
				msg = SERVER_MSG_UNTIL;
				control_flow_until = false;
				control_flow_continue = false;
				until_addr = 0;
				hit = true;
			}
		}

		if (hit) {
			server_send_msg(msg, addr);
		}
	}

	if (control_flow_next) {
		control_flow_next = false;
		hit = true;
	}

	if (hit) {
		emulation_paused = true;
	}

#endif
	return hit;
}

static inline server_reply_t do_peek(const struct peek *peek) {
	struct peek_reply peek_reply;
	switch (peek->type) {
		case PEEK_CPU:
			cpu_peek(peek, &peek_reply);
			break;
		case PEEK_PPU:
			ppu_peek(peek, &peek_reply);
			break;
	}
	return (server_reply_t){ .size = peek_reply.size, .payload = peek_reply.payload };
}

static server_reply_t handle_inspect_op_len(uint32_t addr) {
	struct peek peek = {.type = PEEK_CPU, .subtype = CPU_PEEK_OP_LEN, .req = addr};
	return do_peek(&peek);
}

static server_reply_t handle_inspect_instr_at_addr(uint32_t addr) {
	struct peek peek = {.type = PEEK_CPU, .subtype = CPU_PEEK_INSTR_AT_ADDR, .req = addr};
	return do_peek(&peek);
}

static server_reply_t handle_inspect_addr(uint32_t addr) {
	struct peek peek = {.type = PEEK_CPU, .subtype = CPU_PEEK_ADDR, .req = addr};
	return do_peek(&peek);
}

static server_reply_t handle_inspect_ppu_reg(uint32_t reg) {
	struct peek peek = {.type = PEEK_PPU, .subtype = PPU_PEEK_REG, .req = reg};
	return do_peek(&peek);
}

static server_reply_t handle_inspect_cpu_reg(uint32_t reg) {
	struct peek peek = {.type = PEEK_CPU, .subtype = CPU_PEEK_REG, .req = reg};
	return do_peek(&peek);
}

static void handle_monitor_resume() {
	emulation_paused = false;
}

static void handle_monitor_stop() {
	emulation_paused = true;
}

static void handle_control_flow_until(uint32_t addr) {
	control_flow_until = true;
	until_addr = addr;
	emulation_paused = false;
}

static void handle_control_flow_next() {
	control_flow_next = true;
	emulation_paused = false;
}

static void handle_control_flow_break(uint32_t addr) {
	list_add(breakpoint_list, (uintptr_t)addr);
}

static void handle_control_flow_delete(uint32_t addr) {
	uint32_t breakpoint;
	list_for_each(breakpoint_list, breakpoint) {
		if (breakpoint == addr || addr == 0) {
			list_remove(breakpoint_list, breakpoint);
			break;
		}
	}
}

static void handle_control_flow_continue() {
	control_flow_continue = true;
	emulation_paused = false;
}

static inline bool emulator_is_running() {
	return !emulation_paused;
}

int monitor_run() {
	server_set_dispatch_table(&(server_dispatch_impl_t) {
		.dispatch_monitor_resume = handle_monitor_resume,
		.dispatch_monitor_stop = handle_monitor_stop,
		.dispatch_control_flow_break = handle_control_flow_break,
		.dispatch_control_flow_continue = handle_control_flow_continue,
		.dispatch_control_flow_delete = handle_control_flow_delete,
		.dispatch_control_flow_next = handle_control_flow_next,
		.dispatch_control_flow_until = handle_control_flow_until,
		.dispatch_inspect_cpu = handle_inspect_cpu_reg,
		.dispatch_inspect_ppu = handle_inspect_ppu_reg,
		.dispatch_inspect_op_len = handle_inspect_op_len,
		.dispatch_inspect_instr_at_addr = handle_inspect_instr_at_addr,
		.dispatch_inspect_addr = handle_inspect_addr
	});

	while (1) {
		if (emulator_is_running()) {
			exec_next();
		}
		check_hit_breakpoint();
		events_process(emulation_paused);
	}

	return 0;
}

void monitor_fini() {
	if (!monitor_initialized)
		return;
}

int monitor_init() {
	cpu_init();
	mbc_init();
	joy_init();

	breakpoint_list = create_list();
	if (!breakpoint_list) {
		return -1;
	}

	monitor_initialized = true;

	return 0;
}
