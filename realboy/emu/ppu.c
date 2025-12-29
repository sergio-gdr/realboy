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

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ppu.h"

#include "cpu.h"
#include "monitor.h"
#include "render.h"

#include "config.h"

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

uint8_t *oam_hash[17][40] = {};
#define OBJ_ATTR_FLIP_Y 0x40
#define OBJ_ATTR_FLIP_X 0x20

#define SCANLINE 160
uint32_t color_pal[] = { 0xe8fccc, 0xacd490, 0x548c70, 0x142c38 };

static uint16_t *get_tile_line(const uint8_t tile_indx, uint8_t line) {
	struct ppu_regs *regs = &ppu.regs;

	bool is_mode_8k = regs->lcdc & LCDC_BITMASK_BGWIN_TILE_DATA;
	uint8_t *tiledata =  is_mode_8k ? ppu.tile_data : &ppu.tile_data[0x1000];
	if (is_mode_8k)
		tiledata += 16 * tile_indx;
	else
		tiledata = (uint8_t *)((int8_t *)tiledata + ((int8_t)tile_indx * 16));

	tiledata += line * 2;
	return (uint16_t*)tiledata;
}

static uint16_t get_tile_line_from_object(uint8_t *obj, uint8_t line) {
	uint8_t *tiledata =  ppu.tile_data;
	tiledata += obj[2] * 16;

	uint8_t size_obj = (ppu.regs.lcdc & LCDC_BITMASK_OBJ_SIZE ? 16 : 8);
	tiledata += obj[3] & OBJ_ATTR_FLIP_Y ? ((size_obj-1) - line) * 2 : line * 2;

	uint16_t tileline = *(uint16_t *)tiledata;
	return tileline;
}

static int get_objs_at_xy(uint32_t x, uint32_t y, uint8_t **objs) {
	int num_objs = 0;
	for (int i = 0; i < 2; i++) {
		uint8_t **obj = oam_hash[(y+((i)*16))>>4];
		while (*obj && num_objs < 10) {
			uint8_t size = ppu.regs.lcdc & LCDC_BITMASK_OBJ_SIZE ? 16 : 8;
			//printf("inspecting obj %d x %d y %d oam[0] %d oam[1] %d\n", (*obj)[2], x, y , (*obj)[1], (*obj)[0]);
			if (((y+16) - (*obj)[0]) < size &&
					((x+8) - (*obj)[1]) < 8) {
				objs[num_objs] = (void *)*obj;
				num_objs++;
			}
			obj++;
		}
	}
	return num_objs;
}

static uint32_t get_pixel_priority_from_objs(uint8_t **objs, int num_objs, int x, uint8_t **out_obj) {
	uint8_t pixel;
	uint8_t *curr_priority = objs[0];
	*out_obj = objs[0];
	for (int i = 0; i < num_objs; i++) {
		uint16_t tileline = get_tile_line_from_object(objs[i], (ppu.regs.ly+16)-(objs)[i][0]);
		bool flip_x = objs[i][3] & OBJ_ATTR_FLIP_X;
		uint8_t beg_pixel_in_tile = flip_x ? 7 - ((x)-(objs[i][1])) : (x)-(objs[i][1]);
		uint8_t indx = (bool)(tileline & (0x8000 >> ((beg_pixel_in_tile)&7)))<<1;
		indx |= (((bool)(tileline & (0x80 >> ((beg_pixel_in_tile)&7)))));
		assert(indx <= 3);
		if (i == 0) {
			pixel = indx;
		}
		else {
			if (pixel == 0 || (objs[i][1] <= curr_priority[1])) {
				if (pixel && (objs[i][1] == curr_priority[1])) {
					uint8_t *oam = ppu.oam;
					for (int j = 0; j < 40; j++) {
						if (oam == objs[i] || oam == curr_priority) {
							break;
						}
						oam += 4;
					}
					if (oam == objs[i]) {
						pixel = indx;
						curr_priority = objs[i];
						*out_obj = objs[i];
					}
				}
				else if (indx) {
					pixel = indx;
					curr_priority = objs[i];
					*out_obj = objs[i];
				}
			}
		}
	}
	return pixel;
}

