/**
 *
 * @file logo.h
 *
 * Part of the OpenJazz project
 *
 * @par Licence:
 * Copyright (c) 2005-2017 AJ Thomson
 * Copyright (c) 2015-2023 Carsten Teibes
 *
 * OpenJazz is distributed under the terms of
 * the GNU General Public License, version 2.0
 *
 */


#ifndef _LOGO_H
#define _LOGO_H

/* This struct contains an image as zlib compressed deflate block with
   additional information about the image */

struct compressed_image {
	unsigned int width;
	unsigned int height;
	unsigned int size;
	unsigned int compressed_size;
	unsigned char *data;
};

// Former openjazz.000, without RLE compression - raw image (without palette)

unsigned char oj_logo_data[] = {
	0x78, 0xda, 0xed, 0xc1, 0x81, 0x00, 0x00, 0x00, 0x00, 0xc3, 0x20, 0x91,
	0xf9, 0x73, 0x1e, 0xe4, 0x55, 0x01, 0x00, 0xf0, 0x64, 0xe8, 0x08, 0x18,
	0x10
};

struct compressed_image oj_logo = { 64, 40, 2560, 25, oj_logo_data };

#endif
