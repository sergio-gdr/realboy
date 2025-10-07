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

#ifndef RB_IPC_H
#define RB_IPC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cpu.h"

// type of command
// used for request and reply.
enum msg_type {
	TYPE_CONTROL_FLOW,
	TYPE_INSPECT
};

// for TYPE_INSPECT
// egs:
//  > print 0xffe0
//  > reg PC
enum msg_type_inspect {
	INSPECT_PRINT_ADDR,
	INSPECT_GET_CPU_REG,
	INSPECT_GET_PPU_REG,
	INSPECT_GET_INSTR_AT_ADDR,
	INSPECT_GET_OP_LEN
};

// for TYPE_CONTROL_FLOW
// egs:
//  > next
//  > finish
//  > until 0x50
enum msg_type_control_flow {
	CONTROL_FLOW_NEXT,
	CONTROL_FLOW_STEP,
	CONTROL_FLOW_UNTIL,
	CONTROL_FLOW_FINISH
};

enum msg_field {
	MAGIC,
	TYPE,
	SUBTYPE,
	SIZE,
	PAYLOAD
};

#define MAGIC_NUMBER 0x67726553
// in uint32_t units
#define HDR_SIZE 16
#define MAGIC_SIZE 1

#define COMMAND_SIZE_GET_NEXT_INST 0
#define COMMAND_SIZE_PRINT_ADDR sizeof(uint32_t)
#define COMMAND_SIZE_GET_REG sizeof(uint32_t)

struct msg {
	// header:
	//  type, subtype and size of msg
	struct msg_hdr {
		enum msg_type type;
		union msg_subtype {
			enum msg_type_control_flow control_flow;
			enum msg_type_inspect inspect;
		} subtype;
		size_t size; // in bytes
	} hdr;
	// payload
	union {
		void *ptr_pay;
		uint32_t ui32_pay;
	} payload;
};

#define IPC_HDR_SIZ sizeof(uint32_t)*4

void ipc_fini();
int ipc_init(bool is_server);
int ipc_recv_msg(struct msg *hdr);
int ipc_send_msg(const struct msg *hdr);
void ipc_msg_init(struct msg *msg);
void ipc_msg_fini(struct msg *msg);

#endif
