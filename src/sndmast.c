/* 
 * OpenTyrian: A modern cross-platform port of Tyrian
 * Copyright (C) 2007-2009  The OpenTyrian Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include "sndmast.h"

#include "opentyr.h"

const char soundTitle[SOUND_COUNT][9] = /* [1..soundnum + 9] of string [8] */
{
	/*   1 */ "SCALEDN2",
	/*   2 */ "F2",
	/*   3 */ "TEMP10",
	/*   4 */ "EXPLSM",
	/*   5 */ "PASS3",
	/*   6 */ "TEMP2",
	/*   7 */ "BYPASS1",
	/*   8 */ "EXP1RT",
	/*   9 */ "EXPLLOW",
	/*  10 */ "TEMP13",
	/*  11 */ "EXPRETAP",
	/*  12 */ "MT2BOOM",
	/*  13 */ "TEMP3",
	/*  14 */ "LAZB",
	/*  15 */ "LAZGUN2",
	/*  16 */ "SPRING",
	/*  17 */ "WARNING",
	/*  18 */ "ITEM",
	/*  19 */ "HIT2",
	/*  20 */ "MACHNGUN",
	/*  21 */ "HYPERD2",
	/*  22 */ "EXPLHUG",
	/*  23 */ "CLINK1",
	/*  24 */ "CLICK",
	/*  25 */ "SCALEDN1",
	/*  26 */ "TEMP11",
	/*  27 */ "TEMP16",
	/*  28 */ "SMALL1",
	/*  29 */ "POWERUP",
	/*  30 */ "MARS3",
	/*  31 */ "NEEDLE2",
	/*  32 */ "",
	/*  33 */ "",
	/*  34 */ "",
	/*  35 */ "",
	/*  36 */ "",
	/*  37 */ "",
	/*  38 */ "",
	/*  39 */ "",
	/*  40 */ "",
	/*  41 */ "",
	/*  42 */ "",
	/*  43 */ "",
	/*  44 */ "",
	/*  45 */ "",
	/*  46 */ "",
	/*  47 */ "",
	/*  48 */ "",
	/*  49 */ "",
	/*  50 */ "",
	/*  51 */ "",
	/*  52 */ "",
	/*  53 */ "",
	/*  54 */ "",
	/*  55 */ "",
	/*  56 */ "",
	/*  57 */ "",
	/*  58 */ "",
	/*  59 */ "",
	/*  60 */ "",
	/*  61 */ "",
	/*  62 */ "",
	/*  63 */ "",
	/*  64 */ "VOICE1",
	/*  65 */ "VOICE2",
	/*  66 */ "VOICE3",
	/*  67 */ "VOICE4",
	/*  68 */ "VOICE5",
	/*  69 */ "VOICE6",
	/*  70 */ "VOICE7",
	/*  71 */ "VOICE8",
	/*  72 */ "VOICE9",
	/*  73 */ "",
	/*  74 */ "",
	/*  75 */ "",
	/*  76 */ "",
	/*  77 */ "",
	/*  78 */ "",
	/*  79 */ "",
	/*  80 */ "",
	/*  81 */ "",
	/*  82 */ "",
	/*  83 */ "",
	/*  84 */ "",
	/*  85 */ "",
	/*  86 */ "",
	/*  87 */ "",
	/*  88 */ "",
	/*  89 */ "",
	/*  90 */ "",
	/*  91 */ "",
	/*  92 */ "",
	/*  93 */ "",
	/*  94 */ "",
	/*  95 */ "",
	/*  96 */ "VOICE1C",
	/*  97 */ "VOICE2C",
	/*  98 */ "VOICE3C",
	/*  99 */ "VOICE4C",
	/* 100 */ "VOICE5C",
	/* 101 */ "VOICE6C",
	/* 102 */ "VOICE7C",
	/* 103 */ "VOICE8C",
	/* 104 */ "VOICE9C",
};

const JE_byte windowTextSamples[9] = /* [1..9] */
{
	V_DANGER,
	V_BOSS,
	V_ENEMIES,
	V_CLEARED_PLATFORM,
	V_DANGER,
	V_SPIKES,
	V_ACCELERATE,
	V_DANGER,
	V_ENEMIES
};
