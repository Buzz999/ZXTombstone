/*
 *  Copyright (C) 2008-2019 Florent Bedoiseau (electronique.fb@free.fr)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

// VERSION : 2019/10/20 - Morroco -

// BUGS
// Splash sometimes not replaced by a cactus
// Optimsations : Bzz Spectrum slower (>20ms / loop)

#include "ZXTombstone.h"

#include <arch/zx.h>
#include <stdio.h>
#include <stdlib.h> // rand
#include <input.h>

// ISR calls
#pragma output CRT_ORG_CODE = 0x8184   // move org just above the im2 table and jump (see below)
#pragma printf = "%u"                  // let's cut out the printf converters you're not using

#include <string.h>        // memset
#include <im2.h>           // im2 macros and functions
#include <intrinsic.h>
#include <z80.h>           // bpoke, etc

// frame counter
unsigned int frames; //  1 frame = 20ms

// Keys
#define UP_KEY IN_KEY_SCANCODE_e
#define DOWN_KEY IN_KEY_SCANCODE_x
#define LEFT_KEY IN_KEY_SCANCODE_s
#define RIGHT_KEY IN_KEY_SCANCODE_d
#define FIRE_KEY IN_KEY_SCANCODE_y
#define QUIT_KEY IN_KEY_SCANCODE_0
#define PAUSE_KEY IN_KEY_SCANCODE_p
#define TAB_KEY IN_KEY_SCANCODE_SPACE

#define OUT 128 // Why not ...

// Timing (ticks)
// 20ms / frame
#define PLAYER_TICK 2 /* 100ms */
#define SHOOT_TICK 1 /* 200ms */
#define MOVE_BUISSONS_TICK 10 /* 120ms */
#define CREATE_BUISSONS_TICK 100 /* 4s */
#define CREATE_MONSTERS_TICK 200 /* 7s */
#define CHANGE_CAGE_TICK 150 /* 5s */
#define MOVE_MONSTERS_TICK 10 /* 100ms */
#define SPLASH_TICK 5


// Tiles & tables

// Rand tables
static unsigned char rand_idx=0;
static unsigned char rand_02[]={
1,0,1,0,0,0,0,0,1,1,0,1,0,1,1,0,
0,1,1,1,0,1,0,0,1,1,0,1,1,1,0,0,
0,1,1,1,0,0,1,0,0,0,0,1,1,1,0,1,
1,1,0,1,1,1,0,0,1,0,1,1,0,1,1,0
};
static unsigned char rand_03[]={
0,0,0,1,2,0,2,0,0,1,2,0,2,2,1,1,
0,0,2,1,1,1,2,2,1,0,1,2,0,1,1,1,
0,1,2,2,1,2,0,1,1,0,2,2,1,2,0,1,
2,2,1,2,0,2,2,0,2,2,2,2,0,0,2,1
};
static unsigned char rand_18[]={
15,9,17,12,10,14,9,5,16,12,6,1,6,14,9,9,
3,4,5,3,10,2,12,1,14,7,5,7,17,7,3,9,
7,7,1,1,3,2,14,11,13,3,17,14,4,0,9,8,
2,0,14,4,13,13,15,9,13,4,3,15,12,15,17,10
};
static unsigned char rand_28[]={
6,7,0,21,6,24,16,11,9,10,23,24,24,27,1,20,
14,2,4,1,21,18,18,9,23,25,2,25,24,9,22,27,
18,7,16,15,5,23,20,10,16,27,21,9,0,3,22,16,
11,0,14,8,16,11,23,1,1,5,16,8,1,21,9,6
};

unsigned char rand_mod(unsigned char modulo) {
	++rand_idx;
	if (rand_idx >= 64) rand_idx = 0;
	
	switch (modulo) {
		case 2: return rand_02[rand_idx]; break;
		case 3: return rand_03[rand_idx]; break;
		case 18: return rand_18[rand_idx]; break;
		case 28: return rand_28[rand_idx]; break;
	}
	return 0;
}
	
// Upper
//#define x_width 32x8bits
//#define x_height 24bits
static unsigned char upper_tile[] = {
0xFF,0xFF,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x00,0x01,0xFF,0xFF,
0xFF,0xFF,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x2A,0x00,0x03,0xFF,0xFF,
0xFF,0xFF,0xE0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3A,0x00,0x07,0xFF,0xFF,
0xFF,0xFF,0xF0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0E,0x00,0x0F,0xFF,0xFF,
0xFF,0xFF,0xF8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x00,0x1F,0xFF,0xFF,
0xFF,0xFF,0xFC,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x00,0x3F,0xFF,0xFF,
0xFF,0xFF,0xFE,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7E,0x00,0x7F,0xFF,0xFF,
0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0x80,0x08,0x00,0x00,0x00,0xFC,0xFC,0x84,0xFC,0xFC,0xFC,0xFC,0xCC,0xFC,0x00,0xFC,0x30,0xFC,0xCC,0x00,0x00,0x00,0x01,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xC0,0x2A,0x00,0x00,0x00,0xFC,0xFC,0xCC,0xFC,0xFC,0xFC,0xFC,0xCC,0xFC,0x00,0xFC,0x30,0xFC,0xCC,0x00,0x00,0x00,0x03,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xE0,0x3A,0x00,0x00,0x00,0x30,0xCC,0xFC,0xCC,0xC0,0x30,0xCC,0xEC,0xC0,0x00,0xC0,0x30,0x30,0xCC,0x00,0x00,0x00,0x07,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xF0,0x0E,0x00,0x00,0x00,0x30,0xCC,0xFC,0xCC,0xC0,0x30,0xCC,0xFC,0xC0,0x00,0xC0,0x30,0x30,0xCC,0x00,0x00,0x00,0x0F,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xF8,0x08,0x00,0x00,0x00,0x30,0xCC,0xCC,0xCC,0xC0,0x30,0xCC,0xDC,0xC0,0x00,0xC0,0x30,0x30,0xCC,0x00,0x00,0x00,0x1F,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFC,0x08,0x00,0x00,0x00,0x30,0xCC,0xCC,0xCC,0xC0,0x30,0xCC,0xCC,0xC0,0x00,0xC0,0x30,0x30,0xCC,0x00,0x00,0x00,0x3F,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFE,0x7E,0x00,0x00,0x00,0x30,0xCC,0xCC,0xFC,0xFC,0x30,0xCC,0xCC,0xF0,0x00,0xC0,0x30,0x30,0xFC,0x00,0x00,0x00,0x7F,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x30,0xCC,0xCC,0xFC,0xFC,0x30,0xCC,0xCC,0xF0,0x00,0xC0,0x30,0x30,0xFC,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x80,0x00,0x30,0xCC,0xCC,0xCC,0x0C,0x30,0xCC,0xCC,0xC0,0x00,0xC0,0x30,0x30,0x30,0x00,0x01,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xE0,0x00,0x30,0xCC,0xCC,0xCC,0x0C,0x30,0xCC,0xCC,0xC0,0x00,0xC0,0x30,0x30,0x30,0x00,0x07,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xF8,0x00,0x30,0xCC,0xCC,0xCC,0x0C,0x30,0xCC,0xCC,0xC0,0x00,0xC0,0x30,0x30,0x30,0x00,0x1F,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFE,0x00,0x30,0xCC,0xCC,0xCC,0x0C,0x30,0xCC,0xCC,0xC0,0x00,0xC0,0x30,0x30,0x30,0x00,0x7F,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x80,0x30,0xCC,0xCC,0xCC,0x0C,0x30,0xCC,0xCC,0xC0,0x00,0xC0,0x30,0x30,0x30,0x01,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xE0,0x30,0xCC,0xCC,0xCC,0x0C,0x30,0xCC,0xCC,0xC0,0x00,0xC0,0x30,0x30,0x30,0x07,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xF8,0x30,0xFC,0xCC,0xFC,0xFC,0x30,0xFC,0xCC,0xFC,0x00,0xFC,0x30,0x30,0x30,0x1F,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x30,0xFC,0xCC,0xFC,0xFC,0x30,0xFC,0xCC,0xFC,0x00,0xFC,0x30,0x30,0x30,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
}; 

