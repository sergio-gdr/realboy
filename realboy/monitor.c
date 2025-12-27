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
#include "mbc.h"

static mbc_iface_t *mbc_impl;

static void exec_next() {
	int cycles = cpu_exec_next();
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
