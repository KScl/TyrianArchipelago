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
#include "config.h"

#include "episodes.h"
#include "file.h"
#include "joystick.h"
#include "loudness.h"
#include "mtrand.h"
#include "nortsong.h"
#include "opentyr.h"
#include "player.h"
#include "varz.h"
#include "vga256d.h"
#include "video.h"
#include "video_scale.h"

#include <stdio.h>
#include <sys/stat.h>

#ifdef _MSC_VER
#include <direct.h>
#define mkdir _mkdir
#else
#include <unistd.h>
#endif

#include "archipelago/apconnect.h"

/* Configuration Load/Save handler */

#if 0
const JE_byte cryptKey[10] = /* [1..10] */
{
	15, 50, 89, 240, 147, 34, 86, 9, 32, 208
};

const DosKeySettings defaultDosKeySettings =
{
	72, 80, 75, 77, 57, 28, 29, 56
};
#endif

const KeySettings defaultKeySettings =
{
	SDL_SCANCODE_UP,
	SDL_SCANCODE_DOWN,
	SDL_SCANCODE_LEFT,
	SDL_SCANCODE_RIGHT,
	SDL_SCANCODE_SPACE,
	SDL_SCANCODE_RETURN,
	SDL_SCANCODE_LCTRL,
	SDL_SCANCODE_LALT,
};

static const char *const keySettingNames[] =
{
	"up",
	"down",
	"left",
	"right",
	"fire",
	"change fire",
	"left sidekick",
	"right sidekick",
};

const char defaultHighScoreNames[34][23] = /* [1..34] of string [22] */
{/*1P*/
/*TYR*/   "The Prime Chair", /*13*/
          "Transon Lohk",
          "Javi Onukala",
          "Mantori",
          "Nortaneous",
          "Dougan",
          "Reid",
          "General Zinglon",
          "Late Gyges Phildren",
          "Vykromod",
          "Beppo",
          "Borogar",
          "ShipMaster Carlos",

/*OTHER*/ "Jill", /*5*/
          "Darcy",
          "Jake Stone",
          "Malvineous Havershim",
          "Marta Louise Velasquez",

/*JAZZ*/  "Jazz Jackrabbit", /*3*/
          "Eva Earlong",
          "Devan Shell",

/*OMF*/   "Crystal Devroe", /*11*/
          "Steffan Tommas",
          "Milano Angston",
          "Christian",
          "Shirro",
          "Jean-Paul",
          "Ibrahim Hothe",
          "Angel",
          "Cossette Akira",
          "Raven",
          "Hans Kreissack",

/*DARE*/  "Tyler", /*2*/
          "Rennis the Rat Guard"
};

const char defaultTeamNames[22][25] = /* [1..22] of string [24] */
{
	"Jackrabbits",
	"Team Tyrian",
	"The Elam Brothers",
	"Dare to Dream Team",
	"Pinball Freaks",
	"Extreme Pinball Freaks",
	"Team Vykromod",
	"Epic All-Stars",
	"Hans Keissack's WARriors",
	"Team Overkill",
	"Pied Pipers",
	"Gencore Growlers",
	"Microsol Masters",
	"Beta Warriors",
	"Team Loco",
	"The Shellians",
	"Jungle Jills",
	"Murderous Malvineous",
	"The Traffic Department",
	"Clan Mikal",
	"Clan Patrok",
	"Carlos' Crawlers"
};

const JE_EditorItemAvailType initialItemAvail =
{
	1,1,1,0,0,1,1,0,1,1,1,1,1,0,1,0,1,1,1,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0, /* Front/Rear Weapons 1-38  */
	0,0,0,0,0,0,0,0,0,0,1,                                                           /* Fill                     */
	1,0,0,0,0,1,0,0,0,1,1,0,1,0,0,0,0,0,                                             /* Sidekicks          51-68 */
	0,0,0,0,0,0,0,0,0,0,0,                                                           /* Fill                     */
	1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,                                                   /* Special Weapons    81-93 */
	0,0,0,0,0                                                                        /* Fill                     */
};

/* Last 2 bytes = Word
 *
 * Max Value = 1680
 * X div  60 = Armor  (1-28)
 * X div 168 = Shield (1-12)
 * X div 280 = Engine (1-06)
 */

