/* RealBoy Debugger.
 * Copyright (C) 2025 Sergio Gómez Del Real <sgdr>
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

#ifndef CLI_H
#define CLI_H

#include <ncurses.h>

#include "list.h"

struct cli_window {
	WINDOW *win;
	int max_y, max_x;

	int current_pos_y;
	uint32_t longest_str_size;

	struct instr *instr_current_highlight;
	list_t *instrs;
};

struct cmd {
	int argc;
	char **argv;
};

WINDOW *cli_init();
int cli_parse(struct cmd **cmd_out);

#endif
