/* RealBoy Emulator: Free, Fast, Yet Accurate, Game Boy/Game Boy Color Emulator.
 * Copyright (C) 2013 Sergio Andrés Gómez del Real
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
#ifndef GBOY_JOY_H
#define GBOY_JOY_H 1

/* Key presses */
#define RET_MASK 0x1
#define D_MASK 0x2
#define S_MASK 0x4
#define A_MASK 0x8
#define UP_MASK 0x10
#define DOWN_MASK 0x20
#define LEFT_MASK 0x40
#define RIGHT_MASK 0x80


#include <SDL/SDL.h>
long joy_event(SDL_KeyboardEvent *key, Uint32 type);
#endif
