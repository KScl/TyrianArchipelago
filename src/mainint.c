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
#include "mainint.h"

#include "apmsg.h"
#include "backgrnd.h"
#include "config.h"
#include "episodes.h"
#include "file.h"
#include "font.h"
#include "fonthand.h"
#include "helptext.h"
#include "helptext.h"
#include "joystick.h"
#include "keyboard.h"
#include "lds_play.h"
#include "loudness.h"
#include "mouse.h"
#include "mtrand.h"
#include "network.h"
#include "nortsong.h"
#include "nortvars.h"
#include "opentyr.h"
#include "palette.h"
#include "params.h"
#include "pcxmast.h"
#include "picload.h"
#include "player.h"
#include "shots.h"
#include "sndmast.h"
#include "sprite.h"
#include "varz.h"
#include "vga256d.h"
#include "video.h"

#include "archipelago/apconnect.h"

#include <assert.h>
#include <ctype.h>
#include <string.h>

bool button[4];

#define MAX_PAGE 8
#define TOPICS 6
const JE_byte topicStart[TOPICS] = { 0, 1, 2, 3, 7, 255 };

JE_shortint constantLastX;
JE_word textErase;
JE_boolean useLastBank; /* See if I want to use the last 16 colors for DisplayText */

bool pause_pressed = false, ingamemenu_pressed = false;

void JE_outCharGlow(JE_word x, JE_word y, const char *s)
{
	JE_integer maxloc, loc, z;
	JE_shortint glowcol[60]; /* [1..60] */
	JE_shortint glowcolc[60]; /* [1..60] */
	JE_word textloc[60]; /* [1..60] */
	const JE_byte bank = (useLastBank) ? 15 : 14;

	setDelay2(1);

	if (s[0] == '\0')
		return;

	if (frameCountMax == 0)
	{
		JE_textShade(VGAScreen, x, y, s, bank, 0, PART_SHADE);
		JE_showVGA();
	}
	else
	{
		maxloc = strlen(s);
		for (z = 0; z < 60; z++)
		{
			glowcol[z] = -8;
			glowcolc[z] = 1;
		}

		loc = x;
		for (z = 0; z < maxloc; z++)
		{
			textloc[z] = loc;

			int sprite_id = font_ascii[(unsigned char)s[z]];

			if (s[z] == ' ')
				loc += 6;
			else if (sprite_id != -1)
				loc += sprite(TINY_FONT, sprite_id)->width + 1;
		}

		for (loc = 0; (unsigned)loc < strlen(s) + 28; loc++)
		{
			if (!ESCPressed)
			{
				setDelay(frameCountMax);

				NETWORK_KEEP_ALIVE();

				int sprite_id = -1;

				for (z = loc - 28; z <= loc; z++)
				{
					if (z >= 0 && z < maxloc)
					{
						sprite_id = font_ascii[(unsigned char)s[z]];

						if (sprite_id != -1)
						{
							blit_sprite_hv(VGAScreen, textloc[z], y, TINY_FONT, sprite_id, bank, glowcol[z]);

							glowcol[z] += glowcolc[z];
							if (glowcol[z] > 9)
								glowcolc[z] = -1;
						}
					}
				}
				if (sprite_id != -1 && --z < maxloc)
					blit_sprite_dark(VGAScreen, textloc[z] + 1, y + 1, TINY_FONT, sprite_id, true);

				if (JE_anyButton())
					frameCountMax = 0;

				do
				{
					if (levelWarningDisplay)
						JE_updateWarning(VGAScreen);

					SDL_Delay(16);
				} while (!(getDelayTicks() == 0 || ESCPressed));

				JE_showVGA();
			}
		}
	}
}

void JE_initPlayerData(void)
{
	/* JE: New Game Items/Data */

	player[0].items.ship = 1;                     // USP Talon
	player[0].items.weapon[FRONT_WEAPON].id = 1;  // Pulse Cannon
	player[0].items.weapon[REAR_WEAPON].id = 0;   // None
	player[0].items.shield = 4;                   // Gencore High Energy Shield
	player[0].items.generator = 2;                // Advanced MR-12
	for (uint i = 0; i < COUNTOF(player[0].items.sidekick); ++i)
		player[0].items.sidekick[i] = 0;          // None
	player[0].items.special = 0;                  // None

	player[1].items = player[0].items;
	player[1].items.weapon[REAR_WEAPON].id = 15;  // Vulcan Cannon
	player[1].items.sidekick_level = 101;         // 101, 102, 103
	player[1].items.sidekick_series = 0;          // None

	onePlayerAction = false;
	superArcadeMode = SA_NONE;
	superTyrian = false;
	twoPlayerMode = false;

	//secretHint = (mt_rand() % 3) + 1;

	for (uint p = 0; p < COUNTOF(player); ++p)
	{
		for (uint i = 0; i < COUNTOF(player->items.weapon); ++i)
		{
			player[p].items.weapon[i].power = 1;
		}

		player[p].weapon_mode = 1;
		player[p].armor = 10;

		player[p].is_dragonwing = (p == 1);
		player[p].lives = &player[p].items.weapon[p].power;

	}

	mainLevel = FIRST_LEVEL;
	saveLevel = FIRST_LEVEL;
}

void JE_gammaCorrect_func(JE_byte *col, JE_real r)
{
	int temp = roundf(*col * r);
	if (temp > 255)
	{
		temp = 255;
	}
	*col = temp;
}

void JE_gammaCorrect(Palette *colorBuffer, JE_byte gamma)
{
	int x;
	JE_real r = 1 + (JE_real)gamma / 10;

	for (x = 0; x < 256; x++)
	{
		JE_gammaCorrect_func(&(*colorBuffer)[x].r, r);
		JE_gammaCorrect_func(&(*colorBuffer)[x].g, r);
		JE_gammaCorrect_func(&(*colorBuffer)[x].b, r);
	}
}

JE_boolean JE_gammaCheck(void)
{
	bool temp = keysactive[SDL_SCANCODE_F11] != 0;
	if (temp)
	{
		keysactive[SDL_SCANCODE_F11] = false;
		newkey = false;
		gammaCorrection = (gammaCorrection + 1) % 4;
		memcpy(colors, palettes[pcxpal[3-1]], sizeof(colors));
		JE_gammaCorrect(&colors, gammaCorrection);
		set_palette(colors, 0, 255);
	}
	return temp;
}

void JE_doInGameSetup(void)
{
	mouseSetRelative(false);

	haltGame = false;

#ifdef WITH_NETWORK
	if (isNetworkGame)
	{
		network_prepare(PACKET_GAME_MENU);
		network_send(4);  // PACKET_GAME_MENU

		while (true)
		{
			service_SDL_events(false);

			if (packet_in[0] && SDLNet_Read16(&packet_in[0]->data[0]) == PACKET_GAME_MENU)
			{
				network_update();
				break;
			}

			network_update();
			network_check();

			SDL_Delay(16);
		}
	}
#endif

	if (yourInGameMenuRequest)
	{
		if (JE_inGameSetup())
		{
			reallyEndLevel = true;
			playerEndLevel = true;
		}
		quitRequested = false;

		keysactive[SDL_SCANCODE_ESCAPE] = false;

#ifdef WITH_NETWORK
		if (isNetworkGame)
		{
			if (!playerEndLevel)
			{
				network_prepare(PACKET_WAITING);
				network_send(4);  // PACKET_WAITING
			}
			else
			{
				network_prepare(PACKET_GAME_QUIT);
				network_send(4);  // PACKET_GAMEQUIT
			}
		}
#endif
	}

#ifdef WITH_NETWORK
	if (isNetworkGame)
	{
		SDL_Surface *temp_surface = VGAScreen;
		VGAScreen = VGAScreenSeg; /* side-effect of game_screen */

		if (!yourInGameMenuRequest)
		{
			JE_barShade(VGAScreen, 3, 60, 257, 80); /*Help Box*/
			JE_barShade(VGAScreen, 5, 62, 255, 78);
			JE_dString(VGAScreen, 10, 65, "Other player in options menu.", SMALL_FONT_SHAPES);
			JE_showVGA();

			while (true)
			{
				service_SDL_events(false);
				JE_showVGA();

				if (packet_in[0])
				{
					if (SDLNet_Read16(&packet_in[0]->data[0]) == PACKET_WAITING)
					{
						network_check();
						break;
					}
					else if (SDLNet_Read16(&packet_in[0]->data[0]) == PACKET_GAME_QUIT)
					{
						reallyEndLevel = true;
						playerEndLevel = true;

						network_check();
						break;
					}
				}

				network_update();
				network_check();

				SDL_Delay(16);
			}
		}
		else
		{
			/*
			JE_barShade(3, 160, 257, 180); /-*Help Box*-/
			JE_barShade(5, 162, 255, 178);
			tempScreenSeg = VGAScreen;
			JE_dString(VGAScreen, 10, 165, "Waiting for other player.", SMALL_FONT_SHAPES);
			JE_showVGA();
			*/
		}

		while (!network_is_sync())
		{
			service_SDL_events(false);

			network_check();
			SDL_Delay(16);
		}

		VGAScreen = temp_surface; /* side-effect of game_screen */
	}
#endif

	yourInGameMenuRequest = false;

	//skipStarShowVGA = true;

	mouseSetRelative(true);
}

