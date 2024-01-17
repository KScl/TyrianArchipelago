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
#include "lvlmast.h"

#include "file.h"
#include "opentyr.h"
#include "tyrian2.h"

#include <assert.h>

#include "archipelago/patcher.h"

const JE_char shapeFile[34] = /* [1..34] */
{
	'2', '4', '7', '8', 'A', 'B', 'C', 'D', 'E', 'F',
	'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
	'Q', 'R', 'S', 'T', 'U', '5', '#', 'V', '0', '@',  // [25] should be '&' rather than '5'
	'3', '^', '5', '9'
};

// ----------------------------------------------------------------------------

struct JE_EventRecType eventRec[EVENT_MAXIMUM]; /* [1..eventMaximum] */
JE_word maxEvent;

static void level_loadEvents(FILE *level_f)
{
	fread_u16_die(&maxEvent, 1, level_f);

	Uint16 x = 0;
	while (x < maxEvent)
	{
		int skips;
		if ((skips = Patcher_DoPatch(&x)))
		{
			fseek(level_f, 11*skips, SEEK_CUR);
			continue;
		}

		fread_u16_die(&eventRec[x].eventtime, 1, level_f);
		fread_u8_die( &eventRec[x].eventtype, 1, level_f);
		fread_s16_die(&eventRec[x].eventdat,  1, level_f);
		fread_s16_die(&eventRec[x].eventdat2, 1, level_f);
		fread_s8_die( &eventRec[x].eventdat3, 1, level_f);
		fread_s8_die( &eventRec[x].eventdat5, 1, level_f);
		fread_s8_die( &eventRec[x].eventdat6, 1, level_f);
		fread_u8_die( &eventRec[x].eventdat4, 1, level_f);
		++x;
	}
	eventRec[x].eventtime = 65500;  /*Not needed but just in case*/
}

// ----------------------------------------------------------------------------

static struct {
	Uint16 episodeNum;
	Uint16 levelNum;
	Uint16 levelNumHard;
	Uint8 musicNum;
	const char *levelName;
	bool endEpisode;
} levelData[] = {
	// ------ Episode Level LHard Music   Name
	/* 01 */ {      1,    9,   15,   18, "TYRIAN   "},
	/* 02 */ {      1,    4,    0,   26, "BUBBLES  "},
	/* 03 */ {      1,   11,    0,   24, "HOLES    "},
	/* 04 */ {      1,   13,    0,   16, "SOH JIN  "},
	/* 05 */ {      1,    1,    0,    1, "ASTEROID1"},
	/* 06 */ {      1,    2,    0,    2, "ASTEROID2"},
	/* 07 */ {      1,    3,    0,    2, "ASTEROID?"},
	/* 08 */ {      1,    6,    0,   23, "MINEMAZE "},
	/* 09 */ {      1,   14,    0,   26, "WINDY    "},
	/* 10 */ {      1,   12,    0,   33, "SAVARA II"},
	/* 11 */ {      1,    8,    0,   33, "SAVARA   "},
	/* 12 */ {      1,   10,    0,   18, "BONUS    "},
	/* 13 */ {      1,    7,    0,   17, "MINES    "},
	/* 14 */ {      1,    5,    0,   14, "DELIANI  "},
	/* 15 */ {      1,   17,    0,   33, "SAVARA V "},
	/* 16 */ {      1,   16,    0,   36, "ASSASSIN ", true},

	/* 17 */ {      2,    1,    0,   28, "TORM     "},
	/* 18 */ {      2,    2,    0,   14, "GYGES    "},
	/* 19 */ {      2,    3,    0,   28, "BONUS 1  "},
	/* 20 */ {      2,    4,    0,    1, "ASTCITY  "},
	/* 21 */ {      2,   12,    0,   28, "BONUS 2  "},
	/* 22 */ {      2,    7,    0,   24, "GEM WAR  "},
	/* 23 */ {      2,    8,    0,    2, "MARKERS  "},
	/* 24 */ {      2,    9,    0,   17, "MISTAKES "},
	/* 25 */ {      2,    5,    0,    4, "SOH JIN  "},
	/* 26 */ {      2,   10,    0,   24, "BOTANY A "},
	/* 27 */ {      2,   11,    0,   13, "BOTANY B "},
	/* 28 */ {      2,    6,    0,   24, "GRYPHON  ", true},

	/* 29 */ {      3,    3,    0,    1, "GAUNTLET "},
	/* 30 */ {      3,    4,    0,   14, "IXMUCANE "},
	/* 31 */ {      3,    5,    0,   28, "BONUS    "},
	/* 32 */ {      3,    7,    0,   16, "STARGATE "},
	/* 33 */ {      3,    2,    0,    2, "AST. CITY"},
	/* 34 */ {      3,    9,    0,   14, "SAWBLADES"},
	/* 35 */ {      3,    1,    0,    4, "CAMANIS  "},
	/* 36 */ {      3,   12,    0,    7, "MACES    "},
	/* 37 */ {      3,    6,    0,   29, "FLEET    ", true},
	/* 38 */ {      3,    8,    0,   18, "TYRIAN X "},
	/* 39 */ {      3,   10,    0,   21, "SAVARA Y "},
	/* 40 */ {      3,   11,    0,   14, "NEW DELI "},

	/* 41 */ {      4,    4,    0,   38, "SURFACE  "},
	/* 42 */ {      4,    2,    0,    1, "WINDY    "},
	/* 43 */ {      4,   12,    0,   17, "LAVA RUN "},
	/* 44 */ {      4,    5,    0,   23, "CORE     "},
	/* 45 */ {      4,    9,    0,   29, "LAVA EXIT"},
	/* 46 */ {      4,   10,    0,    2, "DESERTRUN"},
	/* 47 */ {      4,    6,    0,   23, "SIDE EXIT"},
	/* 48 */ {      4,   11,    0,    6, "?TUNNEL? "},
	/* 49 */ {      4,    8,    0,    4, "ICE EXIT "},
	/* 50 */ {      4,   20,    0,   35, "ICESECRET"},
	/* 51 */ {      4,    1,    0,   16, "HARVEST  "},
	/* 52 */ {      4,    7,    0,   36, "UNDERDELI"},
	/* 53 */ {      4,   19,    0,   36, "APPROACH "},
	/* 54 */ {      4,    3,    0,   33, "SAVARA IV"},
	/* 55 */ {      4,   14,    0,   35, "DREAD-NOT"},
	/* 56 */ {      4,   13,    0,   13, "EYESPY   "},
	/* 57 */ {      4,   15,    0,   38, "BRAINIAC "},
	/* 58 */ {      4,   16,    0,   18, "NOSE DRIP", true},

	/* 59 */ {      5,    5,    0,   35, "ASTEROIDS"},
	/* 60 */ {      5,    2,    0,    2, "AST ROCK "},
	/* 61 */ {      5,    6,    0,    6, "MINERS   "},
	/* 62 */ {      5,    4,    0,   21, "SAVARA   "},
	/* 63 */ {      5,    7,    0,   37, "CORAL    "},
	/* 64 */ {      5,    3,    0,   23, "STATION  "},
	/* 65 */ {      5,    8,    0,   14, "FRUIT    ", true},
};