static void ppu_render_line() {
	struct ppu_regs *regs = &ppu.regs;

	uint8_t *bg_tilemap = regs->lcdc & LCDC_BITMASK_BG_TILE_MAP ? ppu.tile_map2 : ppu.tile_map1;
	// tilemaps are 32x32 1-byte entries that represent the index into the tile data
	// area.
	// here we point to the correct row of the 32x32 entries.
	uint8_t *bg_tilemap_row_beg = bg_tilemap + ((((regs->ly+regs->scy)>>3)<<5) & 0x3ff);
	uint8_t bg_beg_pixel_in_tile = regs->scx&7;

	for (int i=0; i < SCANLINE; i+=8) {
		for (int j = 0; j < 8; j++) {
			uint32_t final_pixel;

			uint32_t obj_pixel = 0;
			uint8_t *obj_prio = nullptr;
			if (regs->lcdc & LCDC_BITMASK_OBJ_ENABLE) {
				uint8_t *objs[10];
				int num_objs = get_objs_at_xy(i+j, ppu.regs.ly, objs);
				if (num_objs) {
					uint32_t obj_indx = get_pixel_priority_from_objs(objs, num_objs, i+j, &obj_prio);
					if (obj_indx) {
						uint8_t obj_palette = obj_prio[3] & 0x10 ? regs->obp1 : regs->obp0;
						obj_pixel = color_pal[(obj_palette & (3<<(obj_indx<<1)))>> (obj_indx<<1)];
						final_pixel = obj_pixel;
					}
					//printf("drawing obj %d, obj_x %d obj_y %d at x %d y %d pixel %d\n", (*obj)[2], (*obj)[1], (*obj)[0], i+j, ppu.regs.ly, pixel);
				}
			}

			if (regs->lcdc & LCDC_BITMASK_BG_ENABLE &&
					(!obj_pixel || obj_prio[3] & 0x80)) {
				// adjust the column position in each iteration, since it may wrap at any time
				// in the line rendering.
				bg_tilemap = bg_tilemap_row_beg + (((i + j + regs->scx) &0xff)>>3);

				// get bg tileline
				uint16_t *bg_tileline;
				bg_tileline = get_tile_line(*bg_tilemap, (regs->ly+regs->scy)&7);
				uint8_t bg_indx = ((bool)(*bg_tileline & (0x8000 >> ((j+bg_beg_pixel_in_tile)&7))))<<1;
				bg_indx |= (((bool)(*bg_tileline & (0x80 >> ((j+bg_beg_pixel_in_tile)&7)))));
				assert(bg_indx <= 3);
				uint32_t bg_pixel = color_pal[(regs->bgp & (3<<(bg_indx<<1))) >> (bg_indx<<1)];
				if (!obj_pixel || (bg_indx != 0))
					final_pixel = bg_pixel;
			}

			if (regs->lcdc & LCDC_BITMASK_WIN_ENABLE &&
					(!obj_pixel || obj_prio[3] & 0x80) &&
					(regs->wy <= regs->ly && (i+j+7 >= (regs->wx)) &&
					 regs->wx < 166 && regs->wy < 143)) {
				uint8_t *win_tilemap = regs->lcdc & LCDC_BITMASK_WIN_TILE_MAP ? ppu.tile_map2 : ppu.tile_map1;
				// tilemaps are 32x32 1-byte entries that represent the index into the tile data
				// area.
				// here we point to the correct row of the 32x32 entries.
				uint8_t *win_tilemap_row_beg = win_tilemap + ((((regs->ly-regs->wy)>>3)<<5)&0x3ff);

				// adjust the column position each iteration, since it may wrap at any time
				// in the line rendering.
				win_tilemap = win_tilemap_row_beg + ((((i+j+7-regs->wx))>>3));

				uint16_t *win_tileline;
				win_tileline = get_tile_line(*win_tilemap, (regs->ly-regs->wy)&7);

				uint8_t win_indx = (bool)(win_tileline[0] & (0x8000 >> ((i+j+7-regs->wx)&7)))<<1;
				win_indx |= (((bool)(win_tileline[0] & (0x80 >> ((i+j+7-regs->wx)&7)))));
				assert(win_indx <= 3);

				uint32_t win_pixel = color_pal[(regs->bgp & (3<<(win_indx<<1)))>> (win_indx<<1)];
				if (!obj_pixel || (win_indx != 0))
					final_pixel = win_pixel;
			}
			render_draw_pixel(i+j, regs->ly, final_pixel);
		}
	}
}

static void change_phase() {
	switch (ppu.mode) {
		// hblank cycle
		case PPU_HBLANK:
			hblank_end_cycle();
			break;
		case PPU_VBLANK:
			render_draw_framebuffer();
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
			ppu_render_line();
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

static void oam_rehash(uint16_t addr, uint8_t new_y) {
	uint8_t **obj = oam_hash[(ppu.oam[addr])>>4];

	bool found = false;
	while (*obj) {
		if (*obj == &ppu.oam[addr] || found) {
			found = true;
			*obj = *(obj+1);
		}
		obj++;
	}

	obj = oam_hash[new_y>>4];
	while (*obj)
		obj++;
	*obj = &ppu.oam[addr];
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
		if (!(addr & 0x3) && (ppu.oam[addr-0xfe00]>>4) != (value>>4))
			oam_rehash(addr-0xfe00, value);
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

#ifdef HAVE_LIBEMU
static uint16_t peek_get_ppu_reg(uintptr_t ppu_reg) {
	enum ppu_reg reg = ppu_reg;
	switch (reg) {
		case PPU_REG_LY:
			return ppu_rd(0xff44);
		default:
			fprintf(stderr, "error: peek_get_ppu_reg()");
			return -1;
	}
}

void ppu_peek(struct peek *peek, struct peek_reply *reply) {
	switch (peek->subtype) {
		case PPU_PEEK_REG:
			reply->size = sizeof(uint32_t);
			reply->payload = malloc(reply->size); // caller frees
			uint32_t ppu_reg = peek_get_ppu_reg(peek->req);
			memcpy(reply->payload, &ppu_reg, sizeof(uint32_t));
			break;
		default:
			fprintf(stderr, "error: ppu_peek()");
	}
}
#endif
