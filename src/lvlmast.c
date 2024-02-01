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

int currentLevelID = 0;

leveldata_t allLevelData[LEVELDATA_COUNT] = {
	{1,  9, 15,  1, 18,      0, 1000,    "TYRIAN   ", "Tyrian"},
	{1,  4,  0, 13, 26,     10, 1010,    "BUBBLES  ", "Bubbles"},
	{1, 11,  0, 14, 24,     20, 1020,    "HOLES    ", "Holes"},
	{1, 13,  0,  5, 16,     30, 1030,    "SOH JIN  ", "Soh Jin"},
	{1,  1,  0,  2,  1,     40, 1040,    "ASTEROID1", "Asteroid 1"},
	{1,  2,  0,  3,  2,     50, 1050,    "ASTEROID2", "Asteroid 2"},
	{1,  3,  0, 13,  2,     60, 1060,    "ASTEROID?", "Asteroid ?"},
	{1,  6,  0, 14, 23,     70, 1070,    "MINEMAZE ", "MineMaze"},
	{1, 14,  0,  1, 26,     80, 1080,    "WINDY    ", "Windy"},
	{1,  8,  0,  8, 33,     90, 1090,    "SAVARA   ", "Savara"},
	{1, 12,  0,  8, 33,    100, 1100,    "SAVARA II", "Savara II"},
	{1, 10,  0, 15, 18,    110, 1110,    "BONUS    ", "Bonus"},
	{1,  7,  0, 10, 17,    120, 1120,    "MINES    ", "Mines"},
	{1,  5,  0, 10, 14,    130, 1130,    "DELIANI  ", "Deliani"},
	{1, 17,  0,  8, 33,    140, 1140,    "SAVARA V ", "Savara V"},
	{1, 16,  0,  1, 36,    150, 1150,    "ASSASSIN ", "Assassin", true},

	{2,  1,  0, 12, 28,    160, 1160,    "TORM     ", "Torm"},
	{2,  2,  0,  9, 14,    170, 1170,    "GYGES    ", "Gyges"},
	{2,  3,  0, 15, 28,    180, 1180,    "BONUS 1  ", "Bonus 1"},
	{2,  4,  0, 19,  1,    190, 1190,    "ASTCITY  ", "Ast. City"},
	{2, 12,  0, 13, 28,    200, 1200,    "BONUS 2  ", "Bonus 2"},
	{2,  7,  0, 14, 24,    210, 1210,    "GEM WAR  ", "Gem War"},
	{2,  8,  0, 15,  2,    220, 1220,    "MARKERS  ", "Markers"},
	{2,  9,  0, 13, 17,    230, 1230,    "MISTAKES ", "Mistakes"},
	{2,  5,  0,  5,  4,    240, 1240,    "SOH JIN  ", "Soh Jin"},
	{2, 10,  0, 17, 24,    250, 1250,    "BOTANY A ", "Botany A"},
	{2, 11,  0, 18, 13,    260, 1260,    "BOTANY B ", "Botany B"},
	{2,  6,  0,  7, 24,    270, 1270,    "GRYPHON  ", "Gryphon", true},

	{3,  3,  0, 20,  1,    280, 1280,    "GAUNTLET ", "Gauntlet"},
	{3,  4,  0, 11, 14,    290, 1290,    "IXMUCANE ", "Ixmucane"},
	{3,  5,  0, 15, 28,    300, 1300,    "BONUS    ", "Bonus"},
	{3,  7,  0, 14, 16,    310, 1310,    "STARGATE ", "Stargate"},
	{3,  2,  0, 13,  2,    320, 1320,    "AST. CITY", "Ast. City"},
	{3,  9,  0, 14, 14,    330, 1330,    "SAWBLADES", "Sawblades"},
	{3,  1,  0,  6,  4,    340, 1340,    "CAMANIS  ", "Camanis"},
	{3, 12,  0, 15,  7,    350, 1350,    "MACES    ", "Maces"},
	{3,  6,  0, 16, 29,    360, 1360,    "FLEET    ", "Fleet", true},
	{3,  8,  0,  1, 18,    370, 1370,    "TYRIAN X ", "Tyrian X"},
	{3, 10,  0,  8, 21,    380, 1380,    "SAVARA Y ", "Savara Y"},
	{3, 11,  0, 10, 14,    390, 1390,    "NEW DELI ", "New Deli"},

	{4,  4,  0, 11, 38,    400, 1400,    "SURFACE  ", "Surface"},
	{4,  2,  0, 11,  1,    410, 1410,    "WINDY    ", "Windy"},
	{4, 12,  0, 11, 17,    420, 1420,    "LAVA RUN ", "Lava Run"},
	{4,  5,  0, 11, 23,    430, 1430,    "CORE     ", "Core"},
	{4,  9,  0, 11, 29,    440, 1440,    "LAVA EXIT", "Lava Exit"},
	{4, 10,  0, 11,  2,    450, 1450,    "DESERTRUN", "DesertRun"},
	{4,  6,  0, 11, 23,    460, 1460,    "SIDE EXIT", "Side Exit"},
	{4, 11,  0, 11,  6,    470, 1470,    "?TUNNEL? ", "?Tunnel?"},
	{4,  8,  0, 11,  4,    480, 1480,    "ICE EXIT ", "Ice Exit"},
	{4, 20,  0,  6, 35,    490, 1490,    "ICESECRET", "IceSecret"},
	{4,  1,  0, 10, 16,    500, 1500,    "HARVEST  ", "Harvest"},
	{4,  7,  0, 10, 36,    510, 1510,    "UNDERDELI", "UnderDeli"},
	{4, 19,  0,  8, 36,    520, 1520,    "APPROACH ", "Approach"},
	{4,  3,  0,  8, 33,    530, 1530,    "SAVARA IV", "Savara IV"},
	{4, 14,  0, 16, 35,    540, 1540,    "DREAD-NOT", "Dread-Not"},
	{4, 13,  0,  9, 13,    550, 1550,    "EYESPY   ", "EyeSpy"},
	{4, 15,  0,  9, 38,    560, 1560,    "BRAINIAC ", "Brainiac"},
	{4, 16,  0,  1, 18,    570, 1570,    "NOSE DRIP", "Nose Drip", true},

	{5,  5,  0,  4, 35,    580, 1580,    "ASTEROIDS", "Asteroids"},
	{5,  2,  0,  2,  2,    590, 1590,    "AST ROCK ", "Ast. Rock"},
	{5,  6,  0,  5,  6,    600, 1600,    "MINERS   ", "Miners"},
	{5,  4,  0,  8, 21,    610, 1610,    "SAVARA   ", "Savara"},
	{5,  7,  0,  6, 37,    620, 1620,    "CORAL    ", "Coral"},
	{5,  3,  0,  9, 23,    630, 1630,    "STATION  ", "Station"},
	{5,  8,  0, 13, 14,    640, 1640,    "FRUIT    ", "Fruit", true},
};

