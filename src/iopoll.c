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
#include <pthread.h>
#include <stdio.h>
#include <sys/epoll.h>

#include "server.h"
#include "backends/backends.h"

static pthread_t epoll_thread;

static pthread_mutex_t mtx_init = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond_init = PTHREAD_COND_INITIALIZER;
static bool init_success; // protected by the above
static void *iopoll_thread(void *v) {
	pthread_mutex_lock(&mtx_init);

	int epoll_fd = epoll_create1(0);
	init_success = true;
	int *fds;

	int num_fds = backends_get_fds(&fds);
	for (int i = 0; i < num_fds; i++) {
		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fds[i],
				&(struct epoll_event) {
				.events = EPOLLIN,
				.data.fd = fds[i]
			}) == -1) {
			perror("epoll_ctl()");
			init_success = false;
			break;
		}
	}

	int server_fd = server_get_fd();
	if (server_fd) {
		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd,
				&(struct epoll_event) {
				.events = EPOLLIN,
				.data.fd = server_fd
			}) == -1) {
			perror("epoll_ctl()");
			init_success = false;
		}
	}

	// signal the main thread about init success or failure.
	pthread_mutex_unlock(&mtx_init);
	pthread_cond_signal(&cond_init);

	if (init_success) {
		struct epoll_event event_list[10];
		while (1) {
			int num_events;
			if ((num_events = epoll_wait(epoll_fd, event_list, 10, -1)) == -1) {
				perror("epoll_wait()");
				continue;
			}
			for (int i = 0; i < num_events; i++) {
				if (server_fd == event_list[i].data.fd) {
					server_recv_request_and_dispatch();
				}
				else {
					backends_dispatch(event_list[i].data.fd);
				}
			}
		}
	}
	pthread_exit(NULL);
}


int iopoll_init() {
	pthread_mutex_lock(&mtx_init);

	if (pthread_create(&epoll_thread, NULL, iopoll_thread, NULL)) {
		pthread_mutex_unlock(&mtx_init);
		return -1;
	}

	pthread_cond_wait(&cond_init, &mtx_init);
	pthread_mutex_unlock(&mtx_init);
	if (!init_success) {
		return -1;
	}

	return 0;
}
