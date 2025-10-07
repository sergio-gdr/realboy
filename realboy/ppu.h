/* RealBoy Emulator
 * Copyright (C) 2025 Sergio Gómez Del Real
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

#ifndef PPU_H
#define PPU_H

#include <stdint.h>

enum ppu_peek_type {
	PPU_PEEK_REG
};

enum ppu_lcdc_bitmask {
	LCDC_BITMASK_BG_ENABLE = 1,
	LCDC_BITMASK_OBJ_ENABLE = 1 << 1,
	LCDC_BITMASK_OBJ_SIZE = 1 << 2,
	LCDC_BITMASK_BG_TILE_MAP = 1 << 3,
	LCDC_BITMASK_BGWIN_TILE_DATA = 1 << 4,
	LCDC_BITMASK_WIN_ENABLE = 1 << 5,
	LCDC_BITMASK_WIN_TILE_MAP = 1 << 6,
	LCDC_BITMASK_PPU_ENABLE = 1 << 7
};

uint8_t ppu_rd(uint16_t addr);
void ppu_wr(uint16_t addr, uint8_t value);

void ppu_refresh(uint8_t cycles);
void ppu_init();
#include "monitor.h"
void ppu_peek(struct peek *peek, struct peek_reply *reply);
uint64_t ppu_get_frame_count();

#endif