void level_loadFromLevelID(Uint16 levelID)
{
	// Level ID we get is 1-based, convert to 0-based
	assert(levelID != 0);
	--levelID;

	Uint16 totalLevelCount;
	Sint32 filePosition;

	SDL_strlcpy(levelName, levelData[levelID].levelName, 10);
	levelSong = levelData[levelID].musicNum;

	sprintf(tempStr, "tyrian%hd.lvl", levelData[levelID].episodeNum);
	FILE *f = dir_fopen_die(data_dir(), tempStr, "rb");

	fread_u16_die(&totalLevelCount, 1, f);
	printf("%hu\n", totalLevelCount);

	if (levelData[levelID].episodeNum <= 3) // Stored "globally" in tyrian.hdt
	{
		printf("Initting globally\n");
		FILE *global_f = dir_fopen_die(data_dir(), "tyrian.hdt", "rb");
		fread_s32_die(&filePosition, 1, global_f);
		fseek(global_f, filePosition, SEEK_SET);

		JE_loadItemDat(global_f);
		fclose(global_f);
	}
	else // Stored as the last "level" in the level file
	{
		printf("Initting locally\n");
		fseek(f, (totalLevelCount - 1) * sizeof(Sint32), SEEK_CUR);
		fread_s32_die(&filePosition, 1, f);
		fseek(f, filePosition, SEEK_SET);

		JE_loadItemDat(f);
		fseek(f, 2, SEEK_SET);
	}

	Uint16 baseLevelNum = levelData[levelID].levelNum;
	if (levelData[levelID].levelNumHard && difficultyLevel >= DIFFICULTY_HARD)
		baseLevelNum = levelData[levelID].levelNumHard;

	Uint16 levelFileNum = (baseLevelNum - 1) * 2;
	if (levelFileNum > totalLevelCount)
		levelFileNum = 0;

	fseek(f, levelFileNum * sizeof(Sint32), SEEK_CUR);
	fread_s32_die(&filePosition, 1, f);
	fseek(f, filePosition, SEEK_SET);

	// ------------------------------------------------------------------------

	JE_char char_shapeFile;
	fseek(f, 1, SEEK_CUR); // char_mapFile (unused)
	fread_die(&char_shapeFile, 1, 1, f);

	Patcher_ReadyPatch(tyrian2000detected ? "Tyrian 2000" : "Tyrian 2.1",
	                   levelData[levelID].episodeNum,
	                   levelData[levelID].levelNum);

	JE_loadMapData(f);
	level_loadEvents(f);
	JE_loadMapShapes(f, char_shapeFile);

	fclose(f);
}
