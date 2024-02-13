
#include "opentyr.h"

#include "animlib.h"
#include "apmenu.h"
#include "fonthand.h"
#include "helptext.h"
#include "keyboard.h"
#include "loudness.h"
#include "lvlmast.h"
#include "mainint.h"
#include "nortsong.h"
#include "nortvars.h"
#include "pcxmast.h"
#include "picload.h"
#include "varz.h"
#include "video.h"

#include "archipelago/apconnect.h"

// ============================================================================
// Level End
// ============================================================================

void levelend_doLevelStats(void) // fka JE_endLevelAni
{
	char string_buffer[96];

	// TODO? Can probably find a better place to do rank manipulation
	//adjust_difficulty();

	// Mark completion
	const Uint16 episode = allLevelData[currentLevelID].episodeNum - 1;
	const Uint16 levelID = allLevelData[currentLevelID].episodeLevelID;
	APStats.Clears[episode] |= (1 << levelID);
	lastLevelCompleted = true;

	// Scout for new shop checks that just opened
	Archipelago_ScoutShopItems(allLevelData[currentLevelID].shopStart);

	// Add obtained cash to AP cash total
	APStats.Cash += player[0].cash;
	player[0].cash = 0;

	player[0].last_items = player[0].items;
	strcpy(lastLevelName, levelName);

	JE_wipeKey();
	frameCountMax = 3;
	textGlowFont = SMALL_FONT_SHAPES;

	SDL_Color white = { 255, 255, 255 };
	set_colors(white, 254, 254);

	if (!levelTimer || levelTimerCountdown > 0)
		nortsong_playVoice(V_LEVEL_END);
	else
		play_song(21);

	// ----- Completed: LEvELNAME -----------------------------------
	const char *levelAction = (levelTimer && levelTimerCountdown <= 0) ? "Failed:" : "Completed:";
	snprintf(string_buffer, sizeof(string_buffer), "%s %s", levelAction, levelName);
	JE_outTextGlow(VGAScreenSeg, 20, 20, string_buffer);

	// ----- Total Score: 0000000 -----------------------------------
	// Even in a theoretical two player mode, cash would be shared between players.
	snprintf(string_buffer, sizeof(string_buffer),  "%s %llu", miscText[28-1], (unsigned long long)APStats.Cash);
	JE_outTextGlow(VGAScreenSeg, 30, 50, string_buffer);

	// ----- Destruction: 00% ---------------------------------------
	const int destruction = (totalEnemy == 0) ? 0 : roundf(enemyKilled * 100 / totalEnemy);
	snprintf(string_buffer, sizeof(string_buffer),  "%s %d%%", miscText[63-1], destruction);
	JE_outTextGlow(VGAScreenSeg, 40, 90, string_buffer);

#if 0
	JE_outTextGlow(VGAScreenSeg, 30, 120, miscText[4-1]);   /*Cubes*/

	if (cubeMax > 0)
	{
		if (cubeMax > 4)
			cubeMax = 4;

		if (frameCountMax != 0)
			frameCountMax = 1;

		for (temp = 1; temp <= cubeMax; temp++)
		{
			JE_playSampleNum(S_ITEM);
			x = 20 + 30 * temp;
			y = 135;
			JE_drawCube(VGAScreenSeg, x, y, 9, 0);
			JE_showVGA();

			for (i = -15; i <= 10; i++)
			{
				setDelay(frameCountMax);

				blit_sprite_hv(VGAScreenSeg, x, y, OPTION_SHAPES, 25, 0x9, i);

				if (JE_anyButton())
					frameCountMax = 0;

				JE_showVGA();

				wait_delay();
			}
			for (i = 10; i >= 0; i--)
			{
				setDelay(frameCountMax);

				blit_sprite_hv(VGAScreenSeg, x, y, OPTION_SHAPES, 25, 0x9, i);

				if (JE_anyButton())
					frameCountMax = 0;

				JE_showVGA();

				wait_delay();
			}
		}
	}
	else
	{
		JE_outTextGlow(VGAScreenSeg, 50, 135, miscText[15-1]);
	}
#endif

	// ----- Press a key --------------------------------------------
	bool skipped_already = (frameCountMax == 0);
	if (!skipped_already)
		frameCountMax = 4;

	JE_outTextGlow(VGAScreenSeg, 90, 160, miscText[5-1]);

	//if (!constantPlay)
	//{
	do
	{
		JE_showVGA();
		SDL_Delay(16);
	} while (!(JE_anyButton() || (!skipped_already && frameCountMax == 0)));
	//}

	wait_noinput(false, false, true); // TODO: should up the joystick repeat temporarily instead

	fade_black(15);
	JE_clr256(VGAScreen);
}

// ============================================================================
// Episode End
// ============================================================================

