
#include "font.h"
#include "fonthand.h"
#include "joystick.h"
#include "jukebox.h"
#include "keyboard.h"
#include "loudness.h"
#include "mouse.h"
#include "nortsong.h"
#include "palette.h"
#include "picload.h"
#include "sprite.h"
#include "tyrian2.h"
#include "varz.h"
#include "video.h"

#include "SDL_clipboard.h"

#include "archipelago/apconnect.h"

typedef struct { 
	int count;
	struct { Sint16 x, y, w, h; } items[32];
} mousetargets_t;

static int apmenu_mouseInTarget(const mousetargets_t *targets, int x, int y)
{
	if (!has_mouse || mouseInactive)
		return -1;

	for (int i = 0; i < targets->count; ++i)
	{
		const Sint16 xmin = targets->items[i].x;
		const Sint16 xmax = xmin + targets->items[i].w;
		const Sint16 ymin = targets->items[i].y;
		const Sint16 ymax = ymin + targets->items[i].h;

		if (x >= xmin && x <= xmax && y >= ymin && y <= ymax)
			return i;
	}
	return -1;
}

bool apmenu_quitRequest(void)
{
	bool quit_selected = true;
	bool done = false;

	int mouseTarget = -1, lastMouseTarget = -1;
	const mousetargets_t popupTargets = {2, {
		{ 57, 124, 84, 24}, // OK
		{152, 124, 84, 24} // CANCEL
	}};

	Uint8 col = 8;
	int colC = 1;
	int updateColTime = 3;

	JE_clearKeyboard();

	JE_barShade(VGAScreen, 65, 55, 255, 155);
	blit_sprite(VGAScreen, 50, 50, OPTION_SHAPES, 35);  // message box
	JE_textShade(VGAScreen, 70, 60, "Are you sure you want to quit?", 0, 5, FULL_SHADE);
	JE_helpBox(VGAScreen, 70, 90, "You will be disconnected from the Archipelago server.", 30);
	draw_font_hv_shadow(VGAScreen,  54+45, 128, "OK",     FONT_SHAPES, centered, 15, -5, false, 2);
	draw_font_hv_shadow(VGAScreen, 149+45, 128, "CANCEL", FONT_SHAPES, centered, 15, -5, false, 2);

	JE_mouseStart();
	JE_showVGA();
	JE_mouseReplace();
	wait_noinput(true, true, true);

	while (true)
	{
		push_joysticks_as_keyboard();
		service_SDL_events(true);
		lastMouseTarget = mouseTarget;
		mouseTarget = apmenu_mouseInTarget(&popupTargets, mouse_x, mouse_y);

		if (newmouse && lastmouse_but == SDL_BUTTON_LEFT)
		{
			mouseTarget = apmenu_mouseInTarget(&popupTargets, lastmouse_x, lastmouse_y);
			if (mouseTarget >= 0)
			{
				quit_selected = (mouseTarget == 0);
				done = true;
			}
		}
		else if (mouseTarget != lastMouseTarget && mouseTarget >= 0)
		{
			quit_selected = (mouseTarget == 0);
			JE_playSampleNum(S_CURSOR);

			// Reset animation
			col = 8;
			colC = 1;
			updateColTime = 3;
		}

		if (newkey)
		{
			switch (lastkey_scan)
			{
				case SDL_SCANCODE_ESCAPE:
					JE_playSampleNum(S_SPRING);
					return false;

				case SDL_SCANCODE_LEFT:
				case SDL_SCANCODE_RIGHT:
				case SDL_SCANCODE_TAB:
					quit_selected = !quit_selected;
					JE_playSampleNum(S_CURSOR);

					// Reset animation
					col = 8;
					colC = 1;
					updateColTime = 3;
					break;

				case SDL_SCANCODE_RETURN:
				case SDL_SCANCODE_SPACE:
					done = true;
					break;

				default:
					break;
			}
		}

		if (done)
			break;

		// We run at full speed now, but this pulsating effect doesn't; only run it every third frame
		if (!(--updateColTime))
		{
			col += colC;
			if (col > 8 || col < 2)
				colC = -colC;
			updateColTime = 3;
		}

		draw_font_hv(VGAScreen,  54+45, 128, "OK",     FONT_SHAPES, centered, 15,  quit_selected ? col - 12 : -5);
		draw_font_hv(VGAScreen, 149+45, 128, "CANCEL", FONT_SHAPES, centered, 15, !quit_selected ? col - 12 : -5);

		JE_mouseStart();
		JE_showVGA();
		JE_mouseReplace();

		SDL_Delay(16);
	}

	if (quit_selected)
	{
		JE_playSampleNum(S_SELECT);
		return true;
	}

	JE_playSampleNum(S_CLICK);
	return false;
}


