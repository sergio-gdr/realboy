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
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>

#include "backends/backends.h"
#include "monitor.h"
#include "iopoll.h"
#include "render.h"
#include "server.h"

FILE *rom;

static sigjmp_buf fini;
static void sigterm_handler(int sig) {
	longjmp(fini, 1);
}

int main(int argc, char *argv[]) {
	int ret = 0;

	if (argc == 1) {
		return 0;
	}

	bool wait_for_client = false;
	int option;
	do {
		option = getopt(argc, argv, "s");
		switch (option) {
			case 's':
				wait_for_client = true;
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

	ret = server_init(wait_for_client);
	if (ret == -1) {
		goto err_server;
	}

	ret = monitor_init();
	if (ret == -1) {
		goto err_monitor;
	}

	// initialize the io poll thread
	ret = iopoll_init();
	if (ret == -1) {
		goto err_io;
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

err_io:
	monitor_fini();
err_monitor:
	server_fini();
err_server:
	backends_fini();
err_backends:
	render_fini();
err_render:
	fclose(rom);
err_open:
	return ret;
}