bool allCompletions[LEVELDATA_COUNT] = {false};

void level_loadFromLevelID(int levelID)
{
	assert(levelID >= 0);
	currentLevelID = levelID;

	Uint16 totalLevelCount;
	Sint32 filePosition;

	SDL_strlcpy(levelName, allLevelData[levelID].levelName, 10);
	levelSong = allLevelData[levelID].musicNum;

	sprintf(tempStr, "tyrian%hd.lvl", allLevelData[levelID].episodeNum);
	FILE *f = dir_fopen_die(data_dir(), tempStr, "rb");

	fread_u16_die(&totalLevelCount, 1, f);

	if (allLevelData[levelID].episodeNum <= 3) // Stored "globally" in tyrian.hdt
	{
		FILE *global_f = dir_fopen_die(data_dir(), "tyrian.hdt", "rb");
		fread_s32_die(&filePosition, 1, global_f);
		fseek(global_f, filePosition, SEEK_SET);

		JE_loadItemDat(global_f);
		fclose(global_f);
	}
	else // Stored as the last "level" in the level file
	{
		fseek(f, (totalLevelCount - 1) * sizeof(Sint32), SEEK_CUR);
		fread_s32_die(&filePosition, 1, f);
		fseek(f, filePosition, SEEK_SET);

		JE_loadItemDat(f);
		fseek(f, 2, SEEK_SET);
	}

	Uint16 baseLevelNum = allLevelData[levelID].levelNum;
	if (allLevelData[levelID].levelNumHard && difficultyLevel >= DIFFICULTY_HARD)
		baseLevelNum = allLevelData[levelID].levelNumHard;

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
	                   allLevelData[levelID].episodeNum, baseLevelNum);

	JE_loadMapData(f);
	level_loadEvents(f);
	JE_loadMapShapes(f, char_shapeFile);

	fclose(f);
}

// Search by episode number (as given, starts from 1), and level ID (zero indexed).
// Returns ID of given level, or -1 if no such level.
int level_getByEpisode(Uint8 episode, Uint8 levelID)
{
	switch (episode)
	{
		case 1: break;
		case 2: levelID += 16; break;
		case 3: levelID += 28; break;
		case 4: levelID += 40; break;
		case 5: levelID += 58; break;
		default: return -1;
	}
	if (levelID >= COUNTOF(allLevelData) || allLevelData[levelID].episodeNum != episode)
		return -1;
	return levelID;
}
