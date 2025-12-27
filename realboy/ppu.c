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

#include <stdint.h>
#include <stdio.h>

#include "ppu.h"

#include "cpu.h"
#include "monitor.h"

enum ppu_mode {
	PPU_HBLANK=0,
	PPU_VBLANK=1,
	PPU_OAM=2,
	PPU_DRAW=3
};

enum stat_interrupts {
	STAT_INTR_HBLANK = 0b00001000,
	STAT_INTR_VBLANK = 0b00010000,
	STAT_INTR_OAM = 0b00100000,
	STAT_INTR_LYLC = 0b01000000,
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

struct ppu_regs {
	uint8_t lcdc;
	uint8_t ly;
	uint8_t lyc;
	uint8_t stat;
	uint8_t scx;
	uint8_t scy;
	uint8_t dma;
	uint8_t bgp;
	uint8_t obp0;
	uint8_t obp1;
	uint8_t wx;
	uint8_t wy;
};

typedef struct {
	struct ppu_regs regs;
	uint8_t tile_data[0x1800]; // address space range 0x8000-0x97ff
	uint8_t tile_map1[0x400]; // address space range 0x9800-0x9bff
	uint8_t tile_map2[0x400]; // address space range 0x9c00-0x9fff
	uint8_t oam[0x100]; // address space range 0xfe00-0xfe9f
	enum ppu_mode mode;
	int16_t dots_remaining;
	uint64_t frame_count;
	uint32_t vblank_dots;
} ppu_t;
ppu_t ppu;

#define DOTS_DRAW 230
#define DOTS_HBLANK 145
#define DOTS_OAM 80
#define DOTS_VBLANK 4560
// vblank starts at ly==144.
// ly goes from 144 to 152 in the whole
// duration of the vblank.
#define DOTS_VBLANK_LINE (4560/10)
#define CYCLES_TO_DOTS(cycles) cycles<<2;

#define LY_LYC_FLAG 0x4
static void check_ly_lyc() {
	struct ppu_regs *regs = &ppu.regs;
	if (regs->ly == regs->lyc) {
		regs->stat |= LY_LYC_FLAG;
		if (regs->stat & STAT_INTR_LYLC)
			cpu_request_intr(REQUEST_INTR_LCD);
	}
	else {
		regs->stat &= ~LY_LYC_FLAG;
	}
}

static void set_ly(uint8_t value) {
	ppu.regs.ly = value;
	check_ly_lyc();
}

static void hblank_end_cycle() {
	struct ppu_regs *regs = &ppu.regs;

	set_ly(regs->ly+1);

	// change to vblank phase if the rendering line is >= 144.
	// else change to oam phase.
	if (regs->ly >= 144) {
		ppu.vblank_dots = 0;
		ppu.dots_remaining += DOTS_VBLANK;
		ppu.mode = PPU_VBLANK;
		regs->stat |= 1;
		if (regs->stat & STAT_INTR_VBLANK) {
			cpu_request_intr(REQUEST_INTR_LCD);
		}
		cpu_request_intr(REQUEST_INTR_VBLANK);
	}
	else {
		ppu.dots_remaining += DOTS_OAM;
		ppu.mode = PPU_OAM;
		regs->stat |= 2;
		if (regs->stat & STAT_INTR_OAM) {
			cpu_request_intr(REQUEST_INTR_LCD);
		}
	}
}

static void vblank_end_cycle() {
	struct ppu_regs *regs = &ppu.regs;
	ppu.dots_remaining += DOTS_OAM;
	set_ly(0);
	ppu.mode = PPU_OAM;
	regs->stat &= ~3;
	regs->stat |= 2;

	ppu.frame_count++;
	monitor_throttle_fps();
}

static void change_phase() {
	switch (ppu.mode) {
		// hblank cycle
		case PPU_HBLANK:
			hblank_end_cycle();
			break;
		case PPU_VBLANK:
			vblank_end_cycle();
			break;
		case PPU_OAM:
			ppu.dots_remaining += DOTS_DRAW;
			ppu.regs.stat |= 3;
			ppu.mode = PPU_DRAW;
			break;
		case PPU_DRAW:
			ppu.dots_remaining += DOTS_HBLANK;
			ppu.regs.stat &= ~3;
			ppu.mode = PPU_HBLANK;
			break;
		default:
	}
}

uint64_t ppu_get_frame_count() {
	return ppu.frame_count;
}

static void ppu_reset() {
	ppu.dots_remaining = DOTS_OAM;
	ppu.mode = PPU_OAM;
	ppu.regs.ly = 0;
}

static void wr_reg(uint16_t addr, uint8_t value) {
	struct ppu_regs *regs = &ppu.regs;

	switch (addr) {
		case 0xff40:
			// turn on/off ppu
			if ((regs->lcdc & 0x80) != (value & 0x80)) {
				ppu_reset();
				if (!(value & 0x80))
					regs->stat &= ~3;
			}
			regs->lcdc = value;
			break;
		case 0xff41:
			regs->stat = value;
			break;
		case 0xff42:
			regs->scy = value;
			break;
		case 0xff43:
			regs->scx = value;
			break;
		case 0xff44:
			regs->ly = value;
			break;
		case 0xff45:
			regs->lyc = value;
			break;
		case 0xff46:
			{
			uint16_t addr = value*0x100;
			for (int i = 0; i < 0xa0; i++)
				monitor_wr_mem(0xfe00+i, monitor_rd_mem(addr+i));
			break;
			}
		case 0xff47:
			regs->bgp = value;
			break;
		case 0xff48:
			regs->obp0 = value;
			break;
		case 0xff49:
			regs->obp1 = value;
			break;
		case 0xff4a:
			regs->wy = value;
			break;
		case 0xff4b:
			regs->wx = value;
			break;
		default:
			fprintf(stderr, "error: ppu_wd_reg()");
	}
}

void ppu_wr(uint16_t addr, uint8_t value) {
	if (addr >= 0xff40) {
		wr_reg(addr, value);
	}
	else if (addr >= 0x8000 && addr <= 0x97ff) {
		ppu.tile_data[addr-0x8000] = value;
	}
	else if (addr >= 0x9800 && addr <= 0x9bff) {
		ppu.tile_map1[addr-0x9800] = value;
	}
	else if (addr >= 0x9c00 && addr <= 0x9fff) {
		ppu.tile_map2[addr-0x9c00] = value;
	}
	else if (addr >= 0xfe00 && addr <= 0xfe9f) {
		ppu.oam[addr-0xfe00] = value;
	}
	else {
		fprintf(stderr, "error: ppu_wr()");
	}
}

static uint8_t rd_reg(uint16_t addr) {
	struct ppu_regs *regs = &ppu.regs;

	switch (addr) {
		case 0xff40:
			return regs->lcdc;
		case 0xff41:
			return regs->stat;
		case 0xff42:
			return regs->scy;
		case 0xff43:
			return regs->scx;
		case 0xff44:
			return regs->ly;
		case 0xff45:
			return regs->lyc;
		case 0xff46:
			return regs->dma;
		case 0xff47:
			return regs->bgp;
		case 0xff48:
			return regs->obp0;
		case 0xff49:
			return regs->obp1;
		case 0xff4a:
			return regs->wy;
		case 0xff4b:
			return regs->wx;
		default:
			fprintf(stderr, "error: rd_reg()");
			return 0;
	}
}

uint8_t ppu_rd(uint16_t addr) {
	if (addr >= 0xff40) {
		return rd_reg(addr);
	}
	else if (addr >= 0x8000 && addr <= 0x97ff) {
		return ppu.tile_data[addr-0x8000];
	}
	else if (addr >= 0x9800 && addr <= 0x9bff) {
		return ppu.tile_map1[addr-0x9800];
	}
	else if (addr >= 0x9c00 && addr <= 0x9fff) {
		return ppu.tile_map2[addr-0x9c00];
	}
	else if (addr >= 0xfe00 && addr <= 0xfe9f) {
		return ppu.oam[addr-0xfe00];
	}
	else {
		fprintf(stderr, "error: ppu_rd()");
		return 0;
	}
}

void ppu_refresh(uint8_t ticks) {
	if (!(ppu.regs.lcdc & LCDC_BITMASK_PPU_ENABLE)) {
		return;
	}

	if (!ticks) {
		if (ppu.mode != PPU_VBLANK) {
			ppu.dots_remaining = 12;
		}
		ticks = 3;
	}

	ppu.dots_remaining -= CYCLES_TO_DOTS(ticks);
	if (ppu.dots_remaining <= 0 || ppu.regs.ly == 0x99) {
		change_phase();
		return;
	}
	if (ppu.mode == PPU_VBLANK) {
		ppu.vblank_dots += CYCLES_TO_DOTS(ticks);
		if (ppu.vblank_dots / DOTS_VBLANK_LINE) {
			set_ly(ppu.regs.ly+1);
			ppu.vblank_dots = ppu.vblank_dots % DOTS_VBLANK_LINE;
		}
	}
}
