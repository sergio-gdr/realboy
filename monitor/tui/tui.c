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

#include <ncurses.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ipc.h"
#include "cli.h"
#include "client.h"
#include "list.h"

struct instr {
	uint16_t addr;
	size_t len;
	char *str;
};

struct source_window {
	WINDOW *win;
	int max_y, max_x;

	int current_pos_y;
	uint32_t longest_str_size;

	struct instr *instr_current_highlight;
	list_t *instrs;
};
struct source_window wsrc;

typedef struct {
	WINDOW *src_window;
	WINDOW *cli_window;
	WINDOW *reg_window;
	WINDOW *focus_window;
	WINDOW *help_window;
	WINDOW *misc_window;
	struct instr *current_instr;
	struct instr *target_instr;
} tui_t;
tui_t tui;

static struct instr *get_instr_at_addr(uint16_t addr) {
	struct msg req = (struct msg){
		.hdr.type = TYPE_INSPECT,
		.hdr.subtype.inspect = INSPECT_GET_INSTR_AT_ADDR,
		.hdr.size = 4,
		.payload.ui32_pay = addr
	};
	struct msg reply = (struct msg){};
	char *str_reply;
	str_reply = client_dispatch(&req, &reply);
	if (!str_reply) {
		return NULL;
	}

	uint32_t *payload = reply.payload.ptr_pay;
	req = (struct msg){
		.hdr.type = TYPE_INSPECT,
		.hdr.subtype.inspect = INSPECT_GET_OP_LEN,
		.hdr.size = 4,
		.payload.ui32_pay = payload[1]
	};
	reply = (struct msg){};
	client_dispatch(&req, &reply);

	if (reply.hdr.size > sizeof(uint32_t))
		free(reply.payload.ptr_pay);

	// caller owns
	struct instr *instr = malloc(sizeof(*instr));
	instr->addr = addr;
	instr->len = reply.payload.ui32_pay;
	instr->str = str_reply;
	return instr;
}

static int until(const struct cmd *cmd) {
	uint16_t addr = 0;
	char *str = cmd->argv[1];
	if (str[0] == '0' && str[1] == 'x')
		str += 2;

	for (size_t i = 0; i < strlen(str); i++) {
		if (str[i] >= '0' && str[i] <= '9')
			str[i] -= 0x30;
		else
			str[i] -= 0x57;
		addr |= str[i] << ((strlen(str)-1-i) * 4);
	}

	free(tui.target_instr);
	tui.target_instr = get_instr_at_addr(addr);
	return 1;
}

static void change_focus() {
	if (tui.focus_window == tui.src_window) {
		wborder(tui.focus_window, 0, 0, 0, 0, 0, 0, 0, 0);
		wrefresh(tui.focus_window);
		tui.focus_window = tui.cli_window;
		wborder(tui.focus_window, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD);
		wrefresh(tui.focus_window);
		curs_set(1);
		echo();
	}
	else if (tui.focus_window == tui.cli_window) {
		wborder(tui.focus_window, 0, 0, 0, 0, 0, 0, 0, 0);
		wrefresh(tui.focus_window);
		tui.focus_window = tui.reg_window;
		wborder(tui.focus_window, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD);
		wrefresh(tui.focus_window);
		curs_set(0);
		noecho();
	}
	else {
		wborder(tui.focus_window, 0, 0, 0, 0, 0, 0, 0, 0);
		wrefresh(tui.focus_window);
		tui.focus_window = tui.src_window;
		wborder(tui.focus_window, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD);
		wrefresh(tui.focus_window);
	}
}

static void sigint_handler(int _) {
	if (tui.focus_window == tui.cli_window) {
		return;
	}

	const char *exit_monitor_str = "send ctrl+c again to exit monitor";
	wborder(tui.misc_window, 0, 0, 0, 0, 0, 0, 0, 0);
	int max_x, max_y;
   	getmaxyx(tui.misc_window, max_y, max_x);
	mvwaddstr(tui.misc_window, max_y/2, max_x/2 - strlen(exit_monitor_str)/2, exit_monitor_str);
	wrefresh(tui.misc_window);

	signal(SIGINT, SIG_DFL);
	sleep(3);
	struct sigaction sig = { .sa_handler = sigint_handler, .sa_flags = SA_NODEFER };
	sigaction(SIGINT, &sig, NULL);
	wclear(tui.misc_window);
	wrefresh(tui.misc_window);

	touchwin(tui.reg_window);
	touchwin(tui.cli_window);
	touchwin(tui.src_window);
	touchwin(tui.help_window);
	wrefresh(tui.reg_window);
	wrefresh(tui.cli_window);
	wrefresh(tui.src_window);
	wrefresh(tui.help_window);
}

