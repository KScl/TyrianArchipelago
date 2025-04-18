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
#include "varz.h"

#include "config.h"
#include "episodes.h"
#include "joystick.h"
#include "lds_play.h"
#include "loudness.h"
#include "mainint.h"
#include "mouse.h"
#include "mtrand.h"
#include "network.h"
#include "nortsong.h"
#include "nortvars.h"
#include "opentyr.h"
#include "shots.h"
#include "sprite.h"
#include "vga256d.h"
#include "video.h"

#include "archipelago/apconnect.h"

JE_integer tempDat, tempDat2, tempDat3;

const JE_byte SANextShip[SA + 2] /* [0..SA + 1] */ = { 3, 9, 6, 2, 5, 1, 4, 3, 7 }; // 0 -> 3 -> 2 -> 6 -> 4 -> 5 -> 1 -> 9 -> 7
const JE_word SASpecialWeapon[SA] /* [1..SA] */  = { 7, 8, 9, 10, 11, 12, 13 };
const JE_word SASpecialWeaponB[SA] /* [1..SA] */ = {37, 6, 15, 40, 16, 14, 41 };
const JE_byte SAShip[SA] /* [1..SA] */ = { 3, 1, 5, 10, 2, 11, 12 };
const JE_word SAWeapon[SA][5] /* [1..SA, 1..5] */ =
{  /*  R  Bl  Bk  G   P */
	{  9, 31, 32, 33, 34 },  /* Stealth Ship */
	{ 19,  8, 22, 41, 34 },  /* StormWind    */
	{ 27,  5, 20, 42, 31 },  /* Techno       */
	{ 15,  3, 28, 22, 12 },  /* Enemy        */
	{ 23, 35, 25, 14,  6 },  /* Weird        */
	{  2,  5, 21,  4,  7 },  /* Unknown      */
	{ 40, 38, 37, 41, 36 }   /* NortShip Z   */
};

const JE_byte specialArcadeWeapon[PORT_NUM] /* [1..Portnum] */ =
{
	17,17,18,0,0,0,10,0,0,0,0,0,44,0,10,0,19,0,0,-0,0,0,0,0,0,0,
	-0,0,0,0,45,0,0,0,0,0,0,0,0,0,0,0
};

const JE_byte optionSelect[16][3][2] /* [0..15, 1..3, 1..2] */ =
{	/*  MAIN    OPT    FRONT */
	{ { 0, 0},{ 0, 0},{ 0, 0} },  /**/
	{ { 1, 1},{16,16},{30,30} },  /*Single Shot*/
	{ { 2, 2},{29,29},{29,20} },  /*Dual Shot*/
	{ { 3, 3},{21,21},{12, 0} },  /*Charge Cannon*/
	{ { 4, 4},{18,18},{16,23} },  /*Vulcan*/
	{ { 0, 0},{ 0, 0},{ 0, 0} },  /**/
	{ { 6, 6},{29,16},{ 0,22} },  /*Super Missile*/
	{ { 7, 7},{19,19},{19,28} },  /*Atom Bomb*/
	{ { 0, 0},{ 0, 0},{ 0, 0} },  /**/
	{ { 0, 0},{ 0, 0},{ 0, 0} },  /**/
	{ {10,10},{21,21},{21,27} },  /*Mini Missile*/
	{ { 0, 0},{ 0, 0},{ 0, 0} },  /**/
	{ { 0, 0},{ 0, 0},{ 0, 0} },  /**/
	{ {13,13},{17,17},{13,26} },  /*MicroBomb*/
	{ { 0, 0},{ 0, 0},{ 0, 0} },  /**/
	{ {15,15},{15,16},{15,16} }   /*Post-It*/
};

const JE_word linkGunWeapons[38] /* [1..38] */ =
{
	0,0,0,0,0,0,0,0,444,445,446,447,0,448,449,0,0,0,0,0,450,451,0,506,0,564,
	  445,446,447,448,449,445,446,447,448,449,450,451
};
const JE_word chargeGunWeapons[38] /* [1..38] */ =
{
	0,0,0,0,0,0,0,0,476,458,464,482,0,488,470,0,0,0,0,0,494,500,0,528,0,558,
	  458,458,458,458,458,458,458,458,458,458,458,458
};
const JE_byte randomEnemyLaunchSounds[3] /* [1..3] */ = {13,6,26};

/* YKS: Twiddle cheat sheet:
 * 1: UP
 * 2: DOWN
 * 3: LEFT
 * 4: RIGHT
 * 5: UP+FIRE
 * 6: DOWN+FIRE
 * 7: LEFT+FIRE
 * 8: RIGHT+FIRE
 * 9: Release all keys (directions and fire)
 */
