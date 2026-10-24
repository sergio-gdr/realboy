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

#include "config.h"

#include "server.h"

#include <string.h>

#ifdef HAVE_LIBEMU
#include <libemu.h>
#include <stdio.h>
#include <stdlib.h>
#endif

static bool server_initialized;
static bool client_connected;

static server_dispatch_impl_t dispatch_tbl;

#ifdef HAVE_LIBEMU
static void dispatch_monitor(const struct msg *req) {
	switch (req->hdr.subtype.monitor) {
		case MONITOR_STOP:
			dispatch_tbl.dispatch_monitor_stop();
			break;
		case MONITOR_RESUME:
			dispatch_tbl.dispatch_monitor_resume();
			break;
	}
}

static void dispatch_control_flow(const struct msg *req) {
	switch (req->hdr.subtype.control_flow) {
		case CONTROL_FLOW_UNTIL:
			uint32_t until_addr = *(uint32_t*)req->payload;
			dispatch_tbl.dispatch_control_flow_until(until_addr);
			break;
		case CONTROL_FLOW_DELETE:
			if (!req->hdr.size) {
				dispatch_tbl.dispatch_control_flow_delete(0);
			}
			else {
				dispatch_tbl.dispatch_control_flow_delete(*(uint32_t*)req->payload);
			}
			break;
		case CONTROL_FLOW_CONTINUE:
			dispatch_tbl.dispatch_control_flow_continue();
			break;
		case CONTROL_FLOW_NEXT:
			dispatch_tbl.dispatch_control_flow_next();
			break;
		case CONTROL_FLOW_BREAK:
			dispatch_tbl.dispatch_control_flow_break(*(uint32_t*)req->payload);
			break;
		default:
			fprintf(stderr, "error: control_flow_get_answer()");
			return;
	}
}

static void dispatch_inspect(const struct msg *req, struct msg *answer) {
	server_reply_t reply = {};
	switch (req->hdr.subtype.inspect) {
		case INSPECT_GET_CPU_REG:
			answer->hdr.subtype.inspect = INSPECT_GET_CPU_REG;
			reply = dispatch_tbl.dispatch_inspect_cpu(*(uint32_t*)req->payload);
			break;
		case INSPECT_GET_PPU_REG:
			answer->hdr.subtype.inspect = INSPECT_GET_PPU_REG;
			reply = dispatch_tbl.dispatch_inspect_ppu(*(uint32_t*)req->payload);
			break;
		case INSPECT_PRINT_ADDR:
			answer->hdr.subtype.inspect = INSPECT_PRINT_ADDR;
			reply = dispatch_tbl.dispatch_inspect_addr(*(uint32_t*)req->payload);
			break;
		case INSPECT_GET_INSTR_AT_ADDR:
			answer->hdr.subtype.inspect = INSPECT_GET_INSTR_AT_ADDR;
			reply = dispatch_tbl.dispatch_inspect_instr_at_addr(*(uint32_t*)req->payload);
			break;
		case INSPECT_GET_OP_LEN:
			answer->hdr.subtype.inspect = INSPECT_GET_OP_LEN;
			reply = dispatch_tbl.dispatch_inspect_op_len(*(uint32_t*)req->payload);
			break;
		default:
			fprintf(stderr, "error: handle_inspect()");
	}

	answer->hdr.size = reply.size;
	answer->payload = reply.payload;
}
#endif

int server_recv_request_and_dispatch() {
#ifdef HAVE_LIBEMU
	struct msg req = {};
	int ret;
	// if successful, allocates request payload
	if ((ret = emu_recv_msg(&req, 0)) == -1)
		goto out;

	switch (req.hdr.type) {
		case TYPE_CONTROL_FLOW:
			dispatch_control_flow(&req);
			break;
		case TYPE_INSPECT:
			struct msg answer = {};
			answer.hdr.type = TYPE_INSPECT;
			dispatch_inspect(&req, &answer); // allocates reply payload
			ret = emu_send_msg(&answer);
			free((void *)answer.payload); // frees reply payload
			break;
		case TYPE_MONITOR:
			dispatch_monitor(&req);
			break;
	}

out:
	// we own the possible allocation from libemu
	if (req.hdr.size)
		free(req.payload);
	return ret;
#else
	return 0;
#endif
}

int server_get_fd() {
#ifdef HAVE_LIBEMU
	return emu_get_fd();
#else
	return 0;
#endif
}

bool server_is_client_connected() {
	return client_connected;
}

void server_fini() {
	if (!server_initialized)
		return;
}

void server_set_dispatch_table(const server_dispatch_impl_t *tbl) {
	dispatch_tbl = *tbl;
}

int server_send_msg(enum server_msg msg, uint32_t addr) {
#ifdef HAVE_LIBEMU
	struct msg msg_reply;
	msg_reply.hdr.type = TYPE_CONTROL_FLOW;

	int ret;
	switch (msg) {
		case SERVER_MSG_BREAK:
			msg_reply.hdr.subtype.control_flow = CONTROL_FLOW_BREAK;
			msg_reply.hdr.size = 4;
			msg_reply.payload = malloc(sizeof(uint32_t));
			memcpy(msg_reply.payload, &addr, sizeof(uint32_t));
			ret = emu_send_msg(&msg_reply);
			free(msg_reply.payload);
			break;
		case SERVER_MSG_UNTIL:
			msg_reply.hdr.subtype.control_flow = CONTROL_FLOW_UNTIL;
			msg_reply.hdr.size = 4;
			msg_reply.payload = malloc(sizeof(uint32_t));
			memcpy(msg_reply.payload, &addr, sizeof(uint32_t));
			ret = emu_send_msg(&msg_reply);
			free(msg_reply.payload);
			break;
	}

	return ret;
#else
	return 0;
#endif

}

int server_init(bool wait_for_client) {
#ifdef HAVE_LIBEMU
	if (emu_init(true) == -1)
		return -1;

	if (wait_for_client) {
		if ( (emu_wait_for_client()) == -1) {
			return -1;
		}
		client_connected = true;
	}

	server_initialized = true;

	return 0;
#else
	return 0;
#endif
}
