/* RealBoy Emulator
 * Copyright (C) 2025 Sergio Gómez Del Real <sgdr>
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

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "ipc.h"

int socket_fd;

int ipc_send_msg(const struct msg *msg) {
	uint32_t *buf = malloc(msg->hdr.size + HDR_SIZE);
	buf[MAGIC] = 0x67726553;
	buf[TYPE] = msg->hdr.type;
	buf[SUBTYPE] = msg->hdr.type == TYPE_CONTROL_FLOW ?
		msg->hdr.subtype.control_flow :
		msg->hdr.subtype.inspect;
	buf[SIZE] = msg->hdr.size;
	if (msg->hdr.size) {
		if (msg->hdr.size > sizeof(uint32_t))
			memcpy(buf+PAYLOAD, msg->payload.ptr_pay, msg->hdr.size);
		else
			buf[PAYLOAD] = msg->payload.ui32_pay;
	}

	int ret = send(socket_fd, buf, HDR_SIZE + msg->hdr.size, 0);
	free(buf);
	buf = NULL;

	if (ret == -1)
		perror("send");
	return ret;
}

// this function allocates space for the reply payload
// caller is responsible for freeing it
int ipc_recv_msg(struct msg *msg) {
	int ret;
	uint32_t magic;
	ret = recv(socket_fd, &magic, sizeof(uint32_t), 0);
	assert(magic == MAGIC_NUMBER);

	// get type, subtype and size
	uint32_t hdr[3];
	ret = recv(socket_fd, hdr, 3*sizeof(uint32_t), 0);
	if (ret == -1) {
		perror("recv");
		return -1;
	}

	// msg type
	msg->hdr.type = hdr[0];
	// msg subtype
	if (msg->hdr.type == TYPE_CONTROL_FLOW)
		msg->hdr.subtype.control_flow = hdr[1];
	else if (msg->hdr.type == TYPE_INSPECT)
		msg->hdr.subtype.inspect = hdr[1];
	else
		perror("SUBTYPE");

	// payload size
	msg->hdr.size = hdr[2];

	// payload: if the size is only 1 uint32_t, then use the uint32_t type in
	// the payload union; no need to allocate
	if (msg->hdr.size) {
		if (msg->hdr.size > sizeof(uint32_t)) {
			// caller owns
			uint32_t *buf = malloc(msg->hdr.size);
			ret = recv(socket_fd, buf, msg->hdr.size, 0);
			msg->payload.ptr_pay = buf; // msg owns buffer
		}
		else {
			uint32_t buf;
			ret = recv(socket_fd, &buf, msg->hdr.size, 0);
			msg->payload.ui32_pay = buf;
		}
	}

	return ret;
}

void ipc_fini() {
	close(socket_fd);
}

int ipc_init(bool is_server) {
	socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (socket_fd == -1) {
		perror("Unable to create IPC socket");
	}
	if (fcntl(socket_fd, F_SETFD, FD_CLOEXEC) == -1) {
		perror("Unable to set CLOEXEC on IPC socket");
	}

	// socket path
	char *dir = getenv("XDG_RUNTIME_DIR");
	if (!dir) {
		dir = "/tmp";
	}
	struct sockaddr_un ipc_sockaddr = {};
	ipc_sockaddr.sun_family = AF_UNIX;
	snprintf(ipc_sockaddr.sun_path, sizeof(ipc_sockaddr.sun_path), "%s/realboy-.sock", dir);
	//snprintf(ipc_sockaddr.sun_path, sizeof(ipc_sockaddr.sun_path), "%s/realboy-%i.sock", dir, getpid());

	if (is_server) {
		unlink(ipc_sockaddr.sun_path);
		if (bind(socket_fd, (struct sockaddr *)&ipc_sockaddr, sizeof(ipc_sockaddr)) == -1) {
			perror("Unable to bind IPC socket");
		}

		if (listen(socket_fd, 3) == -1) {
			perror("Unable to listen on IPC socket");
		}

		int dbg_sock = accept(socket_fd, NULL, NULL);
		if (!dbg_sock) {
			perror("dbg_sock");
		}
		socket_fd = dbg_sock;
	}
	else {
		if (connect(socket_fd, (struct sockaddr *)&ipc_sockaddr, sizeof(ipc_sockaddr)) == -1) {
			perror("Couldn't connect to socket\n");
		}
	}

	return 0;
}