typedef enum
{
	SCENE_E1S1,
	SCENE_E1S2,
	SCENE_E2S1,
	SCENE_E3S1,
	SCENE_E3S2,
	SCENE_E3S3,
	SCENE_E4S1,
	SCENE_E4S2,
	SCENE_E4S3,
	SCENE_E4S4,
	SCENE_E4S5,
	SCENE_E4S6,
	SCENE_E5S1,
	SCENE_E5S2,
	NUM_SCENES
} scenelist_t;

static const char *const textCutscenes[NUM_SCENES][12] = {
	{ // Episode 1 Scene 1
		"That was too close. You've been nearly vaporized, crushed,",
		"blown to pieces, flame broiled, and electrocuted one too",
		"many times. Only your incredible targeting skills and class",
		"1 fancy flying has kept you alive.",
		"",
		"Those boys at Deliani clearly have some even more hair ",
		"raising missions for you up their sleeve. Why were YOU",
		"chosen to be their errand boy?  Why is half the galaxy after",
		"your head? Can you beat the odds and, most importantly, can",
		"you survive without becoming space bacon?"
	},
	{ // Episode 1 Scene 2
		"If not, things could become unpleasant."
	},

	{ // Episode 2 Scene 1
		"First half of the sector wants you dead only because of",
		"what you know. Then, you seek help only to be suckered",
		"into a scheme by them to wipe out the other half of the",
		"sector. Third, when you learn their plans, you're sent",
		"to prison! Now that you've blasted your way to freedom,",
		"just how free are you? There is an invasion at hand,",
		"and it looks like you're the only one who can stop it.",
		"",
		"Otherwise...     There won't be anyplace left to hide."
	},

	{ // Episode 3 Scene 1
		"Your ship streams from the incandescent glow of the",
		"explosions you have just created in the midst of the",
		"Microsol fleet, and hunts down the final escape pod",
		"from the mother ship just as it punches into hyperspace. ",
		"",
		"It is finally over. The people of Deliani are saved",
		"and you have stopped a galaxy wide catastrophe."
	},
	{ // Episode 3 Scene 2
		"\"Congratulations!\" shouts Transon Lohk over the comm",
		"screen. \"Our forces were ready to defend the city",
		"after your last transmission, but thanks to you we",
		"haven't even suffered a scratch. I will make sure you",
		"are richly rewarded for this...\""
	},
	{ // Episode 3 Scene 3
		"You smile and flick off the comm, your mouth twisting",
		"into a frown. No reward could bring back your parents,",
		"and your best friend. Though you have saved millions of",
		"lives, why did the people closest to you have to suffer?",
		"Still, there's nothing you can do about it, and you vow",
		"to make sure that the same thing never happens again.",
		"From now on Tyrian sector will be a name synonymous",
		"with peace..."
	},

	{ // Episode 4 Scene 1
		"",
		"Poor Vykromod.  I feel sorry for him now.  What a waste.",
		"",
		"Still, that should be the last of the war for now.  At least",
		"it better be.  Is there no way out of this war?",
		"",
		"I am...",
		"",
		"            alone."
	},
	{ // Episode 4 Scene 2
		"Now that I have won the war, I can return to Savara",
		"and receive a hero's welcome!  Millions of",
		"Holo-viewers will bow at my feet and bless the very",
		"channels I appear on.",
		"",
		"Sages will cross a thousand desert streams to be",
		"at my side.  Streams of small dwarves will dance the",
		"jig of a thousand hours at my celebration.  I could",
		"be made into anything."
	},
	{ // Episode 4 Scene 3
		"But I don't want to be a hero.  I hate this life.",
		"",
		"Why, O me, do I strut upon this stage once more?",
		"",
		"That's it...",
		"",
		"   I won't go!"
	},
	{ // Episode 4 Scene 4
		"Computer, lock in to a random galaxy at least 100",
		"years away!",
		"",
		"Earth?  Funny name for a planet.  Wonder if it's",
		"inhabited.",
		"",
		"Who cares?  I'll jump into cryofreeze and sit in",
		"stasis until we reach this... Earth.",
		"",
		"Nobody will EVER find me!"
	},
	{ // Episode 4 Scene 5
		"And so, space-pilot Trent Hawkins straps down in his",
		"little craft and sets the thermostat to CRYO.",
		"",
		"His ship streams off at maximum speed to his unknown",
		"destination.  For 101 years, he waits deep within",
		"his frozen ship.  The galaxy searches for his body",
		"but he is considered lost and given a hero's",
		"funeral."
	},
	{ // Episode 4 Scene 6
		"Little do they know...",
		"",
		"You'll soon be on Earth, ready to slip unnoticed",
		"into whatever life lurks there."
	},

	{ // Episode 5 Scene 1
		"",
		"Related by your onetime friendship with Buce, you were",
		"sought out by his people, the Hazudra Collectors, and sent",
		"to destroy the Zinglonites.",
		"",
		"With none of their forces left and the deadly fruit weapons",
		"destroyed, the universe as a whole is silent once again.  No",
		"response ever comes of your inquiry to the state of the",
		"Hazudra or who sent you on this journey.  The secret haunts",
		"you in your sleep.",
		"",
		"Still a hero once again, as always..."
	},
	{ // Episode 5 Scene 2
		"Congratulations!"
	}
};