// ----------------------------------------------------------------------------

bool ap_connectScreen(void)
{
	JE_loadPic(VGAScreen2, 2, false);
	draw_font_hv_shadow(VGAScreen2, 10,  10, "01", small_font, centered,  1, 6, false, 1);
	draw_font_hv_shadow(VGAScreen2, 10,  20, "02", small_font, centered,  2, 6, false, 1);
	draw_font_hv_shadow(VGAScreen2, 10,  30, "03", small_font, centered,  3, 6, false, 1);
	draw_font_hv_shadow(VGAScreen2, 10,  40, "04", small_font, centered,  4, 6, false, 1);
	draw_font_hv_shadow(VGAScreen2, 10,  50, "05", small_font, centered,  5, 6, false, 1);
	draw_font_hv_shadow(VGAScreen2, 10,  60, "06", small_font, centered,  6, 6, false, 1);
	draw_font_hv_shadow(VGAScreen2, 10,  70, "07", small_font, centered,  7, 6, false, 1);
	draw_font_hv_shadow(VGAScreen2, 10,  80, "08", small_font, centered,  8, 6, false, 1);
	draw_font_hv_shadow(VGAScreen2, 10,  90, "09", small_font, centered,  9, 6, false, 1);
	draw_font_hv_shadow(VGAScreen2, 10, 100, "10", small_font, centered, 10, 6, false, 1);
	draw_font_hv_shadow(VGAScreen2, 10, 110, "11", small_font, centered, 11, 6, false, 1);
	draw_font_hv_shadow(VGAScreen2, 10, 120, "12", small_font, centered, 12, 6, false, 1);
	draw_font_hv_shadow(VGAScreen2, 10, 130, "13", small_font, centered, 13, 6, false, 1);
	draw_font_hv_shadow(VGAScreen2, 10, 140, "14", small_font, centered, 14, 6, false, 1);
	draw_font_hv_shadow(VGAScreen2, 10, 150, "15", small_font, centered, 15, 6, false, 1);
	memcpy(VGAScreen->pixels, VGAScreen2->pixels, VGAScreen->pitch * VGAScreen->h);
	fade_palette(colors, 10, 0, 255);

	int error_time = 0;
	int visual_time = 0;
	int visual_pulse = 0;

// void draw_font_hv_shadow(SDL_Surface *surface, int x, int y, const char *text, Font font, FontAlignment alignment, Uint8 hue, Sint8 value, bool black, int shadow_dist)

	while (true)
	{
		service_SDL_events(true);
		push_joysticks_as_keyboard();

		++visual_time;
		visual_time &= 0x1F;

		if (visual_time & 0x10)
			visual_pulse = -2 + ((visual_time ^ 0x1F) >> 2);
		else
			visual_pulse = -2 + ( visual_time         >> 2);

		switch(Archipelago_ConnectionStatus())
		{
			case APCONN_NOT_CONNECTED:
				if (error_time)
				{
					--error_time;
					visual_pulse = 6;
					if (--error_time < 60)
						visual_pulse = -8 + (error_time >> 2);

					draw_font_hv_shadow(VGAScreen, 160, 100, "Failed to connect to the Archipelago server:", small_font, centered, 4, visual_pulse, false, 1);
					draw_font_hv_shadow(VGAScreen, 160, 110, Archipelago_GetConnectionError(), small_font, centered, 4, visual_pulse, false, 1);
				}

				if (newkey && lastkey_scan == SDL_SCANCODE_RETURN)
				{
					error_time = 300;
					visual_time = 0;
					Archipelago_Connect();
				}
				else if (newkey && lastkey_scan == SDL_SCANCODE_ESCAPE)
				{
					JE_playSampleNum(S_SPRING);
					fade_black(15);
					return false;
				}
				else if (newkey && lastkey_scan == SDL_SCANCODE_F3)
				{
					FILE *f = fopen("local.aptyrian", "r");
					if (!f)
						break;
					bool result = Archipelago_StartLocalGame(f);
					fclose(f);
					if (!result)
						break;

					// Force
					JE_playSampleNum(S_SELECT);
					fade_black(15);
					onePlayerAction = false;
					twoPlayerMode = false;
					difficultyLevel = initialDifficulty = ArchipelagoOpts.game_difficulty;
					player[0].cash = 0;
					sprites_loadMainShapeTables(ArchipelagoOpts.christmas);
					JE_initEpisode(1); // Temporary: We need to init items before first menu

					return true;
				}
				break;

			case APCONN_CONNECTING:
				draw_font_hv_shadow(VGAScreen, 160, 100, "Connecting...", small_font, centered, 12, visual_pulse, false, 1);
				break;

			case APCONN_TENTATIVE:
				draw_font_hv_shadow(VGAScreen, 160, 100, "Authenticating...", small_font, centered, 12, visual_pulse, false, 1);
				break;

			case APCONN_READY:
				// Ready to start the game
				JE_playSampleNum(S_SELECT);
				fade_black(15);
				onePlayerAction = false;
				twoPlayerMode = false;
				difficultyLevel = initialDifficulty = ArchipelagoOpts.game_difficulty;
				player[0].cash = 0;
				sprites_loadMainShapeTables(ArchipelagoOpts.christmas);
				JE_initEpisode(1); // Temporary: We need to init items before first menu

				return true;
		}

		JE_showVGA();
		SDL_Delay(16);
		memcpy(VGAScreen->pixels, VGAScreen2->pixels, VGAScreen->pitch * VGAScreen->h);
	}
}

