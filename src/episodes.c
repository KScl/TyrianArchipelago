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
#include "episodes.h"

#include "config.h"
#include "file.h"
#include "lvlmast.h"
#include "opentyr.h"
#include "varz.h"

// Removed (unreferenced) item types
//JE_ShieldType  shields; // Replaced with "Shield Up" item
//JE_ShipType    ships; // Replaced with custom ship graphics + "Armor Up" items

/* MAIN Weapons Data */
JE_WeaponPortType weaponPort;
JE_WeaponType     weapons[WEAP_NUM + 1]; /* [0..weapnum] */

/* Items */
JE_PowerType   powerSys;
JE_OptionType  options[OPTION_NUM + 1]; /* [0..optionnum] */
JE_SpecialType special;

/* Enemy data */
JE_EnemyDatType enemyDat;

/* EPISODE variables */
JE_byte    initial_episode_num, episodeNum = 0;

/* Tells the game whether the level currently loaded is a bonus level. */
JE_boolean bonusLevel;

/* Tells if the game jumped back to Episode 1 */
JE_boolean jumpBackToEpisode1;

void JE_loadItemDat(FILE *f)
{
#if 0
	FILE *f = NULL;
	
	if (episodeNum <= 3)
	{
		f = dir_fopen_die(data_dir(), "tyrian.hdt", "rb");
		fread_s32_die(&episode1DataLoc, 1, f);
		fseek(f, episode1DataLoc, SEEK_SET);
	}
	else
	{
		// episode 4 stores item data in the level file
		f = dir_fopen_die(data_dir(), levelFile, "rb");

		fseek(f, lvlPos[lvlNum-1], SEEK_SET);
	}
#endif

	enum {
		ITEMNUM_WEAPSHOT = 0,
		ITEMNUM_PORT,
		ITEMNUM_GENERATOR,
		ITEMNUM_SHIP,
		ITEMNUM_OPTION,
		ITEMNUM_SHIELD,
		ITEMNUM_ENEMY,
		ITEMNUM_SPECIAL,
		COUNT_ITEMNUM
	};

	JE_word itemNum[COUNT_ITEMNUM]; /* [1..7] */
	fread_u16_die(itemNum, COUNT_ITEMNUM - 1, f);

	bool isTyrian2000 = (itemNum[ITEMNUM_WEAPSHOT] == 818 && itemNum[ITEMNUM_PORT] == 60);
	if (tyrian2000detected != isTyrian2000)
	{
		fprintf(stderr, "Data file version mismatch detected.");
		JE_tyrianHalt(1);
	}

	// Special item count isn't in data files, and does differ between versions
	itemNum[ITEMNUM_SPECIAL] = isTyrian2000 ? 54 : 46; 

	// Tyrian 2000 splits weapon shot types into 2 sections
	for (int section_num = 0; section_num < (isTyrian2000 ? 2 : 1); ++section_num)
	{
		const int section_start = section_num * 1000;
		const int section_end = section_start + itemNum[ITEMNUM_WEAPSHOT] + 1;

		for (int i = section_start; i < section_end; ++i)
		{
			fread_u16_die(&weapons[i].drain,           1, f);
			fread_u8_die( &weapons[i].shotrepeat,      1, f);
			fread_u8_die( &weapons[i].multi,           1, f);
			fread_u16_die(&weapons[i].weapani,         1, f);
			fread_u8_die( &weapons[i].max,             1, f);
			fread_u8_die( &weapons[i].tx,              1, f);
			fread_u8_die( &weapons[i].ty,              1, f);
			fread_u8_die( &weapons[i].aim,             1, f);
			fread_u8_die(  weapons[i].attack,          8, f);
			fread_u8_die(  weapons[i].del,             8, f);
			fread_s8_die(  weapons[i].sx,              8, f);
			fread_s8_die(  weapons[i].sy,              8, f);
			fread_s8_die(  weapons[i].bx,              8, f);
			fread_s8_die(  weapons[i].by,              8, f);
			fread_u16_die( weapons[i].sg,              8, f);
			fread_s8_die( &weapons[i].acceleration,    1, f);
			fread_s8_die( &weapons[i].accelerationx,   1, f);
			fread_u8_die( &weapons[i].circlesize,      1, f);
			fread_u8_die( &weapons[i].sound,           1, f);
			fread_u8_die( &weapons[i].trail,           1, f);
			fread_u8_die( &weapons[i].shipblastfilter, 1, f);
		}
	}
	
	for (int i = 0; i < itemNum[ITEMNUM_PORT] + 1; ++i)
	{
		Uint8 nameLen;
		fread_u8_die( &nameLen,                   1, f);
		fread_die(    &weaponPort[i].name,    1, 30, f);
		weaponPort[i].name[MIN(nameLen, 30)] = '\0';
		fread_u8_die( &weaponPort[i].opnum,       1, f);
		fread_u16_die( weaponPort[i].op[0],      11, f);
		fread_u16_die( weaponPort[i].op[1],      11, f);
		fread_u16_die(&weaponPort[i].cost,        1, f);
		fread_u16_die(&weaponPort[i].itemgraphic, 1, f);
		fread_u16_die(&weaponPort[i].poweruse,    1, f);
	}

	for (int i = 0; i < itemNum[ITEMNUM_SPECIAL] + 1; ++i)
	{
		Uint8 nameLen;
		fread_u8_die( &nameLen,                1, f);
		fread_die(    &special[i].name,    1, 30, f);
		special[i].name[MIN(nameLen, 30)] = '\0';
		fread_u16_die(&special[i].itemgraphic, 1, f);
		fread_u8_die( &special[i].pwr,         1, f);
		fread_u8_die( &special[i].stype,       1, f);
		fread_u16_die(&special[i].wpn,         1, f);
	}

	for (int i = 0; i < itemNum[ITEMNUM_GENERATOR] + 1; ++i)
	{
		Uint8 nameLen;
		fread_u8_die( &nameLen,                 1, f);
		fread_die(    &powerSys[i].name,    1, 30, f);
		powerSys[i].name[MIN(nameLen, 30)] = '\0';
		fread_u16_die(&powerSys[i].itemgraphic, 1, f);
		fread_u8_die( &powerSys[i].power,       1, f);
		fread_s8_die( &powerSys[i].speed,       1, f);
		fread_u16_die(&powerSys[i].cost,        1, f);
	}

#if 0
	for (int i = 0; i < itemNum[ITEMNUM_SHIP] + 1; ++i)
	{
		Uint8 nameLen;
		fread_u8_die( &nameLen,                 1, f);
		fread_die(    &ships[i].name,       1, 30, f);
		ships[i].name[MIN(nameLen, 30)] = '\0';
		fread_u16_die(&ships[i].shipgraphic,    1, f);
		fread_u16_die(&ships[i].itemgraphic,    1, f);
		fread_u8_die( &ships[i].ani,            1, f);
		fread_s8_die( &ships[i].spd,            1, f);
		fread_u8_die( &ships[i].dmg,            1, f);
		fread_u16_die(&ships[i].cost,           1, f);
		fread_u8_die( &ships[i].bigshipgraphic, 1, f);
		printf("%s - %d %d %d %d\n", ships[i].name, ships[i].shipgraphic, ships[i].itemgraphic, ships[i].spd, ships[i].ani);
	}
#else
	// Skip ships (replaced by "Custom Ships" option and "Armor Up" items)
	fseek(f, 41 * (itemNum[ITEMNUM_SHIP] + 1), SEEK_CUR);
#endif

	for (int i = 0; i < itemNum[ITEMNUM_OPTION] + 1; ++i)
	{
		Uint8 nameLen;
		fread_u8_die(  &nameLen,                1, f);
		fread_die(     &options[i].name,    1, 30, f);
		options[i].name[MIN(nameLen, 30)] = '\0';
		fread_u8_die(  &options[i].pwr,         1, f);
		fread_u16_die( &options[i].itemgraphic, 1, f);
		fread_u16_die( &options[i].cost,        1, f);
		fread_u8_die(  &options[i].tr,          1, f);
		fread_u8_die(  &options[i].option,      1, f);
		fread_s8_die(  &options[i].opspd,       1, f);
		fread_u8_die(  &options[i].ani,         1, f);
		fread_u16_die(  options[i].gr,         20, f);
		fread_u8_die(  &options[i].wport,       1, f);
		fread_u16_die( &options[i].wpnum,       1, f);
		fread_u8_die(  &options[i].ammo,        1, f);
		fread_bool_die(&options[i].stop,           f);
		fread_u8_die(  &options[i].icongr,      1, f);
	}

#if 0
	for (int i = 0; i < itemNum[ITEMNUM_SHIELD] + 1; ++i)
	{
		Uint8 nameLen;
		fread_u8_die( &nameLen,                1, f);
		fread_die(    &shields[i].name,    1, 30, f);
		shields[i].name[MIN(nameLen, 30)] = '\0';
		fread_u8_die( &shields[i].tpwr,        1, f);
		fread_u8_die( &shields[i].mpwr,        1, f);
		fread_u16_die(&shields[i].itemgraphic, 1, f);
		fread_u16_die(&shields[i].cost,        1, f);
	}
#else
	// Skip shields (replaced by "Shield Up" item)
	fseek(f, 37 * (itemNum[ITEMNUM_SHIELD] + 1), SEEK_CUR);
#endif

	// Tyrian 2000 splits enemies into two sections
	for (int section_num = 0; section_num < (isTyrian2000 ? 2 : 1); ++section_num)
	{
		const int section_start = section_num * 1001;
		const int section_end = section_start + itemNum[ITEMNUM_ENEMY] + 1;

		for (int i = section_start; i < section_end; ++i)
		{
			fread_u8_die( &enemyDat[i].ani,           1, f);
			fread_u8_die(  enemyDat[i].tur,           3, f);
			fread_u8_die(  enemyDat[i].freq,          3, f);
			fread_s8_die( &enemyDat[i].xmove,         1, f);
			fread_s8_die( &enemyDat[i].ymove,         1, f);
			fread_s8_die( &enemyDat[i].xaccel,        1, f);
			fread_s8_die( &enemyDat[i].yaccel,        1, f);
			fread_s8_die( &enemyDat[i].xcaccel,       1, f);
			fread_s8_die( &enemyDat[i].ycaccel,       1, f);
			fread_s16_die(&enemyDat[i].startx,        1, f);
			fread_s16_die(&enemyDat[i].starty,        1, f);
			fread_s8_die( &enemyDat[i].startxc,       1, f);
			fread_s8_die( &enemyDat[i].startyc,       1, f);
			fread_u8_die( &enemyDat[i].armor,         1, f);
			fread_u8_die( &enemyDat[i].esize,         1, f);
			fread_u16_die( enemyDat[i].egraphic,     20, f);
			fread_u8_die( &enemyDat[i].explosiontype, 1, f);
			fread_u8_die( &enemyDat[i].animate,       1, f);
			fread_u8_die( &enemyDat[i].shapebank,     1, f);
			fread_s8_die( &enemyDat[i].xrev,          1, f);
			fread_s8_die( &enemyDat[i].yrev,          1, f);
			fread_u16_die(&enemyDat[i].dgr,           1, f);
			fread_s8_die( &enemyDat[i].dlevel,        1, f);
			fread_s8_die( &enemyDat[i].dani,          1, f);
			fread_u8_die( &enemyDat[i].elaunchfreq,   1, f);
			fread_u16_die(&enemyDat[i].elaunchtype,   1, f);
			fread_s16_die(&enemyDat[i].value,         1, f);
			fread_u16_die(&enemyDat[i].eenemydie,     1, f);
		}
	}

	// ------------------------------------------------------------------------
	// Apply patches to item data.
	// I can't be bothered to move this into the JSON patcher yet.
	// ------------------------------------------------------------------------

	// Weapons which were designed for Super Arcade Mode all have very low poweruse settings,
	// because Super Arcade mode does not use generator power. So we set some more reasonable
	// values here, based on their damage output and rate of fire.
	weaponPort[ 6].poweruse =  70; // Protron Z
	weaponPort[31].poweruse =  70; // Guided Bombs
	weaponPort[32].poweruse =  70; // Shuriken Field
	weaponPort[33].poweruse = 120; // Poison Bomb
	weaponPort[36].poweruse =  50; // NortShip Super Pulse
	weaponPort[37].poweruse =  50; // NortShip Spreader
	weaponPort[38].poweruse =  90; // NortShip Spreader B
	weaponPort[39].poweruse = 100; // Atomic RailGun
	weaponPort[37].opnum = 1; // Normally unused mode switch to Spreader B from Spreader A removed

	// Correct typos in item names.
	snprintf(weaponPort[32].name, 30, "Shuriken Field");
	snprintf(powerSys[5].name, 30, "Advanced MicroFusion");
}

void JE_initEpisode(JE_byte newEpisode)
{
	if (newEpisode == episodeNum)
		return;
	
	episodeNum = newEpisode;

	// Temporary: We need to load items at least once
	Sint32 filePosition;
	FILE *f = dir_fopen_die(data_dir(), "tyrian.hdt", "rb");
	fread_s32_die(&filePosition, 1, f);
	fseek(f, filePosition, SEEK_SET);
	JE_loadItemDat(f);
	fclose(f);
}
