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

#ifndef RB_MONITOR_H
#define RB_MONITOR_H

#include <stdint.h>

#include <linux/input.h>

#include "config.h"

// the 'peek' interface.
// it is implemented by the modules (eg, cpu_peek(), ppu_peek()) to read their
// state in the context of an IPC request.
enum peek_type {
	PEEK_CPU,
	PEEK_PPU,
};
struct peek {
	enum peek_type type;
	uint32_t subtype;
	uint32_t req;
};
struct peek_reply {
	size_t size;
	void *payload;
};

// main loop for the execution environment.
// call this function to run the emulator.
int monitor_run();

void monitor_throttle_fps();

// read/write to an arbitrary address.
// the monitor maps the address to the correct memory mechanism:
// egs:
//  monitor_wr_mem(0xc000, 0) writes 0 to the cpu's working ram
//  monitor_rd_mem(0xff40) reads from the ppu's lcdc register
uint8_t monitor_rd_mem(uint16_t addr);
void monitor_wr_mem(uint16_t addr, uint8_t value);

// interfaces with the system's input mechanism.
// eg, the wayland driver calls this to inform about the linux input EV_KEY.
// only linux right now.
void monitor_set_key(struct input_event *ev);

// initializes the monitor.
// wait_for_client asks to wait for an IPC connection from another program.
// this argument is relevant only for use with libemu
int monitor_init(bool wait_for_client);

void monitor_fini();

#ifdef HAVE_LIBEMU
#include <libemu.h>
// msg_type TYPE_CONTROL_FLOW to control breakpoints, etc.
void monitor_set_control_flow(enum msg_type_control_flow, uint32_t addr);

// msg_type TYPE_MONITOR triggers these
void monitor_stop_execution();
void monitor_resume_execution();
#endif

#endif
