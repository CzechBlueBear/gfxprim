/*****************************************************************************
 * This file is part of gfxprim library.                                     *
 *                                                                           *
 * Gfxprim is free software; you can redistribute it and/or                  *
 * modify it under the terms of the GNU Lesser General Public                *
 * License as published by the Free Software Foundation; either              *
 * version 2.1 of the License, or (at your option) any later version.        *
 *                                                                           *
 * Gfxprim is distributed in the hope that it will be useful,                *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU         *
 * Lesser General Public License for more details.                           *
 *                                                                           *
 * You should have received a copy of the GNU Lesser General Public          *
 * License along with gfxprim; if not, write to the Free Software            *
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,                        *
 * Boston, MA  02110-1301  USA                                               *
 *                                                                           *
 * Copyright (C) 2009-2013 Cyril Hrubis <metan@ucw.cz>                       *
 *                                                                           *
 *****************************************************************************/

#include "GP_Font.h"

static int8_t tiny_glyphs[] = {
	/* ' ' */ 	4, 5, 0, 5, 4,
	         	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* '!' */ 	1, 5, 1, 5, 3,
	         	0x80, 0x80, 0x80, 0x00, 0x80, 0x00, 0x00,
	/* '"' */	4, 5, 0, 5, 6,
	         	0x50, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* '#' */	5, 5, 0, 5, 6,
	        	0x50, 0xf8, 0x50, 0xf8, 0x50, 0x00, 0x00,
	/* '$' */ 	3, 5, 0, 5, 4,
	         	0x40, 0xe0, 0x40, 0xe0, 0x40, 0x00, 0x00,
	/* '%' */  	5, 5, 0, 5, 6,
	         	0xd0, 0x90, 0x20, 0x48, 0x58, 0x00, 0x00,
	/* '&' */  	4, 5, 0, 5, 5,
	         	0x60, 0x90, 0x40, 0xa0, 0xd0, 0x00, 0x00,
	/* ''' */  	1, 5, 0, 5, 2,
	         	0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* '(' */  	2, 5, 0, 5, 3,
	         	0x40, 0x80, 0x80, 0x80, 0x40, 0x00, 0x00,
	/* ')' */  	2, 5, 0, 5, 3,
	         	0x80, 0x40, 0x40, 0x40, 0x80, 0x00, 0x00,
	/* '*' */  	3, 5, 0, 5, 4,
	         	0x00, 0xa0, 0x40, 0xa0, 0x00, 0x00, 0x00,
	/* '+' */  	3, 5, 0, 5, 4,
	         	0x00, 0x40, 0xe0, 0x40, 0x00, 0x00, 0x00,
	/* ',' */  	1, 6, 0, 5, 2,
	         	0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x00,
	/* '-' */  	3, 5, 0, 5, 4,
	         	0x00, 0x00, 0xe0, 0x00, 0x00, 0x00, 0x00,
	/* '.' */  	1, 5, 0, 5, 2,
	         	0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00,
	/* '/' */  	3, 5, 0, 5, 4,
	         	0x20, 0x20, 0x40, 0x80, 0x80, 0x00, 0x00,
	/* '0' */  	4, 5, 0, 5, 5,
	         	0x60, 0xb0, 0x90, 0xd0, 0x60, 0x00, 0x00,
	/* '1' */  	3, 5, 0, 5, 4,
	         	0xc0, 0x40, 0x40, 0x40, 0xe0, 0x00, 0x00,
	/* '2' */  	4, 5, 0, 5, 5,
	         	0xe0, 0x10, 0x60, 0x80, 0xf0, 0x00, 0x00,
	/* '3' */  	4, 5, 0, 5, 5,
	         	0xe0, 0x10, 0x60, 0x10, 0xe0, 0x00, 0x00,
	/* '4' */  	4, 5, 0, 5, 5,
	         	0x20, 0x60, 0xa0, 0xf0, 0x20, 0x00, 0x00,
	/* '5' */  	4, 5, 0, 5, 5,
	         	0xf0, 0x80, 0xe0, 0x10, 0xe0, 0x00, 0x00,
	/* '6' */  	4, 5, 0, 5, 5,
	         	0x60, 0x80, 0xe0, 0x90, 0x60, 0x00, 0x00,
	/* '7' */ 	4, 5, 0, 5, 5,
	         	0xf0, 0x10, 0x20, 0x40, 0x40, 0x00, 0x00,
	/* '8' */  	4, 5, 0, 5, 5,
	         	0x60, 0x90, 0x60, 0x90, 0x60, 0x00, 0x00,
	/* '9' */  	4, 5, 0, 5, 5,
	                0x60, 0x90, 0x70, 0x10, 0x60, 0x00, 0x00,
	/* ':' */  	1, 5, 0, 5, 2,
	         	0x00, 0x80, 0x00, 0x80, 0x00, 0x00, 0x00,
	/* ';' */  	1, 5, 0, 5, 2,
	         	0x00, 0x80, 0x00, 0x80, 0x80, 0x00, 0x00,
	/* '<' */  	3, 5, 0, 5, 4,
	         	0x20, 0x40, 0x80, 0x40, 0x20, 0x00, 0x00,
	/* '=' */  	3, 5, 0, 5, 4,
	         	0x00, 0xe0, 0x00, 0xe0, 0x00, 0x00, 0x00,
	/* '>' */  	3, 5, 0, 5, 4,
	         	0x80, 0x40, 0x20, 0x40, 0x80, 0x00, 0x00,
	/* '?' */  	3, 5, 0, 5, 4,
	         	0xc0, 0x20, 0x40, 0x00, 0x40, 0x00, 0x00,
	/* '@' */  	5, 5, 0, 5, 6,
	         	0x70, 0x88, 0xb0, 0x80, 0x70, 0x00, 0x00,
	/* 'A' */  	4, 5, 0, 5, 5,
	         	0x60, 0x90, 0xf0, 0x90, 0x90, 0x00, 0x00,
	/* 'B' */  	4, 5, 0, 5, 5,
	         	0xe0, 0x90, 0xe0, 0x90, 0xe0, 0x00, 0x00,
	/* 'C' */  	4, 5, 0, 5, 5,
	         	0x70, 0x80, 0x80, 0x80, 0x70, 0x00, 0x00,
	/* 'D' */  	4, 5, 0, 5, 5,
	         	0xe0, 0x90, 0x90, 0x90, 0xe0, 0x00, 0x00,
	/* 'E' */  	4, 5, 0, 5, 5,
	         	0xf0, 0x80, 0xe0, 0x80, 0xf0, 0x00, 0x00,
	/* 'F' */  	4, 5, 0, 5, 5,
	         	0xf0, 0x80, 0xe0, 0x80, 0x80, 0x00, 0x00,
	/* 'G' */  	5, 5, 0, 5, 6,
	         	0x70, 0x80, 0x98, 0x88, 0x70, 0x00, 0x00,
	/* 'H' */  	4, 5, 0, 5, 5,
	         	0x90, 0x90, 0xf0, 0x90, 0x90, 0x00, 0x00,
	/* 'I' */  	3, 5, 0, 5, 4,
	         	0xe0, 0x40, 0x40, 0x40, 0xe0, 0x00, 0x00,
	/* 'J' */  	4, 5, 0, 5, 5,
	         	0xf0, 0x10, 0x10, 0x90, 0x60, 0x00, 0x00,
	/* 'K' */  	4, 5, 0, 5, 5,
	         	0x90, 0xa0, 0xc0, 0xa0, 0x90, 0x00, 0x00,
	/* 'L' */  	4, 5, 0, 5, 5,
	         	0x80, 0x80, 0x80, 0x80, 0xf0, 0x00, 0x00,
	/* 'M' */  	5, 5, 0, 5, 6,
	         	0x88, 0xd8, 0xa8, 0x88, 0x88, 0x00, 0x00,
	/* 'N' */  	4, 5, 0, 5, 5,
	         	0x90, 0xd0, 0xb0, 0x90, 0x90, 0x00, 0x00,
	/* 'O' */  	4, 5, 0, 5, 5,
	         	0x60, 0x90, 0x90, 0x90, 0x60, 0x00, 0x00,
	/* 'P' */  	4, 5, 0, 5, 5,
	         	0xe0, 0x90, 0xe0, 0x80, 0x80, 0x00, 0x00,
	/* 'Q' */  	4, 5, 0, 5, 5,
	         	0x60, 0x90, 0x90, 0xa0, 0x50, 0x00, 0x00,
	/* 'R' */  	4, 5, 0, 5, 5,
	         	0xe0, 0x90, 0xe0, 0x90, 0x90, 0x00, 0x00,
	/* 'S' */  	4, 5, 0, 5, 5,
	         	0x70, 0x80, 0x60, 0x10, 0xe0, 0x00, 0x00,
	/* 'T' */  	5, 5, 0, 5, 6,
	         	0xf8, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00,
	/* 'U' */  	4, 5, 0, 5, 5,
	         	0x90, 0x90, 0x90, 0x90, 0x60, 0x00, 0x00,
	/* 'V' */  	5, 5, 0, 5, 6,
	         	0x88, 0x88, 0x50, 0x50, 0x20, 0x00, 0x00,
	/* 'W' */  	5, 5, 0, 5, 6,
	         	0x88, 0x88, 0x88, 0xa8, 0x50, 0x00, 0x00,
	/* 'X' */  	4, 5, 0, 5, 5,
	         	0x90, 0x90, 0x60, 0x90, 0x90, 0x00, 0x00,
	/* 'Y' */  	4, 5, 0, 5, 5,
	         	0x90, 0x90, 0x70, 0x10, 0x60, 0x00, 0x00,
	/* 'Z' */  	4, 5, 0, 5, 5,
	         	0xf0, 0x10, 0x60, 0x80, 0xf0, 0x00, 0x00,
	/* '[' */  	2, 5, 0, 5, 3,
	         	0xc0, 0x80, 0x80, 0x80, 0xc0, 0x00, 0x00,
	/* '\' */  	3, 5, 0, 5, 4,
	         	0x80, 0x80, 0x40, 0x20, 0x20, 0x00, 0x00,
	/* ']' */  	2, 5, 0, 5, 3,
	         	0xc0, 0x40, 0x40, 0x40, 0xc0, 0x00, 0x00,
	/* '^' */  	3, 5, 0, 5, 4,
	         	0x40, 0xa0, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* '_' */  	4, 5, 0, 5, 5,
	         	0x00, 0x00, 0x00, 0x00, 0xf0, 0x00, 0x00,
	/* '`' */  	2, 5, 0, 5, 3,
	         	0x80, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 'a' */  	4, 5, 0, 5, 5,
	         	0x00, 0x60, 0x90, 0x90, 0x70, 0x00, 0x00,
	/* 'b' */  	4, 5, 0, 5, 5,
	         	0x80, 0xe0, 0x90, 0x90, 0xe0, 0x00, 0x00,
	/* 'c' */  	4, 5, 0, 5, 5,
	         	0x00, 0x70, 0x80, 0x80, 0x70, 0x00, 0x00,
	/* 'd' */  	4, 5, 0, 5, 5,
	         	0x10, 0x70, 0x90, 0x90, 0x70, 0x00, 0x00,
	/* 'e' */  	4, 5, 0, 5, 5,
	         	0x00, 0x60, 0xf0, 0x80, 0x70, 0x00, 0x00,
	/* 'f' */  	4, 7, 0, 5, 5,
	         	0x00, 0x70, 0x80, 0xc0, 0x80, 0x80, 0x80,
	/* 'g' */  	4, 7, 0, 5, 5,
	         	0x00, 0x60, 0x90, 0x90, 0x70, 0x10, 0x60,
	/* 'h' */  	4, 5, 0, 5, 5,
	                0x80, 0xe0, 0x90, 0x90, 0x90, 0x00, 0x00,
	/* 'i' */  	3, 5, 0, 5, 4,
	         	0x40, 0x00, 0x40, 0x40, 0xe0, 0x00, 0x00,
	/* 'j' */  	4, 5, 0, 5, 5,
	         	0x00, 0x10, 0x10, 0x90, 0x60, 0x00, 0x00,
	/* 'k' */  	4, 5, 0, 5, 5,
	         	0x80, 0xb0, 0xc0, 0xa0, 0x90, 0x00, 0x00,
	/* 'l' */  	2, 5, 0, 5, 3,
	         	0x80, 0x80, 0x80, 0x80, 0x40, 0x00, 0x00,
	/* 'm' */  	5, 5, 0, 5, 6,
	         	0x00, 0xd0, 0xa8, 0xa8, 0xa8, 0x00, 0x00,
	/* 'n' */  	4, 5, 0, 5, 5,
	                0x00, 0xe0, 0x90, 0x90, 0x90, 0x00, 0x00,
	/* 'o' */  	4, 5, 0, 5, 5,
	         	0x00, 0x60, 0x90, 0x90, 0x60, 0x00, 0x00,
	/* 'p' */  	4, 7, 0, 5, 5,
	         	0x00, 0xe0, 0x90, 0x90, 0xe0, 0x80, 0x80,
	/* 'q' */       4, 7, 0, 5, 5,
	         	0x00, 0x70, 0x90, 0x90, 0x70, 0x10, 0x10,
	/* 'r' */       4, 5, 0, 5, 5,
	         	0x00, 0x70, 0x80, 0x80, 0x80, 0x00, 0x00,
	/* 's' */	4, 5, 0, 5, 5,
	         	0x00, 0x70, 0xc0, 0x30, 0xe0, 0x00, 0x00,
	/* 't' */       4, 5, 0, 5, 5,
	         	0x40, 0xf0, 0x40, 0x40, 0x30, 0x00, 0x00,
	/* 'u' */	4, 5, 0, 5, 5,
	         	0x00, 0x90, 0x90, 0x90, 0x70, 0x00, 0x00,
	/* 'v' */	4, 5, 0, 5, 5,
	         	0x00, 0x90, 0x90, 0x90, 0x60, 0x00, 0x00,
	/* 'w' */	5, 5, 0, 5, 6,
	         	0x00, 0x88, 0xa8, 0xa8, 0x50, 0x00, 0x00,
	/* 'x' */	4, 5, 0, 5, 5,
	         	0x00, 0x90, 0x60, 0x60, 0x90, 0x00, 0x00,
	/* 'y' */	4, 7, 0, 5, 5,
	         	0x00, 0x90, 0x90, 0x90, 0x70, 0x10, 0x60,
	/* 'z' */	4, 5, 0, 5, 5,
	         	0x00, 0xf0, 0x20, 0x40, 0xf0, 0x00, 0x00,
	/* '{' */	3, 5, 0, 5, 4,
	         	0x60, 0x80, 0x40, 0x80, 0x60, 0x00, 0x00,
	/* '|' */	1, 5, 0, 5, 2,
	         	0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00,
	/* '}' */	3, 5, 0, 5, 4,
	         	0xc0, 0x20, 0x40, 0x20, 0xc0, 0x00, 0x00,
	/* '~' */	4, 5, 0, 5, 5,
	         	0x00, 0x50, 0xa0, 0x00, 0x00, 0x00, 0x00,
};

static struct GP_FontFace tiny = {
	.family_name = "Tiny",
	.style_name = "",
	.charset = GP_CHARSET_7BIT,
	.ascend  = 5,
	.descend = 3,
	.max_glyph_width = 5,
	.max_glyph_advance = 6,
	.glyph_bitmap_format = GP_FONT_BITMAP_1BPP,
	.glyphs = tiny_glyphs,
	.glyph_offsets = {12},
};

struct GP_FontFace *GP_FontTiny = &tiny;
