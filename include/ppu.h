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

#ifndef RB_PPU_H
#define RB_PPU_H

#include <stdint.h>

// get the total number of frames since the start
// of the emulation.
uint64_t ppu_get_frame_count();

void ppu_refresh(uint8_t ticks);

// access ppu's internal state.
uint8_t ppu_rd(uint16_t addr);
void ppu_wr(uint16_t addr, uint8_t value);

#endif
