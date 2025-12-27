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
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <linux/input.h>
#include <poll.h>
#include <pthread.h>

#include "monitor.h"

#include "cpu.h"
#include "mbc.h"
#include "backend/wayland.h"

#include "ppu.h"

#include "config.h"

#ifdef HAVE_LIBEMU
	#include <libemu.h>
	#include "server.h"
	list_t *breakpoint_list;
	pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;
	bool server_got_request;
	bool monitor_is_executing;
	uint16_t until_addr;
	bool control_flow_continue;
	bool control_flow_until;
	bool control_flow_next;
#endif

static mbc_iface_t *mbc_impl;

// these are accessed by the evdev thread and the main thread.
// protect with a mutex.
static pthread_mutex_t mtx_buttons = PTHREAD_MUTEX_INITIALIZER;
static bool start_released=true;
static bool select_released=true;
static bool a_released=true;
static bool b_released=true;
static bool up_released=true;
static bool down_released=true;
static bool left_released=true;
static bool right_released=true;

static void exec_next() {
	int cycles = cpu_exec_next();
	ppu_refresh(cycles);
}

void monitor_throttle_fps() {
	static uint64_t frame_count_last = 0;
	static struct timespec last;
	static int sleep_factor = 60;

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	if (!frame_count_last) {
		frame_count_last = ppu_get_frame_count();
		last = now;
		return;
	}

	if (now.tv_sec > last.tv_sec) {
		uint64_t curr_frame_count = ppu_get_frame_count();
		uint64_t fps = curr_frame_count - frame_count_last;
		printf("FPS %lu sleep_factor %d\n", fps, sleep_factor);
		if (fps > 60)
			sleep_factor--;
		else if (fps < 60)
			sleep_factor++;
		frame_count_last = curr_frame_count;
		last = now;
	}
	struct timespec spec = { .tv_sec = 0, .tv_nsec = 1000000000/sleep_factor };
	nanosleep(&spec, NULL);
}

uint8_t tmp_ioregs[0xffff];
uint8_t monitor_rd_mem(uint16_t addr) {
	if ((addr >= 0x8000 && addr <= 0x9fff) ||
			(addr >= 0xfe00 && addr <= 0xfe9f) ||
			(addr >= 0xff40 && addr <= 0xff4b)) {
		return ppu_rd(addr);
	}
	if (addr >= 0xc000 && addr <= 0xdfff) {
		return cpu_rd(addr);
	}
	if (addr >= 0xff80 && addr <= 0xfffe) {
		return cpu_rd(addr);
	}
	if (addr <= 0xbfff) {
		return mbc_impl->rd_mem(addr);
	}

	switch (addr) {
		case 0xffff:
		case 0xff0f:
		case 0xff04:
		case 0xff05:
		case 0xff06:
		case 0xff07:
		case 0xff50:
			return cpu_rd(addr);
		default:
			if (addr == 0xff00) {
				if ((tmp_ioregs[addr] & 0x30) == 0x30) {
					return 0x3f;
				}
				else {
					uint8_t ret;
					pthread_mutex_lock(&mtx_buttons);
					if (tmp_ioregs[addr] & 0x10) {
						ret = tmp_ioregs[addr] |
							(start_released<<3|select_released<<2|b_released<<1|a_released);
					}
					else {
						ret = tmp_ioregs[addr] |
							(down_released<<3|up_released<<2|left_released<<1|right_released);
					}
					pthread_mutex_unlock(&mtx_buttons);
					return ret;
				}
			}
			else {
				return tmp_ioregs[addr];
			}
	}
}

void monitor_wr_mem(uint16_t addr, uint8_t value) {
	if (addr == 0xffff) {
		cpu_wr(addr, value);
	}
	else if (addr == 0xff0f) {
		cpu_wr(addr, value);
	}
	else if (addr == 0xff04) {
		cpu_wr(addr, value);
	}
	else if (addr == 0xff05) {
		cpu_wr(addr, value);
	}
	else if (addr == 0xff06) {
		cpu_wr(addr, value);
	}
	else if (addr == 0xff07) {
		cpu_wr(addr, value);
	}
	else if (addr == 0xff50) {
		cpu_wr(addr, value);
		mbc_impl->wr_mem(addr, value);
	}
	else if ((addr >= 0x8000 && addr <= 0x9fff) ||
			(addr >= 0xfe00 && addr <= 0xfe9f) ||
			(addr >= 0xff40 && addr <= 0xff4b)) {
		ppu_wr(addr, value);
	}
	else if (addr >= 0xc000 && addr <= 0xdfff) {
		cpu_wr(addr, value);
	}
	else if (addr >= 0xff80 && addr <=	0xfffe) {
		cpu_wr(addr, value);
	}
	else if (addr <= 0xbfff) {
		mbc_impl->wr_mem(addr, value);
	}
	else {
		if (addr == 0xff00) {
			tmp_ioregs[addr] = value&0x30;
		}
		else
			tmp_ioregs[addr] = value;
	}
}

void monitor_set_key(struct input_event *ev) {
	if (wayland_backend_is_focus()) {
		pthread_mutex_lock(&mtx_buttons);
		switch (ev->code) {
			case KEY_ENTER:
				start_released = !ev->value;
				break;
			case KEY_DOWN:
			case KEY_J:
				down_released = !ev->value;
				break;
			case KEY_SPACE:
				select_released = !ev->value;
				break;
			case KEY_UP:
			case KEY_K:
				up_released = !ev->value;
				break;
			case KEY_S:
				b_released = !ev->value;
				break;
			case KEY_LEFT:
			case KEY_H:
				left_released = !ev->value;
				break;
			case KEY_D:
				a_released = !ev->value;
				break;
			case KEY_RIGHT:
			case KEY_L:
				right_released = !ev->value;
				break;
			default:
				fprintf(stderr, "monitor_set_key()");
		}
		pthread_mutex_unlock(&mtx_buttons);
	}
}

