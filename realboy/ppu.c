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

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdio.h>

#include "ppu.h"

#include "cpu.h"
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
	uint8_t vram[0x2000];
	uint8_t oam[0x100];
	enum ppu_mode mode;
	int16_t dots_remaining;

	uint32_t vblank_dots;
} ppu_t;
ppu_t ppu;

#define DOTS_DRAW 289
#define DOTS_HBLANK 204
#define DOTS_OAM 80
#define DOTS_VBLANK 4560
// vblank starts at ly==144.
// ly goes from 144 to 153 in the whole
// duration of the vblank.
#define DOTS_VBLANK_LINE (4560/(153-144))

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
	// COLOR GAME BOY XXX
	//			if (gboy_mode==1 && cpu_state.hdma_on==1) {
	//				int i;
	//				char *dma_src = (char *)cpu_state.hbln_dma_src;
	//				char *dma_dst = (char *)cpu_state.hbln_dma_dst;
	//				Uint16 *ptr_tmp;
	//				hdma_tmp = (addr_sp[VRAM_DMA]&0x7f)-1;
	//				addr_sp[VRAM_DMA] &= 0x80;
	//				addr_sp[VRAM_DMA] |= hdma_tmp;
	//				for (i=0; i<16; i++)
	//					*dma_dst++ = *dma_src++;
	//				ptr_tmp = (Uint16 *)(addr_sp+VRAM_DMA_SRC);
	//				*ptr_tmp = (addr_sp[VRAM_DMA_SRC+1] | (addr_sp[VRAM_DMA_SRC]<<8))+16;
	//				ptr_tmp = (Uint16 *)(addr_sp+VRAM_DMA_DST);
	//				*ptr_tmp = (addr_sp[VRAM_DMA_DST+1] | (addr_sp[VRAM_DMA_DST]<<8))+16;
	//				cpu_state.hbln_dma_dst += 16;
	//				cpu_state.hbln_dma_src += 16;
	//				if (--cpu_state.hbln_dma < 0)
	//					cpu_state.hdma_on=0, addr_sp[VRAM_DMA]=0xff;
	//			}

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
	regs->stat &= ~3; // XXX oam
	regs->stat |= 2; // oam
//else {
//	if (ppu_vbln_hbln_ctrl > gb_vbln_clks[0]) {
//		ppu_vbln_hbln_ctrl -= gb_line_clks;
//		addr_sp[LY_REG]++;
//		addr_sp[LCDS_REG] &= ~LY_LYC_FLAG;
//		if (addr_sp[LY_REG] == addr_sp[LYC_REG]) {
//			addr_sp[LCDS_REG] |= LY_LYC_FLAG;
//			if (addr_sp[LCDS_REG] & 0x40)
//				addr_sp[IR_REG] |= 2;
//		}
//	}
//}
}

enum LCDC_BITMASK {
	LCDC_BITMASK_BIT0 = 1, // BG & Window enable / priority
	LCDC_BITMASK_BIT1 = 1<<1, // OBJ enable
	LCDC_BITMASK_BIT2 = 1<<2, // OBJ size
	LCDC_BITMASK_BIT3 = 1<<3, // BG tile map
	LCDC_BITMASK_BIT4 = 1<<4, // BG & Window tiles
	LCDC_BITMASK_BIT5 = 1<<4, // Window enable
	LCDC_BITMASK_BIT6 = 1<<4, // Window tile map
	LCDC_BITMASK_BIT7 = 1<<4 // LCD/PPU enable
};

#define TILE_HEIGHT 8
#define TILES_PER_SCANLINE 32
#define BYTES_PER_TILE 16
#define BYTES_PER_TILE_LINE 2
#define PIXELS_PER_SCANLINE 160
#define SCANLINE 160

void draw_tileline(uint8_t tiledata[2], uint8_t pixel_start, uint8_t x) {
	struct ppu_regs *regs = &ppu.regs;

	for (uint8_t bitmask = 0x80 >> pixel_start; bitmask; bitmask >>= 1) {
		backend_wayland_draw_pixel(x++, regs->ly, (tiledata[1]&bitmask)>>(6-pixel_start) | (tiledata[1]&bitmask)>>(7-pixel_start));
	}
}

#define PIXELS_PER_ROW 256
#define BITS_PER_PIXEL 2
void draw_line(uint8_t *tilemap) {
	struct ppu_regs *regs = &ppu.regs;

	tilemap += (TILES_PER_SCANLINE) * ((regs->ly+regs->scy));
	tilemap += regs->scx;
	for (int i=0; i < SCANLINE; i+=8) {
		bool is_mode_8k = regs->lcdc & LCDC_BITMASK_BIT4;
		uint8_t *tiledata =  is_mode_8k ? ppu.tile_data : &ppu.tile_data[0x1000];
		tiledata += is_mode_8k ? *tilemap * BYTES_PER_TILE : *((int8_t *)tilemap) * BYTES_PER_TILE;
		if (i == 0 || i == 143) {
			draw_tileline(tiledata, regs->scx & 0x7, i);
		}
		else {
			draw_tileline(tiledata, 0, i);
		}
	}
}

static void render_back() {
	struct ppu_regs *regs = &ppu.regs;

	uint8_t *tilemap;
	if (regs->lcdc & LCDC_BITMASK_BIT3)
		tilemap = ppu.tile_map2;
	else
		tilemap = ppu.tile_map1;

	draw_line(tilemap);
}

static void render_line() {
	struct ppu_regs *regs = &ppu.regs;

	if (regs->lcdc & LCDC_BITMASK_BIT0) {
		render_back();
	}
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
//gb_vram_clks[0] += (gb_oam_clks[0]) + spr_extr_cycles[nb_spr>>3] + (addr_sp[SCR_X]&0x4);
//spr_cur_extr = spr_extr_cycles[nb_spr>>3];
//addr_sp[LCDS_REG] |= 3;
			break;
		case PPU_DRAW:
			ppu.dots_remaining += DOTS_HBLANK;
			ppu.regs.stat &= ~3;
			ppu.mode = PPU_HBLANK;
			render_line();
//gb_hblank_clks[0] -= spr_cur_extr;
//gb_hblank_clks[0] -= (addr_sp[SCR_X]&0x4);
//render_scanline(skip_next_frame);
//addr_sp[IR_REG] |= ((addr_sp[LCDS_REG]&H_BLN_INT)>>2);
			break;
		default:
			perror("change_phase()\n");
	}
}

void ppu_refresh(uint8_t ticks) {
	ppu.dots_remaining -= CYCLES_TO_DOTS(ticks);
	if (ppu.dots_remaining <= 0) {
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

static void wr_reg(uint16_t addr, uint8_t value) {
	struct ppu_regs *regs = &ppu.regs;

	// XXX make this an array
	switch (addr) {
		case 0xff40:
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
			regs->dma = value;
			break;
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
	else if (addr >= 0x8000 && addr <= 0x9fff) {
		ppu.vram[addr-0x8000] = value;
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
	else if (addr >= 0x8000 && addr <= 0x9fff) {
		return ppu.vram[addr-0x8000];
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