const JE_byte keyboardCombos[26][8] /* [1..26, 1..8] */ =
{
	{ 2, 1,   2,   5, 137,           0, 0, 137}, /*  1 Invulnerability*/
	{ 4, 3,   2,   5, 138,           0, 0, 138}, /*  2 Atom Bomb*/
	{ 3, 4,   6, 139,             0, 0, 0, 139}, /*  3 Seeker Bombs*/
	{ 2, 5, 142,               0, 0, 0, 0, 142}, /*  4 Ice Blast*/
	{ 6, 2,   6, 143,             0, 0, 0, 143}, /*  5 Auto Repair*/
	{ 6, 7,   5,   8,   6,   7,  5,        112}, /*  6 Spin Wave*/
	{ 7, 8, 101,               0, 0, 0, 0, 101}, /*  7 Repulsor*/
	{ 1, 7,   6, 146,             0, 0, 0, 146}, /*  8 Protron Field*/
	{ 8, 6,   7,   1, 120,           0, 0, 120}, /*  9 Minefield*/
	{ 3, 6,   8,   5, 121,           0, 0, 121}, /* 10 Post-It Blast*/
	{ 1, 2,   7,   8, 119,           0, 0, 119}, /* 11 Drone Ship - TBC*/
	{ 3, 4,   3,   6, 123,           0, 0, 123}, /* 12 Repair Player 2*/
	{ 6, 7,   5,   8, 124,           0, 0, 124}, /* 13 Super Bomb - TBC*/
	{ 1, 6, 125,               0, 0, 0, 0, 125}, /* 14 Hot Dog*/
	{ 9, 5, 126,               0, 0, 0, 0, 126}, /* 15 Lightning UP      */
	{ 1, 7, 127,               0, 0, 0, 0, 127}, /* 16 Lightning UP+LEFT */
	{ 1, 8, 128,               0, 0, 0, 0, 128}, /* 17 Lightning UP+RIGHT*/
	{ 9, 7, 129,               0, 0, 0, 0, 129}, /* 18 Lightning    LEFT */
	{ 9, 8, 130,               0, 0, 0, 0, 130}, /* 19 Lightning    RIGHT*/
	{ 4, 2,   3,   5, 131,           0, 0, 131}, /* 20 Warfly            */
	{ 3, 1,   2,   8, 132,           0, 0, 132}, /* 21 FrontBlaster      */
	{ 2, 4,   5, 133,             0, 0, 0, 133}, /* 22 Gerund            */
	{ 3, 4,   2,   8, 134,           0, 0, 134}, /* 23 FireBomb          */
	{ 1, 4,   6, 135,             0, 0, 0, 135}, /* 24 Indigo            */
	{ 1, 3,   6, 137,             0, 0, 0, 137}, /* 25 Invulnerability [easier] */
	{ 1, 4,   3,   4,   7, 136,         0, 136}  /* 26 D-Media Protron Drone    */
};

const JE_byte shipCombosB[21] /* [1..21] */ =
	{15,16,17,18,19,20,21,22,23,24, 7, 8, 5,25,14, 4, 6, 3, 9, 2,26};
  /*!! SUPER Tyrian !!*/
const JE_byte superTyrianSpecials[4] /* [1..4] */ = {1,2,4,5};

const JE_byte shipCombos[14][3] /* [0..12, 1..3] */ =
{
	{ 5, 4, 7},  /*2nd Player ship*/
	{ 1, 2, 0},  /*USP Talon*/
	{14, 4, 0},  /*Super Carrot*/
	{ 4, 5, 0},  /*Gencore Phoenix*/
	{ 6, 5, 0},  /*Gencore Maelstrom*/
	{ 7, 8, 0},  /*MicroCorp Stalker*/
	{ 7, 9, 0},  /*MicroCorp Stalker-B*/
	{10, 3, 5},  /*Prototype Stalker-C*/
	{ 5, 8, 9},  /*Stalker*/
	{ 1, 3, 0},  /*USP Fang*/
	{ 7,16,17},  /*U-Ship*/
	{ 2,11,12},  /*1st Player ship*/
	{ 3, 8,10},  /*Nort ship*/
	{ 0, 0, 0}   // Dummy entry added for Stalker 21.126
};

/*Street-Fighter Commands*/
JE_byte SFCurrentCode[2][21]; /* [1..2, 1..21] */
JE_byte SFExecuted[2]; /* [1..2] */