JE_boolean smoothies[9] = /* [1..9] */
{ 0, 0, 0, 0, 0, 0, 0, 0, 0 };

JE_byte starShowVGASpecialCode;

/* CubeData */
JE_word lastCubeMax, cubeMax;
JE_word cubeList[4]; /* [1..4] */

/* High-Score Stuff */
JE_boolean gameHasRepeated;  // can only get highscore on first play-through

/* Difficulty */
JE_shortint difficultyLevel, oldDifficultyLevel,
            initialDifficulty;  // can only get highscore on initial episode

/* Player Stuff */
uint    generatorPower, lastGenPower;
JE_byte shieldWait;

JE_byte          shotRepeat[11], shotMultiPos[11];
JE_boolean       portConfigChange, portConfigDone;

/* Level Data */
char    lastLevelName[11], levelName[11]; /* string [10] */
JE_byte mainLevel, nextLevel, saveLevel;   /*Current Level #*/

/* Keyboard Junk */
//DosKeySettings dosKeySettings;
KeySettings keySettings;

/* Configuration */
JE_shortint levelFilter, levelFilterNew, levelBrightness, levelBrightnessChg;
JE_boolean  filtrationAvail, filterActive, filterFade, filterFadeStart;

JE_boolean gameJustLoaded;

JE_boolean galagaMode;

JE_boolean extraGame;

JE_boolean twoPlayerMode, twoPlayerLinked, onePlayerAction, superTyrian;
JE_boolean trentWin = false;
JE_byte    superArcadeMode;

JE_byte    superArcadePowerUp;

JE_real linkGunDirec;
JE_byte inputDevice[2] = { 1, 2 }; // 0:any  1:keyboard  2:mouse  3+:joystick

//JE_byte secretHint;
JE_byte background3over;
JE_byte background2over;
JE_byte gammaCorrection;
JE_boolean superPause = false;
JE_boolean explosionTransparent,
           displayScore,
           background2, smoothScroll, wild, superWild, starActive,
           topEnemyOver,
           skyEnemyOverAll,
           background2notTransparent;

JE_byte    fastPlay;
JE_boolean pentiumMode;

/* Savegame files */
JE_byte    gameSpeed;
JE_byte    processorType;  /* 1=386 2=486 3=Pentium Hyper */

#ifdef LEVEL_CHEATS
JE_boolean youAreCheating;
#endif

Config opentyrian_config;  // implicitly initialized

const char *get_user_directory(void)
{
#if   defined(TARGET_WIN32)
	mkdir(".\\save");
	return ".\\save";
#elif !defined(USE_HOME_DIR)
	mkdir("./save", 0744);
	return "./save";
#else
	// If requested, use home directory
	static char user_dir[500] = "";

	if (strlen(user_dir) == 0)
	{
		char *xdg_config_home = getenv("XDG_CONFIG_HOME");
		if (xdg_config_home != NULL)
		{
			snprintf(user_dir, sizeof(user_dir), "%s/aptyrian", xdg_config_home);
		}
		else
		{
			char *home = getenv("HOME");
			if (home != NULL)
			{
				snprintf(user_dir, sizeof(user_dir), "%s/.config/aptyrian", home);
			}
			else
			{
				strcpy(user_dir, "./save");
			}
		}
	}

	mkdir(user_dir, 0744);
	return user_dir;
#endif
}

// ----------------------------------------------------------------------------

static void loadOpts_archipelago(ConfigSection *section)
{
	uint temp_uint;
	const char *temp_str;

	if (section != NULL)
	{
		if (config_get_string_option(section, "uuid", &temp_str))
			Archipelago_SetUUID(temp_str);
		else
			Archipelago_GenerateUUID();

		if (config_get_uint_option(section, "game_speed", &temp_uint))
			gameSpeed = (Uint8)(temp_uint & 0xFF);
	}
	else
	{
		Archipelago_GenerateUUID();
		gameSpeed = 4;
	}
}

static void saveOpts_archipelago(ConfigSection *section)
{
	if (section == NULL) // out of memory
		exit(EXIT_FAILURE);

	config_set_string_option(section, "uuid", Archipelago_GetUUID());
	config_set_uint_option(section, "game_speed", gameSpeed);
}

