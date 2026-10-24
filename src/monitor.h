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

// main loop for the execution environment.
// call this function to run the emulator.
int monitor_run();

void monitor_server_request();

void monitor_throttle_fps();

// interfaces with the system's input mechanism.
// eg, the wayland driver calls this to inform about the linux input EV_KEY.
// only linux right now.
void monitor_set_key(struct input_event *ev);

// initializes the monitor.
int monitor_init();

void monitor_fini();

#endif