JE_boolean JE_inGameSetup(void)
{
	bool result = false;

	SDL_Surface *temp_surface = VGAScreen;
	VGAScreen = VGAScreenSeg; /* side-effect of game_screen */

	enum MenuItemIndex
	{
		MENU_ITEM_MUSIC_VOLUME = 0,
		MENU_ITEM_EFFECTS_VOLUME,
		MENU_ITEM_DETAIL_LEVEL,
		MENU_ITEM_GAME_SPEED,
		MENU_ITEM_RETURN_TO_GAME,
		MENU_ITEM_QUIT,
	};

	const size_t helpIndexes[] = { 14, 14, 27, 28, 25, 26 };

	if (shopSpriteSheet.data == NULL)
		JE_loadCompShapes(&shopSpriteSheet, '1');  // need mouse pointer sprites

	bool restart = true;

	const size_t menuItemsCount = COUNTOF(inGameText);
	size_t selectedIndex = MENU_ITEM_MUSIC_VOLUME;

	const int yMenuItems = 20;
	const int dyMenuItems = 20;
	const int xMenuItem = 10;
	const int xMenuItemName = xMenuItem;
	const int wMenuItemName = 110;
	const int xMenuItemValue = xMenuItemName + wMenuItemName;
	const int wMenuItemValue = 90;
	const int wMenuItem = wMenuItemName + wMenuItemValue;
	const int hMenuItem = 13;

	for (bool done = false; !done; )
	{
		setDelay(2);

		if (restart)
		{
			// Main box
			JE_barShade(VGAScreen, 3, 13, 217, 137);
			JE_barShade(VGAScreen, 5, 15, 215, 135);

			// Help box
			JE_barShade(VGAScreen, 3, 143, 257, 157);
			JE_barShade(VGAScreen, 5, 145, 255, 155);

			memcpy(VGAScreen2->pixels, VGAScreen->pixels, VGAScreen2->pitch * (VGAScreen2->h - 30));

			mouseCursor = MOUSE_POINTER_NORMAL;

			restart = false;
		}

		// Restore background.
		memcpy(VGAScreen->pixels, VGAScreen2->pixels, (size_t)VGAScreen->pitch * (VGAScreen->h - 30));

		// Draw menu items.
		for (size_t i = 0; i < menuItemsCount; ++i)
		{
			const int y = yMenuItems + dyMenuItems * i;

			const char *const name = inGameText[i];

			const bool selected = i == selectedIndex;
			const bool disabled = (i == MENU_ITEM_GAME_SPEED && APSeedSettings.ForceGameSpeed);

			draw_font_hv_shadow(VGAScreen, xMenuItemName, y, name, normal_font, left_aligned, 15,
				(disabled ? -8 : -4) + (selected ? 2 : 0), false, 2);

			switch (i)
			{
			case MENU_ITEM_MUSIC_VOLUME:
			{
				JE_barDrawShadow(VGAScreen, xMenuItemValue, y, 1, music_disabled ? 12 : 16, (tyrMusicVolume + 6) / 12, 3, 13);
				break;
			}
			case MENU_ITEM_EFFECTS_VOLUME:
			{
				JE_barDrawShadow(VGAScreen, xMenuItemValue, y, 1, samples_disabled ? 12 : 16, (fxVolume + 6) / 12, 3, 13);
				break;
			}
			case MENU_ITEM_DETAIL_LEVEL:
			{
				draw_font_hv_shadow(VGAScreen, xMenuItemValue, y, detailLevel[processorType-1], normal_font, left_aligned, 15,
					(disabled ? -8 : -4) + (selected ? 2 : 0), false, 2);
				break;
			}
			case MENU_ITEM_GAME_SPEED:
			{
				const char *gameSpeedName = (APSeedSettings.ForceGameSpeed)
					? gameSpeedText[APSeedSettings.ForceGameSpeed - 1]
					: gameSpeedText[gameSpeed - 1];

				draw_font_hv_shadow(VGAScreen, xMenuItemValue, y, gameSpeedName, normal_font, left_aligned, 15,
					(disabled ? -8 : -4) + (selected ? 2 : 0), false, 2);
				break;
			}
			}
		}

		// Draw help text.
		JE_outTextAdjust(VGAScreen, 10, 147, mainMenuHelp[helpIndexes[selectedIndex]], 14, 6, TINY_FONT, true);

		push_joysticks_as_keyboard();
		service_SDL_events(false);

		// Special case start/menu as exiting the pause menu.
		for (int j = 0; j < joysticks; ++j)
		{
			if (joystick[j].action_pressed[4])
				done = true;
		}

		// Handle interaction.
		bool action = false;
		bool leftAction = false;
		bool rightAction = false;

		if (!done && mouseActivity == MOUSE_ACTIVE)
		{
			// Find menu item that was hovered or clicked.
			if (mouse_x >= xMenuItem && mouse_x < xMenuItem + wMenuItem)
			{
				for (size_t i = 0; i < menuItemsCount; ++i)
				{
					const int yMenuItem = yMenuItems + dyMenuItems * i;
					if (mouse_y >= yMenuItem && mouse_y < yMenuItem + hMenuItem)
					{
						if (selectedIndex != i)
						{
							JE_playSampleNum(S_CURSOR);

							selectedIndex = i;
						}

						if (newmouse && lastmouse_but == SDL_BUTTON_LEFT &&
						    lastmouse_x >= xMenuItem && lastmouse_x < xMenuItem + wMenuItem &&
						    lastmouse_y >= yMenuItem && lastmouse_y < yMenuItem + hMenuItem)
						{
							// Act on menu item via name.
							if (lastmouse_x >= xMenuItemName && lastmouse_x < xMenuItemName + wMenuItemName)
							{
								action = true;
							}

							// Act on menu item via value.
							else if (lastmouse_x >= xMenuItemValue && lastmouse_x < xMenuItemValue + wMenuItemValue)
							{
								switch (i)
								{
								case MENU_ITEM_MUSIC_VOLUME:
								{
									JE_playSampleNum(S_CURSOR);

									const int w = ((255 + 6) / 12) * (3 + 1) - 1;

									int value = (lastmouse_x - xMenuItemValue) * 255 / (w - 1);
									tyrMusicVolume = MIN(MAX(0, value), 255);

									set_volume(tyrMusicVolume, fxVolume);
									break;
								}
								case MENU_ITEM_EFFECTS_VOLUME:
								{
									const int w = ((255 + 6) / 12) * (3 + 1) - 1;

									int value = (lastmouse_x - xMenuItemValue) * 255 / (w - 1);
									fxVolume = MIN(MAX(0, value), 255);

									set_volume(tyrMusicVolume, fxVolume);

									JE_playSampleNum(S_CURSOR);
									break;
								}
								case MENU_ITEM_DETAIL_LEVEL:
								case MENU_ITEM_GAME_SPEED:
								{
									rightAction = true;
									break;
								}
								default:
									break;
								}
							}
						}

						break;
					}
				}
			}
		}

		if (done)
		{
			JE_playSampleNum(S_SPRING);
		}
		else if (newmouse)
		{
			if (lastmouse_but == SDL_BUTTON_RIGHT)
			{
				JE_playSampleNum(S_SPRING);

				done = true;
			}
		}
		else if (newkey)
		{
			switch (lastkey_scan)
			{
			case SDL_SCANCODE_UP:
			{
				JE_playSampleNum(S_CURSOR);

				selectedIndex = selectedIndex == 0
					? menuItemsCount - 1
					: selectedIndex - 1;
				break;
			}
			case SDL_SCANCODE_DOWN:
			{
				JE_playSampleNum(S_CURSOR);

				selectedIndex = selectedIndex == menuItemsCount - 1
					? 0
					: selectedIndex + 1;
				break;
			}
			case SDL_SCANCODE_LEFT:
			{
				leftAction = true;
				break;
			}
			case SDL_SCANCODE_RIGHT:
			{
				rightAction = true;
				break;
			}
			case SDL_SCANCODE_SPACE:
			case SDL_SCANCODE_RETURN:
			{
				action = true;
				break;
			}
			case SDL_SCANCODE_ESCAPE:
			{
				JE_playSampleNum(S_SPRING);

				done = true;
				break;
			}
			case SDL_SCANCODE_W:
			{
				if (selectedIndex == MENU_ITEM_DETAIL_LEVEL)
				{
					processorType = 6;
					JE_initProcessorType();
				}
				break;
			}
			default:
				break;
			}
		}

		if (action)
		{
			switch (selectedIndex)
			{
			case MENU_ITEM_MUSIC_VOLUME:
			{
				JE_playSampleNum(S_SELECT);

				music_disabled = !music_disabled;
				break;
			}
			case MENU_ITEM_EFFECTS_VOLUME:
			{
				samples_disabled = !samples_disabled;

				JE_playSampleNum(S_SELECT);
				break;
			}
			case MENU_ITEM_RETURN_TO_GAME:
			{
				JE_playSampleNum(S_SELECT);

				done = true;
				break;
			}
			case MENU_ITEM_QUIT:
			{
				JE_playSampleNum(S_SELECT);

#if 0
				if (constantPlay)
					JE_tyrianHalt(0);

				if (isNetworkGame)
				{
					/*Tell other computer to exit*/
					haltGame = true;
					playerEndLevel = true;
				}
#endif

				result = true;
				done = true;
				break;
			}
			default:
				break;
			}
		}
		else if (leftAction)
		{
			switch (selectedIndex)
			{
			case MENU_ITEM_MUSIC_VOLUME:
			{
				JE_playSampleNum(S_CURSOR);

				JE_changeVolume(&tyrMusicVolume, -12, &fxVolume, 0);
				break;
			}
			case MENU_ITEM_EFFECTS_VOLUME:
			{
				JE_changeVolume(&tyrMusicVolume, 0, &fxVolume, -12);

				JE_playSampleNum(S_CURSOR);
				break;
			}
			case MENU_ITEM_DETAIL_LEVEL:
			{
				JE_playSampleNum(S_CURSOR);

				processorType = processorType > 1 ? processorType - 1 : 4;
				JE_initProcessorType();
				JE_setNewGameSpeed();
				break;
			}
			case MENU_ITEM_GAME_SPEED:
			{
				if (APSeedSettings.ForceGameSpeed)
				{
					JE_playSampleNum(S_CLINK);
					break;
				}

				JE_playSampleNum(S_CURSOR);

#ifdef UNBOUNDED_SPEED
				gameSpeed = gameSpeed > 1 ? gameSpeed - 1 : 6;
#else
				gameSpeed = gameSpeed > 1 ? gameSpeed - 1 : 5;
#endif
				JE_initProcessorType();
				JE_setNewGameSpeed();
				break;
			}
			default:
				break;
			}
		}
		else if (rightAction)
		{
			switch (selectedIndex)
			{
			case MENU_ITEM_MUSIC_VOLUME:
			{
				JE_playSampleNum(S_CURSOR);

				JE_changeVolume(&tyrMusicVolume, 12, &fxVolume, 0);
				break;
			}
			case MENU_ITEM_EFFECTS_VOLUME:
			{
				JE_changeVolume(&tyrMusicVolume, 0, &fxVolume, 12);

				JE_playSampleNum(S_CURSOR);
				break;
			}
			case MENU_ITEM_DETAIL_LEVEL:
			{
				JE_playSampleNum(S_CURSOR);

				processorType = processorType < 4 ? processorType + 1 : 1;
				JE_initProcessorType();
				JE_setNewGameSpeed();
				break;
			}
			case MENU_ITEM_GAME_SPEED:
			{
				if (APSeedSettings.ForceGameSpeed)
				{
					JE_playSampleNum(S_CLINK);
					break;
				}

				JE_playSampleNum(S_CURSOR);

#ifdef UNBOUNDED_SPEED
				gameSpeed = gameSpeed < 6 ? gameSpeed + 1 : 1;
#else
				gameSpeed = gameSpeed < 5 ? gameSpeed + 1 : 1;
#endif
				JE_initProcessorType();
				JE_setNewGameSpeed();
				break;
			}
			default:
				break;
			}
		}

		apmsg_manageQueueInGame();
		service_SDL_events(true);
		JE_mouseStart();
		JE_showVGA();
		JE_mouseReplace();
		wait_delay();
	}

	VGAScreen = temp_surface; /* side-effect of game_screen */

	return result;
}

void JE_inGameHelp(void)
{
	SDL_Surface *temp_surface = VGAScreen;
	VGAScreen = VGAScreenSeg; /* side-effect of game_screen */

	//tempScreenSeg = VGAScreenSeg;

	JE_clearKeyboard();
	JE_wipeKey();

	JE_barShade(VGAScreen, 1, 1, 262, 182); /*Main Box*/
	JE_barShade(VGAScreen, 3, 3, 260, 180);
	JE_barShade(VGAScreen, 5, 5, 258, 178);
	JE_barShade(VGAScreen, 7, 7, 256, 176);
	fill_rectangle_xy(VGAScreen, 9, 9, 254, 174, 0);

	if (twoPlayerMode)  // Two-Player Help
	{
		helpBoxColor = 3;
		helpBoxBrightness = 3;
		JE_HBox(VGAScreen, 20,  4, 36, 50);

		// weapon help
		blit_sprite(VGAScreenSeg, 2, 21, OPTION_SHAPES, 43);
		helpBoxColor = 5;
		helpBoxBrightness = 3;
		JE_HBox(VGAScreen, 55, 20, 37, 40);

		// sidekick help
		blit_sprite(VGAScreenSeg, 5, 36, OPTION_SHAPES, 41);
		helpBoxColor = 5;
		helpBoxBrightness = 3;
		JE_HBox(VGAScreen, 40, 43, 34, 44);

		// shield/armor help
		blit_sprite(VGAScreenSeg, 2, 79, OPTION_SHAPES, 42);
		helpBoxColor = 5;
		helpBoxBrightness = 3;
		JE_HBox(VGAScreen, 54, 84, 35, 40);

		helpBoxColor = 5;
		helpBoxBrightness = 3;
		JE_HBox(VGAScreen, 5, 126, 38, 55);
		helpBoxColor = 5;
		helpBoxBrightness = 3;
		JE_HBox(VGAScreen, 5, 160, 39, 55);
	}
	else
	{
		// power bar help
		blit_sprite(VGAScreenSeg, 15, 5, OPTION_SHAPES, 40);
		helpBoxColor = 5;
		helpBoxBrightness = 3;
		JE_HBox(VGAScreen, 40, 10, 31, 45);

		// weapon help
		blit_sprite(VGAScreenSeg, 5, 37, OPTION_SHAPES, 39);
		helpBoxColor = 5;
		helpBoxBrightness = 3;
		JE_HBox(VGAScreen, 40, 40, 32, 44);
		helpBoxColor = 5;
		helpBoxBrightness = 3;
		JE_HBox(VGAScreen, 40, 60, 33, 44);

		// sidekick help
		blit_sprite(VGAScreenSeg, 5, 98, OPTION_SHAPES, 41);
		helpBoxColor = 5;
		helpBoxBrightness = 3;
		JE_HBox(VGAScreen, 40, 103, 34, 44);

		// shield/armor help
		blit_sprite(VGAScreenSeg, 2, 138, OPTION_SHAPES, 42);
		helpBoxColor = 5;
		helpBoxBrightness = 3;
		JE_HBox(VGAScreen, 54, 143, 35, 40);
	}

	// "press a key"
	blit_sprite(VGAScreenSeg, 16, 189, OPTION_SHAPES, 36);  // in-game text area
	JE_outText(VGAScreenSeg, 120 - JE_textWidth(miscText[5-1], TINY_FONT) / 2 + 20, 190, miscText[5-1], 0, 4);

	do
	{
		service_SDL_events(true);

		JE_mouseStart();
		JE_showVGA();
		JE_mouseReplace();

		SDL_Delay(16);

		push_joysticks_as_keyboard();
		service_SDL_events(false);

		NETWORK_KEEP_ALIVE();
	} while (!(newkey || newmouse));

	textErase = 1;

	VGAScreen = temp_surface;
}