/*Special General Data*/
JE_byte lvlFileNum;
JE_word eventLoc;
/*JE_word maxenemies;*/
JE_word tempBackMove, explodeMove; /*Speed of background movement*/
JE_byte levelEnd;
JE_word levelEndFxWait;
JE_shortint levelEndWarp;
JE_boolean endLevel, reallyEndLevel, waitToEndLevel, playerEndLevel,
           normalBonusLevelCurrent, bonusLevelCurrent,
           smallEnemyAdjust, readyToEndLevel, quitRequested;

JE_byte newPL[10]; /* [0..9] */ /*Eventsys event 75 parameter*/
JE_word returnLoc;
JE_boolean returnActive;
JE_word galagaShotFreq;
JE_longint galagaLife;

JE_boolean debug = false; /*Debug Mode*/
Uint32 debugTime, lastDebugTime;
JE_longint debugHistCount;
JE_real debugHist;
JE_word curLoc; /*Current Pixel location of background 1*/

JE_boolean firstGameOver, gameLoaded, enemyStillExploding;

/* Destruction Ratio */
JE_word totalEnemy;
JE_word enemyKilled;

/* Shape/Map Data - All in one Segment! */
struct JE_MegaDataType1 megaData1;
struct JE_MegaDataType2 megaData2;
struct JE_MegaDataType3 megaData3;

/* Secret Level Display */
JE_byte flash;
JE_shortint flashChange;
JE_byte displayTime;

/* Demo Stuff */
bool play_demo = false, record_demo = false, stopped_demo = false;
Uint8 demo_num = 0;
FILE *demo_file = NULL;

Uint8 demo_keys;
Uint16 demo_keys_wait;

/* Sound Effects Queue */
JE_byte soundQueue[8]; /* [0..7] */

/*Level Event Data*/
JE_boolean enemyContinualDamage;
JE_boolean enemiesActive;
JE_boolean forceEvents;
JE_boolean stopBackgrounds;
JE_byte stopBackgroundNum;
JE_byte damageRate;  /*Rate at which a player takes damage*/
JE_boolean background3x1;  /*Background 3 enemies use Background 1 X offset*/
JE_boolean background3x1b; /*Background 3 enemies moved 8 pixels left*/

JE_boolean levelTimer;
JE_word    levelTimerCountdown;
JE_word    levelTimerJumpTo;
JE_boolean randomExplosions;

//JE_boolean editShip1, editShip2;

// No longer bool, but int (to be able to count events)
JE_byte globalFlags[10]; /* [1..10] */
JE_byte levelSong;

/* MapView Data */
//JE_word mapOrigin, mapPNum;
//JE_byte mapPlanet[5], mapSection[5]; /* [1..5] */

/* Interface Constants */
//JE_boolean moveTyrianLogoUp;
JE_boolean skipStarShowVGA;

/*EnemyData*/
JE_MultiEnemyType enemy;
JE_EnemyAvailType enemyAvail;  /* values: 0: used, 1: free, 2: secret pick-up */
JE_word enemyOffset;
JE_word enemyOnScreen;
JE_word superEnemy254Jump;

/*EnemyShotData*/
JE_boolean fireButtonHeld;
JE_boolean enemyShotAvail[ENEMY_SHOT_MAX]; /* [1..Enemyshotmax] */
EnemyShotType enemyShot[ENEMY_SHOT_MAX]; /* [1..Enemyshotmax]  */

/* Player Shot Data */
JE_byte     zinglonDuration;
JE_byte     astralDuration;
JE_word     flareDuration;
JE_boolean  flareStart;
JE_shortint flareColChg;
JE_byte     specialWait;
JE_byte     nextSpecialWait;
JE_boolean  spraySpecial;
JE_byte     doIced;
JE_boolean  infiniteShot;

/*PlayerData*/
JE_boolean allPlayersGone; /*Both players dead and finished exploding*/

const uint shadowYDist = 10;

JE_real optionSatelliteRotate;

JE_integer optionAttachmentMove;
JE_boolean optionAttachmentLinked, optionAttachmentReturn;

JE_byte chargeWait, chargeLevel, chargeMax, chargeGr, chargeGrWait;

JE_word neat;

/*ExplosionData*/
Explosion explosions[MAX_EXPLOSIONS]; /* [1..ExplosionMax] */
JE_integer explosionFollowAmountX, explosionFollowAmountY;

/*Repeating Explosions*/
rep_explosion_type rep_explosions[MAX_REPEATING_EXPLOSIONS]; /* [1..20] */

/*SuperPixels*/
superpixel_type superpixels[MAX_SUPERPIXELS]; /* [0..MaxSP] */
unsigned int last_superpixel;

/*Temporary Numbers*/
JE_byte temp, temp2, temp3;
JE_word tempW;

JE_word x, y;
JE_integer b;