// ----------------------------------------------------------------------------

const char *APMenuTexts[] =
{
	"Start Game",
	"Options",
	"Jukebox",
	"Quit"
};
enum MenuItemIndex
{
	MENU_ITEM_START_GAME = 0,
	MENU_ITEM_OPTIONS,
	MENU_ITEM_JUKEBOX,
	MENU_ITEM_QUIT,
	COUNT_MENU_ITEMS
};

static void titleScreenBackground(bool moveTyrianLogoUp)
{
	const int xCenter = VGAScreen->w / 2;
	const int yMenuItems = 140;
	const int hMenuItem = 13;

	play_song(SONG_TITLE);

	JE_loadPic(VGAScreen, 4, false);
	draw_font_hv_shadow(VGAScreen, 2, 192, opentyrian_version, small_font, left_aligned, 15, 0, false, 1);

	if (moveTyrianLogoUp)
	{
		memcpy(VGAScreen2->pixels, VGAScreen->pixels, VGAScreen2->pitch * VGAScreen2->h);

		blit_sprite(VGAScreenSeg, 11, 62, PLANET_SHAPES, 146); // tyrian logo
		fade_palette(colors, 10, 0, 255 - 16);

		for (int y = 60; y >= 4; y -= 2)
		{
			setDelay(2);

			memcpy(VGAScreen->pixels, VGAScreen2->pixels, VGAScreen->pitch * VGAScreen->h);
			blit_sprite(VGAScreenSeg, 11, y, PLANET_SHAPES, 146); // tyrian logo
			JE_showVGA();

			service_wait_delay();
		}
	}
	else
	{
		blit_sprite(VGAScreenSeg, 11, 4, PLANET_SHAPES, 146); // tyrian logo
		fade_palette(colors, 10, 0, 255 - 16);
	}

	// Draw menu items.
	for (size_t i = 0; i < COUNT_MENU_ITEMS; ++i)
	{
		const int y = yMenuItems + hMenuItem * i;

		draw_font_hv(VGAScreen, xCenter - 1, y - 1, APMenuTexts[i], normal_font, centered, 15, -10);
		draw_font_hv(VGAScreen, xCenter + 1, y + 1, APMenuTexts[i], normal_font, centered, 15, -10);
		draw_font_hv(VGAScreen, xCenter + 1, y - 1, APMenuTexts[i], normal_font, centered, 15, -10);
		draw_font_hv(VGAScreen, xCenter - 1, y + 1, APMenuTexts[i], normal_font, centered, 15, -10);
		draw_font_hv(VGAScreen, xCenter,     y,     APMenuTexts[i], normal_font, centered, 15, -3);
	}

	memcpy(VGAScreen2->pixels, VGAScreen->pixels, VGAScreen2->pitch * VGAScreen2->h);

	mouseCursor = MOUSE_POINTER_NORMAL;

	// Fade in menu items.
	fade_palette(colors, 20, 255 - 16 + 1, 255);
}