// Demo playback isn't _really_ supported any longer, but the code stays here just in case
// someone wants to fix it up later (probably me)
int mainint_loadNextDemo(void)
{
	if (++demo_num > 5)
		demo_num = 1;

	char demo_filename[9];
	snprintf(demo_filename, sizeof(demo_filename), "demo.%d", demo_num);
	demo_file = dir_fopen_die(data_dir(), demo_filename, "rb"); // TODO: only play demos from existing file (instead of dying)

	difficultyLevel = DIFFICULTY_NORMAL;
	bonusLevelCurrent = false;

	Uint8 episode, levelNum;
	fread_u8_die(&episode, 1, demo_file);

	fread_die(levelName, 1, 10, demo_file);
	levelName[10] = '\0';

	fread_u8_die(&levelNum, 1, demo_file);

	JE_initEpisode(episode);

	fread_u8_die(&player[0].items.weapon[FRONT_WEAPON].id,  1, demo_file);
	fread_u8_die(&player[0].items.weapon[REAR_WEAPON].id,   1, demo_file);
	fread_u8_die(&player[0].items.super_arcade_mode,        1, demo_file);
	fread_u8_die(&player[0].items.sidekick[LEFT_SIDEKICK],  1, demo_file);
	fread_u8_die(&player[0].items.sidekick[RIGHT_SIDEKICK], 1, demo_file);
	fread_u8_die(&player[0].items.generator,                1, demo_file);

	fread_u8_die(&player[0].items.sidekick_level,           1, demo_file); // could probably ignore
	fread_u8_die(&player[0].items.sidekick_series,          1, demo_file); // could probably ignore

	fread_u8_die(&initial_episode_num,                      1, demo_file); // could probably ignore

	fread_u8_die(&player[0].items.shield,                   1, demo_file);
	fread_u8_die(&player[0].items.special,                  1, demo_file);
	fread_u8_die(&player[0].items.ship,                     1, demo_file);

	for (uint i = 0; i < 2; ++i)
		fread_u8_die(&player[0].items.weapon[i].power,      1, demo_file);

	Uint8 unused[3];
	fread_u8_die(unused, 3, demo_file);

	fread_u8_die(&levelSong, 1, demo_file);

	demo_keys = 0;

	Uint8 temp2[2] = { 0, 0 };
	fread_u8(temp2, 2, demo_file);
	demo_keys_wait = (temp2[0] << 8) | temp2[1];

	printf("loaded demo '%s'\n", demo_filename);

	sprites_loadMainShapeTables(false);

	// Put in some defaults into AP specific structures
	memset(&APItems, 0, sizeof(APItems));
	memset(&APStats, 0, sizeof(APStats));
	APStats.GeneratorLevel = 3;
	APStats.ArmorLevel = 10;
	APStats.ShieldLevel = 10;

	for (int i = 0; i < LEVELDATA_COUNT; ++i)
	{
		if (allLevelData[i].episodeNum == episode && allLevelData[i].levelNum == levelNum)
			return i;
	}
	return 0;
}

bool replay_demo_keys(void)
{
	while (demo_keys_wait == 0)
	{
		demo_keys = 0;
		fread_u8(&demo_keys, 1, demo_file);

		Uint8 temp2[2] = { 0, 0 };
		fread_u8(temp2, 2, demo_file);
		demo_keys_wait = (temp2[0] << 8) | temp2[1];

		if (feof(demo_file))
		{
			// no more keys
			return false;
		}
	}

	demo_keys_wait--;

	if (demo_keys & (1 << 0))
		player[0].y -= CURRENT_KEY_SPEED;
	if (demo_keys & (1 << 1))
		player[0].y += CURRENT_KEY_SPEED;

	if (demo_keys & (1 << 2))
		player[0].x -= CURRENT_KEY_SPEED;
	if (demo_keys & (1 << 3))
		player[0].x += CURRENT_KEY_SPEED;

	button[0] = (bool)(demo_keys & (1 << 4));
	button[3] = (bool)(demo_keys & (1 << 5));
	button[1] = (bool)(demo_keys & (1 << 6));
	button[2] = (bool)(demo_keys & (1 << 7));

	return true;
}

/*Street Fighter codes*/
void JE_SFCodes(JE_byte playerNum_, JE_integer PX_, JE_integer PY_, JE_integer mouseX_, JE_integer mouseY_)
{
	// No twiddles for the galaga-like extra games
	if (galagaMode)
		return;

	JE_byte buttonCount = (mouseY_ > PY_) +    /*UP*/
	                      (mouseY_ < PY_) +    /*DOWN*/
	                      (PX_ < mouseX_) +    /*LEFT*/
	                      (PX_ > mouseX_);     /*RIGHT*/
	JE_byte inputCode = (mouseY_ > PY_) * 1 + /*UP*/
	                    (mouseY_ < PY_) * 2 + /*DOWN*/
	                    (PX_ < mouseX_) * 3 + /*LEFT*/
	                    (PX_ > mouseX_) * 4;  /*RIGHT*/

	// If no directions are being pressed, and fire isn't either, that's input code 9
	if (buttonCount == 0 && !button[0])
		inputCode = 9;

	// Otherwise, only proceed if exactly one direction pressed
	else if (buttonCount != 1)
		return;

	// Add in the fire button state.
	inputCode += button[0] * 4;

	// In extra games, we use the original list of twiddles for the Stalker 21.126.
	// Otherwise we use the AP specified twiddle list.
	const int numTwiddles = (extraGame) ? 21 : APTwiddles.Count;
	const Uint8 *curCode; // Uint8[8]

	for (int i = 0; i < numTwiddles; i++)
	{
		if (extraGame)
			curCode = keyboardCombos[shipCombosB[i] - 1];
		else
			curCode = APTwiddles.Code[i].Command;

		JE_byte nextInputCode = curCode[SFCurrentCode[playerNum_-1][i]];

		if (nextInputCode == inputCode) // correct key
		{
			++SFCurrentCode[playerNum_-1][i];

			nextInputCode = curCode[SFCurrentCode[playerNum_-1][i]];
			if (nextInputCode > 100 && nextInputCode <= 100 + SPECIAL_NUM)
			{
				SFCurrentCode[playerNum_-1][i] = 0;
				SFExecuted[playerNum_-1] = i + 1;
			}
		}
		else
		{
			if ((inputCode != 9) &&
			    (nextInputCode - 1) % 4 != (inputCode - 1) % 4 &&
			    (SFCurrentCode[playerNum_-1][i] == 0 ||
			     curCode[SFCurrentCode[playerNum_-1][i]-1] != inputCode))
			{
				SFCurrentCode[playerNum_-1][i] = 0;
			}
		}
	}
}

void JE_inGameDisplays(void)
{
	char stemp[21];
	char tempstr[256];

#ifdef DEBUG_OPTIONS
	if (debugDamageViewer)
	{
		snprintf(tempstr, sizeof(tempstr), "Gen: %02d", powerSys[6].power);
		JE_textShade(VGAScreen, 30, 167, tempstr, 3, 4, FULL_SHADE);
		snprintf(tempstr, sizeof(tempstr), "%.2f", damagePerSecondAvg);
	}
	else
	{
		if (debug)
		{
			snprintf(tempstr, sizeof(tempstr), "%s", difficultyNameB[difficultyLevel+1]);
			JE_textShade(VGAScreen, 30, 167, tempstr, 3, 4, FULL_SHADE);
		}
#endif
	if (extraGame)
		snprintf(tempstr, sizeof(tempstr), "%lu", player[0].cash);
	else
	{
		const Uint64 totalCash = APStats.Cash + player[0].cash;
		snprintf(tempstr, sizeof(tempstr), "%llu", (unsigned long long)totalCash);
	}
#ifdef DEBUG_OPTIONS	
	}
#endif

	if (smoothies[6-1]) // This is the same way Tyrian 2000 fixed this and it's good enough for me
		JE_textShade(VGAScreen, 30, 175, tempstr, 8, 8, FULL_SHADE);
	else
		JE_textShade(VGAScreen, 30, 175, tempstr, 2, 4, FULL_SHADE);

	/*Special Weapon?*/
	if (player[0].items.special)
	{
		// Don't use the sprite chosen in game, use the AP item sprite instead.
		// We've updated some of the sprites away from their original question marks.
		sprites_blitArchipelagoItem(VGAScreen, 25, 1, APItemChoices.Special.Item);

		// This code was moved out of JE_doSpecialshot because ... why there??
		// (This means it's no longer affected by filters, just like every other HUD element)
		const bool specialReady = (shotRepeat[SHOT_SPECIAL] == 0
			&& specialWait == 0 && flareDuration < 2 && zinglonDuration < 2);
		blit_sprite2(VGAScreen, 47, 4, spriteSheet9, specialReady ? 94 : 93);
	}

	/*Lives Left*/
	if (onePlayerAction || twoPlayerMode)
	{
		for (int temp = 0; temp < (onePlayerAction ? 1 : 2); temp++)
		{
			const uint extra_lives = *player[temp].lives - 1;

			int y = (temp == 0 && player[0].items.special > 0) ? 35 : 15;
			tempW = (temp == 0) ? 30: 270;

			if (extra_lives >= 5)
			{
				blit_sprite2(VGAScreen, tempW, y, spriteSheet9, 285);
				tempW = (temp == 0) ? 45 : 250;
				sprintf(tempstr, "%d", extra_lives);
				JE_textShade(VGAScreen, tempW, y + 3, tempstr, 15, 1, FULL_SHADE);
			}
			else if (extra_lives >= 1)
			{
				for (uint i = 0; i < extra_lives; ++i)
				{
					blit_sprite2(VGAScreen, tempW, y, spriteSheet9, 285);

					tempW += (temp == 0) ? 12 : -12;
				}
			}

			strcpy(stemp, (temp == 0) ? miscText[49-1] : miscText[50-1]);
			if (isNetworkGame)
			{
				strcpy(stemp, JE_getName(temp+1));
			}

			tempW = (temp == 0) ? 28 : (285 - JE_textWidth(stemp, TINY_FONT));
			JE_textShade(VGAScreen, tempW, y - 7, stemp, 2, 6, FULL_SHADE);
		}
	}

	/*Super Bombs!!*/
	for (uint i = 0; i < COUNTOF(player); ++i)
	{
		int x = (i == 0) ? 30 : 270;

		for (uint j = player[i].superbombs; j > 0; --j)
		{
			blit_sprite2(VGAScreen, x, 160, spriteSheet9, 304);
			x += (i == 0) ? 12 : -12;
		}
	}

	if (youAreCheating)
		JE_outText(VGAScreen, 90, 170, "Cheaters always prosper.", 3, 4);
}

void JE_mainKeyboardInput(void)
{
	JE_gammaCheck();

	/* { In-Game Help } */
	if (keysactive[SDL_SCANCODE_F1])
	{
		if (isNetworkGame)
		{
			helpRequest = true;
		}
		else
		{
			JE_inGameHelp();
			skipStarShowVGA = true;
		}
	}

	if (!Archipelago_IsRacing() && !extraGame)
	{
		// Cheat: Invulnerability
#ifdef SIMPLIFIED_LEVEL_CHEATS
		if (keysactive[SDL_SCANCODE_F2])
#else
		if (keysactive[SDL_SCANCODE_F2] && keysactive[SDL_SCANCODE_F3] && keysactive[SDL_SCANCODE_F6])
#endif
			youAreCheating = !youAreCheating;

	// Cheat: Level Skip
#ifdef SIMPLIFIED_LEVEL_CHEATS
		if (keysactive[SDL_SCANCODE_F3])
#else
		if (keysactive[SDL_SCANCODE_F2] && keysactive[SDL_SCANCODE_F6] && keysactive[SDL_SCANCODE_F7])
#endif
		{
			levelTimer = true;
			levelTimerCountdown = 0;
			endLevel = true;
			levelEnd = 40;
		}
#ifdef SIMPLIFIED_LEVEL_CHEATS
		else if (keysactive[SDL_SCANCODE_F4])
#else
		else if (keysactive[SDL_SCANCODE_F2] && keysactive[SDL_SCANCODE_F6] && keysactive[SDL_SCANCODE_F8])
#endif
		{
			endLevel = true;
			levelEnd = 40;
		}

	// Cheat: Debug
#ifdef SIMPLIFIED_LEVEL_CHEATS
		if (keysactive[SDL_SCANCODE_F10])
		{
			debug = !debug;

			debugHist = 1;
			debugHistCount = 1;

			/* YKS: clock ticks since midnight replaced by SDL_GetTicks */
			lastDebugTime = SDL_GetTicks();
		}
#endif
	}

	/* pause game */
	pause_pressed = pause_pressed || keysactive[SDL_SCANCODE_P];

	/* in-game setup */
	ingamemenu_pressed = ingamemenu_pressed || keysactive[SDL_SCANCODE_ESCAPE];

	// This isn't a cheat, it's always available.
	// Screenshot pause (hides "PAUSED" text on screen while paused)
	if (keysactive[SDL_SCANCODE_BACKSPACE] && keysactive[SDL_SCANCODE_P])
		superPause = !superPause;

	// This isn't a cheat, it's always available.
	// In-game random music selection
	if (keysactive[SDL_SCANCODE_BACKSPACE] && keysactive[SDL_SCANCODE_SCROLLLOCK])
		play_song(mt_rand() % MUSIC_NUM);
}

