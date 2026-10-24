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
#include <string.h>

#include "peek.h"

#include "cpu/cpu.h"
#include "ppu.h"

extern const uint8_t op_len[256];

void ppu_peek(const struct peek *peek, struct peek_reply *reply) {
	switch (peek->subtype) {
		case PPU_PEEK_REG:
			reply->size = sizeof(uint32_t);
			reply->payload = malloc(reply->size); // caller frees
			uint32_t ppu_reg = peek_get_ppu_reg(peek->req);
			memcpy(reply->payload, &ppu_reg, sizeof(uint32_t));
			break;
		default:
			fprintf(stderr, "error: ppu_peek()");
	}
}

void cpu_peek(const struct peek *peek, struct peek_reply *reply) {
	switch (peek->subtype) {
		case CPU_PEEK_REG:
			reply->size = sizeof(uint32_t);
			reply->payload = malloc(reply->size); // caller frees
			uint32_t cpu_reg = peek_get_cpu_reg(peek->req);
			memcpy(reply->payload, &cpu_reg, reply->size);
			break;
		case CPU_PEEK_ADDR:
			reply->size = sizeof(uint32_t);
			reply->payload = malloc(reply->size); // caller frees
			uint32_t addr = cpu_rd_mem(peek->req);
			memcpy(reply->payload, &addr, reply->size);
			break;
		case CPU_PEEK_INSTR_AT_ADDR:
			{
				uint32_t opcode = cpu_rd_mem(peek->req);
				reply->size = opcode == OPCODE_PREFIX ? 2 : op_len[opcode];
				reply->size *= sizeof(uint32_t);
				reply->payload = malloc(reply->size); // caller frees
				memcpy(reply->payload, &opcode, sizeof(uint32_t));
				for (size_t i=1; i < (reply->size)/sizeof(uint32_t); i++) {
					uint32_t req = cpu_rd_mem(peek->req+i);
					memcpy((uint32_t*)reply->payload+i, &req, sizeof(uint32_t));
				}
			}
			break;
		case CPU_PEEK_OP_LEN:
			reply->size = sizeof(uint32_t);
			reply->payload = malloc(reply->size); // caller frees
			uint32_t len = op_len[peek->req];
			memcpy(reply->payload, &len, sizeof(uint32_t));
			break;
		default:
			fprintf(stderr, "error: cpu_peek()");
	}
}

