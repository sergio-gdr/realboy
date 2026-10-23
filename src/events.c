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
#include <unistd.h>
#include <sys/epoll.h>

#include "backends.h"
#include "server.h"

static bool events_initialized;

static int epoll_fd;
static int server_fd;

void events_process(bool block) {
	struct epoll_event event_list[10];
	int num_events;
	if ((num_events = epoll_wait(epoll_fd, event_list, 10, block ? -1 : 0)) == -1) {
		perror("epoll_wait()");
		return;
	}

	for (int i = 0; i < num_events; i++) {
		if (server_fd && server_fd == event_list[i].data.fd) {
			server_recv_request_and_dispatch();
		}
		else {
			backends_dispatch(event_list[i].data.fd);
		}
	}
}

void events_fini() {
	if (!events_initialized)
		return;
}

int events_init() {
	epoll_fd = epoll_create1(0);
	if (epoll_fd == -1) {
		perror("epoll_create1\n");
		return -1;
	}

	int *fds;
	int num_fds = backends_get_fds(&fds);
	for (int i = 0; i < num_fds; i++) {
		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fds[i],
				&(struct epoll_event) {
				.events = EPOLLIN,
				.data.fd = fds[i]
			}) == -1) {
			perror("events_init(): epoll_ctl");
		}
	}

	server_fd = server_get_fd();
	if (server_fd) {
		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd,
				&(struct epoll_event) {
				.events = EPOLLIN,
				.data.fd = server_fd
			}) == -1) {
			close(epoll_fd);
			perror("events_init(): epoll_ctl");
			return -1;
		}
	}

	events_initialized = true;
	return 0;
}