//#define x_width 32x8bits
//#define x_height 16bits
static unsigned char lower_tile[] = {
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x78,0x38,0x44,0x00,0x00,0x78,0x7C,0x78,0x44,0x40,0x38,0x7C,0x38,0x7C,0x44,0x00,0x00,0x38,0x38,0x44,0x7C,0x7C,0x44,0x7C,0x78,0x38,0x00,0x00,0x00,
0x00,0x00,0x00,0x24,0x44,0x44,0x00,0x00,0x44,0x44,0x44,0x44,0x40,0x44,0x10,0x10,0x44,0x64,0x00,0x00,0x44,0x44,0x44,0x44,0x44,0x64,0x40,0x44,0x44,0x00,0x00,0x00,
0x00,0x00,0x00,0x24,0x44,0x28,0x00,0x00,0x44,0x44,0x44,0x44,0x40,0x44,0x10,0x10,0x44,0x64,0x00,0x00,0x40,0x40,0x44,0x44,0x44,0x64,0x40,0x44,0x40,0x00,0x00,0x00,
0x00,0x00,0x00,0x24,0x7C,0x10,0x00,0x00,0x78,0x44,0x78,0x44,0x40,0x7C,0x10,0x10,0x44,0x54,0x00,0x00,0x38,0x40,0x7C,0x44,0x44,0x54,0x78,0x78,0x38,0x00,0x00,0x00,
0x00,0x00,0x00,0x24,0x44,0x10,0x00,0x00,0x40,0x44,0x40,0x44,0x40,0x44,0x10,0x10,0x44,0x4C,0x00,0x00,0x04,0x40,0x44,0x44,0x44,0x4C,0x40,0x50,0x04,0x00,0x00,0x00,
0x00,0x00,0x00,0x24,0x44,0x10,0x00,0x00,0x40,0x44,0x40,0x44,0x40,0x44,0x10,0x10,0x44,0x4C,0x00,0x00,0x44,0x44,0x44,0x44,0x44,0x4C,0x40,0x48,0x44,0x00,0x00,0x00,
0x00,0x00,0x00,0x78,0x44,0x10,0x00,0x00,0x40,0x7C,0x40,0x38,0x7C,0x44,0x10,0x38,0x7C,0x44,0x00,0x00,0x38,0x38,0x44,0x7C,0x7C,0x44,0x7C,0x44,0x38,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
}; 
	
// G Patterns

/* Numero des tiles */
#define TILE_FLOOR      0
#define TILE_BLOCK_1    1
#define TILE_BLOCK_2    2
#define TILE_BUISSON_1  3
#define TILE_BUISSON_2  4
#define TILE_CACTUS_1   5
#define TILE_CACTUS_2   6
#define TILE_MONSTER_1  7
#define TILE_MONSTER_2  8
#define TILE_PLAYER_NORD   9
#define TILE_PLAYER_EST   10
#define TILE_PLAYER_SUD   11
#define TILE_PLAYER_WEST   12
#define TILE_SPLASH        13
#define TILE_TIR_NS      14
#define TILE_TIR_EW      15

static unsigned char sprites[]     = { 
	0x00, 0x00, 0x80, 0x00, 0x00, 0x02, 0x00, 0x00, // TILE_FLOOR
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // TILE_BLOCK_1
	0xFF, 0xFF, 0xC3, 0xC3, 0xC3, 0xC3, 0xFF, 0xFF, // TILE_BLOCK_2
	0x00, 0x59, 0x3C, 0x7E, 0x7E, 0x3C, 0x59, 0x00, // TILE_BUISSON_1
	0x00, 0x18, 0x7E, 0x3C, 0x3C, 0x7E, 0x18, 0x00, // TILE_BUISSON_2
	0x08, 0x2A, 0x3A, 0x0E, 0x08, 0x08, 0x7E, 0xFF, // TILE_CACTUS_1
	0x08, 0x2A, 0x3A, 0x0E, 0x08, 0x08, 0x7E, 0xFF, // TILE_CACTUS_2
	0x3C, 0x7E, 0xFF, 0xFF, 0x7E, 0x3C, 0x42, 0x84, // TILE_MONSTER_1
	0xBD, 0x7E, 0xFF, 0xFF, 0x7E, 0x3C, 0x42, 0x21, // TILE_MONSTER_2
	0x00, 0x18, 0x18, 0x18, 0x3C, 0x7E, 0x42, 0x00, // TILE_PLAYER_NORD
	0x00, 0x60, 0x30, 0x3E, 0x3E, 0x30, 0x70, 0x00, // TILE_PLAYER_EST
	0x00, 0x42, 0x7E, 0x3C, 0x18, 0x18, 0x18, 0x00, // TILE_PLAYER_SUD
	0x00, 0x06, 0x0C, 0x7C, 0x7C, 0x0C, 0x06, 0x00, // TILE_PLAYER_WEST
	0x95, 0x56, 0x38, 0x7C, 0xBF, 0x3C, 0x52, 0x89, // SPLASH
	0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, // TILE_TIR_NS
	0x00, 0x00, 0x00, 0x7E, 0x7E, 0x00, 0x00, 0x00  // TILE_TIR_EW
};

static unsigned char numbers[] = {
0x00, 0x1E, 0x22, 0x22, 0x22, 0x22, 0x22, 0x1E,
0x00, 0x08, 0x18, 0x08, 0x08, 0x08, 0x08, 0x1C,
0x00, 0x1C, 0x22, 0x02, 0x04, 0x08, 0x10, 0x3E,
0x00, 0x1C, 0x22, 0x02, 0x0C, 0x02, 0x22, 0x1C,
0x00, 0x04, 0x0C, 0x14, 0x24, 0x3E, 0x04, 0x04,
0x00, 0x3E, 0x20, 0x3C, 0x02, 0x02, 0x22, 0x1C,
0x00, 0x0C, 0x10, 0x20, 0x3C, 0x22, 0x22, 0x1C,
0x00, 0x3E, 0x02, 0x04, 0x08, 0x10, 0x10, 0x10,
0x00, 0x1C, 0x22, 0x22, 0x1C, 0x22, 0x22, 0x1C,
0x00, 0x1C, 0x22, 0x22, 0x1E, 0x02, 0x04, 0x18
};

static unsigned char colors[] = {
INK_YELLOW | PAPER_YELLOW, // TILE_FLOOR
INK_BLUE | PAPER_BLUE, // TILE_BLOCK_1
INK_BLUE | PAPER_CYAN, // TILE_BLOCK_2
INK_MAGENTA | PAPER_YELLOW, // TILE_BUISSON_1
INK_MAGENTA | PAPER_YELLOW, // TILE_BUISSON_2
INK_BLACK | PAPER_YELLOW, // TILE_CACTUS_1
INK_BLACK | PAPER_WHITE | BRIGHT, // TILE_CACTUS_2
INK_GREEN | PAPER_YELLOW, // TILE_MONSTER_1
INK_GREEN | PAPER_YELLOW, // TILE_MONSTER_2
INK_BLUE | PAPER_YELLOW, // TILE_PLAYER_NORD
INK_BLUE | PAPER_YELLOW, // TILE_PLAYER_EST
INK_BLUE | PAPER_YELLOW, // TILE_PLAYER_SUD
INK_BLUE | PAPER_YELLOW, // TILE_PLAYER_WEST
INK_RED | PAPER_YELLOW, // SPLASH
INK_BLUE | PAPER_YELLOW, // TILE_TIR_NS
INK_BLUE | PAPER_YELLOW // TILE_TIR_EW
};


// Direction
#define NORD 1
#define EST  2
#define SUD  3
#define WEST 4

// Vars are global
unsigned char _gameState;

// PLAYER
struct Object _player_Object;
unsigned char _player_direction;
unsigned char _player_blocked;

// FIRE
unsigned char _fire_x, _fire_y;
unsigned char _fire_direction;
unsigned char _fire_in_progress;
unsigned char _shoot_item;

