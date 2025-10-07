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
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "backend/evdev.h"

#include "monitor.h"

#define MAX_EVDEVS 10
static struct libevdev *evdevs[MAX_EVDEVS];
static int num_evdevs;

static int evdev_poll_fd;

void evdev_backend_dispatch() {
	struct epoll_event event_list[10];
	int num_fds;
	num_fds = epoll_wait(evdev_poll_fd, event_list, 10, 0);
	for (int i = 0; i < num_fds; i++) {
		struct input_event ev;
		int ret;
		while ( (ret = libevdev_next_event(event_list[i].data.ptr, LIBEVDEV_READ_FLAG_NORMAL, &ev)) >= 0) {
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
	for (int i = 0; i < num_evdevs; i++) {
		libevdev_free(evdevs[i]);
	}
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
	while ((entry = readdir(input)) && num_evdevs < MAX_EVDEVS) {
		if (strncmp(entry->d_name, "event", strlen("event"))) {
			continue;
		}
		char *event_path = malloc(strlen(dev_input_path)+strlen(entry->d_name)+1);
		strcpy(event_path, dev_input_path);
		strcat(event_path, entry->d_name);

		int device_fd;
		if ( (device_fd = open(event_path, O_RDONLY|O_NONBLOCK)) == -1) {
			free(event_path);
			perror("open()");
			continue;
		}
		free(event_path);

		struct libevdev *evdev;
		if (libevdev_new_from_fd(device_fd, &evdev) != 0) {
			perror("libevdev_new_from_fd()");
			return -1;
		}
		bool has_evkey = libevdev_has_event_type(evdev, EV_KEY);
		bool has_enter = libevdev_has_event_code(evdev, EV_KEY, KEY_ENTER);
		if (has_evkey && has_enter) {
			if (epoll_ctl(evdev_poll_fd, EPOLL_CTL_ADD, device_fd,
						  &(struct epoll_event) {
						  .events = EPOLLIN,
						  .data.ptr = evdev
						  }) == -1) {
				perror("epoll_ctl()");
				ret = -1;
				goto out;
			}
			printf("Input device name: \"%s\"\n", libevdev_get_name(evdev));
			printf("Input device ID: bus %#x vendor %#x product %#x\n",
				   libevdev_get_id_bustype(evdev),
				   libevdev_get_id_vendor(evdev),
				   libevdev_get_id_product(evdev));
			evdevs[num_evdevs++] = evdev;
		}
		else {
			libevdev_free(evdev);
			close(device_fd);
		}
	}
out:
	closedir(input);
	return ret;
}