JE_byte **BKwrap1to, **BKwrap2to, **BKwrap3to,
        **BKwrap1, **BKwrap2, **BKwrap3;

JE_shortint specialWeaponFilter, specialWeaponFreq;
JE_word     specialWeaponWpn;
JE_boolean  linkToPlayer;

JE_word shipGr, shipGr2;
Sprite2_array *shipGrPtr, *shipGr2ptr;

void JE_drawOptions(void)
{
	SDL_Surface *temp_surface = VGAScreen;
	VGAScreen = VGAScreenSeg;

	Player *this_player = &player[twoPlayerMode ? 1 : 0];

	for (uint i = 0; i < COUNTOF(this_player->sidekick); ++i)
	{
		JE_OptionType *this_option = &options[this_player->items.sidekick[i]];

		this_player->sidekick[i].ammo =
		this_player->sidekick[i].ammo_max = this_option->ammo;

		this_player->sidekick[i].ammo_refill_ticks =
		this_player->sidekick[i].ammo_refill_ticks_max = (105 - this_player->sidekick[i].ammo) * 4;

		this_player->sidekick[i].style = this_option->tr;

		this_player->sidekick[i].animation_enabled = (this_option->option == 1);
		this_player->sidekick[i].animation_frame = 0;

		this_player->sidekick[i].charge = 0;
		this_player->sidekick[i].charge_ticks = 20;

		// draw initial sidekick HUD
		const int y = hud_sidekick_y[twoPlayerMode ? 1 : 0][i];

		fill_rectangle_xy(VGAScreenSeg, 284, y, 284 + 28, y + 15, 0);
		if (this_option->icongr > 0)
			blit_sprite(VGAScreenSeg, 284, y, OPTION_SHAPES, this_option->icongr - 1);  // sidekick HUD icon
		draw_segmented_gauge(VGAScreenSeg, 284, y + 13, 112, 2, 2, MAX(1, this_player->sidekick[i].ammo_max / 10), this_player->sidekick[i].ammo);
	}

	VGAScreen = temp_surface;

	JE_drawOptionLevel();
}

void JE_drawOptionLevel(void)
{
	if (twoPlayerMode)
	{
		for (temp = 1; temp <= 3; temp++)
		{
			fill_rectangle_xy(VGAScreenSeg, 268, 127 + (temp - 1) * 6, 269, 127 + 3 + (temp - 1) * 6, 193 + ((player[1].items.sidekick_level - 100) == temp) * 11);
		}
	}
}

void JE_tyrianHalt(JE_byte code)
{
	deinit_audio();
	deinit_video();
	deinit_joysticks();

	/* TODO: NETWORK */

	sprites_freeInterfaceSprites();
	sprites_freeMainShapeTables();

	free_sprite2s(&shopSpriteSheet);
	free_sprite2s(&explosionSpriteSheet);
	free_sprite2s(&destructSpriteSheet);

	for (int i = 0; i < SOUND_COUNT; i++)
	{
		free(soundSamples[i]);
	}

	if (code != 9)
	{
		/*
		TODO?
		JE_drawANSI("exitmsg.bin");
		JE_gotoXY(1,22);*/

		Archipelago_Disconnect(); // Graceful server disconnect, save data
		config_save();
	}

	/* endkeyboard; */

	if (code == 9)
	{
		/* OutputString('call=file0002.EXE' + #0'); TODO? */
	}

	if (code == 5)
	{
		code = 0;
	}

	SDL_Quit();
	exit(code);
}