// SPLASH
struct Object _splash_Object;
unsigned char _splash_in_progress;

/* MONSTER */
unsigned char _mx, _my; /* Position possible de creation d'un monstre */
unsigned char _monsterToBeCreated;

/* CACTUS */
unsigned char _cx, _cy; /* position du cactus blanc */

// Buissons
#define MAX_BUISSONS 16
unsigned char _nBuissons;

struct Object _mListOfCactus[MAX_OBJECTS_IN_LIST]; /* CACTUS */
struct Object _mListOfMonsters[MAX_OBJECTS_IN_LIST]; /* MONSTERS */

unsigned int _time_splash;
unsigned int _time_create_monster;
unsigned int _time_move_monster;
unsigned int _time_create_buissons;
unsigned int _time_move_buissons;
unsigned int _time_player;

unsigned int _score;
unsigned char _day;
unsigned int _life;

// 504 bytes
#define BOARD_H 18
#define BOARD_W 28

unsigned char _board[504]; // y:18, x:28
static unsigned int y_coord[] ={ // Optimization : y*BOARD_W
	0, 28, 56, 84, 112, 140, 168, 196, 224, 252,
	280, 308, 336, 364, 392, 420, 448, 476, 504
};

void NoObject_Place(unsigned char tile, unsigned char x, unsigned char y) {
	unsigned int idx;
	idx = ((unsigned int) y_coord[y]) + x;
	_board[idx]=tile | 0x80; // 0x80 : To redraw
}

void Object_Place(struct Object *o, unsigned char tile, unsigned char x, unsigned char y) {
	unsigned int idx;
	idx = ((unsigned int) y_coord[y]) + x;
	o->_valid = 1;
	o->_old = _board[idx];
    o->_x = x;
    o->_y = y;
    o->_tileNumber = tile;
	_board[idx]=tile | 0x80; // 0x80 : To redraw
}

void Object_Erase(struct Object *o) {
	unsigned int idx;
    if (o->_valid == 0) return; // Nothing to do
    o->_valid = 0;
	idx = ((unsigned int) y_coord[o->_y]) + o->_x;
	_board[idx]= o->_old | 0x80; // 0x80: To redraw
}

unsigned char Object_IncXY (struct Object* o, signed char ix, signed char iy) {
	unsigned char x, y;
	unsigned int idx, oidx;
	
	// New position
	x = o->_x+ix;
	y = o->_y+iy;
	if (Game_IsOut(x,y)) return 0; // Out of the limits
	
	idx = ((unsigned int) y_coord[y]) + x;
	if ((_board[idx] & 0x0F) != TILE_FLOOR) return 0; // Obstacle
	
	// Removing old tile
	oidx = ((unsigned int) y_coord[o->_y]) + o->_x;
	_board[oidx] = TILE_FLOOR | 0x80; // Force the previous tile to be drawn
	
	// New position. Adding the tile
    o->_x = x;
    o->_y = y; // Moving : increment
	_board[idx]=o->_tileNumber | 0x80; // To redraw
	
	return 1; // Success
}

void Object_SetXY (struct Object* o,  unsigned char x, unsigned char y) {
	unsigned int idx;
	
	idx = ((unsigned int) y_coord[o->_y]) + o->_x;
	_board[idx] = TILE_FLOOR | 0x80; // Force the previous tile to be drawn
	
    o->_x=x;
    o->_y=y; // Moving
	idx = ((unsigned int) y_coord[y]) + x;
	_board[idx]=o->_tileNumber | 0x80; // To redraw
}

void PointList_Copy(struct Point* dst, struct Point* src, unsigned char n) {
    unsigned char i;
    for ( i = 0; i < n; ++i) {
        dst[i]._x = src[i]._x; 
		dst[i]._y = src[i]._y; 
    }
}

unsigned char ObjectList_isEmpty(struct Object* liste) {
    unsigned char i;
    for ( i = 0; i < MAX_OBJECTS_IN_LIST; ++i) {
        if (liste[i]._valid) return 0;
    }
    return 1;
}

void ObjectList_Init(struct Object* liste) {
    unsigned char i;
    for (i = 0; i < MAX_OBJECTS_IN_LIST; ++i){
        liste[i]._valid = 0;
    }
}

void Object_Copy(struct Object* dst, struct Object* src) {
	dst->_valid = src->_valid;
    dst->_x = src->_x;
	dst->_y = src->_y;
	dst->_old = src->_old;
	dst->_tileNumber = src->_tileNumber;
}

void Game_ShakeCactusList() {
	unsigned char i;
	struct Object o;
	Object_Copy(&o, &_mListOfCactus[0]);
	for (i=1; i<MAX_OBJECTS_IN_LIST; ++i) {
		Object_Copy(&_mListOfCactus[i-1], &_mListOfCactus[i]);
	}
	Object_Copy(&_mListOfCactus[MAX_OBJECTS_IN_LIST-1], &o);
}

void ObjectList_PlaceObject(struct Object* liste, unsigned char tileNum, unsigned char x, unsigned char y) {
    unsigned char i;
    for ( i= 0; i < MAX_OBJECTS_IN_LIST; ++i) {
        if (liste[i]._valid == 0) {
			Object_Place(&liste[i], tileNum, x, y);
            return;
        }
    }
}

void ObjectList_Erase(struct Object* liste) {
    unsigned char i;
    for ( i= 0; i < MAX_OBJECTS_IN_LIST; ++i) {
        if (liste[i]._valid == 1) {
            Object_Erase(&liste[i]);
        }
    }
}

void sleepNFrames(unsigned int nFrames) {
	unsigned int clk1;
	clk1 = frames; // Start time
	while (frames - clk1 < nFrames) ; // Wait
}

void Game_State () {
    switch (_gameState) {
    case S_INIT:
		Game_DrawUpperTile();
		Game_DrawLowerTile(); 
		Game_Create();
		Game_DisplayBoardTiles();
		Game_DrawNumberTile(2, 20, _day, 2); // Day 
		Game_DrawNumberTile(11, 20, _score, 5); // Population 
		Game_DrawNumberTile(25, 20, _life, 2); // Shooners
	
		Game_DrawString(4, 0, "Y to start. P to pause"); 
		_gameState = S_PAUSE;
        break;

    case S_PAUSE:
        if (in_key_pressed(FIRE_KEY)) { // START
            _gameState = S_RUN;
            while (in_key_pressed(FIRE_KEY)); // WAIT
			Game_DrawString(4, 0, "                      "); 
        }
        else if (in_key_pressed(QUIT_KEY)){ // RESTART
            _gameState = S_INIT;
            while (in_key_pressed(QUIT_KEY)); // WAIT
			Game_DrawString(4, 0, "                      "); 
        }
        break;
		
    case S_RUN:
        if (in_key_pressed(PAUSE_KEY)){ // PAUSE
            _gameState = S_PAUSE;
            while (in_key_pressed(PAUSE_KEY)); 
			Game_DrawString(4, 0, "pause. Y to continue  "); 
        }
        else if (in_key_pressed(QUIT_KEY)){ // RESTART
            _gameState = S_INIT;
            while (in_key_pressed(QUIT_KEY)); 
        }
        break;

    case S_END:
		Game_DrawString(8, 0, "*** GAME OVER ***"); 
		sleepNFrames(250); // Wait 5s
        _gameState = S_OVER;
        break;

    case S_OVER:
        _gameState = S_INIT;
        break;
    default:
        break;
    }
}

	
void Game_Process () {
#ifdef DEBUG
	unsigned int clk1; // DEBUG
	clk1 = frames;
#endif

    if (_gameState != S_RUN) return; 

	Game_TimeMoveBuissons ();
	Game_TimeMoveMonsters();
	Game_TimeMovePlayer ();
	Game_TimeFirePlayer ();
    Game_TimeMoveFire ();
    Game_TimeSplash ();
    Game_TimeCreateMonsters ();
    Game_TimeCreateBuissons ();
	
	Game_DisplayBoardTiles();
		
#ifdef DEBUG
	Game_DrawNumberTile(11, 20, frames - clk1, 5); 
#else
	Game_DrawNumberTile(11, 20, _score, 5); // Population
	Game_DrawNumberTile(2, 20, _day, 2); // Day 
	Game_DrawNumberTile(25, 20, _life, 2); // Shooners
#endif
}