#ifdef HAVE_LIBEMU
void monitor_resume_execution() {
	monitor_is_executing = true;
}

void monitor_stop_execution() {
	monitor_is_executing = false;
}

static bool hit_breakpoint() {
	bool ret = false;

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
	msg_reply.hdr.size = 4;
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
			msg_reply.payload = malloc(sizeof(uint32_t));
			memcpy(msg_reply.payload, &addr, sizeof(uint32_t));
			emu_send_msg(&msg_reply);
			free(msg_reply.payload);
		}
	}

	if (control_flow_next) {
		msg_reply.hdr.subtype.control_flow = CONTROL_FLOW_NEXT;
		control_flow_next = false;
		ret = true;
	}

	if (ret)
		monitor_stop_execution();
	return ret;
}

static void handle_control_flow_next() {
	control_flow_next = true;
	monitor_resume_execution();
}

static void handle_control_flow_until(uint32_t addr) {
	control_flow_until = true;
	until_addr = addr;
	monitor_resume_execution();
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
	monitor_resume_execution();
}

void monitor_set_control_flow(enum msg_type_control_flow control_flow, uint32_t addr) {
	switch (control_flow) {
		case CONTROL_FLOW_UNTIL:
			handle_control_flow_until(addr);
			break;
		case CONTROL_FLOW_BREAK:
			handle_control_flow_break(addr);
			break;
		case CONTROL_FLOW_DELETE:
			handle_control_flow_delete(addr);
			break;
		case CONTROL_FLOW_NEXT:
			handle_control_flow_next();
			break;
		case CONTROL_FLOW_CONTINUE:
			handle_control_flow_continue();
			break;
		default:
			assert(false);
	}
}

static void thread_cleanup(void *arg) {
	if (pthread_rwlock_unlock(&rwlock) != 0)
		perror("pthread_rwlock_wrunlock()");
}

static void *wait_for_server_request(void *val) {
	int client_fd = emu_get_fd();
	struct pollfd client_poll = { .fd = client_fd, .events = POLLIN };

	server_got_request = false;

	if (pthread_rwlock_wrlock(&rwlock) != 0) {
		perror("pthread_rwlock_wrlock()");
		goto err_wrlock;
	}

	pthread_cleanup_push(thread_cleanup, NULL);

	int pipe_lock_acquired = *(int*)val;
	int buf = 1;
	if (write(pipe_lock_acquired, &buf, 1) == -1) {
		perror("write()");
		goto err_write;
	}
	close(pipe_lock_acquired);

	while (1) {
		if (poll(&client_poll, 1, -1) == -1) {
			perror("poll()");
		}
		server_got_request = true;
		break;
	}

	pthread_cleanup_pop(1);
	return NULL;

err_write:
	pthread_rwlock_unlock(&rwlock);
err_wrlock:
	return NULL;
}

static bool got_server_request() {
	if (!pthread_rwlock_tryrdlock(&rwlock)) {
		if (server_got_request) {
			pthread_rwlock_unlock(&rwlock);
			return true;
		}
		pthread_rwlock_unlock(&rwlock);
	}
	return false;
}

static bool should_continue_execution() {
	if (hit_breakpoint()) {
		return false;
	}

	if (got_server_request()) {
		return false;
	}

	return true;
}

static int create_wait_thread(pthread_t *wait_thread) {
	int pipe_lock_acquired[2];
	if (pipe(pipe_lock_acquired) == -1) {
		perror("pipe()");
		goto err_pipe;
	}

	if (pthread_create(wait_thread, NULL, wait_for_server_request, &pipe_lock_acquired[1])) {
		perror("pthread_create()");
		goto err_thread;
	}

	int _dummy;
	if (read(pipe_lock_acquired[0], &_dummy, 1) == -1) {
		perror("read()");
		goto err_read;
	}

	close(pipe_lock_acquired[0]);

	return 0;

err_read:
	pthread_cancel(*wait_thread);
err_thread:
	close(pipe_lock_acquired[0]);
	close(pipe_lock_acquired[1]);
err_pipe:
	return -1;
}

inline static int cancel_wait_thread(const pthread_t *wait_thread) {
	if (pthread_cancel(*wait_thread) != 0) {
		perror("pthread_cancel");
		return -1;
	}
	if (pthread_join(*wait_thread, NULL) != 0) {
		perror("pthread_join");
		return -1;
	}
	return 0;
}
#endif // HAVE_LIBEMU

int monitor_run() {
	while (1) {
#ifdef HAVE_LIBEMU
		if (server_is_client_connected()) {
			if (monitor_is_executing) {
				pthread_t wait_thread;
				if (create_wait_thread(&wait_thread) == -1)
					break;
				do {
					exec_next();
				} while (should_continue_execution());
				if (cancel_wait_thread(&wait_thread) == -1)
					break;
				monitor_stop_execution();
			}
			// we are not executing, so block until a new request from client.
			else {
				server_recv_request_and_dispatch(true);
			}
		}
		else
#endif
			exec_next();
	}
	return -1;
}

void monitor_fini() {
}

int monitor_init(bool wait_for_client) {
	mbc_impl = mbc_init();

	int ret = 0;
#ifdef HAVE_LIBEMU
	breakpoint_list = create_list();
	if (!breakpoint_list) {
		goto out;
	}
	ret = server_init(wait_for_client);
out:
#endif
	return ret;
}
