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

#ifndef RB_SERVER_H
#define RB_SERVER_H

#include <stddef.h>
#include <stdint.h>

enum server_msg {
	SERVER_MSG_BREAK,
	SERVER_MSG_UNTIL
};

typedef struct {
	size_t size;
	void *payload;
} server_reply_t;

typedef struct {
	void (*dispatch_control_flow_break)(uint32_t addr);
	void (*dispatch_control_flow_continue)();
	void (*dispatch_control_flow_delete)(uint32_t addr);
	void (*dispatch_control_flow_next)();
	void (*dispatch_control_flow_until)(uint32_t addr);
	void (*dispatch_monitor_resume)();
	void (*dispatch_monitor_stop)();
	server_reply_t (*dispatch_inspect_addr)(uint32_t addr);
	server_reply_t (*dispatch_inspect_cpu)(uint32_t reg);
	server_reply_t (*dispatch_inspect_ppu)(uint32_t reg);
	server_reply_t (*dispatch_inspect_instr_at_addr)(uint32_t addr);
	server_reply_t (*dispatch_inspect_op_len)(uint32_t addr);
} server_dispatch_impl_t;

int server_recv_request_and_dispatch();
int server_init(bool wait_for_client);
void server_fini();
int server_get_fd();
bool server_is_client_connected();
bool server_is_stopped();
bool server_should_stop_execution();
void server_set_dispatch_table(const server_dispatch_impl_t *table);
int server_send_msg(enum server_msg, uint32_t addr);

#endif
