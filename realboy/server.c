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
#include <stdlib.h>
#include <string.h>

#include "ipc.h"

static void peek_get_payload(const struct msg *req, struct msg *reply) {
	struct cpu_peek peek;
	struct cpu_peek_reply peek_reply;

	switch (req->hdr.type) {
		case TYPE_CONTROL_FLOW:
			switch (req->hdr.subtype.control_flow) {
				case CONTROL_FLOW_NEXT:
					reply->hdr.subtype.control_flow = CONTROL_FLOW_NEXT;
					break;
				default:
					perror("TYPE_CONTROL_FLOW");
			}
			break;
		case TYPE_INSPECT:
			{
				uint32_t payload = req->payload.ui32_pay;
				switch (req->hdr.subtype.inspect) {
					case INSPECT_GET_CPU_REG:
						reply->hdr.subtype.inspect = INSPECT_GET_CPU_REG;
						peek.type = PEEK_CPU_REG;
						peek.cpu_reg = payload;
						break;
					case INSPECT_GET_PPU_REG:
						reply->hdr.subtype.inspect = INSPECT_GET_PPU_REG;
						peek.type = PEEK_PPU_REG;
						peek.ppu_reg = payload;
						break;
					case INSPECT_PRINT_ADDR:
						reply->hdr.subtype.inspect = INSPECT_PRINT_ADDR;
						peek.type = PEEK_ADDR;
						peek.addr = payload;
						break;
					case INSPECT_GET_INSTR_AT_ADDR:
						reply->hdr.subtype.inspect = INSPECT_GET_INSTR_AT_ADDR;
						peek.type = PEEK_INSTR_AT_ADDR;
						peek.addr = payload;
						break;
					case INSPECT_GET_OP_LEN:
						reply->hdr.subtype.inspect = INSPECT_GET_OP_LEN;
						peek.type = PEEK_OP_LEN;
						peek.opcode = (uint8_t)payload;
						break;
					default:
						perror("TYPE_INSPECT");
				}
			}
			break;
		default:
			perror("peek_get_payload");
			goto out;
	}

	cpu_peek(&peek, &peek_reply);
	reply->hdr.size = peek_reply.size;
	if (reply->hdr.size > sizeof(uint32_t)) {
		uint32_t *payload = malloc(reply->hdr.size); // caller frees
		memcpy(payload, peek_reply.payload.ptr_pay, peek_reply.size);
		reply->payload.ptr_pay = payload; // reply owns
		free(peek_reply.payload.ptr_pay);
	}
	else
		reply->payload.ui32_pay = peek_reply.payload.ui32_pay;

out:
	return;
}

int server_recv_request_and_dispatch() {
	struct msg req = {};

	int ret;
	// if successful, allocates request payload
	if ((ret = ipc_recv_msg(&req)) == -1)
		goto out;

	if (req.hdr.type == TYPE_CONTROL_FLOW) {
		ret = 0;
	}
	else if (req.hdr.type == TYPE_INSPECT) {
		struct msg reply = {};
		reply.hdr.type = TYPE_INSPECT;
		peek_get_payload(&req, &reply); // allocates reply payload
		ret = ipc_send_msg(&reply);
		if (reply.hdr.size > sizeof(uint32_t))
			free(reply.payload.ptr_pay); // frees reply payload
	}

	if (req.hdr.size > sizeof(uint32_t))
		free(req.payload.ptr_pay);
out:
	return ret;
}

int server_init() {
	return ipc_init(true);
}