void Game_Create () {
    _player_Object._valid = 0;
	_fire_in_progress = 0;
	_splash_in_progress = 0;
	
    _gameState = S_INIT;
    _score=0; /* POPULATION */
    _day=1;
    _life=10; /* SHOONERS */

	_nBuissons = 0;
    ObjectList_Init(_mListOfCactus);
    ObjectList_Init(_mListOfMonsters);

    Object_Erase(&_player_Object);
	
    Game_PrepareGame ();
}

void Game_Delete (){
    Game_DeleteAllObjects ();
}


unsigned char Game_CheckIfBlocked () {
    unsigned char nb, c, x, y;
	unsigned int idx;
	
    nb = 0;
    for ( x=12; x<=16; x+=2) {
		idx = ((unsigned int) 4*BOARD_W) + x;
        c = _board[idx] & 0x0F;
        if (c==TILE_CACTUS_1 || c==TILE_CACTUS_2) nb++;
		idx = ((unsigned int) 12*BOARD_W) + x;
        if (c==TILE_CACTUS_1 || c==TILE_CACTUS_2) nb++;
    }

    for ( y=6; y<=10; y+=2){
		idx = ((unsigned int) y_coord[y]) + 10;
        c =_board[idx] & 0x0F; 
        if (c==TILE_CACTUS_1 || c==TILE_CACTUS_2) nb++;
		idx = ((unsigned int) y_coord[y]) + 18;
        c = _board[idx] & 0x0F; 
        if (c==TILE_CACTUS_1 || c==TILE_CACTUS_2) nb++;
    }

    if (nb!=12) return 0;
	
    return 1; // Blocked
}


void Game_ChangeDay () {
    _day++;
    Game_PrepareGame ();
}

void Game_PrepareGame () {
    unsigned char i, j;
	unsigned int idx;
	// Clearing all the board
	for (j=0; j < BOARD_H; j++) {
        for (i=0; i < BOARD_W; i++) {
			idx = ((unsigned int) y_coord[j]) + i; // j*BOARD_W) + i;
			_board[idx]=TILE_FLOOR | 0x80;
        }
    }

    _fire_direction=NORD;
    _fire_in_progress = 0;
	_splash_in_progress = 0;
	
    _shoot_item=SI_NONE;

    Game_DeleteAllObjects ();

    /* Autre */
    _mx=0; // Monster
    _my=0;
    _cx=0; // Cactus
    _cy=0;

    _monsterToBeCreated=0;

    /* Les flags de temps */
    _time_create_monster=0; /* Pas de monstre a creer */
    _time_move_monster=0; /* Pas de monstre a deplacer */
    _time_move_buissons=0; /* Pas de buisson a deplacer */
    _time_create_buissons=0; /* Pas de buisson a creer */
    _time_splash=0; /* Pas de splash */

    Game_CreateAllCactus ();
    Game_CreateAllBuissons ();
    Game_CreateAllCages (TILE_BLOCK_1);
    Game_CreatePlayer ();
}

void Game_CreatePlayer (){
    Object_Erase(&_player_Object); // Deleting first
    Object_Place(&_player_Object, TILE_PLAYER_NORD, 14, 8); 
    _player_blocked=0;
    _time_player=PLAYER_TICK;
    _player_direction=NORD; /* Up */
}

void Game_DeleteAllObjects (){
    ObjectList_Erase(_mListOfCactus);
    ObjectList_Erase(_mListOfMonsters);
    Object_Erase(&_player_Object);
}

void Game_ChangeLife (){
    if (_life <= 0) {  /* Game Over */
        Game_CreatePlayer ();
        _gameState = S_END;
        return;
    }

    /* Restart board */
	Game_DisplayFire(0);
	
    _fire_in_progress=0;
	_splash_in_progress=0;
	
    _life--;

    Object_Erase(&_player_Object);

    Game_CreateAllCages ( TILE_BLOCK_1);
    Game_CreatePlayer ();
}

void Game_CreateAllCages (unsigned char tileCage){
    unsigned char x, y;
	unsigned int idx;

    for (y=5; y <= 11; y+=2) {  
        for (x=11; x <= 17; x+=2) {
			idx = ((unsigned int) y_coord[y]) + x;
			_board[idx]=tileCage | 0x80; // 0x80 : To redraw
        }
    }
}

void Game_CreateAllCactus (){
	unsigned char tab_dir [8]; /* 8 directions a scruter */
    unsigned char x1;
    unsigned char y1;
    unsigned char x2;
    unsigned char y2;
    
    unsigned char n;
    unsigned char nb;
    unsigned char i;
	unsigned char trouve_second;
	unsigned char trouve_first;
	
	x1=0;
	x2=0;
	y1=0;
	y2=0;
	
    ObjectList_Init(_mListOfCactus);
    n=5 + (4 * (_day - 1)); /* Le nombre de cactus total / 2 */

    for ( nb=0; nb < n; ++nb) {
    trouve_second=0;
    while (trouve_second == 0){
            /* On cherche une premiere case vide */
            trouve_first=0;
            while (trouve_first == 0){ 
				x1=rand_mod(BOARD_W);
				y1=rand_mod(BOARD_H);
                if (Game_CheckObstacles (x1, y1)==TILE_FLOOR) {
                    trouve_first=1;
					
				}
            }

            ShakeDirection (tab_dir);

            for ( i=0; (i < 8) && (trouve_second==0); ++i) {
                switch (tab_dir [i]){
                case 0:
                    x2=x1;
                    y2=y1 - 1;
                    break;
                case 1:
                    x2=x1 + 1;
                    y2=y1 - 1;
                    break;
                case 2:
                    x2=x1 + 1;
                    y2=y1;
                    break;
                case 3:
                    x2=x1 + 1;
                    y2=y1 + 1;
                    break;
                case 4:
                    x2=x1;
                    y2=y1 + 1;
                    break;
                case 5:
                    x2=x1 - 1;
                    y2=y1 + 1;
                    break;
                case 6:
                    x2=x1 - 1;
                    y2=y1;
                    break;
                case 7:
                    x2=x1 - 1;
                    y2=y1 - 1;
                    break;
                default:
                    break;
                }

                if (Game_CheckObstacles ( x2, y2)==TILE_FLOOR) {
                    trouve_second=1;
				}
            }
        }

		ObjectList_PlaceObject(_mListOfCactus, TILE_CACTUS_1, x1, y1);
		ObjectList_PlaceObject(_mListOfCactus, TILE_CACTUS_1, x2, y2);
    }
}

void Game_CreateAllBuissons (){
    unsigned char x, y, nb;
    for ( nb=0; nb < MAX_BUISSONS; ++nb) {
        Game_FindEmptySpace (&x, &y);
		NoObject_Place(TILE_BUISSON_1, x, y); 
    }
	_nBuissons = MAX_BUISSONS;
}

void Game_FindEmptySpace (unsigned char* x, unsigned char* y) {
    // Try to find an empty space
    while (1) {
        *x=rand_mod(BOARD_W);
        *y=rand_mod(BOARD_H);
        if (Game_CheckObstacles (*x, *y)==TILE_FLOOR) return;
    }
}

