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

#include "joy.h"

#include "backends.h"

static struct buttons *buttons;
static uint8_t joypad_input_reg;

uint8_t joy_rd(uint16_t addr) {
	buttons = backend_get_buttons();
	if ((joypad_input_reg & 0x30) == 0x30) {
		return 0x3f;
	}
	else {
		if (joypad_input_reg & 0x10) {
			return joypad_input_reg |
				(buttons->start_released<<3|buttons->select_released<<2|buttons->b_released<<1|buttons->a_released);
		}
		else {
			return joypad_input_reg |
				(buttons->down_released<<3|buttons->up_released<<2|buttons->left_released<<1|buttons->right_released);
		}
	}
}

void joy_wr(uint16_t addr, uint8_t value) {
	joypad_input_reg = value&0x30;
}