void JE_specialComplete(JE_byte playerNum, JE_byte specialType, JE_byte sfCodeCost)
{
	nextSpecialWait = 0;
	switch (special[specialType].stype)
	{
		/*Weapon*/
		case 1:
			if (playerNum == 1)
				b = player_shot_create(0, SHOT_SPECIAL2, player[0].x, player[0].y, mouseX, mouseY, special[specialType].wpn, playerNum);
			else
				b = player_shot_create(0, SHOT_SPECIAL2, player[1].x, player[1].y, mouseX, mouseY, special[specialType].wpn, playerNum);

			shotRepeat[SHOT_SPECIAL] = shotRepeat[SHOT_SPECIAL2];
			break;
		/*Repulsor*/
		case 2:
			for (temp = 0; temp < ENEMY_SHOT_MAX; temp++)
			{
				if (!enemyShotAvail[temp])
				{
					if (player[0].x > enemyShot[temp].sx)
						enemyShot[temp].sxm--;
					else if (player[0].x < enemyShot[temp].sx)
						enemyShot[temp].sxm++;

					if (player[0].y > enemyShot[temp].sy)
						enemyShot[temp].sym--;
					else if (player[0].y < enemyShot[temp].sy)
						enemyShot[temp].sym++;
				}
			}
			break;
		/*Zinglon Blast*/
		case 3:
			zinglonDuration = 50;
			shotRepeat[SHOT_SPECIAL] = 100;
			soundQueue[7] = S_SOUL_OF_ZINGLON;
			break;
		/*Attractor*/
		case 4:
			for (temp = 0; temp < 100; temp++)
			{
				if (enemyAvail[temp] != 1 && enemy[temp].scoreitem &&
				    enemy[temp].evalue != 0)
				{
					if (player[0].x > enemy[temp].ex)
						enemy[temp].exc++;
					else if (player[0].x < enemy[temp].ex)
						enemy[temp].exc--;

					if (player[0].y > enemy[temp].ey)
						enemy[temp].eyc++;
					else if (player[0].y < enemy[temp].ey)
						enemy[temp].eyc--;
				}
			}
			break;
		/*Flare*/
		case 5:
		case 6:
		case 7:
		case 8:
		case 9:
		case 10:
		case 11:
		case 16:
			if (flareDuration == 0)
				flareStart = true;

			specialWeaponWpn = special[specialType].wpn;
			linkToPlayer = false;
			spraySpecial = false;
			switch (special[specialType].stype)
			{
				case 5:
					specialWeaponFilter = 7;
					specialWeaponFreq = 2;
					flareDuration = 50;
					break;
				case 6:
					specialWeaponFilter = 1;
					specialWeaponFreq = 7;
					flareDuration = 200 + 25 * player[0].items.weapon[FRONT_WEAPON].power;
					break;
				case 7:
					specialWeaponFilter = 3;
					specialWeaponFreq = 3;
					flareDuration = 50 + 10 * player[0].items.weapon[FRONT_WEAPON].power;
					zinglonDuration = 50;
					shotRepeat[SHOT_SPECIAL] = 100;
					soundQueue[7] = S_SOUL_OF_ZINGLON;
					break;
				case 8:
					specialWeaponFilter = -99;
					specialWeaponFreq = 7;
					flareDuration = 10 + player[0].items.weapon[FRONT_WEAPON].power;
					break;
				case 9:
					specialWeaponFilter = -99;
					specialWeaponFreq = 8;
					flareDuration = 8 + 2 * player[0].items.weapon[FRONT_WEAPON].power;
					linkToPlayer = true;
					nextSpecialWait = special[specialType].pwr;
					break;
				case 10:
					specialWeaponFilter = -99;
					specialWeaponFreq = 8;
					flareDuration = 14 + 4 * player[0].items.weapon[FRONT_WEAPON].power;
					linkToPlayer = true;
					break;
				case 11:
					specialWeaponFilter = -99;
					specialWeaponFreq = special[specialType].pwr;
					flareDuration = 10 + 10 * player[0].items.weapon[FRONT_WEAPON].power;
					astralDuration = 20 + 10 * player[0].items.weapon[FRONT_WEAPON].power;
					break;
				case 16:
					specialWeaponFilter = -99;
					specialWeaponFreq = 8;
					flareDuration = sfCodeCost * 16 + 8;
					linkToPlayer = true;
					spraySpecial = true;
					break;
			}
			break;
		case 12:
			if (!sfCodeCost)
			{
				player[playerNum-1].invulnerable_ticks = 100;
				shotRepeat[SHOT_SPECIAL] = 250;
			}
			else
			{
				player[playerNum-1].invulnerable_ticks = sfCodeCost * 10;
			}
			break;
		case 13:
			player_boostArmor(&player[0], sfCodeCost / 4 + 1);
			soundQueue[3] = S_POWERUP;
			break;
		case 14:
			player_boostArmor(&player[1], sfCodeCost / 4 + 1);
			soundQueue[3] = S_POWERUP;
			break;

		case 17:  // spawn left or right sidekick
			soundQueue[3] = S_POWERUP;

			if (player[0].items.sidekick[LEFT_SIDEKICK] == special[specialType].wpn)
			{
				player[0].items.sidekick[RIGHT_SIDEKICK] = special[specialType].wpn;
				shotMultiPos[RIGHT_SIDEKICK] = 0;
			}
			else
			{
				player[0].items.sidekick[LEFT_SIDEKICK] = special[specialType].wpn;
				shotMultiPos[LEFT_SIDEKICK] = 0;
			}

			JE_drawOptions();
			break;

		case 18:  // spawn right sidekick
			player[0].items.sidekick[RIGHT_SIDEKICK] = special[specialType].wpn;

			JE_drawOptions();

			soundQueue[3] = S_POWERUP;

			shotMultiPos[RIGHT_SIDEKICK] = 0;
			break;
	}
}

