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
#ifndef OPENTYR_H
#define OPENTYR_H

#include "SDL_types.h"

#include <math.h>
#include <stdbool.h>

#define COUNTOF(x) ((unsigned)(sizeof(x) / sizeof *(x)))  // use only on arrays!
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#ifndef M_PI
#define M_PI    3.14159265358979323846  // pi
#endif
#ifndef M_PI_2
#define M_PI_2  1.57079632679489661923  // pi/2
#endif
#ifndef M_PI_4
#define M_PI_4  0.78539816339744830962  // pi/4
#endif

typedef unsigned int uint;
typedef unsigned long ulong;

// Pascal types, yuck.
typedef Sint32 JE_longint;
typedef Sint16 JE_integer;
typedef Sint8  JE_shortint;
typedef Uint16 JE_word;
typedef Uint8  JE_byte;
typedef bool   JE_boolean;
typedef char   JE_char;
typedef float  JE_real;

// Not used: We support either version.
//#define TYRIAN_VERSION "2.1"

extern const char *opentyrian_str;
extern const char *opentyrian_version;

extern bool tyrian2000detected;

void setupMenu(void);

void tyrianError(const char *msg, ...);

// ------------------------------------------------------------------
// Debugging Cheats
// ------------------------------------------------------------------

// Enables debug options like DPS tests
#define DEBUG_OPTIONS

// Show enemy health on screen numerically, useful when making logic.
//#define DEBUG_SHOW_ENEMY_HEALTH

// Spawns item drops normally reserved for Arcade modes, useful when making logic.
//#define DEBUG_ARCADE_ITEM_DROPS

// Enables "Unbounded" speed option for quickly advancing.
//#define UNBOUNDED_SPEED

// Simplifies cheats to F2=invuln, F3=level skip (fail), F4=level skip, F10=debug
//#define SIMPLIFIED_LEVEL_CHEATS

#endif /* OPENTYR_H */