void Game_TimeMovePlayer (){
    unsigned char x, y;
    if (_time_player < PLAYER_TICK){
        _time_player++;
        return;
    }

    _time_player=0;

    x = _player_Object._x;
    y = _player_Object._y;

	if (in_key_pressed(DOWN_KEY)) {  /* DOWN */
        _player_Object._tileNumber = TILE_PLAYER_SUD;
        _player_direction=SUD;
        if (!Object_IncXY(&_player_Object, 0, 1)) Object_SetXY(&_player_Object, x, y);
		return;
    }
    
	if (in_key_pressed(UP_KEY)) {    /* UP */
        _player_Object._tileNumber = TILE_PLAYER_NORD;
        _player_direction=NORD;
        if (!Object_IncXY(&_player_Object, 0, -1)) Object_SetXY(&_player_Object, x, y);
		return;
    }
	
    if (in_key_pressed(LEFT_KEY)) {    /* LEFT */
        _player_Object._tileNumber = TILE_PLAYER_WEST;
        _player_direction=WEST;
        if (!Object_IncXY(&_player_Object, -1, 0)) Object_SetXY(&_player_Object, x, y);
		return;
    }
	
    if (in_key_pressed(RIGHT_KEY)) {    /* RIGHT */
        _player_Object._tileNumber = TILE_PLAYER_EST;
        _player_direction=EST;
        if (!Object_IncXY(&_player_Object, 1, 0)) Object_SetXY(&_player_Object, x, y);
		return;
    }
	
    if (in_key_pressed(TAB_KEY)) {    /* PANIC */
        Object_SetXY(&_player_Object, 14, 8);
        _player_direction=NORD; /* Up */
        _player_Object._tileNumber = TILE_PLAYER_NORD;
        _score=0;
    }
}

void Game_TimeFirePlayer () {
    if (_fire_in_progress) return; // Already triggered
	if (_splash_in_progress) return; // Splash in progress
    if (!in_key_pressed(FIRE_KEY)) return; // No fire triggered

    // Starting at player position (not drawn in this case)
	_fire_x = _player_Object._x;
    _fire_y = _player_Object._y;
	_fire_direction = _player_direction;	
    _shoot_item=SI_NONE; // Nothing targetted
	_fire_in_progress=1; // Now in progress
}

void Game_TimeMoveFire () {
    unsigned char ax, ay;
    int bounces;

    if (_fire_in_progress == 0) return; // No fire in progress

    ax = _fire_x;
    ay = _fire_y; 

	Game_DisplayFire(0); // Clearing first before moving
	
    // Computing next position of the fire
    switch (_fire_direction) {
    case NORD: ay -= 1; break; // UP
    case EST:  ax += 1; break; // RIGHT
    case SUD:  ay += 1; break; // DOWN
    case WEST: ax -= 1; break; // LEFT
    default: break;
    }

    // Checking the position of the fire
    bounces = Game_CheckBounces (ax, ay);

    switch (bounces) {
    case TILE_FLOOR: // Moving it
		 _fire_x = ax;
		 _fire_y = ay;
		 Game_DisplayFire(1); // Drawing it now
    break;

    case OUT:
    case TILE_BLOCK_1:
    case TILE_BLOCK_2:
    case TILE_CACTUS_1:
    case TILE_CACTUS_2:
        _fire_in_progress=0; // No more fire
    break;

    case TILE_BUISSON_1:
    case TILE_BUISSON_2:
        _fire_in_progress=0; // No more fire
		_shoot_item=SI_BUSH;
		--_nBuissons;
		NoObject_Place(TILE_FLOOR, ax, ay);
		
		_splash_in_progress=1;
		_time_splash = 0; 
		
		Object_Place(&_splash_Object, TILE_SPLASH, ax, ay);
		
        _score+=100;
    break;

    case TILE_MONSTER_1:
    case TILE_MONSTER_2: {
		_shoot_item=SI_MONSTER;
        _fire_in_progress=0;

        Game_RemoveXYObject (_mListOfMonsters, ax, ay);
		
		_splash_in_progress=1;
		_time_splash = 0;
        
        Object_Place(&_splash_Object, TILE_SPLASH, ax, ay);
		
        _score+=150;
    }
    break;

    default:
        break; 
    }
}

void Game_TimeSplash () {
	unsigned char sx, sy;
    if (!_splash_in_progress) return; // No splash

    if (_time_splash < SPLASH_TICK) { 
        _time_splash++;
        return;
	}
	
	sx = _splash_Object._x;
	sy = _splash_Object._y;
	Object_Erase(&_splash_Object);
	
	_splash_in_progress=0; //  No more splash
	_time_splash = 0;
	
    // Creating or changing according to the fired item
    if (_shoot_item==SI_MONSTER) {
		ObjectList_PlaceObject(_mListOfCactus, TILE_CACTUS_1, sx, sy); 
        Game_ReplaceCactusByMonster (sx, sy); 
    }

    _shoot_item=SI_NONE;
}

void Game_TimeCreateBuissons () {
	if (_nBuissons != 0) return;
	 
    if (_time_create_buissons >= CREATE_BUISSONS_TICK){
        _time_create_buissons=0;
        Game_CreateAllBuissons ();
    }
    else {
        _time_create_buissons++;
	}
}

void Game_TimeMoveBuissons (){
    unsigned char x, y, px, py, c;
	unsigned int idx;
	
	if (_nBuissons == 0) return;
	
    if (_time_move_buissons < MOVE_BUISSONS_TICK){
		_time_move_buissons++;
		return;
	}
	_time_move_buissons=0;
	
	px = _player_Object._x;
    py = _player_Object._y;

	// Row : Left side
	for (x=1; x < px; ++x) {
		idx = ((unsigned int) y_coord[py]) + x;		
		c = _board[idx] & 0x0F;
		if (c == TILE_BUISSON_1) c = TILE_BUISSON_2;
		else if (c == TILE_BUISSON_2) c = TILE_BUISSON_1;
		else continue;
		
		if (Game_CheckObstacles(x-1, py) == TILE_FLOOR) {
			NoObject_Place(c, x-1, py); 
			NoObject_Place(TILE_FLOOR, x, py);
		}
	}
	
	// Row : Right side
	for (x=BOARD_W-2; x > px; --x) {
		idx = ((unsigned int) y_coord[py]) + x;		
		c = _board[idx] & 0x0F;
		if (c == TILE_BUISSON_1) c = TILE_BUISSON_2;
		else if (c == TILE_BUISSON_2) c = TILE_BUISSON_1;
		else continue;
		
		if (Game_CheckObstacles(x+1, py) == TILE_FLOOR) {
			NoObject_Place(c, x+1, py); 
			NoObject_Place(TILE_FLOOR, x, py);
		}
	}
	
	// Column : Upper side
	for (y=1; y < py; ++y) {
		idx = ((unsigned int) y_coord[y]) + px;		
		c = _board[idx] & 0x0F;
		if (c == TILE_BUISSON_1) c = TILE_BUISSON_2;
		else if (c == TILE_BUISSON_2) c = TILE_BUISSON_1;
		else continue;
		
		if (Game_CheckObstacles(px, y-1) == TILE_FLOOR) {
			NoObject_Place(c, px, y-1);
			NoObject_Place(TILE_FLOOR, px, y);
		}
		
	}
	
	// Column : Lower side
	for (y = BOARD_H-2; y > py ; --y) {
		idx = ((unsigned int) y_coord[y]) + px;		
		c = _board[idx] & 0x0F;
		if (c == TILE_BUISSON_1) c = TILE_BUISSON_2;
		else if (c == TILE_BUISSON_2) c = TILE_BUISSON_1;
		else continue;
			
		if (Game_CheckObstacles(px, y+1) == TILE_FLOOR) {
			NoObject_Place(c, px, y+1);
			NoObject_Place(TILE_FLOOR, px, y);
		}
	}
}