void JE_doSpecialShot(JE_byte playerNum, uint *armor, uint *shield)
{
	if (shotRepeat[SHOT_SPECIAL] > 0)
	{
		--shotRepeat[SHOT_SPECIAL];
	}
	if (specialWait > 0)
	{
		specialWait--;
	}

	const JE_byte sfCodeIdx = SFExecuted[playerNum-1];

	if (sfCodeIdx > 0 && shotRepeat[SHOT_SPECIAL] == 0 && flareDuration == 0)
	{
		JE_byte sfCommand, sfCost;
		if (extraGame) // use Stalker 21.126's twiddles
		{
			sfCommand = keyboardCombos[shipCombosB[sfCodeIdx - 1] - 1][7] - 100;
			sfCost = special[sfCommand].pwr;
		}
		else // use twiddles from AP
		{
			sfCommand = APTwiddles.Code[sfCodeIdx - 1].Command[7] - 100;
			sfCost = APTwiddles.Code[sfCodeIdx - 1].Cost;
		}

		bool can_afford = true;

		if (sfCost > 0)
		{
			if (sfCost < 98)  // costs some shield
			{
				if (*shield >= sfCost)
					*shield -= sfCost;
				else
					can_afford = false;
			}
			else if (sfCost == 98)  // costs all shield
			{
				if (*shield < 4)
					can_afford = false;
				sfCost = *shield;
				*shield = 0;
			}
			else if (sfCost == 99)  // costs half shield
			{
				sfCost = *shield / 2;
				*shield = sfCost;
			}
			else  // costs some armor
			{
				sfCost -= 100;
				if (*armor > sfCost)
					*armor -= sfCost;
				else
					can_afford = false;
			}
		}

		shotMultiPos[SHOT_SPECIAL] = 0;
		shotMultiPos[SHOT_SPECIAL2] = 0;

		if (can_afford)
			JE_specialComplete(playerNum, sfCommand, sfCost);

		SFExecuted[playerNum-1] = 0;

		player_wipeShieldArmorBars();
		player_drawShield();
		player_drawArmor();
	}

	if (playerNum == 1 && player[0].items.special > 0)
	{  /*Main Begin*/

		if (superArcadeMode > 0 && (button[2-1] || button[3-1]))
		{
			fireButtonHeld = false;
		}
		if (!button[1-1] && !(superArcadeMode != SA_NONE && (button[2-1] || button[3-1])))
		{
			fireButtonHeld = false;
		}
		else if (shotRepeat[SHOT_SPECIAL] == 0 && !fireButtonHeld && !(flareDuration > 0) && specialWait == 0)
		{
			fireButtonHeld = true;
			JE_specialComplete(playerNum, player[0].items.special, 0);
		}

	}  /*Main End*/

	if (astralDuration > 0)
		astralDuration--;

	shotAvail[MAX_PWEAPON-1] = 0;
	if (flareDuration > 1)
	{
		if (specialWeaponFilter != -99)
		{
			if (levelFilter == -99 && levelBrightness == -99)
			{
				filterActive = false;
			}
			if (!filterActive)
			{
				levelFilter = specialWeaponFilter;
				if (levelFilter == 7)
				{
					levelBrightness = 0;
				}
				filterActive = true;
			}

			if (mt_rand() % 2 == 0)
				flareColChg = -1;
			else
				flareColChg = 1;

			if (levelFilter == 7)
			{
				if (levelBrightness < -6)
				{
					flareColChg = 1;
				}
				if (levelBrightness > 6)
				{
					flareColChg = -1;
				}
				levelBrightness += flareColChg;
			}
		}

		if ((signed)(mt_rand() % 6) < specialWeaponFreq)
		{
			b = MAX_PWEAPON;

			if (linkToPlayer)
			{
				if (shotRepeat[SHOT_SPECIAL] == 0)
				{
					b = player_shot_create(0, SHOT_SPECIAL, player[0].x, player[0].y, mouseX, mouseY, specialWeaponWpn, playerNum);
				}
			}
			else
			{
				b = player_shot_create(0, SHOT_SPECIAL, mt_rand() % 280, mt_rand() % 180, mouseX, mouseY, specialWeaponWpn, playerNum);
			}

			if (spraySpecial && b != MAX_PWEAPON)
			{
				playerShotData[b].shotXM = (mt_rand() % 5) - 2;
				playerShotData[b].shotYM = (mt_rand() % 5) - 2;
				if (playerShotData[b].shotYM == 0)
				{
					playerShotData[b].shotYM++;
				}
			}
		}

		flareDuration--;
		if (flareDuration == 1)
		{
			specialWait = nextSpecialWait;
		}
	}
	else if (flareStart)
	{
		flareStart = false;
		shotRepeat[SHOT_SPECIAL] = linkToPlayer ? 15 : 200;
		flareDuration = 0;
		if (levelFilter == specialWeaponFilter)
		{
			levelFilter = -99;
			levelBrightness = -99;
			filterActive = false;
		}
	}

	if (zinglonDuration > 1)
	{
		temp = 25 - abs(zinglonDuration - 25);

		JE_barBright(VGAScreen, player[0].x + 7 - temp,     0, player[0].x + 7 + temp,     184);
		JE_barBright(VGAScreen, player[0].x + 7 - temp - 2, 0, player[0].x + 7 + temp + 2, 184);

		zinglonDuration--;
		if (zinglonDuration % 5 == 0)
		{
			shotAvail[MAX_PWEAPON-1] = 1;
		}
	}
}