void JE_pauseGame(void)
{
	mouseSetRelative(false);

	JE_boolean done = false;
	JE_word mouseX, mouseY;

	SDL_Surface *temp_surface = VGAScreen;
	VGAScreen = VGAScreenSeg; /* side-effect of game_screen */

	//tempScreenSeg = VGAScreenSeg; // sega000
	if (!superPause)
	{
		JE_dString(VGAScreenSeg, 120, 90, miscText[22], FONT_SHAPES);

		VGAScreen = VGAScreenSeg;
		JE_showVGA();
	}

	set_volume(tyrMusicVolume / 2, fxVolume);

#ifdef WITH_NETWORK
	if (isNetworkGame)
	{
		network_prepare(PACKET_GAME_PAUSE);
		network_send(4);  // PACKET_GAME_PAUSE

		while (true)
		{
			service_SDL_events(false);

			if (packet_in[0] && SDLNet_Read16(&packet_in[0]->data[0]) == PACKET_GAME_PAUSE)
			{
				network_update();
				break;
			}

			network_update();
			network_check();

			SDL_Delay(16);
		}
	}
#endif

	wait_noinput(false, false, true); // TODO: should up the joystick repeat temporarily instead

	do
	{
		setDelay(2);

		push_joysticks_as_keyboard();
		service_SDL_events(true);

		if ((newkey && lastkey_scan != SDL_SCANCODE_LCTRL && lastkey_scan != SDL_SCANCODE_RCTRL && lastkey_scan != SDL_SCANCODE_LALT && lastkey_scan != SDL_SCANCODE_RALT) ||
		    JE_mousePosition(&mouseX, &mouseY) > 0)
		{
#ifdef WITH_NETWORK
			if (isNetworkGame)
			{
				network_prepare(PACKET_WAITING);
				network_send(4);  // PACKET_WAITING
			}
#endif
			done = true;
		}

#ifdef WITH_NETWORK
		if (isNetworkGame)
		{
			network_check();

			if (packet_in[0] && SDLNet_Read16(&packet_in[0]->data[0]) == PACKET_WAITING)
			{
				network_check();

				done = true;
			}
		}
#endif

		apmsg_manageQueueInGame();
		JE_showVGA();
		wait_delay();
	} while (!done);

#ifdef WITH_NETWORK
	if (isNetworkGame)
	{
		while (!network_is_sync())
		{
			service_SDL_events(false);

			network_check();
			SDL_Delay(16);
		}
	}
#endif

	set_volume(tyrMusicVolume, fxVolume);

	//skipStarShowVGA = true;

	VGAScreen = temp_surface; /* side-effect of game_screen */

	mouseSetRelative(true);
}

