/* RealBoy Emulator
 * Copyright (C) 2013-2025 Sergio Gómez Del Real <sgdr>
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

#define _GNU_SOURCE
#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "conf.h"

int conf_init() {
	char *conf_path;
	char *conf_dir_name;

	// get configuration path and directory name
	if (getenv("XDG_CONFIG_HOME")) {
		conf_path = getenv("XDG_CONFIG_HOME");
		conf_dir_name = "realboy";
	}
	else {
		conf_path = getenv("HOME");
		conf_dir_name = ".realboy";
	}
	assert(conf_path);

	// chdir to the configuration path
	if (chdir(conf_path) == -1) {
		perror("chdir");
		goto err;
	}

	// search for realboy configure directory
	DIR *conf_dir = opendir(conf_path);
	if (!conf_dir) {
		perror("opendir");
		goto err;
	}
	struct dirent *conf_ent;
	while ((conf_ent = readdir(conf_dir)) != NULL) {
		if (conf_ent->d_type == DT_DIR &&
				!(strncmp(conf_ent->d_name, conf_dir_name, strlen(conf_dir_name)))) {
			break;
		}
	}
	closedir(conf_dir);

	// create configuration directory if it doesn't exist
	if (!conf_ent) {
		if ( (mkdir(conf_dir_name, (mode_t)0755)) == -1) {
			perror("mkdir");
			goto err;
		}
	}

	return 0;

err:
	return -1;
}