void Game_TimeCreateMonsters () {
    unsigned char xx, yy;
    xx = 0;
    yy = 0;

	_time_create_monster++;
	
    if (_time_create_monster==CHANGE_CAGE_TICK) {
        if (Game_CheckIfBlocked ()) {  // Is there an exit into the cages ?
            if (_player_blocked==0) {  // Stick it up !
				Game_FindEmptySpace ( &xx, &yy);
                Object_SetXY(&_player_Object, xx, yy);
                _player_blocked=1;
            }
        }
        else {
            _player_blocked=0;
        }

        Game_CreateAllCages ( TILE_BLOCK_2);

        /* On regarde si l'on peut creer un monstre */
        if ((_monsterToBeCreated = Game_ComputeNewMonsterPosition ())==1){
            /* mx, my, cx et cy sont initialises */
            /* On remplace le cactus pres duquel va apparaitre le monstre */
            if (Game_RemoveXYObject (_mListOfCactus, _cx, _cy)) {
                ObjectList_PlaceObject(_mListOfCactus, TILE_CACTUS_2, _cx, _cy);
			}
        }
    }

    if (_time_create_monster >= CREATE_MONSTERS_TICK){
        _time_create_monster=0;
		
        if (!_monsterToBeCreated && ObjectList_isEmpty(_mListOfMonsters)){
            Game_ChangeDay ();
		}
        else {
            Game_CreateAllCages ( TILE_BLOCK_1);
            if (Game_RemoveXYObject (_mListOfCactus, _cx, _cy)) {  /* Si cactus existe encore on doit creer un monstre */
                ObjectList_PlaceObject(_mListOfCactus, TILE_CACTUS_1, _cx, _cy);

                if (_monsterToBeCreated) {
                    _monsterToBeCreated=0;
					ObjectList_PlaceObject(_mListOfMonsters, TILE_MONSTER_1, _mx, _my); 
                }
            }
            else { // No more cactus. No need to create monster anymore
                _monsterToBeCreated=0;
            }
        }
    }
}

void Game_TimeMoveMonsters (){
    if (_time_move_monster < MOVE_MONSTERS_TICK) {
		_time_move_monster++;
		return;
	}
    
	_time_move_monster = 0;
    Game_MoveMonsters ();
}

unsigned char Game_MoveMonsterX (struct Object *o, signed char dx){
    unsigned char check;
    signed char incx, incy;

    incx=sign (dx);
    incy=0;
    if (incx==1 || incx==-1) {
        check=Game_CheckObstacles ( o->_x + incx, o->_y + incy);

        /* Hit ? */
        if (check==TILE_PLAYER_NORD || check==TILE_PLAYER_EST || check==TILE_PLAYER_SUD || check==TILE_PLAYER_WEST){
            Game_ChangeLife ();
            return 1;
        }
        else if (check==TILE_FLOOR) {    // Move allowed
            Object_IncXY(o, incx, incy);
            return 1;
        }
        else {
            incy=rand_mod(3) - 1;
            if (incy==0)
                incx=sign (dx);
            else
                incx=0;

            check=Game_CheckObstacles ( o->_x + incx, o->_y + incy);

            /* Hit ? */
            if (check==TILE_PLAYER_NORD || check==TILE_PLAYER_EST || check==TILE_PLAYER_SUD || check==TILE_PLAYER_WEST) {
                Game_ChangeLife ();
                return 1;
            }
            else if (check==TILE_FLOOR) {
                Object_IncXY(o, incx, incy);
                return 1;
            }
        }
    }
    return 0;
}

unsigned char Game_MoveMonsterY (struct Object *o, signed char dy){
    signed char incy, incx;
	incy=sign (dy);
    incx=0;
	
    if (incy==1 || incy==-1){
        unsigned char check=Game_CheckObstacles ( o->_x + incx, o->_y + incy);

        /* Hit ? */
        if (check==TILE_PLAYER_NORD || check==TILE_PLAYER_EST || check==TILE_PLAYER_SUD || check==TILE_PLAYER_WEST){
            Game_ChangeLife ();
            return 1;
        }
        else if (check==TILE_FLOOR){
            Object_IncXY(o, incx, incy);
            return 1;
        }
        else {
            incx=rand_mod(3) -1;
            if (incx==0)
                incy=sign (dy);
            else
                incy=0;

            check=Game_CheckObstacles ( o->_x + incx, o->_y + incy);

            /* Hit ? */
            if (check==TILE_PLAYER_NORD || check==TILE_PLAYER_EST || check==TILE_PLAYER_SUD || check==TILE_PLAYER_WEST){
                Game_ChangeLife ();
                return 1;
            }
            else if (check==TILE_FLOOR) {
                Object_IncXY(o, incx, incy);
                return 1;
            }
        }
    }
    return 0;
}

void Game_MoveMonsters () {
    unsigned char it, xp, yp;
    signed char dx, dy;

	struct Object* po;
	
    xp = _player_Object._x;
    yp = _player_Object._y;

    for ( it = 0; it < MAX_OBJECTS_IN_LIST; ++it) {
		po = &_mListOfMonsters[it];
        if (!po->_valid) continue;

        // Tile change
        if (po->_tileNumber == TILE_MONSTER_1) po->_tileNumber = TILE_MONSTER_2;
        else po->_tileNumber = TILE_MONSTER_1;

        dx=(signed char) (xp - po->_x);
        dy=(signed char) (yp - po->_y);

        // Computing new direction
		if (rand_mod(2) == 0) {
            if (!Game_MoveMonsterX ( po, dx)) Game_MoveMonsterY ( po, dy);
		}
        else{
            /* Si on peut se deplacer en Y on ne se deplace pas en X */
            if (!Game_MoveMonsterY ( po, dy)) {
                Game_MoveMonsterX ( po, dx);
			}
        }
    }
}

unsigned char Game_ComputeNewMonsterPosition () {
    unsigned char x, y, it;
    struct Object* po;
	
	// To select another couple of cactus
	Game_ShakeCactusList();
	
    for ( it = 0; it < MAX_OBJECTS_IN_LIST; ++it){
		po = &_mListOfCactus[it];
        if (!po->_valid) continue;

        x = po->_x;
        y = po->_y;

        _cx=x;
        _cy=y;

        /* X+1, Y */
        if (Game_CheckObstacles ( x+1, y)==TILE_CACTUS_1) {
            if (Game_CheckObstacles ( x, y-1)==TILE_FLOOR) {
                _mx=x;
                _my=y-1;
                return 1;
            }
			
            if (Game_CheckObstacles ( x, y+1)==TILE_FLOOR) {
                _mx=x;
                _my=y+1;
                return 1;
            }
			
            if (Game_CheckObstacles ( x+1, y-1)==TILE_FLOOR) {
                _mx=x+1;
                _my=y-1;
                return 1;
            }
			
            if (Game_CheckObstacles ( x+1, y+1)==TILE_FLOOR) {
                _mx=x+1;
                _my=y+1;
                return 1;
            }
        }

        /* X-1, Y */
        if (Game_CheckObstacles ( x-1, y)==TILE_CACTUS_1) {
            if (Game_CheckObstacles ( x, y-1)==TILE_FLOOR) {
                _mx=x;
                _my=y-1;
                return 1;
            }
			
            if (Game_CheckObstacles ( x, y+1)==TILE_FLOOR) {
                _mx=x;
                _my=y+1;
                return 1;
            }
			
            if (Game_CheckObstacles ( x-1, y-1)==TILE_FLOOR) {
                _mx=x-1;
                _my=y-1;
                return 1;
            }
			
            if (Game_CheckObstacles ( x-1, y+1)==TILE_FLOOR) {
                _mx=x-1;
                _my=y+1;
                return 1;
            }
        }

        /* X, Y+1 */
        if (Game_CheckObstacles ( x, y+1)==TILE_CACTUS_1) {
            if (Game_CheckObstacles ( x-1, y)==TILE_FLOOR) {
                _mx=x-1;
                _my=y;
                return 1;
            }
			
            if (Game_CheckObstacles ( x+1, y)==TILE_FLOOR) {
                _mx=x+1;
                _my=y;
                return 1;
            }
			
            if (Game_CheckObstacles ( x-1, y+1)==TILE_FLOOR) {
                _mx=x-1;
                _my=y+1;
                return 1;
            }
			
            if (Game_CheckObstacles ( x+1, y+1)==TILE_FLOOR)
            {
                _mx=x+1;
                _my=y+1;
                return 1;
            }
        }

        /* X, Y-1 */
        if (Game_CheckObstacles ( x, y-1)==TILE_CACTUS_1) {
            if (Game_CheckObstacles ( x-1, y)==TILE_FLOOR) {
                _mx=x-1;
                _my=y;
                return 1;
            }
			
            if (Game_CheckObstacles ( x+1, y)==TILE_FLOOR){
                _mx=x+1;
                _my=y;
                return 1;
            }
			
            if (Game_CheckObstacles ( x-1, y-1)==TILE_FLOOR){
                _mx=x-1;
                _my=y-1;
                return 1;
            }
			
            if (Game_CheckObstacles ( x+1, y-1)==TILE_FLOOR){
                _mx=x+1;
                _my=y-1;
                return 1;
            }
        }

        /* X+1, Y-1 */
        if (Game_CheckObstacles ( x+1, y-1)==TILE_CACTUS_1){
            if (Game_CheckObstacles ( x, y-1)==TILE_FLOOR){
                _mx=x;
                _my=y-1;
                return 1;
            }
			
            if (Game_CheckObstacles ( x+1, y)==TILE_FLOOR) {
                _mx=x+1;
                _my=y;
                return 1;
            }
        }

        /* X+1, Y+1 */
        if (Game_CheckObstacles ( x+1, y+1)==TILE_CACTUS_1) {
            if (Game_CheckObstacles ( x, y+1)==TILE_FLOOR){
                _mx=x;
                _my=y+1;
                return 1;
            }
			
            if (Game_CheckObstacles ( x+1, y)==TILE_FLOOR) {
                _mx=x+1;
                _my=y;
                return 1;
            }
        }

        /* X-1, Y-1 */
        if (Game_CheckObstacles ( x-1, y-1)==TILE_CACTUS_1) {
            if (Game_CheckObstacles ( x, y-1)==TILE_FLOOR){
                _mx=x;
                _my=y-1;
                return 1;
            }
			
            if (Game_CheckObstacles ( x-1, y)==TILE_FLOOR)
            {
                _mx=x-1;
                _my=y;
                return 1;
            }
        }

        /* X-1, Y+1 */
        if (Game_CheckObstacles ( x-1, y+1)==TILE_CACTUS_1) {
            if (Game_CheckObstacles ( x, y+1)==TILE_FLOOR){
                _mx=x;
                _my=y+1;
                return 1;
            }
			
            if (Game_CheckObstacles ( x-1, y)==TILE_FLOOR){
                _mx=x-1;
                _my=y;
                return 1;
            }
        }
    }

    return 0;
}