void JE_setupExplosion(
	JE_integer x,
	JE_integer y,
	JE_integer deltaY,
	JE_integer type,
	bool fixedPosition,  // true when coin/gem value
	bool followPlayer)   // true when player shield (1P only)
{
	const struct {
		JE_word sprite;
		JE_byte ttl;
		Sprite2_array *sheet; // Added for AP
	} explosion_data[55] /* [1..53] */ = {
		{ 144,  7, &explosionSpriteSheet },
		{ 120, 12, &explosionSpriteSheet },
		{ 190, 12, &explosionSpriteSheet },
		{ 209, 12, &explosionSpriteSheet },
		{ 152, 12, &explosionSpriteSheet },
		{ 171, 12, &explosionSpriteSheet },
		{ 133,  7, &explosionSpriteSheet },   /*White Smoke*/
		{   1, 12, &explosionSpriteSheet },
		{  20, 12, &explosionSpriteSheet },
		{  39, 12, &explosionSpriteSheet },
		{  58, 12, &explosionSpriteSheet },
		{ 110,  3, &explosionSpriteSheet },
		{  76,  7, &explosionSpriteSheet },
		{  91,  3, &explosionSpriteSheet },
/*15*/	{ 227,  3, &explosionSpriteSheet },
		{ 230,  3, &explosionSpriteSheet },
		{ 233,  3, &explosionSpriteSheet },
		{ 252,  3, &explosionSpriteSheet },
		{ 246,  3, &explosionSpriteSheet },
/*20*/	{ 249,  3, &explosionSpriteSheet },
		{ 265,  3, &explosionSpriteSheet },
		{ 268,  3, &explosionSpriteSheet },
		{ 271,  3, &explosionSpriteSheet },
		{ 236,  3, &explosionSpriteSheet },
/*25*/	{ 239,  3, &explosionSpriteSheet },
		{ 242,  3, &explosionSpriteSheet },
		{ 261,  3, &explosionSpriteSheet },
		{ 274,  3, &explosionSpriteSheet },
		{ 277,  3, &explosionSpriteSheet },
/*30*/	{ 280,  3, &explosionSpriteSheet },
		{ 299,  3, &explosionSpriteSheet },
		{ 284,  3, &explosionSpriteSheet },
		{ 287,  3, &explosionSpriteSheet },
		{ 290,  3, &explosionSpriteSheet },
/*35*/	{ 293,  3, &explosionSpriteSheet },
		{ 165,  8, &explosionSpriteSheet },   /*Coin Values*/
		{ 184,  8, &explosionSpriteSheet },
		{ 203,  8, &explosionSpriteSheet },
		{ 222,  8, &explosionSpriteSheet },
		{ 168,  8, &explosionSpriteSheet },
		{ 187,  8, &explosionSpriteSheet },
		{ 206,  8, &explosionSpriteSheet },
		{ 225, 10, &explosionSpriteSheet },
		{ 169, 10, &explosionSpriteSheet },
		{ 188, 10, &explosionSpriteSheet },
		{ 207, 20, &explosionSpriteSheet },
		{ 226, 14, &explosionSpriteSheet },
		{ 170, 14, &explosionSpriteSheet },
		{ 189, 14, &explosionSpriteSheet },
		{ 208, 14, &explosionSpriteSheet },
		{ 246, 14, &explosionSpriteSheet },
		{ 227, 14, &explosionSpriteSheet },
		{ 265, 14, &explosionSpriteSheet },
		{  96,  3, &explosionSpriteSheet },  /* Tyrian 2000 */
		{  10, 14, &archipelagoSpriteSheet }   /* AP Item */
	};

	if (y > -16 && y < 190)
	{
		for (int i = 0; i < MAX_EXPLOSIONS; i++)
		{
			if (explosions[i].ttl == 0)
			{
				explosions[i].x = x;
				explosions[i].y = y;
				if (type == 6)
				{
					explosions[i].y += 12;
					explosions[i].x += 2;
				}
				else if (type == 98 || type == 198)
				{
					type = 6;
				}
				explosions[i].sheet = explosion_data[type].sheet;
				explosions[i].sprite = explosion_data[type].sprite;
				explosions[i].ttl = explosion_data[type].ttl;
				explosions[i].followPlayer = followPlayer;
				explosions[i].fixedPosition = fixedPosition;
				explosions[i].deltaY = deltaY;
				break;
			}
		}
	}
}

