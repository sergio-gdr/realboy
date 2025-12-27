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

#ifndef RB_CPU_H
#define RB_CPU_H

// for requesting interrupts when calling cpu_request_intr().
enum interrupt_mask {
	REQUEST_INTR_VBLANK = 0x1,
	REQUEST_INTR_LCD = 0x2,
	REQUEST_INTR_TIMER = 0x4,
	REQUEST_INTR_JOYPAD = 0x8
};

int cpu_exec_next();
#endif
