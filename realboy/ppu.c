/* RealBoy Emulator * Copyright (C) 2025 Sergio Gómez Del Real
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

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include "ppu.h"

#include "cpu.h"
#include "cpu/cpu.h" // XXX
#include "wayland.h"

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
	uint8_t oam[0x100];
	enum ppu_mode mode;
	int16_t dots_remaining;

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

#define HBLANK_DOT_DURATION 204
#define CYCLES_TO_DOTS(cycles) cycles<<2;
#define STAT_VBLANK_INT 0x10

#define LY_LYC_FLAG 0x4
static void check_ly_lyc() {
	struct ppu_regs *regs = &ppu.regs;
	if (regs->ly == regs->lyc) {
		regs->stat |= LY_LYC_FLAG;
		if (regs->stat & STAT_INTR_LYLC)
			cpu_request_intr(INTR_LCD);
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

	// see if we should issue a STAT interrupt
	set_ly(regs->ly+1);

	// change to vblank phase if the rendering line is >= 144.
	// else change to oam phase.
	if (regs->ly >= 144) {
		ppu.vblank_dots = 0;
		ppu.dots_remaining += DOTS_VBLANK;
		ppu.mode = PPU_VBLANK;
		regs->stat |= 1;
		if (regs->stat & STAT_INTR_VBLANK) {
			cpu_request_intr(INTR_LCD);
		}
		cpu_request_intr(INTR_VBLANK);
	}
	else {
		ppu.dots_remaining += DOTS_OAM;
		ppu.mode = PPU_OAM;
		regs->stat |= 2;
		if (regs->stat & STAT_INTR_OAM) {
			cpu_request_intr(INTR_LCD);
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
}

#define TILE_HEIGHT 8
#define TILES_PER_SCANLINE 32
#define BYTES_PER_TILE 16
#define BYTES_PER_TILE_LINE 2
#define PIXELS_PER_SCANLINE 160
#define SCANLINE 160

typedef struct tile {
	uint16_t pixels[8];
} tile_t;

static void get_tile_line(const uint8_t tile_indx, uint8_t line, tile_t *out_tileline) {
	struct ppu_regs *regs = &ppu.regs;

	bool is_mode_8k = regs->lcdc & LCDC_BITMASK_BGWIN_TILE_DATA;
	uint8_t *tiledata =  is_mode_8k ? ppu.tile_data : &ppu.tile_data[0x1000];
	if (is_mode_8k)
		tiledata += 16 * tile_indx;
	else
		tiledata = (uint8_t *)((int8_t *)tiledata + ((int8_t)tile_indx * 16));

	tiledata += line * 2;
	for (int i = 0; i < 8; i ++) {
		out_tileline->pixels[i] = *((uint16_t *)tiledata);
		tiledata += 16;
	}
}

uint32_t color_pal[] = { 0xffffff, 0xb6b6b6, 0x3d3d3d, 0x0 };

static void get_obj_at_xy(int x, int y, uint8_t (**oam)[4]) {
	*oam = (void *)ppu.oam;

	for (int i = 0; i < 40; i++) {
		uint8_t size = ppu.regs.lcdc & LCDC_BITMASK_OBJ_SIZE ? 16 : 8;
		// let's work only with unsigned values.
		// we add 16 since the y and x positions for the obj are
		// really y+16 and x+8.
		// the cast is due to integer promotion rules:
		// C will convert the operands of this sub to ints, and the
		// result of the expression is an int.
		// here we want to use the wraparound of unsigned integers,
		// so we need to cast the aforementioned resultant int to uint8_t.
		if ((uint8_t)((y+16) - (**oam)[0]) < size &&
				(uint8_t)((x+8) - (**oam)[1]) < 8) {
			//printf("got object x %d y %d oam[0] %d oam[1] %d\n", x, y , (**oam)[0], (**oam)[1]);
			return;
		}
		(*oam)++;
	}
	*oam = NULL;
}

static void render_objs() {
	uint8_t (*obj)[4] = NULL;
	for (int i=0; i < SCANLINE; i+=8) {
		get_obj_at_xy(i, ppu.regs.ly, &obj);
		if (obj) {
			tile_t tileline;
			printf("tile index %x\n", (*obj)[2]);
			get_tile_line((*obj)[2], (ppu.regs.ly+(*obj)[0])&0x7, &tileline);
			for (int j = ((*obj)[1]%8); j < 8; j++) {
				uint8_t indx = (bool)(tileline.pixels[0] & (0x8000 >> j));
				uint8_t palette = ppu.regs.obp0;
				indx |= (((bool)(tileline.pixels[0] & (0x80 >> j)))<<1);
				assert(indx <= 3);
				uint32_t pixel = color_pal[(palette & (3<<(indx*2)))>> (2*indx)];
				printf("i+obj-7 %d\n", i+(*obj)[1]-7);
				backend_wayland_draw_pixel(i+j, ppu.regs.ly, pixel);
			}
		}
	}
}

static void render_win() {
	struct ppu_regs *regs = &ppu.regs;
	uint8_t *tilemap = regs->lcdc & LCDC_BITMASK_WIN_TILE_MAP ? ppu.tile_map2 : ppu.tile_map1;
	// tilemaps are 32x32 1-byte entries that represent the index into the tile data
	// area.
	// here we point to the correct row of the 32x32 entries.
	if (regs->wy > regs->ly)
		return;
	uint8_t *tilemap_row_beg = tilemap + 32 * ((regs->ly)/8);
	printf("wx %d wy %d\n", regs->wx, regs->wy);
	tilemap = tilemap_row_beg;
	for (int i=regs->wx-7; i < SCANLINE; i+=8) {
		// adjust the column position each iteration, since it may wrap at any time
		// in the line rendering.

		uint8_t palette;
		uint8_t beg_pixel_in_tile = 0;
		tilemap++;

		// get bg tileline
		tile_t tileline;
		get_tile_line(*tilemap, (regs->ly)%8, &tileline);
		palette = regs->bgp;

		for (int j = beg_pixel_in_tile; j < 8; j++) {
			uint8_t indx = (bool)(tileline.pixels[0] & (0x8000 >> j));
			indx |= (((bool)(tileline.pixels[0] & (0x80 >> j)))<<1);
			assert(indx <= 3);
			uint32_t pixel = color_pal[(palette & (3<<(indx*2)))>> (2*indx)];
			backend_wayland_draw_pixel(i+j+7, regs->ly, pixel);
		}
	}
}

static void render_bg() {
	struct ppu_regs *regs = &ppu.regs;
	uint8_t *tilemap = regs->lcdc & LCDC_BITMASK_BG_TILE_MAP ? ppu.tile_map2 : ppu.tile_map1;
	// tilemaps are 32x32 1-byte entries that represent the index into the tile data
	// area.
	// here we point to the correct row of the 32x32 entries.
	uint8_t *tilemap_row_beg = tilemap + 32 * ((regs->ly+regs->scy)/8);
	//printf("scx %d scy %d ly %d\n", regs->scx, regs->scy, regs->ly);
	for (int i=0; i < SCANLINE; i+=8) {
		// adjust the column position each iteration, since it may wrap at any time
		// in the line rendering.
		tilemap = tilemap_row_beg + (((i + regs->scx) % 256))/8;

		uint8_t palette;
		uint8_t beg_pixel_in_tile = i == 0 ? regs->scx%8 : 0;

		// get bg tileline
		tile_t tileline;
		get_tile_line(*tilemap, (regs->ly+regs->scy)%8, &tileline);
		palette = regs->bgp;

		for (int j = beg_pixel_in_tile; j < 8; j++) {
			uint8_t indx = (bool)(tileline.pixels[0] & (0x8000 >> j));
			indx |= (((bool)(tileline.pixels[0] & (0x80 >> j)))<<1);
			assert(indx <= 3);
			uint32_t pixel = color_pal[(palette & (3<<(indx*2)))>> (2*indx)];
			backend_wayland_draw_pixel(i+j, regs->ly, pixel);
		}
	}
}

static void render_line() {
	struct ppu_regs *regs = &ppu.regs;

	if (regs->lcdc & LCDC_BITMASK_BGWIN_ENABLE) {
		render_bg();
	}
//if (regs->lcdc & LCDC_BITMASK_WIN_ENABLE) {
//	render_win();
//}
//if (regs->lcdc & LCDC_BITMASK_OBJ_ENABLE) {
//	render_objs();
//}
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
			render_line();
			break;
		default:
			perror("change_phase()\n");
	}
}

void ppu_refresh(uint8_t ticks) {
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
			ppu.regs.ly++;
			ppu.vblank_dots = ppu.vblank_dots % DOTS_VBLANK_LINE;
		}
	}
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
			// turn on ppu
			if ((regs->lcdc & 0x80) != (value & 0x80))
				ppu_reset();
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
			for (int i = 0; i < 0x9f; i++)
				wr(0xfe00+i, rd(addr+i));
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
			perror("ppu_wd_reg");
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
		perror("ppu_wr");
	}
}

static uint8_t rd_reg(uint16_t addr) {
	struct ppu_regs *regs = &ppu.regs;

	// XXX make this an array
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
			perror("ppu_rd");
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
		perror("ppu_rd");
		return 0;
	}
}

void ppu_init() {
	ppu.regs.lcdc = 0x00;
	ppu.regs.stat = 0x84;
}

static uint16_t peek_get_ppu_reg(uintptr_t ppu_reg) {
	enum ppu_reg reg = ppu_reg;
	switch (reg) {
		case PPU_REG_LY:
			return ppu_rd(0xff44);
		default:
			perror("peek_get_ppu_reg");
			return -1;
	}
}

void ppu_peek(struct peek *peek, struct peek_reply *reply) {
	switch (peek->subtype) {
		case PPU_PEEK_REG:
			reply->size = 1 * sizeof(uint32_t);
			reply->payload.ui32_pay = peek_get_ppu_reg(peek->req);
			break;
		default:
			perror("ppu_peek");
	}
}