#define SCENECMD_MUSIC      0x1000 // ]M ###[
#define SCENECMD_BG_PIC     0x2000 // ]P 0##[
#define SCENECMD_BG_COLOR   0x2100 // ]P 9##[
#define SCENECMD_BG_SCROLL  0x2200 // ]U ###[
#define SCENECMD_FADE_BLACK 0x3000 // ]C[
#define SCENECMD_FADE_WHITE 0x3100 // ]F[
#define SCENECMD_LASTBANK   0x4000 // ]@[
#define SCENECMD_PLAYANIM   0x5000 // ]A[
#define SCENECMD_RUN        0xF000 // ]Wx ##[
#define SCENECMD_END        0x0000
#define SCENECMD_ENDNOTGOAL 0x0100

static const Uint16 episodeEndScenes[] = 
{
	// Episode 1 ending
	/*  0 */ SCENECMD_MUSIC     | 8,
	/*  1 */ SCENECMD_BG_PIC    | 9,
	/*  2 */ SCENECMD_RUN       | SCENE_E1S1,
	/*  3 */ SCENECMD_MUSIC     | 22,
	/*  4 */ SCENECMD_BG_SCROLL | 8,
	/*  5 */ SCENECMD_RUN       | SCENE_E1S2,
	/*  6 */ SCENECMD_END,

	// Episode 2 ending
	/*  7 */ SCENECMD_MUSIC  | 8,
	/*  8 */ SCENECMD_BG_PIC | 7,
	/*  9 */ SCENECMD_RUN    | SCENE_E2S1,
	/* 10 */ SCENECMD_END,

	// Episode 3 ending
	/* 11 */ SCENECMD_LASTBANK,
	/* 12 */ SCENECMD_MUSIC       | 9,
	/* 13 */ SCENECMD_PLAYANIM,
	/* 14 */ SCENECMD_FADE_WHITE,
	/* 15 */ SCENECMD_BG_COLOR    | 7,
	/* 16 */ SCENECMD_RUN         | SCENE_E3S1,
	/* 17 */ SCENECMD_FADE_BLACK,
	/* 18 */ SCENECMD_BG_COLOR    | 7,
	/* 19 */ SCENECMD_RUN         | SCENE_E3S2,
	/* 20 */ SCENECMD_FADE_BLACK,
	/* 21 */ SCENECMD_BG_COLOR    | 7,
	/* 22 */ SCENECMD_RUN         | SCENE_E3S3,
	/* 23 */ SCENECMD_END,

	// Episode 4 ending
	/* 24 */ SCENECMD_BG_PIC      | 5,
	/* 25 */ SCENECMD_MUSIC       | 9,
	/* 26 */ SCENECMD_RUN         | SCENE_E4S1,
	/* 27 */ SCENECMD_ENDNOTGOAL,
	/* 28 */ SCENECMD_BG_PIC      | 5,
	/* 29 */ SCENECMD_MUSIC       | 40,
	/* 30 */ SCENECMD_RUN         | SCENE_E4S2,
	/* 31 */ SCENECMD_BG_PIC      | 5,
	/* 32 */ SCENECMD_RUN         | SCENE_E4S3,
	/* 33 */ SCENECMD_BG_PIC      | 5,
	/* 34 */ SCENECMD_MUSIC       | 39,
	/* 35 */ SCENECMD_RUN         | SCENE_E4S4,
	/* 36 */ SCENECMD_BG_PIC      | 5,
	/* 37 */ SCENECMD_RUN         | SCENE_E4S5,
	/* 38 */ SCENECMD_BG_PIC      | 5,
	/* 39 */ SCENECMD_RUN         | SCENE_E4S6,
	/* 40 */ SCENECMD_END,

	// Episode 5 ending
	/* 41 */ SCENECMD_MUSIC      | 8,
	/* 42 */ SCENECMD_BG_PIC     | 5,
	/* 43 */ SCENECMD_RUN        | SCENE_E5S1,
	/* 44 */ SCENECMD_FADE_WHITE,
	/* 45 */ SCENECMD_MUSIC      | 31,
	/* 46 */ SCENECMD_BG_PIC     | 14,
	/* 47 */ SCENECMD_RUN        | SCENE_E5S2,
	/* 48 */ SCENECMD_END,
};

