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

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include "monitor.h"

#include "backends/backends.h"
#include "cpu.h"
#include "events.h"
#include "list.h"
#include "mbc.h"
#include "ppu.h"
#include "server.h"

static bool monitor_initialized;
static bool emulation_paused;

static mbc_iface_t *mbc_impl;

static bool start_released=true;
static bool select_released=true;
static bool a_released=true;
static bool b_released=true;
static bool up_released=true;
static bool down_released=true;
static bool left_released=true;
static bool right_released=true;

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

uint8_t tmp_ioregs[0xffff];
uint8_t monitor_rd_mem(uint16_t addr) {
	if ((addr >= 0x8000 && addr <= 0x9fff) ||
			(addr >= 0xfe00 && addr <= 0xfe9f) ||
			(addr >= 0xff40 && addr <= 0xff4b)) {
		return ppu_rd(addr);
	}
	if (addr >= 0xc000 && addr <= 0xdfff) {
		return cpu_rd(addr);
	}
	if (addr >= 0xff80 && addr <= 0xfffe) {
		return cpu_rd(addr);
	}
	if (addr <= 0xbfff) {
		return mbc_impl->rd_mem(addr);
	}

	switch (addr) {
		case 0xffff:
		case 0xff0f:
		case 0xff04:
		case 0xff05:
		case 0xff06:
		case 0xff07:
		case 0xff50:
			return cpu_rd(addr);
		default:
			if (addr == 0xff00) {
				if ((tmp_ioregs[addr] & 0x30) == 0x30) {
					return 0x3f;
				}
				else {
					uint8_t ret;
					if (tmp_ioregs[addr] & 0x10) {
						ret = tmp_ioregs[addr] |
							(start_released<<3|select_released<<2|b_released<<1|a_released);
					}
					else {
						ret = tmp_ioregs[addr] |
							(down_released<<3|up_released<<2|left_released<<1|right_released);
					}
					return ret;
				}
			}
			else {
				return tmp_ioregs[addr];
			}
	}
}

void monitor_wr_mem(uint16_t addr, uint8_t value) {
	if (addr == 0xffff) {
		cpu_wr(addr, value);
	}
	else if (addr == 0xff0f) {
		cpu_wr(addr, value);
	}
	else if (addr == 0xff04) {
		cpu_wr(addr, value);
	}
	else if (addr == 0xff05) {
		cpu_wr(addr, value);
	}
	else if (addr == 0xff06) {
		cpu_wr(addr, value);
	}
	else if (addr == 0xff07) {
		cpu_wr(addr, value);
	}
	else if (addr == 0xff50) {
		cpu_wr(addr, value);
		mbc_impl->wr_mem(addr, value);
	}
	else if ((addr >= 0x8000 && addr <= 0x9fff) ||
			(addr >= 0xfe00 && addr <= 0xfe9f) ||
			(addr >= 0xff40 && addr <= 0xff4b)) {
		ppu_wr(addr, value);
	}
	else if (addr >= 0xc000 && addr <= 0xdfff) {
		cpu_wr(addr, value);
	}
	else if (addr >= 0xff80 && addr <=	0xfffe) {
		cpu_wr(addr, value);
	}
	else if (addr <= 0xbfff) {
		mbc_impl->wr_mem(addr, value);
	}
	else {
		if (addr == 0xff00) {
			tmp_ioregs[addr] = value&0x30;
		}
		else
			tmp_ioregs[addr] = value;
	}
}

void monitor_set_key(struct input_event *ev) {
	struct backend_display_ext *backend = (struct backend_display_ext *)backends_get_backend_by_type(BACKEND_DISPLAY);

	if (backend->is_focus()) {
		switch (ev->code) {
			case KEY_ENTER:
				start_released = !ev->value;
				break;
			case KEY_DOWN:
			case KEY_J:
				down_released = !ev->value;
				break;
			case KEY_SPACE:
				select_released = !ev->value;
				break;
			case KEY_UP:
			case KEY_K:
				up_released = !ev->value;
				break;
			case KEY_S:
				b_released = !ev->value;
				break;
			case KEY_LEFT:
			case KEY_H:
				left_released = !ev->value;
				break;
			case KEY_D:
				a_released = !ev->value;
				break;
			case KEY_RIGHT:
			case KEY_L:
				right_released = !ev->value;
				break;
			default:
				fprintf(stderr, "monitor_set_key()");
		}
	}
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
	mbc_impl = mbc_init();

	breakpoint_list = create_list();
	if (!breakpoint_list) {
		return -1;
	}

	monitor_initialized = true;

	return 0;
}