void JE_setupExplosionLarge(JE_boolean enemyGround, JE_byte exploNum, JE_integer x, JE_integer y)
{
	if (y >= 0)
	{
		if (enemyGround)
		{
			JE_setupExplosion(x - 6, y - 14, 0,  2, false, false);
			JE_setupExplosion(x + 6, y - 14, 0,  4, false, false);
			JE_setupExplosion(x - 6, y,      0,  3, false, false);
			JE_setupExplosion(x + 6, y,      0,  5, false, false);
		}
		else
		{
			JE_setupExplosion(x - 6, y - 14, 0,  7, false, false);
			JE_setupExplosion(x + 6, y - 14, 0,  9, false, false);
			JE_setupExplosion(x - 6, y,      0,  8, false, false);
			JE_setupExplosion(x + 6, y,      0, 10, false, false);
		}

		bool big;

		if (exploNum > 10)
		{
			exploNum -= 10;
			big = true;
		}
		else
		{
			big = false;
		}

		if (exploNum)
		{
			for (int i = 0; i < MAX_REPEATING_EXPLOSIONS; i++)
			{
				if (rep_explosions[i].ttl == 0)
				{
					rep_explosions[i].ttl = exploNum;
					rep_explosions[i].delay = 2;
					rep_explosions[i].x = x;
					rep_explosions[i].y = y;
					rep_explosions[i].big = big;
					break;
				}
			}
		}
	}
}

void JE_doSP(JE_word x, JE_word y, JE_word num, JE_byte explowidth, JE_byte color) /* superpixels */
{
	for (temp = 0; temp < num; temp++)
	{
		JE_real tempr = mt_rand_lt1() * (2 * M_PI);
		signed int tempy = roundf(cosf(tempr) * mt_rand_1() * explowidth);
		signed int tempx = roundf(sinf(tempr) * mt_rand_1() * explowidth);

		if (++last_superpixel >= MAX_SUPERPIXELS)
			last_superpixel = 0;
		superpixels[last_superpixel].x = tempx + x;
		superpixels[last_superpixel].y = tempy + y;
		superpixels[last_superpixel].delta_x = tempx;
		superpixels[last_superpixel].delta_y = tempy + 1;
		superpixels[last_superpixel].color = color;
		superpixels[last_superpixel].z = 15;
	}
}

void JE_drawSP(void)
{
	for (int i = MAX_SUPERPIXELS; i--; )
	{
		if (superpixels[i].z)
		{
			superpixels[i].x += superpixels[i].delta_x;
			superpixels[i].y += superpixels[i].delta_y;

			if (superpixels[i].x < (unsigned)VGAScreen->w && superpixels[i].y < (unsigned)VGAScreen->h)
			{
				Uint8 *s = (Uint8 *)VGAScreen->pixels; /* screen pointer, 8-bit specific */
				s += superpixels[i].y * VGAScreen->pitch;
				s += superpixels[i].x;

				*s = (((*s & 0x0f) + superpixels[i].z) >> 1) + superpixels[i].color;
				if (superpixels[i].x > 0)
					*(s - 1) = (((*(s - 1) & 0x0f) + (superpixels[i].z >> 1)) >> 1) + superpixels[i].color;
				if (superpixels[i].x < VGAScreen->w - 1u)
					*(s + 1) = (((*(s + 1) & 0x0f) + (superpixels[i].z >> 1)) >> 1) + superpixels[i].color;
				if (superpixels[i].y > 0)
					*(s - VGAScreen->pitch) = (((*(s - VGAScreen->pitch) & 0x0f) + (superpixels[i].z >> 1)) >> 1) + superpixels[i].color;
				if (superpixels[i].y < VGAScreen->h - 1u)
					*(s + VGAScreen->pitch) = (((*(s + VGAScreen->pitch) & 0x0f) + (superpixels[i].z >> 1)) >> 1) + superpixels[i].color;
			}

			superpixels[i].z--;
		}
	}
}