// ----------------------------------------------------------------------------

static void loadOpts_video(ConfigSection *section)
{
	uint temp_uint;
	const char *temp_str;

	if (section != NULL)
	{
		config_get_int_option(section, "fullscreen", &fullscreen_display);

		if (config_get_string_option(section, "scaler", &temp_str))
			set_scaler_by_name(temp_str);
		
		if (config_get_string_option(section, "scaling_mode", &temp_str))
			set_scaling_mode_by_name(temp_str);

		if (config_get_uint_option(section, "detail_level", &temp_uint))
			processorType = (Uint8)(temp_uint & 0xFF);

		if (config_get_uint_option(section, "gamma_correction", &temp_uint))
			gammaCorrection = (Uint8)(temp_uint & 0xFF);
	}
	else
	{
		fullscreen_display = -1;
		set_scaler_by_name("Scale2x");
		processorType = 4;
		gammaCorrection = 0;
	}
}

static void saveOpts_video(ConfigSection *section)
{
	if (section == NULL) // out of memory
		exit(EXIT_FAILURE);

	config_set_int_option(section, "fullscreen", fullscreen_display);
	config_set_string_option(section, "scaler", scalers[scaler].name);
	config_set_string_option(section, "scaling_mode", scaling_mode_names[scaling_mode]);
	config_set_uint_option(section, "detail_level", processorType);
	config_set_uint_option(section, "gamma_correction", gammaCorrection);
}

// ------------------------------------------------------------------

static void loadOpts_audio(ConfigSection *section)
{
	uint temp_uint;
	bool temp_bool;

	if (section != NULL)
	{
		if (config_get_bool_option(section, "sound_enabled", &temp_bool))
			samples_disabled = !temp_bool;
		if (config_get_uint_option(section, "sound_volume", &temp_uint))
			fxVolume = (Uint16)(temp_uint & 0xFF);

		if (config_get_bool_option(section, "music_enabled", &temp_bool))
			music_disabled = !temp_bool;
		if (config_get_uint_option(section, "music_volume", &temp_uint))
			tyrMusicVolume = (Uint16)(temp_uint & 0xFF);
	}
	else
	{
		music_disabled = false;
		samples_disabled = false;

		tyrMusicVolume = 191;
		fxVolume = 191;
	}

	set_volume(tyrMusicVolume, fxVolume);
}

static void saveOpts_audio(ConfigSection *section)
{
	if (section == NULL) // out of memory
		exit(EXIT_FAILURE);

	config_set_bool_option(section, "sound_enabled", !samples_disabled, NO_YES);
	config_set_uint_option(section, "sound_volume", fxVolume);
	config_set_bool_option(section, "music_enabled", !music_disabled, NO_YES);
	config_set_uint_option(section, "music_volume", tyrMusicVolume);
}

// ------------------------------------------------------------------

static void loadOpts_keyboard(ConfigSection *section)
{
	if (section != NULL)
	{
		for (size_t i = 0; i < COUNTOF(keySettings); ++i)
		{
			const char *keyName;
			if (config_get_string_option(section, keySettingNames[i], &keyName))
			{
				SDL_Scancode scancode = SDL_GetScancodeFromName(keyName);
				if (scancode != SDL_SCANCODE_UNKNOWN)
					keySettings[i] = scancode;
			}
		}
	}
	else
	{
		memcpy(keySettings, defaultKeySettings, sizeof(keySettings));
	}

}

static void saveOpts_keyboard(ConfigSection *section)
{
	if (section == NULL) // out of memory
		exit(EXIT_FAILURE);

	for (size_t i = 0; i < COUNTOF(keySettings); ++i)
	{
		const char *keyName = SDL_GetScancodeName(keySettings[i]);
		if (keyName[0] == '\0')
			keyName = NULL;
		config_set_string_option(section, keySettingNames[i], keyName);
	}
}

// ------------------------------------------------------------------