static void redraw_reg_window() {
	enum cpu_reg reg_enum = CPU_REG_AF;
	const char *cpu_regs[] = { "AF: ", "BC: ", "DE: ", "HL: ", "SP: ", "PC: " };

	for (size_t i = 0; i < sizeof(cpu_regs)/sizeof(char *); i++) {
		struct msg req;
		req = (struct msg){
			.hdr.type = TYPE_INSPECT,
			.hdr.subtype.inspect = INSPECT_GET_CPU_REG,
			.hdr.size = 4,
			.payload.ui32_pay = reg_enum++ // this might be fragile
		};
		struct msg reply = {};
		const char *reg_str = client_dispatch(&req, &reply);
		char buf[sizeof(cpu_regs)+6] = {};
		strcpy(buf, cpu_regs[i]);
		strcpy(buf + strlen(cpu_regs[i]), reg_str);

		mvwaddstr(tui.reg_window, i+1, 1, buf);
		wrefresh(tui.reg_window);

		if (reply.hdr.size > sizeof(uint32_t))
			free(reply.payload.ptr_pay);
	}

	struct msg req = (struct msg){
		.hdr.type = TYPE_INSPECT,
		.hdr.subtype.inspect = INSPECT_GET_PPU_REG,
		.hdr.size = 4,
		.payload.ui32_pay = PPU_REG_LY
	};
	struct msg reply = {};
	char *reg = client_dispatch(&req, &reply);
	const char *s = "ly: ";
	char buf[sizeof(cpu_regs)+6] = {};
	strcpy(buf, s);
	strcpy(buf+strlen(s), reg);

	mvwaddstr(tui.reg_window, (sizeof(cpu_regs)/sizeof(*cpu_regs))+1, 1, buf);
	wrefresh(tui.reg_window);

	if (reply.hdr.size > sizeof(uint32_t))
		free(reply.payload.ptr_pay);
}

static void redraw_src_window(list_t *instrs) {
	wclear(wsrc.win);
	if (wsrc.win == tui.focus_window)
		wborder(wsrc.win, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD);
	else
		wborder(wsrc.win, 0, 0, 0, 0, 0, 0, 0, 0);
	wrefresh(wsrc.win);
	wsrc.current_pos_y = 1;
	wsrc.longest_str_size = 0;

	// fill the source window with instructions
	struct instr *in;
	list_for_each(instrs, in) {
		if (i == 0) {
			wsrc.instr_current_highlight = in;
			mvwaddch(wsrc.win, i+1, wsrc.max_x/2,
				ACS_DIAMOND);
			wattron(wsrc.win, A_REVERSE);
			mvwaddstr(wsrc.win, i+1, wsrc.max_x/2, in->str);
			wattroff(wsrc.win, A_REVERSE);
		}
		else {
			mvwaddstr(wsrc.win, i+1, wsrc.max_x/2, in->str);
		}
	}
	wrefresh(wsrc.win);

	wsrc.instrs = instrs; // wsrc owns
}

static void get_pc(uint16_t *addr) {
		struct msg req = (struct msg){
			.hdr.type = TYPE_INSPECT,
			.hdr.subtype.inspect = INSPECT_GET_CPU_REG,
			.hdr.size = 4,
			.payload.ui32_pay = CPU_REG_PC
		};
		struct msg reply = {};
		client_dispatch(&req, &reply);

		uint32_t payload[2];
		memcpy(payload, reply.payload.ptr_pay, sizeof(payload));
		*addr = payload[0] | payload[1]<<8;

		if (reply.hdr.size > sizeof(uint32_t))
			free(reply.payload.ptr_pay);
}

static void advance_instr() {
	// send message to server to execute next instruction
	struct msg req = (struct msg){
		.hdr.type = TYPE_CONTROL_FLOW,
		.hdr.subtype.control_flow = CONTROL_FLOW_NEXT,
		.hdr.size = 0,
		.payload.ptr_pay = NULL
	};
	client_dispatch(&req, NULL);

	uint16_t pc;
	get_pc(&pc);

	free(tui.current_instr);
	tui.current_instr = get_instr_at_addr(pc);
}

static void wsrc_move(int ch) {
	if (wsrc.current_pos_y == 1 && ch == 'k')
		return;

	mvwaddstr(wsrc.win, wsrc.current_pos_y, wsrc.max_x/2,
		((struct instr *)(wsrc.instrs->items[wsrc.current_pos_y-1]))->str);
	wsrc.current_pos_y = ch == 'k' ? wsrc.current_pos_y-1 : wsrc.current_pos_y+1;
	wsrc.instr_current_highlight = wsrc.instrs->items[wsrc.current_pos_y-1];
	wattron(wsrc.win, A_REVERSE);
	mvwaddstr(wsrc.win, wsrc.current_pos_y, wsrc.max_x/2,
		((struct instr *)(wsrc.instrs->items[wsrc.current_pos_y-1]))->str);
	wattroff(wsrc.win, A_REVERSE);
	wrefresh(wsrc.win);
}