void JE_playerMovement(Player *this_player,
                       JE_byte inputDevice,
                       JE_byte playerNum_,
                       JE_word shipGr_,
                       Sprite2_array *shipGrPtr_,
                       JE_word *mouseX_, JE_word *mouseY_)
{
	JE_integer mouseXC, mouseYC;
	JE_integer accelXC, accelYC;

	if (playerNum_ == 2 || !twoPlayerMode)
	{
		tempW = weaponPort[this_player->items.weapon[REAR_WEAPON].id].opnum;

		if (this_player->weapon_mode > tempW)
			this_player->weapon_mode = 1;
	}

#ifdef WITH_NETWORK
	if (isNetworkGame && thisPlayerNum == playerNum_)
	{
		network_state_prepare();
		memset(&packet_state_out[0]->data[4], 0, 10);
	}
#endif

redo:

	if (isNetworkGame)
	{
		inputDevice = 0;
	}

	mouseXC = 0;
	mouseYC = 0;
	accelXC = 0;
	accelYC = 0;

	bool link_gun_analog = false;
	float link_gun_angle = 0;

	if (!endLevel)
		player_handleDeathLink(this_player);

	/* Draw Player */
	if (!this_player->is_alive)
	{
		if (this_player->exploding_ticks > 0)
		{
			--this_player->exploding_ticks;

			if (levelEndFxWait > 0)
			{
				levelEndFxWait--;
			}
			else
			{
				levelEndFxWait = (mt_rand() % 6) + 3;
				if ((mt_rand() % 3) == 1)
					soundQueue[6] = S_EXPLOSION_9;
				else
					soundQueue[5] = S_EXPLOSION_11;
			}

			int explosion_x = this_player->x + (mt_rand() % 32) - 16;
			int explosion_y = this_player->y + (mt_rand() % 32) - 16;
			JE_setupExplosionLarge(false, 0, explosion_x, explosion_y + 7);
			JE_setupExplosionLarge(false, 0, this_player->x, this_player->y + 7);

			if (levelEnd > 0)
				levelEnd--;
		}
		else
		{
			if (twoPlayerMode || onePlayerAction)  // if arcade mode
			{
				if (*this_player->lives > 1)  // respawn if any extra lives
				{
					--(*this_player->lives);

					reallyEndLevel = false;
					shotMultiPos[playerNum_-1] = 0;
					calc_purple_balls_needed(this_player);
					twoPlayerLinked = false;
					if (galagaMode)
						twoPlayerMode = false;
					this_player->y = 160;
					this_player->invulnerable_ticks = 100;
					this_player->is_alive = true;
					endLevel = false;

					player_resetDeathLink();

					if (extraGame)
						this_player->armor = 28;
					else if (episodeNum == 4)
						this_player->armor = APStats.ArmorLevel;
					else
						this_player->armor = APStats.ArmorLevel / 2;

					if (galagaMode)
						this_player->shield = 0;
					else if (extraGame)
						this_player->shield = 5;
					else
						this_player->shield = APStats.ShieldLevel / 2;

					player_drawShield();
					player_drawArmor();
					goto redo;
				}
				else
				{
					if (galagaMode)
						twoPlayerMode = false;
					if (allPlayersGone && isNetworkGame)
						reallyEndLevel = true;
				}

			}
		}
	}
#if 0
	else if (constantDie)
	{
		// finished exploding?  start dying again
		if (this_player->exploding_ticks == 0)
		{
			this_player->shield = 0;

			if (this_player->armor > 0)
			{
				--this_player->armor;
			}
			else
			{
				this_player->is_alive = false;
				this_player->exploding_ticks = 60;
				levelEnd = 40;
			}

			player_wipeShieldArmorBars();
			player_drawArmor();

			// as if instant death weren't enough, player also gets infinite lives in order to enjoy an infinite number of deaths -_-
			if (*player[0].lives < 11)
				++(*player[0].lives);
		}
	}
#endif

	if (!this_player->is_alive)
	{
		explosionFollowAmountX = explosionFollowAmountY = 0;
		return;
	}

	if (!endLevel)
	{
		*mouseX_ = this_player->x;
		*mouseY_ = this_player->y;
		button[1-1] = false;
		button[2-1] = false;
		button[3-1] = false;
		button[4-1] = false;

		/* --- Movement Routine Beginning --- */

		if (!isNetworkGame || playerNum_ == thisPlayerNum)
		{
			if (endLevel)
			{
				this_player->y -= 2;
			}
			else
			{
				if (record_demo || play_demo)
					inputDevice = 1;  // keyboard is required device for demo recording

				// demo playback input
				if (play_demo)
				{
					if (!replay_demo_keys())
					{
						endLevel = true;
						levelEnd = 40;
					}
				}

				/* joystick input */
				if ((inputDevice == 0 || inputDevice >= 3) && joysticks > 0)
				{
					int j = inputDevice  == 0 ? 0 : inputDevice - 3;
					int j_max = inputDevice == 0 ? joysticks : inputDevice - 3 + 1;
					for (; j < j_max; j++)
					{
						poll_joystick(j);

						if (joystick[j].analog)
						{
							mouseXC += joystick_axis_reduce(j, joystick[j].x);
							mouseYC += joystick_axis_reduce(j, joystick[j].y);

							link_gun_analog = joystick_analog_angle(j, &link_gun_angle);
						}
						else
						{
							this_player->x += (joystick[j].direction[3] ? -CURRENT_KEY_SPEED : 0) + (joystick[j].direction[1] ? CURRENT_KEY_SPEED : 0);
							this_player->y += (joystick[j].direction[0] ? -CURRENT_KEY_SPEED : 0) + (joystick[j].direction[2] ? CURRENT_KEY_SPEED : 0);
						}

						button[0] |= joystick[j].action[0];
						button[1] |= joystick[j].action[2];
						button[2] |= joystick[j].action[3];
						button[3] |= joystick[j].action_pressed[1];

						ingamemenu_pressed |= joystick[j].action_pressed[4];
						pause_pressed |= joystick[j].action_pressed[5];
					}
				}

				service_SDL_events(false);

				/* mouse input */
				if ((inputDevice == 0 || inputDevice == 2) && has_mouse)
				{
					button[0] |= mouse_pressed[0];
					button[1] |= mouse_pressed[1];
					button[2] |= mouse_has_three_buttons ? mouse_pressed[2] : mouse_pressed[1];

					Sint32 mouseXR;
					Sint32 mouseYR;
					mouseGetRelativePosition(&mouseXR, &mouseYR);
					mouseXC += mouseXR;
					mouseYC += mouseYR;
				}

				/* keyboard input */
				if ((inputDevice == 0 || inputDevice == 1) && !play_demo)
				{
					if (keysactive[keySettings[KEY_SETTING_UP]])
						this_player->y -= CURRENT_KEY_SPEED;
					if (keysactive[keySettings[KEY_SETTING_DOWN]])
						this_player->y += CURRENT_KEY_SPEED;

					if (keysactive[keySettings[KEY_SETTING_LEFT]])
						this_player->x -= CURRENT_KEY_SPEED;
					if (keysactive[keySettings[KEY_SETTING_RIGHT]])
						this_player->x += CURRENT_KEY_SPEED;

					button[0] = button[0] || keysactive[keySettings[KEY_SETTING_FIRE]];
					button[3] = button[3] || keysactive[keySettings[KEY_SETTING_CHANGE_FIRE]];
					button[1] = button[1] || keysactive[keySettings[KEY_SETTING_LEFT_SIDEKICK]];
					button[2] = button[2] || keysactive[keySettings[KEY_SETTING_RIGHT_SIDEKICK]];

#if 0
					if (constantPlay)
					{
						for (unsigned int i = 0; i < 4; i++)
							button[i] = true;

						++this_player->y;
						this_player->x += constantLastX;
					}

					// TODO: check if demo recording still works
					if (record_demo)
					{
						bool new_input = false;

						for (unsigned int i = 0; i < 8; i++)
						{
							bool temp = demo_keys & (1 << i);
							if (temp != keysactive[keySettings[i]])
								new_input = true;
						}

						demo_keys_wait++;

						if (new_input)
						{
							Uint8 temp2[2] = { demo_keys_wait >> 8, demo_keys_wait };
							fwrite_u8(temp2, 2, demo_file);

							demo_keys = 0;
							for (unsigned int i = 0; i < 8; i++)
								demo_keys |= keysactive[keySettings[i]] ? (1 << i) : 0;

							fwrite_u8(&demo_keys, 1, demo_file);

							demo_keys_wait = 0;
						}
					}
#endif
				}

				if (smoothies[9-1])
				{
					*mouseY_ = this_player->y - (*mouseY_ - this_player->y);
					mouseYC = -mouseYC;
				}

				accelXC += this_player->x - *mouseX_;
				accelYC += this_player->y - *mouseY_;

				if (mouseXC > 30)
					mouseXC = 30;
				else if (mouseXC < -30)
					mouseXC = -30;
				if (mouseYC > 30)
					mouseYC = 30;
				else if (mouseYC < -30)
					mouseYC = -30;

				if (mouseXC > 0)
					this_player->x += (mouseXC + 3) / 4;
				else if (mouseXC < 0)
					this_player->x += (mouseXC - 3) / 4;
				if (mouseYC > 0)
					this_player->y += (mouseYC + 3) / 4;
				else if (mouseYC < 0)
					this_player->y += (mouseYC - 3) / 4;

				if (mouseXC > 3)
					accelXC++;
				else if (mouseXC < -2)
					accelXC--;
				if (mouseYC > 2)
					accelYC++;
				else if (mouseYC < -2)
					accelYC--;

			}   /*endLevel*/

#ifdef WITH_NETWORK
			if (isNetworkGame && playerNum_ == thisPlayerNum)
			{
				Uint16 buttons = 0;
				for (int i = 4 - 1; i >= 0; i--)
				{
					buttons <<= 1;
					buttons |= button[i];
				}

				SDLNet_Write16(this_player->x - *mouseX_, &packet_state_out[0]->data[4]);
				SDLNet_Write16(this_player->y - *mouseY_, &packet_state_out[0]->data[6]);
				SDLNet_Write16(accelXC,                   &packet_state_out[0]->data[8]);
				SDLNet_Write16(accelYC,                   &packet_state_out[0]->data[10]);
				SDLNet_Write16(buttons,                   &packet_state_out[0]->data[12]);

				this_player->x = *mouseX_;
				this_player->y = *mouseY_;

				button[0] = false;
				button[1] = false;
				button[2] = false;
				button[3] = false;

				accelXC = 0;
				accelYC = 0;
			}
#endif
		}  /*isNetworkGame*/

		/* --- Movement Routine Ending --- */

		moveOk = true;

#ifdef WITH_NETWORK
		if (isNetworkGame && !network_state_is_reset())
		{
			if (playerNum_ != thisPlayerNum)
			{
				if (thisPlayerNum == 2)
					difficultyLevel = SDLNet_Read16(&packet_state_in[0]->data[16]);

				Uint16 buttons = SDLNet_Read16(&packet_state_in[0]->data[12]);
				for (int i = 0; i < 4; i++)
				{
					button[i] = buttons & 1;
					buttons >>= 1;
				}

				this_player->x += (Sint16)SDLNet_Read16(&packet_state_in[0]->data[4]);
				this_player->y += (Sint16)SDLNet_Read16(&packet_state_in[0]->data[6]);
				accelXC = (Sint16)SDLNet_Read16(&packet_state_in[0]->data[8]);
				accelYC = (Sint16)SDLNet_Read16(&packet_state_in[0]->data[10]);
			}
			else
			{
				Uint16 buttons = SDLNet_Read16(&packet_state_out[network_delay]->data[12]);
				for (int i = 0; i < 4; i++)
				{
					button[i] = buttons & 1;
					buttons >>= 1;
				}

				this_player->x += (Sint16)SDLNet_Read16(&packet_state_out[network_delay]->data[4]);
				this_player->y += (Sint16)SDLNet_Read16(&packet_state_out[network_delay]->data[6]);
				accelXC = (Sint16)SDLNet_Read16(&packet_state_out[network_delay]->data[8]);
				accelYC = (Sint16)SDLNet_Read16(&packet_state_out[network_delay]->data[10]);
			}
		}
#endif

		/*Street-Fighter codes*/
		JE_SFCodes(playerNum_, this_player->x, this_player->y, *mouseX_, *mouseY_);

		if (moveOk)
		{
			/* END OF MOVEMENT ROUTINES */

			/*Linking Routines*/

			if (twoPlayerMode && !twoPlayerLinked && this_player->x == *mouseX_ && this_player->y == *mouseY_ &&
			    abs(player[0].x - player[1].x) < 8 && abs(player[0].y - player[1].y) < 8 &&
			    player[0].is_alive && player[1].is_alive && !galagaMode)
			{
				twoPlayerLinked = true;
			}

			if (playerNum_ == 1 && (button[3-1] || button[2-1]) && !galagaMode)
				twoPlayerLinked = false;

			if (twoPlayerMode && twoPlayerLinked && playerNum_ == 2 &&
			    (this_player->x != *mouseX_ || this_player->y != *mouseY_))
			{
				if (button[0])
				{
					if (link_gun_analog)
					{
						linkGunDirec = link_gun_angle;
					}
					else
					{
						JE_real tempR;

						if (abs(this_player->x - *mouseX_) > abs(this_player->y - *mouseY_))
							tempR = (this_player->x - *mouseX_ > 0) ? M_PI_2 : (M_PI + M_PI_2);
						else
							tempR = (this_player->y - *mouseY_ > 0) ? 0 : M_PI;

						if (fabsf(linkGunDirec - tempR) < 0.3f)
							linkGunDirec = tempR;
						else if (linkGunDirec < tempR && linkGunDirec - tempR > -3.24f)
							linkGunDirec += 0.2f;
						else if (linkGunDirec - tempR < M_PI)
							linkGunDirec -= 0.2f;
						else
							linkGunDirec += 0.2f;
					}

					if (linkGunDirec >= (2 * M_PI))
						linkGunDirec -= (2 * M_PI);
					else if (linkGunDirec < 0)
						linkGunDirec += (2 * M_PI);
				}
				else if (!galagaMode)
				{
					twoPlayerLinked = false;
				}
			}
		}
	}

	if (levelEnd > 0 && all_players_dead())
		reallyEndLevel = true;

	/* End Level Fade-Out */
	if (this_player->is_alive && endLevel)
	{
		if (levelEnd == 0)
		{
			reallyEndLevel = true;
		}
		else
		{
			this_player->y -= levelEndWarp;
			if (this_player->y < -200)
				reallyEndLevel = true;

			int trail_spacing = 1;
			int trail_y = this_player->y;
			int num_trails = abs(41 - levelEnd);
			if (num_trails > 20)
				num_trails = 20;

			for (int i = 0; i < num_trails; i++)
			{
				trail_y += trail_spacing;
				trail_spacing++;
			}

			for (int i = 1; i < num_trails; i++)
			{
				trail_y -= trail_spacing;
				trail_spacing--;

				if (trail_y > 0 && trail_y < 170)
				{
					if (shipGr_ == 0)
					{
						blit_sprite2x2(VGAScreen, this_player->x - 17, trail_y - 7, *shipGrPtr_, 13);
						blit_sprite2x2(VGAScreen, this_player->x + 7 , trail_y - 7, *shipGrPtr_, 51);
					}
					else if (shipGr_ == 1)
					{
						blit_sprite2x2(VGAScreen, this_player->x - 17, trail_y - 7, *shipGrPtr_, 220);
						blit_sprite2x2(VGAScreen, this_player->x + 7 , trail_y - 7, *shipGrPtr_, 222);
					}
					else
					{
						blit_sprite2x2(VGAScreen, this_player->x - 5, trail_y - 7, *shipGrPtr_, shipGr_);
					}
				}
			}
		}
	}

	if (play_demo)
		JE_dString(VGAScreen, 115, 10, miscText[7], SMALL_FONT_SHAPES); // insert coin

	if (this_player->is_alive && !endLevel)
	{
		if (!twoPlayerLinked || playerNum_ < 2)
		{
			if (!twoPlayerMode || shipGr2 != 0)  // if not dragonwing
			{
				if (this_player->sidekick[LEFT_SIDEKICK].style == 0)
				{
					this_player->sidekick[LEFT_SIDEKICK].x = *mouseX_ - 14;
					this_player->sidekick[LEFT_SIDEKICK].y = *mouseY_;
				}

				if (this_player->sidekick[RIGHT_SIDEKICK].style == 0)
				{
					this_player->sidekick[RIGHT_SIDEKICK].x = *mouseX_ + 16;
					this_player->sidekick[RIGHT_SIDEKICK].y = *mouseY_;
				}
			}

			if (this_player->x_friction_ticks > 0)
			{
				--this_player->x_friction_ticks;
			}
			else
			{
				this_player->x_friction_ticks = 1;

				if (this_player->x_velocity < 0)
					++this_player->x_velocity;
				else if (this_player->x_velocity > 0)
					--this_player->x_velocity;
			}

			if (this_player->y_friction_ticks > 0)
			{
				--this_player->y_friction_ticks;
			}
			else
			{
				this_player->y_friction_ticks = 2;

				if (this_player->y_velocity < 0)
					++this_player->y_velocity;
				else if (this_player->y_velocity > 0)
					--this_player->y_velocity;
			}

			this_player->x_velocity += accelXC;
			this_player->y_velocity += accelYC;

			this_player->x_velocity = MIN(MAX(-4, this_player->x_velocity), 4);
			this_player->y_velocity = MIN(MAX(-4, this_player->y_velocity), 4);

			this_player->x += this_player->x_velocity;
			this_player->y += this_player->y_velocity;

			// if player moved, add new ship x, y history entry
			if (this_player->x - *mouseX_ != 0 || this_player->y - *mouseY_ != 0)
			{
				for (uint i = 1; i < COUNTOF(player->old_x); ++i)
				{
					this_player->old_x[i - 1] = this_player->old_x[i];
					this_player->old_y[i - 1] = this_player->old_y[i];
				}
				this_player->old_x[COUNTOF(player->old_x) - 1] = this_player->x;
				this_player->old_y[COUNTOF(player->old_x) - 1] = this_player->y;
			}
		}
		else  /*twoPlayerLinked*/
		{
			if (shipGr_ == 0)
				this_player->x = player[0].x - 1;
			else
				this_player->x = player[0].x;
			this_player->y = player[0].y + 8;

			this_player->x_velocity = player[0].x_velocity;
			this_player->y_velocity = 4;

			// turret direction marker/shield
			shotMultiPos[SHOT_MISC] = 0;
			b = player_shot_create(0, SHOT_MISC, this_player->x + 1 + roundf(sinf(linkGunDirec + 0.2f) * 26), this_player->y + roundf(cosf(linkGunDirec + 0.2f) * 26), *mouseX_, *mouseY_, 148, playerNum_);
			shotMultiPos[SHOT_MISC] = 0;
			b = player_shot_create(0, SHOT_MISC, this_player->x + 1 + roundf(sinf(linkGunDirec - 0.2f) * 26), this_player->y + roundf(cosf(linkGunDirec - 0.2f) * 26), *mouseX_, *mouseY_, 148, playerNum_);
			shotMultiPos[SHOT_MISC] = 0;
			b = player_shot_create(0, SHOT_MISC, this_player->x + 1 + roundf(sinf(linkGunDirec) * 26), this_player->y + roundf(cosf(linkGunDirec) * 26), *mouseX_, *mouseY_, 147, playerNum_);

			if (shotRepeat[SHOT_REAR] > 0)
			{
				--shotRepeat[SHOT_REAR];
			}
			else if (button[1-1])
			{
				shotMultiPos[SHOT_REAR] = 0;
				b = player_shot_create(0, SHOT_REAR, this_player->x + 1 + roundf(sinf(linkGunDirec) * 20), this_player->y + roundf(cosf(linkGunDirec) * 20), *mouseX_, *mouseY_, linkGunWeapons[this_player->items.weapon[REAR_WEAPON].id-1], playerNum_);
				player_shot_set_direction(b, this_player->items.weapon[REAR_WEAPON].id, linkGunDirec);
			}
		}
	}

	if (!endLevel)
	{
		if (this_player->x > 256)
		{
			this_player->x = 256;
			constantLastX = -constantLastX;
		}
		if (this_player->x < 40)
		{
			this_player->x = 40;
			constantLastX = -constantLastX;
		}

		if (isNetworkGame && playerNum_ == 1)
		{
			if (this_player->y > 154)
				this_player->y = 154;
		}
		else
		{
			if (this_player->y > 160)
				this_player->y = 160;
		}

		if (this_player->y < 10)
			this_player->y = 10;

		// Determines the ship banking sprite to display, depending on horizontal velocity and acceleration
		int ship_banking = this_player->x_velocity / 2 + (this_player->x - *mouseX_) / 6;
		ship_banking = MAX(-2, MIN(ship_banking, 2));

		int ship_sprite = ship_banking * 2 + shipGr_;

		explosionFollowAmountX = this_player->x - this_player->last_x_explosion_follow;
		explosionFollowAmountY = this_player->y - this_player->last_y_explosion_follow;

		if (explosionFollowAmountY < 0)
			explosionFollowAmountY = 0;

		this_player->last_x_explosion_follow = this_player->x;
		this_player->last_y_explosion_follow = this_player->y;

		if (shipGr_ == 0)
		{
			if (background2)
			{
				blit_sprite2x2_darken(VGAScreen, this_player->x - 17 - mapX2Ofs + 30, this_player->y - 7 + shadowYDist, *shipGrPtr_, ship_sprite + 13);
				blit_sprite2x2_darken(VGAScreen, this_player->x + 7 - mapX2Ofs + 30, this_player->y - 7 + shadowYDist, *shipGrPtr_, ship_sprite + 51);
				if (superWild)
				{
					blit_sprite2x2_darken(VGAScreen, this_player->x - 16 - mapX2Ofs + 30, this_player->y - 7 + shadowYDist, *shipGrPtr_, ship_sprite + 13);
					blit_sprite2x2_darken(VGAScreen, this_player->x + 6 - mapX2Ofs + 30, this_player->y - 7 + shadowYDist, *shipGrPtr_, ship_sprite + 51);
				}
			}
		}
		else if (shipGr_ == 1)
		{
			if (background2)
			{
				blit_sprite2x2_darken(VGAScreen, this_player->x - 17 - mapX2Ofs + 30, this_player->y - 7 + shadowYDist, *shipGrPtr_, 220);
				blit_sprite2x2_darken(VGAScreen, this_player->x + 7 - mapX2Ofs + 30, this_player->y - 7 + shadowYDist, *shipGrPtr_, 222);
			}
		}
		else
		{
			if (background2)
			{
				blit_sprite2x2_darken(VGAScreen, this_player->x - 5 - mapX2Ofs + 30, this_player->y - 7 + shadowYDist, *shipGrPtr_, ship_sprite);
				if (superWild)
				{
					blit_sprite2x2_darken(VGAScreen, this_player->x - 4 - mapX2Ofs + 30, this_player->y - 7 + shadowYDist, *shipGrPtr_, ship_sprite);
				}
			}
		}

		if (this_player->invulnerable_ticks > 0)
		{
			--this_player->invulnerable_ticks;

			if (shipGr_ == 0)
			{
				blit_sprite2x2_blend(VGAScreen, this_player->x - 17, this_player->y - 7, *shipGrPtr_, ship_sprite + 13);
				blit_sprite2x2_blend(VGAScreen, this_player->x + 7 , this_player->y - 7, *shipGrPtr_, ship_sprite + 51);
			}
			else if (shipGr_ == 1)
			{
				blit_sprite2x2_blend(VGAScreen, this_player->x - 17, this_player->y - 7, *shipGrPtr_, 220);
				blit_sprite2x2_blend(VGAScreen, this_player->x + 7 , this_player->y - 7, *shipGrPtr_, 222);
			}
			else
				blit_sprite2x2_blend(VGAScreen, this_player->x - 5, this_player->y - 7, *shipGrPtr_, ship_sprite);
		}
		else
		{
			if (shipGr_ == 0)
			{
				blit_sprite2x2(VGAScreen, this_player->x - 17, this_player->y - 7, *shipGrPtr_, ship_sprite + 13);
				blit_sprite2x2(VGAScreen, this_player->x + 7, this_player->y - 7, *shipGrPtr_, ship_sprite + 51);
			}
			else if (shipGr_ == 1)
			{
				blit_sprite2x2(VGAScreen, this_player->x - 17, this_player->y - 7, *shipGrPtr_, 220);
				blit_sprite2x2(VGAScreen, this_player->x + 7, this_player->y - 7, *shipGrPtr_, 222);

				int ship_banking = 0;
				switch (ship_sprite)
				{
				case 5:
					blit_sprite2(VGAScreen, this_player->x - 17, this_player->y + 7, *shipGrPtr_, 40);
					tempW = this_player->x - 7;
					ship_banking = -2;
					break;
				case 3:
					blit_sprite2(VGAScreen, this_player->x - 17, this_player->y + 7, *shipGrPtr_, 39);
					tempW = this_player->x - 7;
					ship_banking = -1;
					break;
				case 1:
					ship_banking = 0;
					break;
				case -1:
					blit_sprite2(VGAScreen, this_player->x + 19, this_player->y + 7, *shipGrPtr_, 58);
					tempW = this_player->x + 9;
					ship_banking = 1;
					break;
				case -3:
					blit_sprite2(VGAScreen, this_player->x + 19, this_player->y + 7, *shipGrPtr_, 59);
					tempW = this_player->x + 9;
					ship_banking = 2;
					break;
				}
				if (ship_banking != 0)  // NortSparks
				{
					if (shotRepeat[SHOT_NORTSPARKS] > 0)
					{
						--shotRepeat[SHOT_NORTSPARKS];
					}
					else
					{
						b = player_shot_create(0, SHOT_NORTSPARKS, tempW + (mt_rand() % 8) - 4, this_player->y + (mt_rand() % 8) - 4, *mouseX_, *mouseY_, 671, 1);
						shotRepeat[SHOT_NORTSPARKS] = abs(ship_banking) - 1;
					}
				}
			}
			else
			{
				blit_sprite2x2(VGAScreen, this_player->x - 5, this_player->y - 7, *shipGrPtr_, ship_sprite);
			}
		}

		/*Options Location*/
		if (playerNum_ == 2 && shipGr_ == 0)  // if dragonwing
		{
			if (this_player->sidekick[LEFT_SIDEKICK].style == 0)
			{
				this_player->sidekick[LEFT_SIDEKICK].x = this_player->x - 14 + ship_banking * 2;
				this_player->sidekick[LEFT_SIDEKICK].y = this_player->y;
			}

			if (this_player->sidekick[RIGHT_SIDEKICK].style == 0)
			{
				this_player->sidekick[RIGHT_SIDEKICK].x = this_player->x + 17 + ship_banking * 2;
				this_player->sidekick[RIGHT_SIDEKICK].y = this_player->y;
			}
		}
	}  // !endLevel

	if (moveOk)
	{
		if (this_player->is_alive)
		{
			if (!endLevel)
			{
				this_player->delta_x_shot_move = this_player->x - this_player->last_x_shot_move;
				this_player->delta_y_shot_move = this_player->y - this_player->last_y_shot_move;

				/* PLAYER SHOT Change */
				if (button[4-1])
				{
					portConfigChange = true;
					if (portConfigDone)
					{
						shotMultiPos[SHOT_REAR] = 0;

						if (superArcadeMode != SA_NONE && superArcadeMode <= SA_NORTSHIPZ)
						{
							shotMultiPos[SHOT_SPECIAL] = 0;
							shotMultiPos[SHOT_SPECIAL2] = 0;
							if (player[0].items.special == SASpecialWeapon[superArcadeMode-1])
							{
								player[0].items.special = SASpecialWeaponB[superArcadeMode-1];
								this_player->weapon_mode = 2;
							}
							else
							{
								player[0].items.special = SASpecialWeapon[superArcadeMode-1];
								this_player->weapon_mode = 1;
							}
						}
						else if (++this_player->weapon_mode > player_getPortConfigCount(playerNum_ - 1))
							this_player->weapon_mode = 1;

						APItemChoices.RearMode2 = (this_player->weapon_mode == 2);
						player_drawPortConfigButtons();
						portConfigDone = false;
					}
				}

				/* PLAYER SHOT Creation */

				/*SpecialShot*/
				if (!galagaMode)
					JE_doSpecialShot(playerNum_, &this_player->armor, &this_player->shield);

				/*Normal Main Weapons*/
				if (!(twoPlayerLinked && playerNum_ == 2))
				{
					int min, max;

					if (!twoPlayerMode)
						min = 1, max = 2;
					else
						min = max = playerNum_;

					for (temp = min - 1; temp < max; temp++)
					{
						const uint item = this_player->items.weapon[temp].id;

						if (item > 0)
						{
							if (shotRepeat[temp] > 0)
							{
								--shotRepeat[temp];
							}
							else if (button[1-1])
							{
								const uint item_power = galagaMode ? 0 : this_player->items.weapon[temp].power - 1,
								           item_mode = (temp == REAR_WEAPON) ? this_player->weapon_mode - 1 : 0;

								b = player_shot_create(item, temp, this_player->x, this_player->y, *mouseX_, *mouseY_, weaponPort[item].op[item_mode][item_power], playerNum_);
							}
						}
					}
				}

				/*Super Charge Weapons*/
				if (playerNum_ == 2)
				{

					if (!twoPlayerLinked)
						blit_sprite2(VGAScreen, this_player->x + (shipGr_ == 0) + 1, this_player->y - 13, spriteSheet10, 77 + chargeLevel + chargeGr * 19);

					if (chargeGrWait > 0)
					{
						chargeGrWait--;
					}
					else
					{
						chargeGr++;
						if (chargeGr == 4)
							chargeGr = 0;
						chargeGrWait = 3;
					}

					if (chargeLevel > 0)
					{
						fill_rectangle_xy(VGAScreenSeg, 269, 107 + (chargeLevel - 1) * 3, 275, 108 + (chargeLevel - 1) * 3, 193);
					}

					if (chargeWait > 0)
					{
						chargeWait--;
					}
					else
					{
						if (chargeLevel < chargeMax)
							chargeLevel++;

						chargeWait = 28 - this_player->items.weapon[REAR_WEAPON].power * 2;
						if (difficultyLevel > DIFFICULTY_HARD)
							chargeWait -= 5;
					}

					if (chargeLevel > 0)
						fill_rectangle_xy(VGAScreenSeg, 269, 107 + (chargeLevel - 1) * 3, 275, 108 + (chargeLevel - 1) * 3, 204);

					if (shotRepeat[SHOT_P2_CHARGE] > 0)
					{
						--shotRepeat[SHOT_P2_CHARGE];
					}
					else if (button[1-1] && (!twoPlayerLinked || chargeLevel > 0))
					{
						shotMultiPos[SHOT_P2_CHARGE] = 0;
						b = player_shot_create(16, SHOT_P2_CHARGE, this_player->x, this_player->y, *mouseX_, *mouseY_, chargeGunWeapons[player[1].items.weapon[REAR_WEAPON].id-1] + chargeLevel, playerNum_);

						if (chargeLevel > 0)
							fill_rectangle_xy(VGAScreenSeg, 269, 107 + (chargeLevel - 1) * 3, 275, 108 + (chargeLevel - 1) * 3, 193);

						chargeLevel = 0;
						chargeWait = 30 - this_player->items.weapon[REAR_WEAPON].power * 2;
					}
				}

				/*SUPER BOMB*/
				temp = playerNum_;
				if (temp == 0)
					temp = 1;  /*Get whether player 1 or 2*/

				if (player[temp-1].superbombs > 0)
				{
					if (shotRepeat[SHOT_P1_SUPERBOMB + temp-1] > 0)
					{
						--shotRepeat[SHOT_P1_SUPERBOMB + temp-1];
					}
					else if (button[3-1] || button[2-1])
					{
						--player[temp-1].superbombs;
						shotMultiPos[SHOT_P1_SUPERBOMB + temp-1] = 0;
						b = player_shot_create(16, SHOT_P1_SUPERBOMB + temp-1, this_player->x, this_player->y, *mouseX_, *mouseY_, 535, playerNum_);
					}
				}

				// sidekicks

				if (this_player->sidekick[LEFT_SIDEKICK].style == 4 && this_player->sidekick[RIGHT_SIDEKICK].style == 4)
					optionSatelliteRotate += 0.2f;
				else if (this_player->sidekick[LEFT_SIDEKICK].style == 4 || this_player->sidekick[RIGHT_SIDEKICK].style == 4)
					optionSatelliteRotate += 0.15f;

				switch (this_player->sidekick[LEFT_SIDEKICK].style)
				{
				case 1:  // trailing
				case 3:
					this_player->sidekick[LEFT_SIDEKICK].x = this_player->old_x[COUNTOF(player->old_x) / 2 - 1];
					this_player->sidekick[LEFT_SIDEKICK].y = this_player->old_y[COUNTOF(player->old_x) / 2 - 1];
					break;
				case 2:  // front-mounted
					this_player->sidekick[LEFT_SIDEKICK].x = this_player->x;
					this_player->sidekick[LEFT_SIDEKICK].y = MAX(10, this_player->y - 20);
					break;
				case 4:  // orbiting
					this_player->sidekick[LEFT_SIDEKICK].x = this_player->x + roundf(sinf(optionSatelliteRotate) * 20);
					this_player->sidekick[LEFT_SIDEKICK].y = this_player->y + roundf(cosf(optionSatelliteRotate) * 20);
					break;
				}

				switch (this_player->sidekick[RIGHT_SIDEKICK].style)
				{
				case 4:  // orbiting
					this_player->sidekick[RIGHT_SIDEKICK].x = this_player->x - roundf(sinf(optionSatelliteRotate) * 20);
					this_player->sidekick[RIGHT_SIDEKICK].y = this_player->y - roundf(cosf(optionSatelliteRotate) * 20);
					break;
				case 1:  // trailing
				case 3:
					this_player->sidekick[RIGHT_SIDEKICK].x = this_player->old_x[0];
					this_player->sidekick[RIGHT_SIDEKICK].y = this_player->old_y[0];
					break;
				case 2:  // front-mounted
					if (!optionAttachmentLinked)
					{
						this_player->sidekick[RIGHT_SIDEKICK].y += optionAttachmentMove / 2;
						if (optionAttachmentMove >= -2)
						{
							if (optionAttachmentReturn)
								temp = 2;
							else
								temp = 0;

							if (this_player->sidekick[RIGHT_SIDEKICK].y > (this_player->y - 20) + 5)
							{
								temp = 2;
								optionAttachmentMove -= 1 + optionAttachmentReturn;
							}
							else if (this_player->sidekick[RIGHT_SIDEKICK].y > (this_player->y - 20) - 0)
							{
								temp = 3;
								if (optionAttachmentMove > 0)
									optionAttachmentMove--;
								else
									optionAttachmentMove++;
							}
							else if (this_player->sidekick[RIGHT_SIDEKICK].y > (this_player->y - 20) - 5)
							{
								temp = 2;
								optionAttachmentMove++;
							}
							else if (optionAttachmentMove < 2 + optionAttachmentReturn * 4)
							{
								optionAttachmentMove += 1 + optionAttachmentReturn;
							}

							if (optionAttachmentReturn)
								temp = temp * 2;
							if (abs(this_player->sidekick[RIGHT_SIDEKICK].x - this_player->x) < temp)
								temp = 1;

							if (this_player->sidekick[RIGHT_SIDEKICK].x > this_player->x)
								this_player->sidekick[RIGHT_SIDEKICK].x -= temp;
							else if (this_player->sidekick[RIGHT_SIDEKICK].x < this_player->x)
								this_player->sidekick[RIGHT_SIDEKICK].x += temp;

							if (abs(this_player->sidekick[RIGHT_SIDEKICK].y - (this_player->y - 20)) + abs(this_player->sidekick[RIGHT_SIDEKICK].x - this_player->x) < 8)
							{
								optionAttachmentLinked = true;
								soundQueue[2] = S_CLINK;
							}

							if (button[3-1])
								optionAttachmentReturn = true;
						}
						else  // sidekick needs to catch up to player
						{
							optionAttachmentMove += 1 + optionAttachmentReturn;
							JE_setupExplosion(this_player->sidekick[RIGHT_SIDEKICK].x + 1, this_player->sidekick[RIGHT_SIDEKICK].y + 10, 0, 0, false, false);
						}
					}
					else
					{
						this_player->sidekick[RIGHT_SIDEKICK].x = this_player->x;
						this_player->sidekick[RIGHT_SIDEKICK].y = this_player->y - 20;
						if (button[3-1])
						{
							optionAttachmentLinked = false;
							optionAttachmentReturn = false;
							optionAttachmentMove = -20;
							soundQueue[3] = S_WEAPON_26;
						}
					}

					if (this_player->sidekick[RIGHT_SIDEKICK].y < 10)
						this_player->sidekick[RIGHT_SIDEKICK].y = 10;
					break;
				}

				if (playerNum_ == 2 || !twoPlayerMode)  // if player has sidekicks
				{
					for (uint i = 0; i < COUNTOF(player->items.sidekick); ++i)
					{
						uint shot_i = (i == 0) ? SHOT_LEFT_SIDEKICK : SHOT_RIGHT_SIDEKICK;

						JE_OptionType *this_option = &options[this_player->items.sidekick[i]];

						// fire/refill sidekick
						if (this_option->wport > 0)
						{
							if (shotRepeat[shot_i] > 0)
							{
								--shotRepeat[shot_i];
							}
							else
							{
								const int ammo_max = this_player->sidekick[i].ammo_max;

								if (ammo_max > 0)  // sidekick has limited ammo
								{
									if (this_player->sidekick[i].ammo_refill_ticks > 0)
									{
										--this_player->sidekick[i].ammo_refill_ticks;
									}
									else  // refill one ammo
									{
										this_player->sidekick[i].ammo_refill_ticks = this_player->sidekick[i].ammo_refill_ticks_max;

										if (this_player->sidekick[i].ammo < ammo_max)
											++this_player->sidekick[i].ammo;

										// draw sidekick refill ammo gauge
										const int y = hud_sidekick_y[twoPlayerMode ? 1 : 0][i] + 13;
										draw_segmented_gauge(VGAScreenSeg, 284, y, 112, 2, 2, MAX(1, ammo_max / 10), this_player->sidekick[i].ammo);
									}

									if (button[1 + i] && this_player->sidekick[i].ammo > 0)
									{
										b = player_shot_create(this_option->wport, shot_i, this_player->sidekick[i].x, this_player->sidekick[i].y, *mouseX_, *mouseY_, this_option->wpnum + this_player->sidekick[i].charge, playerNum_);

										--this_player->sidekick[i].ammo;
										if (this_player->sidekick[i].charge > 0)
										{
											shotMultiPos[shot_i] = 0;
											this_player->sidekick[i].charge = 0;
										}
										this_player->sidekick[i].charge_ticks = 20;
										this_player->sidekick[i].animation_enabled = true;

										// draw sidekick discharge ammo gauge
										const int y = hud_sidekick_y[twoPlayerMode ? 1 : 0][i] + 13;
										fill_rectangle_xy(VGAScreenSeg, 284, y, 312, y + 2, 0);
										draw_segmented_gauge(VGAScreenSeg, 284, y, 112, 2, 2, MAX(1, ammo_max / 10), this_player->sidekick[i].ammo);
									}
								}
								else  // has infinite ammo
								{
									// Tyrian 2000: weapons with charge stages do not auto-fire
									const bool can_autofire = (!tyrian2000detected || !this_option->pwr);
									if ((button[0] && can_autofire) || button[1 + i])
									{
										b = player_shot_create(this_option->wport, shot_i, this_player->sidekick[i].x, this_player->sidekick[i].y, *mouseX_, *mouseY_, this_option->wpnum + this_player->sidekick[i].charge, playerNum_);

										if (this_player->sidekick[i].charge > 0)
										{
											shotMultiPos[shot_i] = 0;
											this_player->sidekick[i].charge = 0;
										}
										this_player->sidekick[i].charge_ticks = 20;
										this_player->sidekick[i].animation_enabled = true;
									}
								}
							}
						}
					}
				}  // end of if player has sidekicks
			}  // !endLevel
		} // this_player->is_alive
	} // moveOK

	// draw sidekicks
	if ((playerNum_ == 2 || !twoPlayerMode) && !endLevel)
	{
		for (uint i = 0; i < COUNTOF(this_player->sidekick); ++i)
		{
			JE_OptionType *this_option = &options[this_player->items.sidekick[i]];

			if (this_option->option > 0)
			{
				if (this_player->sidekick[i].animation_enabled)
				{
					if (++this_player->sidekick[i].animation_frame >= this_option->ani)
					{
						this_player->sidekick[i].animation_frame = 0;
						this_player->sidekick[i].animation_enabled = (this_option->option == 1);
					}
				}

				const int x = this_player->sidekick[i].x,
				          y = this_player->sidekick[i].y;
				const uint sprite = this_option->gr[this_player->sidekick[i].animation_frame] + this_player->sidekick[i].charge;

				if (this_player->sidekick[i].style == 1 || this_player->sidekick[i].style == 2)
					blit_sprite2x2(VGAScreen, x - 6, y, spriteSheet10, sprite);
				else
					blit_sprite2(VGAScreen, x, y, spriteSheet9, sprite);
			}

			if (--this_player->sidekick[i].charge_ticks == 0)
			{
				if (this_player->sidekick[i].charge < this_option->pwr)
					++this_player->sidekick[i].charge;
				this_player->sidekick[i].charge_ticks = 20;
			}
		}
	}
}

