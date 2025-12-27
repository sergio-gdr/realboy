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

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "backend/evdev.h"

static const char *dev_input_path = "/dev/input/";

#define MAX_EVDEVS 10
static struct libevdev *evdevs[MAX_EVDEVS];
static int num_evdevs;

static int evdev_poll_fd;

void evdev_backend_fini() {
	for (int i = 0; i < num_evdevs; i++) {
		libevdev_free(evdevs[i]);
	}
	close(evdev_poll_fd);
}

int evdev_backend_init() {
	evdev_poll_fd = epoll_create1(0);
	if (!evdev_poll_fd) {
		perror("epoll_create1()");
		return -1;
	}

	DIR *input = opendir(dev_input_path);
	if (!input) {
		perror("opendir()");
		return -1;
	}

	struct dirent *entry;
	while ((entry = readdir(input)) && num_evdevs < MAX_EVDEVS) {
		if (strncmp(entry->d_name, "event", strlen("event"))) {
			continue;
		}

		char *event_path = malloc(strlen(dev_input_path)+strlen(entry->d_name)+1);
		if (!event_path) {
			perror("malloc()");
			goto err;
		}
		strcpy(event_path, dev_input_path);
		strcat(event_path, entry->d_name);

		int device_fd;
		if ( (device_fd = open(event_path, O_RDONLY|O_NONBLOCK)) == -1) {
			free(event_path);
			perror("open()");
			goto err;
		}
		free(event_path);

		struct libevdev *evdev;
		if (libevdev_new_from_fd(device_fd, &evdev) != 0) {
			perror("libevdev_new_from_fd()");
			goto err;
		}
		if (libevdev_has_event_type(evdev, EV_KEY) &&
				libevdev_has_event_code(evdev, EV_KEY, KEY_ENTER)) {
			if (epoll_ctl(evdev_poll_fd, EPOLL_CTL_ADD, device_fd,
					&(struct epoll_event) {
					.events = EPOLLIN,
					.data.ptr = evdev
					}) == -1) {
				perror("epoll_ctl()");
				goto err;
			}
			evdevs[num_evdevs++] = evdev;
		}
		else {
			libevdev_free(evdev);
			close(device_fd);
		}
	}
	closedir(input);

	return 0;

err:
	for (int i = 0; i < num_evdevs; i++) {
		libevdev_free(evdevs[i]);
	}
	closedir(input);
	close(evdev_poll_fd);
	return -1;
}