void config_load(void)
{
	Config *config = &opentyrian_config;

	FILE *file = dir_fopen_warn(get_user_directory(), "aptyrian.cfg", "r");
	if (file == NULL || !config_parse(config, file))
	{
		// Continue using defaults
		loadOpts_video(NULL);
		loadOpts_audio(NULL);
		loadOpts_keyboard(NULL);
		loadOpts_archipelago(NULL);
	}
	else
	{
		// Load from file
		loadOpts_video(config_find_section(config, "video", NULL));
		loadOpts_audio(config_find_section(config, "audio", NULL));
		loadOpts_keyboard(config_find_section(config, "keyboard", NULL));
		loadOpts_archipelago(config_find_section(config, "archipelago", NULL));
	}

	JE_initProcessorType();
}

void config_save(void)
{
	Config *config = &opentyrian_config;

	saveOpts_video(config_find_or_add_section(config, "video", NULL));
	saveOpts_audio(config_find_or_add_section(config, "audio", NULL));
	saveOpts_keyboard(config_find_or_add_section(config, "keyboard", NULL));
	saveOpts_archipelago(config_find_or_add_section(config, "archipelago", NULL));

	FILE *file = dir_fopen(get_user_directory(), "aptyrian.cfg", "w");
	if (file == NULL)
		return;

	config_write(config, file);

#if _POSIX_C_SOURCE >= 1 || _XOPEN_SOURCE || _POSIX_SOURCE
	fsync(fileno(file));
#endif
	fclose(file);
}

// ----------------------------------------------------------------------------

void JE_initProcessorType(void)
{
	/* SYN: Originally this proc looked at your hardware specs and chose appropriate options. We don't care, so I'll just set
	   decent defaults here. */

	wild = false;
	superWild = false;
	smoothScroll = true;
	explosionTransparent = true;
	filtrationAvail = false;
	background2 = true;
	displayScore = true;

	switch (processorType)
	{
		case 1: /* 386 */
			background2 = false;
			displayScore = false;
			explosionTransparent = false;
			break;
		case 2: /* 486 - Default */
			break;
		case 3: /* High Detail */
			smoothScroll = false;
			break;
		case 4: /* Pentium */
			wild = true;
			filtrationAvail = true;
			break;
		case 5: /* Nonstandard VGA */
			smoothScroll = false;
			break;
		case 6: /* SuperWild */
			wild = true;
			superWild = true;
			filtrationAvail = true;
			break;
	}

	switch (gameSpeed)
	{
		case 1:  /* Slug Mode */
			fastPlay = 3;
			break;
		case 2:  /* Slower */
			fastPlay = 4;
			break;
		case 3: /* Slow */
			fastPlay = 5;
			break;
		case 4: /* Normal */
			fastPlay = 0;
			break;
		case 5: /* Pentium Hyper */
			fastPlay = 1;
			break;
#ifdef UNBOUNDED_SPEED
		case 6: /* Unbounded */
			fastPlay = 6;
			break;
#endif
	}

}

void JE_setNewGameSpeed(void)
{
	pentiumMode = false;

#ifndef UNBOUNDED_SPEED
	if (gameSpeed == 6)
	{
		gameSpeed = 4;
		fastPlay = 0;
	}
#endif

	Uint16 speed;
	switch (fastPlay)
	{
	default:
		assert(false);
		// fall through
	case 0:  // Normal
		speed = 0x4300;
		smoothScroll = true;
		frameCountMax = 2;
		break;
	case 1:  // Pentium Hyper
		speed = 0x3000;
		smoothScroll = true;
		frameCountMax = 2;
		break;
	case 2:
		speed = 0x2000;
		smoothScroll = false;
		frameCountMax = 2;
		break;
	case 3:  // Slug mode
		speed = 0x5300;
		smoothScroll = true;
		frameCountMax = 4;
		break;
	case 4:  // Slower
		speed = 0x4300;
		smoothScroll = true;
		frameCountMax = 3;
		break;
	case 5:  // Slow
		speed = 0x4300;
		smoothScroll = true;
		frameCountMax = 2;
		pentiumMode = true;
		break;
#ifdef UNBOUNDED_SPEED
	case 6:  // Unbounded
		speed = 0x4300;
		frameCountMax = 0;
		break;
#endif
	}

	setDelaySpeed(speed);
	setDelay(frameCountMax);
}
