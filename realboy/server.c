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

#include <stdio.h>
#include <stdlib.h>
#include <libemu.h>
#include <pthread.h>

#include "cpu.h"
#include "ppu.h"
#include "monitor.h"

bool client_connected;

static void handle_monitor(struct msg *req) {
	switch (req->hdr.subtype.monitor) {
		case MONITOR_STOP:
			monitor_stop_execution();
			break;
		case MONITOR_RESUME:
			monitor_resume_execution();
			break;
	}
}

static void handle_control_flow(struct msg *req) {
	switch (req->hdr.subtype.control_flow) {
		case CONTROL_FLOW_UNTIL:
			uint32_t until_addr = *(uint32_t*)req->payload;
			monitor_set_control_flow(CONTROL_FLOW_UNTIL, until_addr);
			break;
		case CONTROL_FLOW_DELETE:
			if (!req->hdr.size) {
				monitor_set_control_flow(CONTROL_FLOW_DELETE, 0);
			}
			else {
				monitor_set_control_flow(CONTROL_FLOW_DELETE, *(uint32_t*)req->payload);
			}
			break;
		case CONTROL_FLOW_CONTINUE:
			monitor_set_control_flow(CONTROL_FLOW_CONTINUE, 0);
			break;
		case CONTROL_FLOW_NEXT:
			monitor_set_control_flow(CONTROL_FLOW_NEXT, 0);
			break;
		case CONTROL_FLOW_BREAK:
			monitor_set_control_flow(CONTROL_FLOW_BREAK, *(uint32_t*)req->payload);
			break;
		default:
			fprintf(stderr, "error: control_flow_get_answer()");
			return;
	}
}

static void handle_inspect(struct msg *req, struct msg *answer) {
	struct peek peek;
	struct peek_reply peek_reply;

	switch (req->hdr.subtype.inspect) {
		case INSPECT_GET_CPU_REG:
			answer->hdr.subtype.inspect = INSPECT_GET_CPU_REG;
			peek.type = PEEK_CPU;
			peek.subtype = CPU_PEEK_REG;
			peek.req = *(uint32_t*)req->payload;
			break;
		case INSPECT_GET_PPU_REG:
			answer->hdr.subtype.inspect = INSPECT_GET_PPU_REG;
			peek.type = PEEK_PPU;
			peek.subtype = PPU_PEEK_REG;
			peek.req = *(uint32_t*)req->payload;
			break;
		case INSPECT_PRINT_ADDR:
			answer->hdr.subtype.inspect = INSPECT_PRINT_ADDR;
			peek.type = PEEK_CPU;
			peek.subtype = CPU_PEEK_ADDR;
			peek.req = *(uint32_t*)req->payload;
			break;
		case INSPECT_GET_INSTR_AT_ADDR:
			answer->hdr.subtype.inspect = INSPECT_GET_INSTR_AT_ADDR;
			peek.type = PEEK_CPU;
			peek.subtype = CPU_PEEK_INSTR_AT_ADDR;
			peek.req = *(uint32_t*)req->payload;
			break;
		case INSPECT_GET_OP_LEN:
			answer->hdr.subtype.inspect = INSPECT_GET_OP_LEN;
			peek.type = PEEK_CPU;
			peek.subtype = CPU_PEEK_OP_LEN;
			peek.req = *(uint32_t*)req->payload;
			break;
		default:
			fprintf(stderr, "error: handle_inspect()");
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
	answer->payload = peek_reply.payload;
}

int server_recv_request_and_dispatch(bool wait) {
	struct msg req = {};

	int ret;
	// if successful, allocates request payload
	if ((ret = emu_recv_msg(&req, wait)) == -1)
		goto out;

	switch (req.hdr.type) {
		case TYPE_CONTROL_FLOW:
			handle_control_flow(&req);
			break;
		case TYPE_INSPECT:
			struct msg answer = {};
			answer.hdr.type = TYPE_INSPECT;
			handle_inspect(&req, &answer); // allocates reply payload
			ret = emu_send_msg(&answer);
			free((void *)answer.payload); // frees reply payload
			break;
		case TYPE_MONITOR:
			handle_monitor(&req);
			break;
	}

	// we own the possible allocation from libemu
	if (req.hdr.size)
		free(req.payload);
out:
	return ret;
}


bool server_is_client_connected() {
	return client_connected;
}

int server_init(bool wait_for_client) {
	int ret = emu_init(true);
	if (wait_for_client) {
		if ( (ret = emu_wait_for_client()) != -1)
			client_connected = true;
	}
	return ret;
}