bool ap_titleScreen(void)
{
	if (shopSpriteSheet.data == NULL)
		JE_loadCompShapes(&shopSpriteSheet, '1');  // need mouse pointer sprites

	bool first_load = true; // On startup or after quit game
	bool reload = true;

	size_t selectedIndex = MENU_ITEM_START_GAME;

	const int xCenter = VGAScreen->w / 2;
	const int yMenuItems = 140;
	const int hMenuItem = 13;

	mousetargets_t menuTargets = {COUNT_MENU_ITEMS};

	for (int i = 0; i < COUNT_MENU_ITEMS; ++i)
	{
		int wMenuItem = JE_textWidth(APMenuTexts[i], normal_font);

		menuTargets.items[i].x = xCenter - wMenuItem / 2;
		menuTargets.items[i].y = yMenuItems + hMenuItem * i;
		menuTargets.items[i].w = wMenuItem;
		menuTargets.items[i].h = hMenuItem;
	}

	while (true)
	{
		if (reload)
		{
			titleScreenBackground(first_load);
			reload = first_load = false;
		}

		memcpy(VGAScreen->pixels, VGAScreen2->pixels, VGAScreen->pitch * VGAScreen->h);

		// Highlight selected menu item.
		draw_font_hv(VGAScreen, xCenter, yMenuItems + hMenuItem * selectedIndex, APMenuTexts[selectedIndex],
			normal_font, centered, 15, -1);

		service_SDL_events(true);

		JE_mouseStartFilter(0xF0);
		JE_showVGA();
		JE_mouseReplace();

		bool mouseMoved = false;
		do
		{
			SDL_Delay(16);

			Uint16 oldMouseX = mouse_x;
			Uint16 oldMouseY = mouse_y;

			push_joysticks_as_keyboard();
			service_SDL_events(false);

			mouseMoved = mouse_x != oldMouseX || mouse_y != oldMouseY;
		} while (!(newkey || new_text || newmouse || mouseMoved));

		// Handle interaction.

		bool action = false;

		if (mouseMoved || newmouse)
		{
			int target = apmenu_mouseInTarget(&menuTargets, mouse_x, mouse_y);
			if (target != -1 && selectedIndex != (unsigned)target)
			{
				JE_playSampleNum(S_CURSOR);
				selectedIndex = (unsigned)target;
			}

			if (newmouse && lastmouse_but == SDL_BUTTON_LEFT
				&& target == apmenu_mouseInTarget(&menuTargets, lastmouse_x, lastmouse_y))
			{
				action = true;
			}
		}

		if (newkey)
		{
			switch (lastkey_scan)
			{
			case SDL_SCANCODE_UP:
				JE_playSampleNum(S_CURSOR);
				selectedIndex = selectedIndex == 0 ? COUNT_MENU_ITEMS - 1 : selectedIndex - 1;
				break;

			case SDL_SCANCODE_DOWN:
				JE_playSampleNum(S_CURSOR);
				selectedIndex = selectedIndex == COUNT_MENU_ITEMS - 1 ? 0 : selectedIndex + 1;
				break;

			case SDL_SCANCODE_SPACE:
			case SDL_SCANCODE_RETURN:
				action = true;
				break;

			case SDL_SCANCODE_ESCAPE:
				JE_playSampleNum(S_SPRING);
				fade_black(15);
				return false;

			default:
				break;
			}
		}

		if (action && selectedIndex < COUNT_MENU_ITEMS)
		{
			JE_playSampleNum(S_SELECT);
			fade_black(15);

			switch (selectedIndex)
			{
				case MENU_ITEM_START_GAME:
					if (ap_connectScreen())
						return true;
					reload = true;
					break;
				case MENU_ITEM_OPTIONS:
					setupMenu();
					reload = true;
					break;
				case MENU_ITEM_JUKEBOX:
					jukebox();
					reload = true;
					break;
				case MENU_ITEM_QUIT:
					return false;
			}
		}
	}
}

// ----------------------------------------------------------------------------

static void ap_drawItem(Sint16 graphic, Sint16 x, Sint16 y)
{
	blit_sprite2x2(VGAScreen, x, y, shopSpriteSheet, graphic);
}

