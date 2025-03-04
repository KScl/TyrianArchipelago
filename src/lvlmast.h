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
#ifndef LVLMAST_H
#define LVLMAST_H

#include "opentyr.h"

#define EVENT_MAXIMUM 2500

#define WEAP_NUM    2000
#define PORT_NUM    64
#define POWER_NUM   6
#define OPTION_NUM  40
#define SHIP_NUM    20
#define SPECIAL_NUM 64

#define ENEMY_NUM   2000

extern const JE_char shapeFile[36];

// ----------------------------------------------------------------------------

struct JE_EventRecType
{
	JE_word     eventtime;
	JE_byte     eventtype;
	JE_integer  eventdat, eventdat2;
	JE_shortint eventdat3, eventdat5, eventdat6;
	JE_byte     eventdat4;
};

extern struct JE_EventRecType eventRec[EVENT_MAXIMUM]; /* [1..eventMaximum] */
extern JE_word maxEvent;

void level_loadFromLevelID(int levelID);

// ----------------------------------------------------------------------------

#define LEVELDATA_COUNT 65

typedef struct {
	Uint16 episodeNum; // Which episode?
	Uint16 episodeLevelID; // Which level in said episode?
	// level_getByEpisode(data[idx].episodeNum, data[idx].episodeLevelID) == idx

	Uint16 levelNum;
	Uint16 levelNumHard;
	Uint8 planetNum;
	Uint8 musicNum;

	// Start of location/shop checks for this level
	Uint16 locStart;
	Uint16 shopStart;

	const char *levelName; // Shown in level
	const char *prettyName; // Shown in menus

	bool endEpisode;
} leveldata_t;

extern int currentLevelID;
extern bool lastLevelCompleted; // False if died/exited last level, true if beaten
extern bool lastLevelFailed; // True if mission failure exit

extern leveldata_t allLevelData[LEVELDATA_COUNT];

int level_getByEpisode(Uint8 episode, Uint8 levelID);

#endif /* LVLMAST_H */