void JE_mainGamePlayerFunctions(void)
{
	/*PLAYER MOVEMENT/MOUSE ROUTINES*/

	if (endLevel && levelEnd > 0)
	{
		levelEnd--;
		levelEndWarp++;
	}

	/*Reset Street-Fighter commands*/
	memset(SFExecuted, 0, sizeof(SFExecuted));

	portConfigChange = false;

	if (twoPlayerMode)
	{
		JE_playerMovement(&player[0],
		                  !galagaMode ? inputDevice[0] : 0, 1, shipGr, shipGrPtr,
		                  &mouseX, &mouseY);
		JE_playerMovement(&player[1],
		                  !galagaMode ? inputDevice[1] : 0, 2, shipGr2, shipGr2ptr,
		                  &mouseXB, &mouseYB);
	}
	else
	{
		JE_playerMovement(&player[0],
		                  0, 1, shipGr, shipGrPtr,
		                  &mouseX, &mouseY);
	}

	/* == Parallax Map Scrolling == */
	JE_word tempX;
	if (twoPlayerMode)
		tempX = (player[0].x + player[1].x) / 2;
	else
		tempX = player[0].x;

	tempW = floorf((float)(260 - (tempX - 36)) / (260 - 36) * (24 * 3) - 1);
	mapX3Ofs   = tempW;
	mapX3Pos   = mapX3Ofs % 24;
	mapX3bpPos = 1 - (mapX3Ofs / 24);

	mapX2Ofs   = (tempW * 2) / 3;
	mapX2Pos   = mapX2Ofs % 24;
	mapX2bpPos = 1 - (mapX2Ofs / 24);

	oldMapXOfs = mapXOfs;
	mapXOfs    = mapX2Ofs / 2;
	mapXPos    = mapXOfs % 24;
	mapXbpPos  = 1 - (mapXOfs / 24);

	if (background3x1)
	{
		mapX3Ofs = mapXOfs;
		mapX3Pos = mapXPos;
		mapX3bpPos = mapXbpPos - 1;
	}
}