unsigned char Game_RemoveXYObject (struct Object* liste, unsigned char x, unsigned char y){
    struct Object* po;
    unsigned char it;

    for ( it = 0; it < MAX_OBJECTS_IN_LIST; ++it){
		po = &liste[it];
        if (!po->_valid) continue;

        if (po->_x==x && po->_y==y){
            Object_Erase(po);
            return 1; /* OK removed */
        }
    }
    return 0; /* Nothing to remove */
}

void Game_ReplaceCactusByMonster (unsigned char x, unsigned char y){
    struct Point aList[MAX_POINTS_IN_LIST];
    unsigned char it, n;

    n = Game_ComputeThreeCactusList (x, y, aList);
    if (n == 0) return; // No cactus to replace

    // Replacing the cactus by a monster
    Game_RemoveXYObject (_mListOfCactus, x, y);

    for (it = 0; it < n; ++it){
        Game_RemoveXYObject (_mListOfCactus, aList[it]._x, aList[it]._y);
    }
    ObjectList_PlaceObject(_mListOfMonsters, TILE_MONSTER_1, x, y);
}

unsigned char Game_ComputeThreeCactusList (unsigned char x, unsigned char y, struct Point* points){
    struct Point l1[MAX_POINTS_IN_LIST];
    struct Point l2[MAX_POINTS_IN_LIST];
	unsigned char n1, n2;
	
    signed char i, j;
    unsigned char check, xx, yy;

	n1 = 0;
    for ( j=-1; j < 2; j++) {  /* -1, 0, 1 */
        for (i=-1; i < 2; i++) {  /* -1, 0, 1 */
            if ((j==0) && (i==0)) continue;
			
            check=Game_CheckObstacles ( x+i, y+j);
            if ( check==TILE_CACTUS_1 || check==TILE_CACTUS_2 ) {
				l1[n1]._x = x+i;
				l1[n1]._y = y+j;
				l1[n1]._y = y+j;
                ++n1;
            }
        }
    }

    if (n1 == 0) return 0; /* Aucun voisin a (x,y) */

    if (n1 > 1) { // At least 3 cactus
        PointList_Copy(points, l1, n1);
        return n1;
    }
	
    /* Taking the single elmt */
    xx = l1[0]._x;
    yy = l1[0]._y;

    /* Making a second list */
    n2 = 0;
    for ( j=-1; j < 2; j++) {  /* -1, 0, 1 */
        for ( i=-1; i < 2; i++) { /* -1, 0, 1 */
            check=Game_CheckObstacles ( xx+i, yy+j);
            if ( check==TILE_CACTUS_1 || check==TILE_CACTUS_2) {
				l2[n2]._x = xx+i;
				l2[n2]._y = yy+j;
				++n2;
            }
        }
    }

    /* The Truth */
    if (n2 == 2) return 0; // No neighbor

    PointList_Copy(points, l2, n2);
	return n2;
}

/* Shake : melange 8 directions (0-7) */
void ShakeDirection (unsigned char *tab_dir) {
    unsigned char tab_indices[8];
    unsigned char  i, j, index, n_indices;

	// Building shaker table
    for ( i=0; i<8; ++i) {
        tab_dir[i]=0;
        tab_indices[i]=i;
    }

    // Mixing directions
    for ( n_indices=8, i=0; i<8; ++i) {
         index=rand() % n_indices;	// system rand
        tab_dir[i]=tab_indices[index] + 1;
        --n_indices;
        for ( j=index; j<n_indices; ++j) {
            tab_indices[j]=tab_indices[j+1];
        }
    }
}

signed char sign (signed char val) {
    if (val < 0) return -1;
    if (val > 0) return 1;
    return 0;
}

unsigned char Game_IsOut (unsigned char x, unsigned char y) {
	if (x > 27) return 1;
	if (y > 17) return 1;
	return 0;
}

unsigned char Game_CheckBounces (unsigned char x, unsigned char y) {
	unsigned int idx;
    if (x > 27) return OUT;
	if (y > 17) return OUT;
	idx = ((unsigned int) y_coord[y]) + x;
    return (_board[idx] & 0x0F);
}

unsigned char Game_CheckObstacles (unsigned char x, unsigned char y) {
	unsigned int idx;
    if (x > 27) return OUT;
	if (y > 17) return OUT;
    if (x >= 11 && x <= 17 && y >= 5 && y <= 11) return TILE_BLOCK_1;
	
	idx = ((unsigned int) y_coord[y]) + x;
    return (_board[idx] & 0x0F);
}

void Game_DrawString(unsigned char x, unsigned char y, char *str) {
	unsigned char *pc;
	unsigned char c;
	             
    pc = str; // initial string
    while (c = *pc) {
		Game_DrawCharacter(x, y, c, color);
		++pc;
		++x;
   }
}

void Game_DrawNumberTile(unsigned char x, unsigned char y, unsigned int v, unsigned char nd) {
  unsigned char bcd[5];
  unsigned char i, xx;
  unsigned char* s;

  bcd[0]=0;
  bcd[1]=0;
  bcd[2]=0;
  bcd[3]=0;
  bcd[4]=0;
  while (v > 9999) {
    v -= 10000; ++bcd[4];
  }
  while (v > 999) {
    v -= 1000; ++bcd[3];
  }
  while (v > 99) {
    v -= 100; ++bcd[2];
  }
  while (v > 9) {
    v -= 10; ++bcd[1];
  }           
  bcd[0]=v;
  xx = x+nd-1; // Right to Left
  
  for (i=0; i<nd; ++i) {	  
	s = (unsigned char*) (numbers + (bcd[i]*8)); // TI99 font
	Game_DrawTile(xx,y,s,PAPER_YELLOW | INK_BLACK);
    --xx;
  }
}

