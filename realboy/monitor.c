/* RealBoy Emulator
 * Copyright (C) 2013-2025 Sergio Gómez Del Real <sgdr>
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

#define _POSIX_C_SOURCE 199309L

#include <stdio.h>

#include <linux/input.h>
#include <libemu.h>
#include <unistd.h>
#include <time.h>

#include "monitor.h"

#include "cpu.h"
#include "mbc.h"
#include "ppu.h"
#include "backend/wayland.h"
#include "config.h"

#ifdef HAVE_LIBEMU
#include "server.h"
bool have_libemu = true;
#else
bool have_libemu = false;
#endif

static mbc_iface_t *mbc_impl;
bool start_released=true;
bool select_released=true;
bool a_released=true;
bool b_released=true;
bool up_released=true;
bool down_released=true;
bool left_released=true;
bool right_released=true;

uint8_t tmp_ioregs[0xffff];
uint8_t rd(uint16_t addr) {
	if (addr == 0xffff) {
		return cpu_rd(addr);
	}
	else if (addr == 0xff0f) {
		return cpu_rd(addr);
	}
	else if (addr == 0xff04) {
		return cpu_rd(addr);
	}
	else if (addr == 0xff05) {
		return cpu_rd(addr);
	}
	else if (addr == 0xff06) {
		return cpu_rd(addr);
	}
	else if (addr == 0xff07) {
		return cpu_rd(addr);
	}
	else if (addr == 0xff50) {
		return cpu_rd(addr);
	}
	else if ((addr >= 0x8000 && addr <= 0x9fff) ||
			(addr >= 0xfe00 && addr <= 0xfe9f) ||
			(addr >= 0xff40 && addr <= 0xff4b)) {
		return ppu_rd(addr);
	}
	else if (addr >= 0xc000 && addr <= 0xdfff) {
		return cpu_rd(addr);
	}
	else if (addr >= 0xff80 && addr <=	0xfffe) {
		return cpu_rd(addr);
	}
	else if (addr <= 0xbfff) {
		return mbc_impl->rd_mem(addr);
	}
	else {
		if (addr == 0xff00) {
			if ((tmp_ioregs[addr] & 0x30) == 0x30) {
				return 0x3f;
			}
			else if (tmp_ioregs[addr] & 0x10) {
				return tmp_ioregs[addr] |
					(start_released<<3|select_released<<2|b_released<<1|a_released);
			}
			else if (tmp_ioregs[addr] & 0x20) {
				return tmp_ioregs[addr] |
					(down_released<<3|up_released<<2|left_released<<1|right_released);
			}
			else
				perror("rd");
		}
		return tmp_ioregs[addr];
	}
}

void wr(uint16_t addr, uint8_t value) {
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

#define JOY_BITMASK_A_RIGHT 1
#define JOY_BITMASK_B_LEFT 2
#define JOY_BITMASK_SELECT_UP 4
#define JOY_BITMASK_START_DOWN 8

void mon_set_key(struct input_event *ev) {
	if (wayland_backend_is_focus()) {
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
				perror("mon_set_key");
		}
	}
}

#define FPS 60
void mon_throttle_fps() {
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

static void exec_next() {
	int cycles = cpu_exec_next();
	if (rd(0xff40) & LCDC_BITMASK_PPU_ENABLE) {
		ppu_refresh(cycles);
	}
}

int mon_run() {
#ifdef HAVE_LIBEMU
	while (1) {
		int ret;
		if (server_is_client_connected()) {
			if ((ret = server_recv_request_and_dispatch()) == -1)
				continue;
			if (server_is_request_control_flow()) {
				do {
					exec_next();
				} while (!server_hit_breakpoint());
			}
		}
		else {
			exec_next();
		}
	}
#else
	while (1) {
		cpu_exec_next();
	}
#endif
}

int mon_init() {
	mbc_impl = mbc_init();

	tmp_ioregs[0xff00] = 0x3f; // XXX
	int ret = 0;
#ifdef HAVE_LIBEMU
	server_init();
#endif
	return ret;
}
