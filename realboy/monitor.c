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
#include <stdio.h>
#include <time.h>

#include <linux/input.h>
#include <pthread.h>

#include "cpu.h"
#include "mbc.h"
#include "backend/wayland.h"

#include "ppu.h"

static mbc_iface_t *mbc_impl;

// these are accessed by the evdev thread and the main thread.
// protect with a mutex.
static pthread_mutex_t mtx_buttons = PTHREAD_MUTEX_INITIALIZER;
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
			return tmp_ioregs[addr];
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
	if (wayland_backend_is_focus()) {
		pthread_mutex_lock(&mtx_buttons);
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
		}
		pthread_mutex_unlock(&mtx_buttons);
	}
}

int monitor_run() {
	while (1) {
		exec_next();
	}
}

void monitor_fini() {
}

int monitor_init() {
	mbc_impl = mbc_init();
	return 0;
}