void Game_DrawBoardTile(unsigned char x, unsigned char y) {
	unsigned char* s;
	unsigned char tnum;
	unsigned int idx;
	
	idx = ((unsigned int)  y_coord[y]) + x;
	tnum = _board[idx];

	if ((tnum & 0x80) == 0) return; // No redraw
	tnum &= 0x0F;
	_board[idx] = tnum; // Writing the not drawing attr
	s = (unsigned char*) (sprites + (tnum*8));
	Game_DrawTile(x,y,s,colors[tnum]);
}

unsigned int Game_getVideoAddr(unsigned char x, unsigned char y) {
	uint16_t Y, X, X1, Y1, Y2, Y3;

	X = (unsigned int) x;
	Y = (unsigned int) y;
	Y1 = (Y << 5) & 0x1800; // Bits 12 & 11 : Y7-Y6
	Y2 = (Y << 2) & 0x00E0; // Bits 7, 6 & 5 : Y5, Y4, Y3
	Y3 = (Y << 8) & 0x0700; // Bits 10, 9 & 8 : Y2, Y1 & Y0
	X1 = X & 0x001F; // Bits X4-X0
	return (0x4000 | Y1 | Y2 | Y3 | X1); // Full video address
}

void Game_DrawUpperTile() {
	unsigned char x, y, t, l;
	unsigned char *p, *s;
	
	for (y = 0; y < 3; ++y) {
		for (x = 0; x < 32; ++x) {
			*zx_cxy2aaddr(x, y) = PAPER_YELLOW | INK_BLUE; // Attr : PAPER | INK
		}
	}

	// 2x Cactus
	*zx_cxy2aaddr(5, 1) = PAPER_YELLOW | INK_GREEN;
	*zx_cxy2aaddr(27, 0) = PAPER_YELLOW | INK_GREEN;
	
	// Upper : PIXELS: 32*8bits x 3x8bits
	s = (unsigned char *) upper_tile;
	for (t=0; t < 3; ++t) { // 3 blocks
		for (l = 0; l < 8; ++l) { // 8 lines per block
			y = (t*8) + l;
			for (x = 0; x < 32; ++x) { // 32 columns
				p = (unsigned char*) Game_getVideoAddr(x, y);
				*p = *s;
				++s; // Next byte
			}
		}
	}
}

void Game_DrawLowerTile() {
	unsigned char x, y, t, l;
	unsigned char *p, *s;
	// Lower : 32*8bits x 2x8bits
	s = (unsigned char *) lower_tile;
	for (t=21; t < 23; ++t) { // 2 blocks of 8 bits
		for (l = 0; l < 8; ++l) { // 8 lines per block
			y = (t*8) + l;
			for (x = 0; x < 32; ++x) { // 32 columns
				p = (unsigned char*) Game_getVideoAddr(x, y);
				*p = *s;
				++s; // Next byte
			}
		}
	}
}

void Game_DrawTile(unsigned char x, unsigned char y, unsigned char* t, unsigned char color) {
	unsigned char*  p;
	unsigned char i;
 
	// Object must be shifted to right (+2) and down (+3)
    *zx_cxy2aaddr(x+2, y+3) = color; // Attr : PAPER | INK
    p = (unsigned char*) zx_cxy2saddr(x+2, y+3); // @of the 1st caracher byte
    for (i = 0; i < 8; ++i) {// Drawing the tile
       *p = *t++;
       p += 256; // Next line
    }
}

void Game_DrawCharacter(unsigned char x, unsigned char y, unsigned char c, unsigned char color) {
	unsigned char *p, *s;
	unsigned char i;
	s = ((unsigned char*) 15360 + (c*8)); // points into the rom character set
    *zx_cxy2aaddr(x, y) = color; 
    p = (unsigned char*) zx_cxy2saddr(x, y); // @of the 1st caracher byte
    for (i = 0; i < 8; ++i) {// Drawing the tile
       *p = *s++;
       p += 256; // Next line
    }
}
void Game_DisplayBoardTiles () {
    unsigned char x,y, tnum;
	unsigned char* s;
	unsigned int idx;

	idx = 0;
	for (y=0 ; y<BOARD_H; ++y) {
		for (x=0 ; x<BOARD_W ; ++x) {	
			tnum = _board[idx];
			if ((tnum & 0x80) != 0) {
				tnum &= 0x0F;
				_board[idx] = tnum; // Writing the drawing attr
				s = (unsigned char*) (sprites + (tnum*8));
				Game_DrawTile(x,y,s,colors[tnum]);
			}
			idx++;
		}
	}
}

void Game_DisplayFire(unsigned char draw) {
	unsigned char *s;
	unsigned char tnum;
	unsigned int idx;
	
	if (!_fire_in_progress) return;	
	if ((_fire_x == _player_Object._x) && (_fire_y == _player_Object._y)) return;

	idx = ((unsigned int) y_coord[_fire_y]) + _fire_x;
	tnum = _board[idx] & 0x0F;
	if (tnum != TILE_FLOOR) return; 
	
	if (draw==0) { // Clearing
		tnum=TILE_FLOOR; 
	}	
	else {
		switch (_fire_direction) {
		case NORD:
		case SUD:
			tnum=TILE_TIR_NS; // UP - DOWN
        break;

		case EST:
		case WEST:
			tnum=TILE_TIR_EW; // RIGHT - LEFT
		break;
		
		default: return; // Skip
		break;
		}
	}

	s = ((unsigned char*) sprites + (tnum*8)); 
	Game_DrawTile(_fire_x, _fire_y, s, colors[tnum]);
}

void Game_IntroScreen() {
	unsigned char *s;
	Game_DrawUpperTile(); 
	
	Game_DrawString(2, 5, "move schooner E,X,S,D  keys");
	Game_DrawString(2, 6, "fire missile  Y");
	Game_DrawString(4, 8, "saguard       0      points");
	Game_DrawString(4, 9, "tumbleweed  100      points");
	Game_DrawString(4,10, "morg        150      points");
	Game_DrawString(2,12, "restart game  0");
	Game_DrawString(2,13, "pause game    P");
	Game_DrawString(2,14, "panic button  1");
	Game_DrawString(2,16, "press any key to continue");
	
	Game_DrawString(2,19, "ZX Spectrum remake");
	Game_DrawString(2,20, "BUZZ - xx/yy/2019");
	
	s = ((unsigned char*) sprites + (TILE_CACTUS_1*8)); 
	Game_DrawTile(0, 5, s, PAPER_YELLOW|INK_BLACK);
	s = ((unsigned char*) sprites + (TILE_BUISSON_1*8)); 
	Game_DrawTile(0, 6, s, PAPER_YELLOW|INK_MAGENTA);
	s = ((unsigned char*) sprites + (TILE_MONSTER_1*8)); 
	Game_DrawTile(0, 7, s,¨PAPER_YELLOW|INK_GREEN);
	
	// in_wait_key(); // Wait for a Key press
	while (in_inkey() == 0);
	zx_cls(PAPER_YELLOW);
}

IM2_DEFINE_ISR_8080(isr){
   ++frames;
}

void main () {
	// set up im2 routine
    // newlib has interrupts disabled at this point
    im2_init((void*)0x8000);           // set z80's I register to point at 0x8000, set im2 mode
    memset((void*)0x8000, 0x81, 257);  // make a 257-byte im2 table at 0x8000 containg 0x81 bytes

    // On interrupt the z80 will jump to address 0x8181
    z80_bpoke(0x8181, 0xc3);                // z80 JP instruction
    z80_wpoke(0x8182, (unsigned int)isr);   // to the isr routine
     
    // Now enable interrupts 
    intrinsic_ei();  // inlining ei would disturb the optimizer
	 
	zx_cls(PAPER_YELLOW);
	zx_border(INK_RED);
	
    srand(frames);
	
	_gameState = S_INIT;
	
	Game_IntroScreen();
	
	while(1) { // 0-20ms / loop
		Game_State();
        Game_Process();
    }
}
