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
#include <getopt.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <sys/epoll.h>

#include "backend/backends.h"
#include "monitor.h"
#include "render.h"

FILE *rom;

static pthread_cond_t cond_init = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t mtx_init = PTHREAD_MUTEX_INITIALIZER;
static bool init_success;
static pthread_t epoll_thread;
static void *io_poll(void *v) {
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

	// signal the main thread about init success or failure.
	pthread_mutex_lock(&mtx_init);
	pthread_cond_signal(&cond_init);
	pthread_mutex_unlock(&mtx_init);

	if (init_success) {
		struct epoll_event event_list[10];
		while (1) {
			int num_events;
			if ((num_events = epoll_wait(epoll_fd, event_list, 10, -1)) == -1) {
				perror("epoll_wait()");
				continue;
			}
			for (int i = 0; i < num_events; i++) {
				backends_dispatch(event_list[i].data.fd);
			}
		}
	}
	pthread_exit(NULL);
}

static sigjmp_buf fini;
static void sigterm_handler(int sig) {
	longjmp(fini, 1);
}

int main(int argc, char *argv[]) {
	int ret = 0;
	bool is_server_mode;

	if (argc == 1) {
		return 0;
	}

	int option;
	do {
		option = getopt(argc, argv, "s");
		switch (option) {
			case 's':
				is_server_mode = true;
				break;
			default:
				break;
		}
	} while (option != -1);

	if (optind < argc) {
		if ((rom = fopen(argv[optind], "r")) == NULL) {
			perror("fopen()");
			ret = -1;
			goto err_open;
		}
	}

	ret = render_init();
	if (ret == -1) {
		goto err_render;
	}

	ret = backends_init();
	if (ret == -1) {
		goto err_backends;
	}

	// wait for the poll thread to initialize.
	pthread_mutex_lock(&mtx_init);
	ret = pthread_create(&epoll_thread, NULL, io_poll, NULL);
	if (ret) {
		pthread_mutex_unlock(&mtx_init);
		goto err_pthread;
	}
	pthread_cond_wait(&cond_init, &mtx_init);
	pthread_mutex_unlock(&mtx_init);
	if (!init_success) {
		goto err_pthread;
	}

	ret = monitor_init(is_server_mode);
	if (ret == -1) {
		goto err_monitor;
	}

	// we handle SIGINT so that the user can ctrl+c to quit.
	// setjmp() will return 0, so monitor_run() will be called.
	//
	// if the user hits ctrl+c, sigterm_handler() will be called.
	// that function calls longjmp(), which will transfer control to the point where setjmp()
	// is called, effectively faking a second return from it, but this time the return value
	// would be 1, skipping the monitor_run() call and continuing to the cleanup code.
	struct sigaction sig = { .sa_handler = sigterm_handler };
	sigaction(SIGINT, &sig, NULL);
	if ((ret = setjmp(fini)) == 0) {
		ret = monitor_run();
	}

	monitor_fini();
err_monitor:
	pthread_cancel(epoll_thread);
err_pthread:
	backends_fini();
err_backends:
	render_fini();
err_render:
	fclose(rom);
err_open:
	return ret;
}