static const Uint8 episodeEndSceneStarts[5] = {0, 7, 11, 24, 41};

static void levelend_displayText(scenelist_t scene) // fka JE_displayText
{
	int yPos = 55;
	for (int i = 0; i < 12; i++)
	{
		if (!textCutscenes[scene][i])
			break;

		if (!ESCPressed)
		{
			JE_outCharGlow(10, yPos, textCutscenes[scene][i]);
			yPos += 10;
		}
	}

	bool advance = false;
	if (frameCountMax != 0)
	{
		frameCountMax = 6;
		advance = true;
	}

	textGlowFont = TINY_FONT;

	JE_outCharGlow(JE_fontCenter(miscText[4], TINY_FONT), 184, miscText[4]);

	do
	{
		if (levelWarningDisplay)
			JE_updateWarning(VGAScreen);

		setDelay(1);
		wait_delay();
	} while (!(JE_anyButton() || (frameCountMax == 0 && advance) || ESCPressed));
	levelWarningDisplay = false;
}

// This is a pared-down version of the episode structure in JE_loadMap
void levelend_playEndScene(unsigned int episode)
{
	if (!episode || episode > 5 || !AP_BITSET(APStats.RestGoalEpisodes, episode - 1))
		return;

	// Clear the bit for this episode's goal, as we've now reached it.
	APStats.RestGoalEpisodes &= ~(1 << (episode - 1));

	const Uint16 *episode_p = &episodeEndScenes[episodeEndSceneStarts[episode - 1]];

	while (*episode_p)
	{
		int var = *episode_p & 0xFF;
		switch ((*episode_p++) & 0xFF00)
		{
			case SCENECMD_MUSIC:
				play_song(var - 1);
				break;

			case SCENECMD_BG_PIC:
				JE_loadPic(VGAScreen, var, false);
				JE_showVGA();
				fade_palette(colors, 10, 0, 255);
				break;

			case SCENECMD_BG_COLOR:
				memcpy(colors, palettes[pcxpal[var - 1]], sizeof(colors));
				JE_clr256(VGAScreen);
				JE_showVGA();
				fade_palette(colors, 1, 0, 255);
				break;

			case SCENECMD_BG_SCROLL:
			{
				Uint8 pic_buffer[320*200]; // screen buffer, 8-bit specific
				Uint8 *vga, *vga2, *pic; // screen pointers, 8-bit specific

				memcpy(VGAScreen2->pixels, VGAScreen->pixels, VGAScreen2->pitch * VGAScreen2->h);
				JE_loadPic(VGAScreen, var, false);
				memcpy(pic_buffer, VGAScreen->pixels, sizeof(pic_buffer));

				service_SDL_events(true);

				for (int z = 0; z <= 199; z++)
				{
					if (!newkey)
					{
						vga = VGAScreen->pixels;
						vga2 = VGAScreen2->pixels;
						pic = pic_buffer + (199 - z) * 320;

						setDelay(1);

						for (int y = 0; y <= 199; y++)
						{
							if (y <= z)
							{
								memcpy(vga, pic, 320);
								pic += 320;
							}
							else
							{
								memcpy(vga, vga2, VGAScreen->pitch);
								vga2 += VGAScreen->pitch;
							}
							vga += VGAScreen->pitch;
						}

						JE_showVGA();
						service_wait_delay();
					}
				}

				memcpy(VGAScreen->pixels, pic_buffer, sizeof(pic_buffer));
				break;
			}

			case SCENECMD_FADE_BLACK:
				fade_black(10);
				JE_clr256(VGAScreen);
				JE_showVGA();
				memcpy(colors, palettes[7], sizeof(colors));
				set_palette(colors, 0, 255);
				break;

			case SCENECMD_FADE_WHITE:
				fade_white(100);
				fade_black(30);
				JE_clr256(VGAScreen);
				JE_showVGA();
				break;

			case SCENECMD_LASTBANK:
				useLastBank = !useLastBank;
				break;

			case SCENECMD_PLAYANIM:
				JE_playAnim("tyrend.anm", 0, 7);
				break;

			case SCENECMD_ENDNOTGOAL:
				if (APStats.RestGoalEpisodes)
					goto exiting_cutscene;
				break;

			default:
				frameCountMax = 3;
				setDelay2(6);
				levelend_displayText(var);
				newkey = false;
				break;
		}
	}

	exiting_cutscene:
	fade_white(100);
	fade_black(30);

	// If no goal episodes remain, inform AP we're done, and roll the credits.
	if (!APStats.RestGoalEpisodes)
	{
		Archipelago_GoalComplete();
		JE_playCredits();
		apmenu_RandomizerStats();
	}
}
