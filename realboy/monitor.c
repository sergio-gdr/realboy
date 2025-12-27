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

#include "cpu.h"
#include "mbc.h"

#include "ppu.h"

static mbc_iface_t *mbc_impl;

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

uint8_t monitor_rd_mem(uint16_t addr) {
	if (addr <= 0xbfff) {
		return mbc_impl->rd_mem(addr);
	}
	return 0;
}

void monitor_wr_mem(uint16_t addr, uint8_t value) {
	if (addr <= 0xbfff) {
		mbc_impl->wr_mem(addr, value);
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
