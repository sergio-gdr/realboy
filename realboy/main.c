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

#include "backend/evdev.h"
#include "backend/wayland.h"
#include "monitor.h"
#include "render.h"

FILE *rom;

static sigjmp_buf fini;

static pthread_cond_t cond_init = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t mtx_init = PTHREAD_MUTEX_INITIALIZER;
static bool init_success;
static pthread_t epoll_thread;

// populate at compile time.
// the __attribute__(section) attribute tells the compiler to create a contiguous
// area in the section "fds_to_poll" (file descriptors to poll), and a pair of symbols
// (__start* and __stop*) so we can traverse the fds.
#define POLL_ADD_SOURCE(name) int __attribute__((section("fds_to_poll"))) name##_fd;
POLL_ADD_SOURCE(wayland)
POLL_ADD_SOURCE(evdev)
extern int __start_fds_to_poll, __stop_fds_to_poll;

static void *io_poll(void *val) {
	int epoll_fd = epoll_create1(0);
	init_success = true;

	for (int *fd_to_poll = &__start_fds_to_poll; fd_to_poll < &__stop_fds_to_poll; fd_to_poll++) {
		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, *fd_to_poll,
				&(struct epoll_event) {
				.events = EPOLLIN,
				.data.fd = *fd_to_poll
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
			}
		}
	}
	pthread_exit(NULL);
}

static void sigterm_handler(int sig) {
	longjmp(fini, 1);
}

int main(int argc, char *argv[]) {
	int ret = 0;

	if (argc == 1) {
		return 0;
	}

	if (optind < argc) {
		if ((rom = fopen(argv[optind], "r")) == NULL) {
			perror("fopen()");
			ret = -1;
			goto err1;
		}
	}

	ret = evdev_backend_init();
	if (ret == -1) {
		goto err2;
	}
	evdev_fd = evdev_backend_get_fd();

	if ( (ret = wayland_backend_init()) == -1) {
		goto err3;
	}
	wayland_fd = wayland_backend_get_fd();

	ret = render_init();
	if (ret == -1) {
		goto err4;
	}

	// wait for the poll thread to initialize.
	pthread_mutex_lock(&mtx_init);
	ret = pthread_create(&epoll_thread, NULL, io_poll, NULL);
	if (ret) {
		pthread_mutex_unlock(&mtx_init);
		goto err5;
	}
	pthread_cond_wait(&cond_init, &mtx_init);
	pthread_mutex_unlock(&mtx_init);
	if (!init_success) {
		goto err5;
	}

	ret = monitor_init();
	if (ret == -1) {
		goto err6;
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
err6:
	pthread_cancel(epoll_thread);
err5:
	render_fini();
err4:
	wayland_backend_fini();
err3:
	evdev_backend_fini();
err2:
	fclose(rom);
err1:
	return ret;
}
