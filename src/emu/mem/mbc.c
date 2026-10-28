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

#include <stdint.h>
#include <stdio.h>

#include "mbc.h"

#include "mbc1.h"

#define CART_TITLE_OFFSET 0x34
#define CART_CGBFLAG_OFFSET 0x43
#define CART_SGBFLAG_OFFSET 0x46
#define CART_TYPE_OFFSET 0x47
#define CART_ROMSIZE_OFFSET 0x48
#define CART_RAMSIZE_OFFSET 0x49

static mbc_iface_t *mbc_impl;

const char *cart_type[] = {
	"ROM ONLY",
	"MBC1",
	"MBC1+RAM",
	"MBC1+RAM+BATTERY",
	"invalid",
	"MBC2",
	"MBC2+BATTERY",
	"invalid",
	"ROM+RAM 11",
	"ROM+RAM+BATTERY 11",
	"invalid",
	"MMM01",
	"MMM01+RAM",
	"MMM01+RAM+BATTERY",
	"invalid",
	"MBC3+TIMER+BATTERY",
	"MBC3+TIMER+RAM+BATTERY 12",
	"MBC3",
	"MBC3+RAM 12",
	"MBC3+RAM+BATTERY 12",
	"invalid",
	"invalid",
	"invalid",
	"invalid",
	"invalid",
	"invalid",
	"MBC5",
	"MBC5+RAM",
	"MBC5+RAM+BATTERY",
	"MBC5+RUMBLE",
	"MBC5+RUMBLE+RAM",
	"MBC5+RUMBLE+RAM+BATTERY",
	"invalid",
	"MBC6",
	"invalid",
	"MBC7+SENSOR+RUMBLE+RAM+BATTERY",
};

static inline void print_cart_info(const uint8_t *cart_hdr) {
	printf("Title: %s\n", &cart_hdr[CART_TITLE_OFFSET]);
	printf("CGB Support: %s\n", cart_hdr[CART_CGBFLAG_OFFSET] & 0x80 ? "Yes" : "No");
	printf("SGB Support: %s\n", cart_hdr[CART_SGBFLAG_OFFSET] == 3 ? "Yes" : "No");
	printf("Cart Type: %s\n", cart_type[cart_hdr[CART_TYPE_OFFSET]]);
	printf("ROM Size: ");
	switch (cart_hdr[CART_ROMSIZE_OFFSET]) {
		case 0:
			printf("32KiB\n");
			break;
		case 1:
			printf("64KiB\n");
			break;
		case 2:
			printf("128KiB\n");
			break;
		case 3:
			printf("256KiB\n");
			break;
		case 4:
			printf("512KiB\n");
			break;
		case 5:
			printf("1MiB\n");
			break;
		case 6:
			printf("2MiB\n");
			break;
		case 7:
			printf("4MiB\n");
			break;
		case 8:
			printf("8MiB\n");
			break;
		default:
			printf("Unknown\n");
	}
	printf("RAM Size: ");
	switch (cart_hdr[CART_RAMSIZE_OFFSET]) {
		case 0:
			printf("No RAM\n");
			break;
		case 2:
			printf("8KiB\n");
			break;
		case 3:
			printf("32KiB\n");
			break;
		case 4:
			printf("128KiB\n");
			break;
		case 5:
			printf("64KiB\n");
			break;
		default:
			printf("Unknown\n");
	}

}

mbc_iface_t *mbc_init() {
	mbc_impl = mbc1_init();

	uint8_t *cart_hdr = mbc_impl->get_cart_hdr();
	print_cart_info(cart_hdr);

	return mbc_impl;
}
