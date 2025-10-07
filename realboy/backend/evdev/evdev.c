/*
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

#include <dirent.h>
#include <fcntl.h>
#include <libseat.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "backend/evdev.h"

#include "monitor.h"

static int evdev_poll_fd;

void evdev_backend_dispatch() {
	struct epoll_event event_list[10];
	int num_fds;
	num_fds = epoll_wait(evdev_poll_fd, event_list, 10, 0);
	for (int i = 0; i < num_fds; i++) {
		struct input_event ev;
		while (libevdev_next_event(event_list[i].data.ptr, LIBEVDEV_READ_FLAG_NORMAL, &ev) >= 0) {
			if (ev.type == EV_KEY) {
				switch (ev.code) {
					case KEY_ENTER:
					case KEY_SPACE:
					case KEY_D:
					case KEY_S:
					case KEY_LEFT:
					case KEY_RIGHT:
					case KEY_UP:
					case KEY_DOWN:
					case KEY_H:
					case KEY_J:
					case KEY_K:
					case KEY_L:
						monitor_set_key(&ev);
						return;
				}
			}
		}
	}
}

void evdev_backend_fini() {
	close(evdev_poll_fd);
}

int evdev_backend_get_fd() {
	return evdev_poll_fd;
}

int evdev_backend_init() {
	evdev_poll_fd = epoll_create1(0);
	if (!evdev_poll_fd) {
		perror("epoll_create1()");
		return -1;
	}

	int ret = 0;

	const char *dev_input_path = "/dev/input/";
	DIR *input = opendir("/dev/input");
	struct dirent *entry;
	while ((entry = readdir(input))) {
		int device_fd;
		if (strncmp(entry->d_name, "event", strlen("event"))) {
			continue;
		}
		char *event_path = malloc(strlen(dev_input_path)+strlen(entry->d_name)+1);
		strcpy(event_path, dev_input_path);
		strcat(event_path, entry->d_name);
		if ( (device_fd = open(event_path, O_RDONLY)) == -1) {
			free(event_path);
			perror("libseat_open_device()");
			continue;
		}
		free(event_path);
		struct libevdev *dev;
		if (libevdev_new_from_fd(device_fd, &dev) != 0) {
			perror("libevdev_new_from_fd()");
			return -1;
		}
		bool has_evkey = libevdev_has_event_type(dev, EV_KEY);
		if (has_evkey) {
			bool has_a = libevdev_has_event_code(dev, EV_KEY, KEY_A);
			if (has_a) {
				if (epoll_ctl(evdev_poll_fd, EPOLL_CTL_ADD, device_fd,
							  &(struct epoll_event) {
							  .events = EPOLLIN,
							  .data.ptr = dev
							  }) == -1) {
					ret = -1;
					perror("epoll_ctl");
					goto out;
				}
				ret = 1;
			}
		}
	}
out:
	return ret;
}
