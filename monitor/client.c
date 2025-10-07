/* RealBoy Debugger.
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "ipc.h"
#include "disasm.h"

static char *handle_get_ppu_reg(const struct msg *reply) {
	uint8_t reg = reply->payload.ui32_pay;
	char *paytostr = malloc(20);
	if (reg < 0x10)
		sprintf(paytostr, "0x0%hx", (uint8_t)reg);
	else
		sprintf(paytostr, "0x%hx", (uint8_t)reg);

	return paytostr;
}

static char *handle_get_cpu_reg(const struct msg *reply) {
	uint32_t *addr = reply->payload.ptr_pay;
	char *paytostr = malloc(10);
	if (addr[1] < 0x10) {
		sprintf(paytostr, "0x0%x", addr[1]);
		if (addr[0] < 0x10) {
			sprintf(paytostr+4, "0%x", addr[0]);
		}
		else {
			sprintf(paytostr+4, "%x", addr[0]);
		}
	}
	else {
		sprintf(paytostr, "0x%x", addr[1]);
		if (addr[0] < 0x10) {
			sprintf(paytostr+4, "0%x", addr[0]);
		}
		else {
			sprintf(paytostr+4, "%x", addr[0]);
		}
	}
	return paytostr;
}

static char *handle_print_addr(const struct msg *reply) {
	uint32_t payload = reply->payload.ui32_pay;
	char *paytostr = malloc(5);
	snprintf(paytostr, 4, "0x%hx", payload);
	return paytostr;
}

static char *handle_get_instr_at_addr(const struct msg *reply) {
	uint32_t *instr = reply->payload.ptr_pay;
	size_t size = reply->hdr.size/sizeof(uint32_t);
	return disasm(instr, size);
}

static char *dispatch(const struct msg *reply) {
	switch (reply->hdr.type) {
		case TYPE_INSPECT:
			switch (reply->hdr.subtype.inspect) {
				case INSPECT_PRINT_ADDR:
					return handle_print_addr(reply);
					break;
				case INSPECT_GET_CPU_REG:
					return handle_get_cpu_reg(reply);
					break;
				case INSPECT_GET_PPU_REG:
					return handle_get_ppu_reg(reply);
					break;
				case INSPECT_GET_INSTR_AT_ADDR:
					return handle_get_instr_at_addr(reply);
					break;
				default:
					return NULL;
			}
			break;
		default:
			perror("dispatch()");
			return NULL;
	}
}

char *client_dispatch(const struct msg *req, struct msg *reply) {
	if (ipc_send_msg(req) == -1)
		goto out;

	if (reply && ipc_recv_msg(reply) != -1)
		return dispatch(reply);

out:
	return NULL;
}

int client_init() {
	int ret;
	if ((ret = ipc_init(false)) == -1)
		goto out;

out:
	return ret;
}