static void init_wins() {
	// source window
	wborder(tui.src_window, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD);
	wrefresh(tui.src_window);

	// register window
	wborder(tui.reg_window, 0, 0, 0, 0, 0, 0, 0, 0);
	wrefresh(tui.reg_window);

	// command-line interface window
	wborder(tui.cli_window, 0, 0, 0, 0, 0, 0, 0, 0);
	mvwaddstr(tui.cli_window, 1, 1, "> ");
	wrefresh(tui.cli_window);

	// help window
	wborder(tui.help_window, 0, 0, 0, 0, 0, 0, 0, 0);
	mvwaddstr(tui.help_window, 1, 1, "SHORTCUTS");
	mvwaddstr(tui.help_window, 2, 2, "Ctrl+C (twice): close debugger");
	mvwaddstr(tui.help_window, 3, 2, "TAB: change window focus");
	mvwaddstr(tui.help_window, 4, 2, "j/k: vim-style up and down");
	wrefresh(tui.help_window);
}

static list_t *get_instrs(uint16_t start_addr) {
	list_t *instr_list = create_list(); // caller owns

	int curr_addr = start_addr;
	int num_instrs = wsrc.max_y-2;
	for (int i = 0; i < num_instrs; i++) {
		struct instr *instr = get_instr_at_addr(curr_addr);
		if (!instr) {
			return NULL;
		}
		list_add(instr_list, instr);
		curr_addr += instr->len;
	}

	return instr_list;
}

static int parse_cmd(const struct cmd *command) {
	if (!strcmp(command->argv[0], "until")) {
		until(command);
		return 1;
	}
	return 0;
}

int tui_run() {
	list_t *instrs = get_instrs(0); // we own this list
	redraw_src_window(instrs); // ...now pass ownership to source window (wsrc)
	redraw_reg_window();

	noecho();
	curs_set(0);

	struct sigaction sig = { .sa_handler = sigint_handler, .sa_flags = SA_NODEFER };
	sigaction(SIGINT, &sig, NULL);

	tui.current_instr = get_instr_at_addr(0);
	int ch;
	while (1) {
		if (tui.focus_window == tui.cli_window) {
			struct cmd *c;
			int ret;
			ret = cli_parse(&c);
			if (!ret)
				change_focus();
			if (c) {
				parse_cmd(c);
			}
			while (tui.target_instr &&
				   tui.target_instr->addr != tui.current_instr->addr) {
				advance_instr();
			}
			redraw_src_window(get_instrs(tui.current_instr->addr));
			redraw_reg_window();
			continue;
		}
		ch = wgetch(tui.focus_window);
		if (ch == '\t') {
			change_focus();
			if (tui.focus_window == tui.cli_window) {
				struct cmd *c;
				int ret;
			   	ret = cli_parse(&c);
				if (!ret)
					change_focus();
				if (c) {
					if (!parse_cmd(c))
						continue;
				}
			}
		}
		else if (tui.focus_window == tui.src_window) {
			if (ch == 'j' || ch == 'k') {
				wsrc_move(ch);
				continue;
			}
			if (ch == '\n') {
				if (wsrc.instr_current_highlight->addr != tui.current_instr->addr)
					tui.target_instr = wsrc.instr_current_highlight;
				else {
					tui.target_instr = NULL;
					advance_instr();
				}
			}
		}

		while (tui.target_instr &&
				tui.target_instr->addr != tui.current_instr->addr) {
			advance_instr();
		}
		redraw_src_window(get_instrs(tui.current_instr->addr));
		redraw_reg_window();
	}

	endwin();
	return(0);
}

int tui_init() {
	initscr();

	// immutable reference
	if (!(tui.cli_window = cli_init())) {
		goto err;
	}

	if (!(wsrc.win = newwin((LINES/3)*2+(LINES%3), COLS/2, 0, 0))) {
		goto err;
	}
	getmaxyx(wsrc.win, wsrc.max_y, wsrc.max_x);
	tui.src_window = wsrc.win; // immutable reference

	if (!(tui.reg_window = newwin((LINES/3)*2+(LINES%3), COLS/2, 0, COLS/2))) {
		goto err;
	}
	if (!(tui.help_window = newwin(LINES/3, COLS/2, LINES-(LINES/3), COLS/2))) {
		goto err;
	}

	// messages window
	if (!(tui.misc_window = newwin(5, COLS/3, LINES/3, COLS/3))) {
		goto err;
	}

	init_wins();
	tui.focus_window = tui.src_window;

	return 0;
err:
	return -1;
}
