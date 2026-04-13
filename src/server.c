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

#define _POSIX_C_SOURCE 200809L

#include "server.h"

#include "config.h"

#ifdef HAVE_LIBEMU
#include <libemu.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cpu.h"
#include "ppu.h"
#endif

static list_t *breakpoint_list;
static uint16_t until_addr;
static bool control_flow_continue;
static bool control_flow_until;
static bool control_flow_next;

static bool client_connected;
static bool server_running;
static bool server_stop_request;

pthread_mutex_t mtx_server = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cnd_server_stop = PTHREAD_COND_INITIALIZER;
pthread_rwlock_t quick_lock = PTHREAD_RWLOCK_INITIALIZER;

static void resume_execution() {
	server_running = true;
}

static void stop_execution() {
	server_running = false;
}

static void handle_monitor_resume() {
	resume_execution();
}

static void handle_monitor_stop() {
	stop_execution();
}

static void handle_control_flow_until(uint32_t addr) {
	control_flow_until = true;
	until_addr = addr;
	resume_execution();
}

static void handle_control_flow_next() {
	control_flow_next = true;
	resume_execution();
}

static void handle_control_flow_break(uint32_t addr) {
	list_add(breakpoint_list, (uintptr_t)addr);
}

static void handle_control_flow_delete(uint32_t addr) {
	uint32_t breakpoint;
	list_for_each(breakpoint_list, breakpoint) {
		if (breakpoint == addr || addr == 0) {
			list_remove(breakpoint_list, breakpoint);
			break;
		}
	}
}

static void handle_control_flow_continue() {
	control_flow_continue = true;
	resume_execution();
}

#ifdef HAVE_LIBEMU
static void dispatch_monitor(struct msg *req) {
	switch (req->hdr.subtype.monitor) {
		case MONITOR_STOP:
			handle_monitor_stop();
			break;
		case MONITOR_RESUME:
			handle_monitor_resume();
			break;
	}
}

static void dispatch_control_flow(struct msg *req) {
	switch (req->hdr.subtype.control_flow) {
		case CONTROL_FLOW_UNTIL:
			uint32_t until_addr = *(uint32_t*)req->payload;
			handle_control_flow_until(until_addr);
			break;
		case CONTROL_FLOW_DELETE:
			if (!req->hdr.size) {
				handle_control_flow_delete(0);
			}
			else {
				handle_control_flow_delete(*(uint32_t*)req->payload);
			}
			break;
		case CONTROL_FLOW_CONTINUE:
			handle_control_flow_continue();
			break;
		case CONTROL_FLOW_NEXT:
			handle_control_flow_next();
			break;
		case CONTROL_FLOW_BREAK:
			handle_control_flow_break(*(uint32_t*)req->payload);
			break;
		default:
			fprintf(stderr, "error: control_flow_get_answer()");
			return;
	}
}

static void dispatch_inspect(struct msg *req, struct msg *answer) {
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
#endif

static int set_stop_request() {
	if (pthread_rwlock_wrlock(&quick_lock) != 0) {
		perror("pthread_rwlock_wrlock()");
		return -1;
	}
	server_stop_request = true;

	pthread_rwlock_unlock(&quick_lock);

	return 0;
}

int server_recv_request_and_dispatch() {
#ifdef HAVE_LIBEMU
	pthread_mutex_lock(&mtx_server);

	struct msg req = {};
	int ret;
	// if successful, allocates request payload
	if ((ret = emu_recv_msg(&req, 0)) == -1)
		goto out;

	switch (req.hdr.type) {
		case TYPE_CONTROL_FLOW:
			if (server_running) {
				fprintf(stderr, "received TYPE_CONTROL_FLOW request while running\n");
				goto out;
			}
			dispatch_control_flow(&req);
			pthread_cond_signal(&cnd_server_stop);
			break;
		case TYPE_INSPECT:
			if (server_running) {
				fprintf(stderr, "received TYPE_INSPECT request while running\n");
				goto out;
			}
			struct msg answer = {};
			answer.hdr.type = TYPE_INSPECT;
			dispatch_inspect(&req, &answer); // allocates reply payload
			ret = emu_send_msg(&answer);
			free((void *)answer.payload); // frees reply payload
			break;
		case TYPE_MONITOR:
			if (server_running && req.hdr.subtype.monitor == MONITOR_RESUME) {
				fprintf(stderr, "received TYPE_MONITOR MONITOR_RESUME request while running\n");
				goto out;
			}
			if (!server_running && req.hdr.subtype.monitor == MONITOR_STOP) {
				fprintf(stderr, "received TYPE_MONITOR MONITOR_STOP request while not running\n");
				goto out;
			}
			dispatch_monitor(&req);
			if (req.hdr.subtype.monitor == MONITOR_STOP) {
				if (set_stop_request() == -1) {
					goto out;
				}
				pthread_cond_wait(&cnd_server_stop, &mtx_server);
			}
			break;
	}

out:
	// we own the possible allocation from libemu
	if (req.hdr.size)
		free(req.payload);
	pthread_mutex_unlock(&mtx_server);
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

bool server_is_stopped() {
	return !server_running;
}

static bool hit_breakpoint() {
	bool ret = false;
#ifdef HAVE_LIBEMU
	struct peek peek = {};
	struct peek_reply peek_reply = {};
	peek.type = PEEK_CPU;
	peek.subtype = CPU_PEEK_REG;
	peek.req = CPU_REG_PC;
	cpu_peek(&peek, &peek_reply);
	uint32_t addr = *(uint32_t*)peek_reply.payload;
	free(peek_reply.payload);

	struct msg msg_reply;
	msg_reply.hdr.type = TYPE_CONTROL_FLOW;
	if (control_flow_continue || control_flow_until) {
		uint32_t breakpoint;
		list_for_each(breakpoint_list, breakpoint) {
			if (breakpoint == addr) {
				msg_reply.hdr.subtype.control_flow = CONTROL_FLOW_BREAK;
				control_flow_continue = 0;
				ret = true;
				break;
			}
		}

		if (control_flow_until) {
			if (until_addr == addr) {
				msg_reply.hdr.subtype.control_flow = CONTROL_FLOW_UNTIL;
				control_flow_until = false;
				control_flow_continue = false;
				until_addr = 0;
				ret = true;
			}
		}
		if (ret) {
			msg_reply.hdr.size = 4;
			msg_reply.payload = malloc(sizeof(uint32_t));
			memcpy(msg_reply.payload, &addr, sizeof(uint32_t));
			emu_send_msg(&msg_reply);
			free(msg_reply.payload);
		}
	}

	if (control_flow_next) {
		msg_reply.hdr.size = 0;
		msg_reply.hdr.subtype.control_flow = CONTROL_FLOW_NEXT;
		emu_send_msg(&msg_reply);
		control_flow_next = false;
		ret = true;
	}

	if (ret)
		stop_execution();
#endif
	return ret;
}

bool got_stop_request() {
	if (!pthread_rwlock_tryrdlock(&quick_lock)) {
		if (server_stop_request) {
			server_stop_request = false;
			pthread_rwlock_unlock(&quick_lock);
			return true;
		}
		pthread_rwlock_unlock(&quick_lock);
	}
	return false;
}

bool server_should_stop_execution() {
	return got_stop_request() || hit_breakpoint();
}

void server_fini() {
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
		server_running = false;
	}
	else {
		server_running = true;
	}

	breakpoint_list = create_list();
	if (!breakpoint_list) {
		return -1;
	}

	return 0;
#else
	server_running = false;
	return 0;
#endif
}