Uint16 ap_itemScreen(void)
{
	char buf[256];

	if (shopSpriteSheet.data == NULL)
		JE_loadCompShapes(&shopSpriteSheet, '1');

	JE_loadPic(VGAScreen2, 1, false);
	play_song(DEFAULT_SONG_BUY);

	VGAScreen = VGAScreenSeg;
//void JE_barDrawShadow(SDL_Surface *surface, JE_word x, JE_word y, JE_word res, JE_word col, JE_word amt, JE_word xsize, JE_word ysize)

	memcpy(VGAScreen->pixels, VGAScreen2->pixels, VGAScreen->pitch * VGAScreen->h);
	set_palette(colors, 0, 255);
	JE_showVGA();

	Uint8 levelID = 1;

	if (shopSpriteSheet.data == NULL)
		JE_loadCompShapes(&shopSpriteSheet, '1');

	while (true)
	{
		draw_font_hv_shadow(VGAScreen, 234, 10, "Archipelago", FONT_SHAPES, centered, 15, -3, false, 2);

		// We must actually redraw these every frame, because an AP Item can arrive that upgrades them.
		JE_barDrawShadow(VGAScreen,  42, 152, 1, 14, 1 + 0, 2, 13); // Armor
		JE_barDrawShadow(VGAScreen, 104, 152, 1, 14, 1 + 0, 2, 13); // Shield

		snprintf(buf, sizeof buf, "%lu", player[0].cash);
		JE_textShade(VGAScreen, 65, 173, buf, 1, 6, DARKEN);

		// Draw player ship
		blit_sprite2x2(VGAScreen, 66, 84, spriteSheet9, ships[1].shipgraphic);

		blit_sprite2x2(VGAScreen,  28,  26, shopSpriteSheet, weaponPort[5].itemgraphic);
		blit_sprite2x2(VGAScreen,  28,  54, shopSpriteSheet, weaponPort[1].itemgraphic);
		blit_sprite2x2(VGAScreen,  28, 120, shopSpriteSheet, powerSys[6].itemgraphic);
		blit_sprite2x2(VGAScreen,   3,  84, shopSpriteSheet, options[1].itemgraphic);
		blit_sprite2x2(VGAScreen, 129,  84, shopSpriteSheet, options[1].itemgraphic);

		JE_textShade(VGAScreen, 28, 26, "Zica Laser", 1, 6, DARKEN);
		JE_textShade(VGAScreen, 28, 54, "Rear Heavy Missile Launcher", 1, 6, DARKEN);
		JE_textShade(VGAScreen, 52, 120, "Gravitron Pulse-Wave", 1, 6, DARKEN);

		service_SDL_events(true);
		push_joysticks_as_keyboard();

		if (newkey && lastkey_scan == SDL_SCANCODE_UP)
		{
			JE_playSampleNum(S_CURSOR);
			if (player[0].items.weapon[0].power < 11)
				printf("power: %d\n", ++player[0].items.weapon[0].power);
		}
		if (newkey && lastkey_scan == SDL_SCANCODE_DOWN)
		{
			JE_playSampleNum(S_CURSOR);
			if (player[0].items.weapon[0].power > 1)
				printf("power: %d\n", --player[0].items.weapon[0].power);
		}
		if (newkey && lastkey_scan == SDL_SCANCODE_LEFT)
		{
			JE_playSampleNum(S_CURSOR);
			if (levelID > 1)
				printf("level: %d\n", --levelID);
		}
		if (newkey && lastkey_scan == SDL_SCANCODE_RIGHT)
		{
			JE_playSampleNum(S_CURSOR);
			if (levelID < 66)
				printf("level: %d\n", ++levelID);
		}

		if (newkey && lastkey_scan == SDL_SCANCODE_RETURN)
		{
			JE_playSampleNum(S_SELECT);
			fade_black(15);

			return levelID;
		}
		if (newkey && lastkey_scan == SDL_SCANCODE_ESCAPE)
		{
			JE_playSampleNum(S_SPRING);
			if (apmenu_quitRequest())
			{
				fade_black(15);
				return 0;				
			}
		}

		JE_showVGA();
		SDL_Delay(16);
		memcpy(VGAScreen->pixels, VGAScreen2->pixels, VGAScreen->pitch * VGAScreen->h);
	}
}
