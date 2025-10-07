/* RealBoy Emulator
 * Copyright (C) 2013-2025 Sergio Gómez Del Real <sgdr>
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

#include <stdio.h>
#include <string.h>
#include <emu_list.h>
#include <emu_ipc.h>

#include "cpu.h"
#include "ppu.h"
#include "monitor.h"

list_t *breakpoint_list;
uint16_t until_addr;
bool control_flow_continue;
bool control_flow_until;
bool control_flow_next;
bool client_connected;

static int new_client_callback(void *fd) {
	client_connected = true;
	return 0;
}

static void control_flow_get_answer(struct msg *req, struct msg *answer) {
	switch (req->hdr.subtype.control_flow) {
		case CONTROL_FLOW_UNTIL:
			control_flow_until = true;
			until_addr = req->payload;
			break;
		case CONTROL_FLOW_DELETE:
			if (!req->hdr.size) {
				list_free(breakpoint_list);
				breakpoint_list = create_list();
			}
			else {
				uintptr_t breakpoint;
				list_for_each(breakpoint_list, breakpoint) {
					if (breakpoint == req->payload) {
						list_remove(breakpoint_list, breakpoint);
						break;
					}
				}
			}
			break;
		case CONTROL_FLOW_CONTINUE:
			control_flow_continue = true;
			break;
		case CONTROL_FLOW_NEXT:
			control_flow_next = true;
			break;
		case CONTROL_FLOW_BREAK:
			uintptr_t breakpoint = req->payload;
			list_add(breakpoint_list, breakpoint);
			break;
		default:
			perror("control_flow_get_answer()");
	}

	answer->hdr.type = req->hdr.type;
	answer->hdr.subtype = req->hdr.subtype;
	answer->hdr.size = 3;
	answer->payload = (uintptr_t)"ack";
}

static void inspect_get_answer(struct msg *req, struct msg *answer) {
	struct peek peek;
	struct peek_reply peek_reply;

	switch (req->hdr.subtype.inspect) {
		case INSPECT_GET_CPU_REG:
			answer->hdr.subtype.inspect = INSPECT_GET_CPU_REG;
			peek.type = PEEK_CPU;
			peek.subtype = CPU_PEEK_REG;
			peek.req = req->payload;
			break;
		case INSPECT_GET_PPU_REG:
			answer->hdr.subtype.inspect = INSPECT_GET_PPU_REG;
			peek.type = PEEK_PPU;
			peek.subtype = PPU_PEEK_REG;
			peek.req = req->payload;
			break;
		case INSPECT_PRINT_ADDR:
			answer->hdr.subtype.inspect = INSPECT_PRINT_ADDR;
			peek.type = PEEK_CPU;
			peek.subtype = CPU_PEEK_ADDR;
			peek.req = req->payload;
			break;
		case INSPECT_GET_INSTR_AT_ADDR:
			answer->hdr.subtype.inspect = INSPECT_GET_INSTR_AT_ADDR;
			peek.type = PEEK_CPU;
			peek.subtype = CPU_PEEK_INSTR_AT_ADDR;
			peek.req = req->payload;
			break;
		case INSPECT_GET_OP_LEN:
			answer->hdr.subtype.inspect = INSPECT_GET_OP_LEN;
			peek.type = PEEK_CPU;
			peek.subtype = CPU_PEEK_OP_LEN;
			peek.req = req->payload;
			break;
		default:
			perror("TYPE_INSPECT");
	}

	switch (peek.type) {
		case PEEK_CPU:
			cpu_peek(&peek, &peek_reply);
			break;
		case PEEK_PPU:
			ppu_peek(&peek, &peek_reply);
			break;
	}
	answer->hdr.size = peek_reply.size;
	if (answer->hdr.size > sizeof(uint32_t)) {
		uint32_t *payload = malloc(answer->hdr.size); // caller frees
		memcpy(payload, peek_reply.payload.ptr_pay, peek_reply.size);
		answer->payload = (uintptr_t)payload; // reply owns
		free(peek_reply.payload.ptr_pay);
	}
	else
		answer->payload = peek_reply.payload.ui32_pay;
}

bool server_hit_breakpoint() {
	struct peek peek = {};
	struct peek_reply reply = {};
	peek.type = PEEK_CPU;
	peek.subtype = CPU_PEEK_REG;
	peek.req = CPU_REG_PC;
	cpu_peek(&peek, &reply);

	uint16_t addr = reply.payload.ui32_pay;

	if (control_flow_continue || control_flow_until) {
		uint32_t breakpoint;
		list_for_each(breakpoint_list, breakpoint) {
			if (breakpoint == addr) {
				control_flow_continue = 0;
				return true;
			}
		}

		if (control_flow_until) {
			if (until_addr == addr) {
				control_flow_until = false;
				control_flow_continue = false;
				until_addr = 0;
				return true;
			}
		}
	}
	else if (control_flow_next) {
		control_flow_next = false;
		return true;
	}

	return false;
}

bool server_is_request_control_flow() {
	return control_flow_continue || control_flow_until || control_flow_next;
}

int server_recv_request_and_dispatch() {
	struct msg req = {};

	int ret;
	// if successful, allocates request payload
	if ((ret = ipc_recv_msg(&req)) == -1)
		goto out;

	struct msg answer = {};
	if (req.hdr.type == TYPE_CONTROL_FLOW) {
		answer.hdr.type = TYPE_CONTROL_FLOW;
		control_flow_get_answer(&req, &answer);
		switch (req.hdr.subtype.control_flow) {
			case CONTROL_FLOW_UNTIL:
				answer.hdr.subtype.control_flow = CONTROL_FLOW_UNTIL;
				break;
			case CONTROL_FLOW_CONTINUE:
				answer.hdr.subtype.control_flow = CONTROL_FLOW_CONTINUE;
				break;
			case CONTROL_FLOW_NEXT:
				answer.hdr.subtype.control_flow = CONTROL_FLOW_NEXT;
				break;
			case CONTROL_FLOW_BREAK:
				answer.hdr.subtype.control_flow = CONTROL_FLOW_BREAK;
				break;
			default:
				perror("TYPE_CONTROL_FLOW");
		}
		ret = ipc_send_msg(&answer);
	}
	else if (req.hdr.type == TYPE_INSPECT) {
		answer.hdr.type = TYPE_INSPECT;
		inspect_get_answer(&req, &answer); // allocates reply payload
		ret = ipc_send_msg(&answer);
		if (answer.hdr.size > sizeof(uint32_t))
			free((void *)answer.payload); // frees reply payload
	}

	if (req.hdr.size > sizeof(uint32_t))
		free((void *)req.payload);
out:
	return ret;
}

bool server_is_client_connected() {
	return client_connected;
}

int server_init() {
	int ret = emu_init_monitor(true, new_client_callback);
	if (ret == 0)
		breakpoint_list = create_list();
	return ret;
}
