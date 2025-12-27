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

#include <getopt.h>
#include <stdio.h>

FILE *rom;

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

err1:
	return ret;
}