const char *JE_getName(JE_byte pnum)
{
	if (pnum == thisPlayerNum && network_player_name[0] != '\0')
		return network_player_name;
	else if (network_opponent_name[0] != '\0')
		return network_opponent_name;

	return miscText[47 + pnum];
}

void JE_playerCollide(Player *this_player, JE_byte playerNum_)
{
	char tempStr[256];

	for (int z = 0; z < 100; z++)
	{
		if (enemyAvail[z] != 1)
		{
			int enemy_screen_x = enemy[z].ex + enemy[z].mapoffset;

			if (abs(this_player->x - enemy_screen_x) < 12 && abs(this_player->y - enemy[z].ey) < 14)
			{   /*Collide*/
				int evalue = enemy[z].evalue;
				if (evalue > 29999)
				{
					if (evalue == 30000)  // spawn dragonwing in galaga mode, otherwise just a purple ball
					{
						this_player->cash += 100;

						if (!galagaMode)
						{
							handle_got_purple_ball(this_player);
						}
						else
						{
							// spawn the dragonwing?
							if (twoPlayerMode)
								this_player->cash += 2400;
							twoPlayerMode = true;
							twoPlayerLinked = true;
							player[1].items.weapon[REAR_WEAPON].power = 1;
							player[1].armor = 10;
							player[1].is_alive = true;
						}
						enemyAvail[z] = 1;
						soundQueue[7] = S_POWERUP;
					}
					else if (superArcadeMode != SA_NONE && evalue > 30000)
					{
						shotMultiPos[SHOT_FRONT] = 0;
						shotRepeat[SHOT_FRONT] = 10;

						tempW = SAWeapon[superArcadeMode-1][evalue - 30000-1];

						// if picked up already-owned weapon, power weapon up
						if (tempW == player[0].items.weapon[FRONT_WEAPON].id)
						{
							this_player->cash += 1000;
							power_up_weapon(this_player, FRONT_WEAPON);
						}
						// else weapon also gives purple ball
						else
						{
							handle_got_purple_ball(this_player);
						}

						player[0].items.weapon[FRONT_WEAPON].id = tempW;
						this_player->cash += 200;
						soundQueue[7] = S_POWERUP;
						enemyAvail[z] = 1;
					}
					else if (evalue > 32100)
					{
						if (playerNum_ == 1)
						{
							this_player->cash += 250;
							player[0].items.special = evalue - 32100;
							shotMultiPos[SHOT_SPECIAL] = 0;
							shotRepeat[SHOT_SPECIAL] = 10;
							shotMultiPos[SHOT_SPECIAL2] = 0;
							shotRepeat[SHOT_SPECIAL2] = 0;

							if (isNetworkGame)
								snprintf(tempStr, sizeof(tempStr)-1, "%s %s %s", JE_getName(1), miscTextB[4-1], special[evalue - 32100].name);
							else if (twoPlayerMode)
								snprintf(tempStr, sizeof(tempStr)-1, "%s %s", miscText[43-1], special[evalue - 32100].name);
							else
								snprintf(tempStr, sizeof(tempStr)-1, "%s %s", miscText[64-1], special[evalue - 32100].name);
							apmsg_drawInGameText(tempStr);
							soundQueue[7] = S_POWERUP;
							enemyAvail[z] = 1;
						}
					}
					else if (evalue > 32000)
					{
						if (playerNum_ == 2)
						{
							enemyAvail[z] = 1;
							if (isNetworkGame)
								snprintf(tempStr, sizeof(tempStr)-1, "%s %s %s", JE_getName(2), miscTextB[4-1], options[evalue - 32000].name);
							else
								snprintf(tempStr, sizeof(tempStr)-1, "%s %s", miscText[44-1], options[evalue - 32000].name);
							apmsg_drawInGameText(tempStr);

							// if picked up a different sidekick than player already has, then reset sidekicks to least powerful, else power them up
							if (evalue - 32000u != player[1].items.sidekick_series)
							{
								player[1].items.sidekick_series = evalue - 32000;
								player[1].items.sidekick_level = 101;
							}
							else if (player[1].items.sidekick_level < 103)
							{
								++player[1].items.sidekick_level;
							}

							uint temp = player[1].items.sidekick_level - 100 - 1;
							for (uint i = 0; i < COUNTOF(player[1].items.sidekick); ++i)
								player[1].items.sidekick[i] = optionSelect[player[1].items.sidekick_series][temp][i];

							shotMultiPos[SHOT_LEFT_SIDEKICK] = 0;
							shotMultiPos[SHOT_RIGHT_SIDEKICK] = 0;
							JE_drawOptions();
							soundQueue[7] = S_POWERUP;
						}
						else if (onePlayerAction)
						{
							enemyAvail[z] = 1;
							snprintf(tempStr, sizeof(tempStr)-1, "%s %s", miscText[64-1], options[evalue - 32000].name);
							apmsg_drawInGameText(tempStr);

							for (uint i = 0; i < COUNTOF(player[0].items.sidekick); ++i)
								player[0].items.sidekick[i] = evalue - 32000;
							shotMultiPos[SHOT_LEFT_SIDEKICK] = 0;
							shotMultiPos[SHOT_RIGHT_SIDEKICK] = 0;

							JE_drawOptions();
							soundQueue[7] = S_POWERUP;
						}
						if (enemyAvail[z] == 1)
							this_player->cash += 250;
					}
					else if (evalue > 31000)
					{
						this_player->cash += 250;
						if (playerNum_ == 2)
						{
							if (isNetworkGame)
								snprintf(tempStr, sizeof(tempStr)-1, "%s %s %s", JE_getName(2), miscTextB[4-1], weaponPort[evalue - 31000].name);
							else
								snprintf(tempStr, sizeof(tempStr)-1, "%s %s", miscText[44-1], weaponPort[evalue - 31000].name);
							apmsg_drawInGameText(tempStr);
							player[1].items.weapon[REAR_WEAPON].id = evalue - 31000;
							shotMultiPos[SHOT_REAR] = 0;
							enemyAvail[z] = 1;
							soundQueue[7] = S_POWERUP;
						}
						else if (onePlayerAction)
						{
							snprintf(tempStr, sizeof(tempStr)-1, "%s %s", miscText[64-1], weaponPort[evalue - 31000].name);
							apmsg_drawInGameText(tempStr);
							player[0].items.weapon[REAR_WEAPON].id = evalue - 31000;
							shotMultiPos[SHOT_REAR] = 0;
							enemyAvail[z] = 1;
							soundQueue[7] = S_POWERUP;

							if (player[0].items.weapon[REAR_WEAPON].power == 0)  // does this ever happen?
								player[0].items.weapon[REAR_WEAPON].power = 1;
						}
					}
					else if (evalue > 30000)
					{
						if (playerNum_ == 1 && twoPlayerMode)
						{
							if (isNetworkGame)
								snprintf(tempStr, sizeof(tempStr)-1, "%s %s %s", JE_getName(1), miscTextB[4-1], weaponPort[evalue - 30000].name);
							else
								snprintf(tempStr, sizeof(tempStr)-1, "%s %s", miscText[43-1], weaponPort[evalue - 30000].name);
							apmsg_drawInGameText(tempStr);
							player[0].items.weapon[FRONT_WEAPON].id = evalue - 30000;
							shotMultiPos[SHOT_FRONT] = 0;
							enemyAvail[z] = 1;
							soundQueue[7] = S_POWERUP;
						}
						else if (onePlayerAction)
						{
							snprintf(tempStr, sizeof(tempStr)-1, "%s %s", miscText[64-1], weaponPort[evalue - 30000].name);
							apmsg_drawInGameText(tempStr);
							player[0].items.weapon[FRONT_WEAPON].id = evalue - 30000;
							shotMultiPos[SHOT_FRONT] = 0;
							enemyAvail[z] = 1;
							soundQueue[7] = S_POWERUP;
						}

						if (enemyAvail[z] == 1)
						{
							player[0].items.special = specialArcadeWeapon[evalue - 30000-1];
							if (player[0].items.special > 0)
							{
								shotMultiPos[SHOT_SPECIAL] = 0;
								shotRepeat[SHOT_SPECIAL] = 0;
								shotMultiPos[SHOT_SPECIAL2] = 0;
								shotRepeat[SHOT_SPECIAL2] = 0;
							}
							this_player->cash += 250;
						}

					}
				}
				else if (evalue >= 28000) // AP
				{
					enemyAvail[z] = 1;
					nortsong_playVoice(V_DATA_CUBE);
					Archipelago_SendCheck(evalue - 28000);
					JE_setupExplosion(enemy_screen_x, enemy[z].ey, 0, 54, true, false);

#ifdef DEBUG_OPTIONS
					if (debugGameInit)
					{
						snprintf(tempStr, sizeof(tempStr)-1, "Location %d checked", evalue - 28000);
						apmsg_drawInGameText(tempStr);
					}
#endif
				}
				else if (evalue > 20000)
				{
					player_boostArmor(this_player, evalue - 20000);
					enemyAvail[z] = 1;
					soundQueue[7] = S_POWERUP;
				}
				else if (evalue > 10000 && enemyAvail[z] == 2)
				{
					if (!bonusLevel)
					{
						play_song(30);  /*Zanac*/
						bonusLevel = true;
						nextLevel = evalue - 10000;
						enemyAvail[z] = 1;
						displayTime = 150;
					}
				}
				else if (enemy[z].scoreitem)
				{
					enemyAvail[z] = 1;
					soundQueue[7] = S_ITEM;
					if (evalue == 1)
					{
						cubeMax++;
						nortsong_playVoice(V_DATA_CUBE);
					}
					else if (evalue == -1)  // got front weapon powerup
					{
						if (isNetworkGame)
							snprintf(tempStr, sizeof(tempStr)-1, "%s %s %s", JE_getName(1), miscTextB[4-1], miscText[45-1]);
						else if (twoPlayerMode)
							snprintf(tempStr, sizeof(tempStr)-1, "%s %s", miscText[43-1], miscText[45-1]);
						else
							strcpy(tempStr, miscText[45-1]);
						apmsg_drawInGameText(tempStr);

						power_up_weapon(&player[0], FRONT_WEAPON);
						soundQueue[7] = S_POWERUP;
					}
					else if (evalue == -2)  // got rear weapon powerup
					{
						if (isNetworkGame)
							snprintf(tempStr, sizeof(tempStr)-1, "%s %s %s", JE_getName(2), miscTextB[4-1], miscText[46-1]);
						else if (twoPlayerMode)
							snprintf(tempStr, sizeof(tempStr)-1, "%s %s", miscText[44-1], miscText[46-1]);
						else
							strcpy(tempStr, miscText[46-1]);
						apmsg_drawInGameText(tempStr);

						power_up_weapon(twoPlayerMode ? &player[1] : &player[0], REAR_WEAPON);
						soundQueue[7] = S_POWERUP;
					}
					else if (evalue == -3)
					{
						// picked up orbiting asteroid killer
						shotMultiPos[SHOT_MISC] = 0;
						b = player_shot_create(0, SHOT_MISC, this_player->x, this_player->y, mouseX, mouseY, 104, playerNum_);
						shotAvail[z] = 0;
					}
					else if (evalue == -4)
					{
						if (player[playerNum_-1].superbombs < 10)
							++player[playerNum_-1].superbombs;
					}
					else if (evalue == -5)
					{
						player[0].items.weapon[FRONT_WEAPON].id = 25;  // HOT DOG!
						player[0].items.weapon[REAR_WEAPON].id = 26;
						player[1].items.weapon[REAR_WEAPON].id = 26;

						for (uint i = 0; i < COUNTOF(player); ++i)
							player[i].weapon_mode = 1;

						memset(shotMultiPos, 0, sizeof(shotMultiPos));
					}
					else if (twoPlayerLinked)
					{
						// players get equal share of pick-up cash when linked
						for (uint i = 0; i < COUNTOF(player); ++i)
							player[i].cash += evalue / COUNTOF(player);
					}
					else
					{
						this_player->cash += evalue;
					}
					JE_setupExplosion(enemy_screen_x, enemy[z].ey, 0, enemyDat[enemy[z].enemytype].explosiontype, true, false);

					// allow special (flag-setting) scoreitems
					if (enemy[z].special)
						mainint_handleEnemyFlag(&enemy[z]);
				}
				else if (this_player->invulnerable_ticks == 0 && enemyAvail[z] == 0 &&
				         (enemyDat[enemy[z].enemytype].explosiontype & 1) == 0) // explosiontype & 1 == 0: not ground enemy
				{
					int armorleft = enemy[z].armorleft;
					if (armorleft > damageRate)
						armorleft = damageRate;

					player_takeDamage(this_player, armorleft, DAMAGE_CONTACT);

					// player ship gets push-back from collision
					if (enemy[z].armorleft > 0)
					{
						this_player->x_velocity += (enemy[z].exc * enemy[z].armorleft) / 2;
						this_player->y_velocity += (enemy[z].eyc * enemy[z].armorleft) / 2;
					}

					int armorleft2 = enemy[z].armorleft;
					if (armorleft2 == 255)
						armorleft2 = 30000;

					temp = enemy[z].linknum;
					if (temp == 0)
						temp = 255;

					b = z;

					if (armorleft2 > armorleft)
					{
						// damage enemy
						if (enemy[z].armorleft != 255)
							enemy[z].armorleft -= armorleft;
						soundQueue[5] = S_ENEMY_HIT;
					}
					else
					{
						// kill enemy
						for (temp2 = 0; temp2 < 100; temp2++)
						{
							if (enemyAvail[temp2] != 1)
							{
								temp3 = enemy[temp2].linknum;
								if (temp2 == b ||
									(temp != 255 &&
									 (temp == temp3 || temp - 100 == temp3 ||
									  (temp3 > 40 && temp3 / 20 == temp / 20 && temp3 <= temp))))
								{
									int enemy_screen_x = enemy[temp2].ex + enemy[temp2].mapoffset;

									enemy[temp2].linknum = 0;

									enemyAvail[temp2] = 1;

									if (enemyDat[enemy[temp2].enemytype].esize == 1)
									{
										JE_setupExplosionLarge(enemy[temp2].enemyground, enemy[temp2].explonum, enemy_screen_x, enemy[temp2].ey);
										soundQueue[6] = S_EXPLOSION_9;
									}
									else
									{
										JE_setupExplosion(enemy_screen_x, enemy[temp2].ey, 0, 1, false, false);
										soundQueue[5] = S_EXPLOSION_4;
									}
								}
							}
						}
						enemyAvail[z] = 1;
					}
				}
			}

		}
	}
}

void mainint_handleEnemyFlag(struct JE_SingleEnemyType *e)
{
	if (!e->special && !e->flagnum)
		return;

	assert((unsigned int)e->flagnum - 1 < COUNTOF(globalFlags));
	switch (e->special)
	{
		case ENEMYFLAG_SET:       globalFlags[e->flagnum - 1] = e->setto;  break;
		case ENEMYFLAG_INCREMENT: globalFlags[e->flagnum - 1] += e->setto; break;
		default: /* Unknown ENEMYFLAG type */ break;
	}
}
