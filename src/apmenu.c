#include "opentyr.h"

#include "apmsg.h"
#include "font.h"
#include "fonthand.h"
#include "joystick.h"
#include "jukebox.h"
#include "keyboard.h"
#include "loudness.h"
#include "lvlmast.h"
#include "mouse.h"
#include "nortsong.h"
#include "nortvars.h"
#include "palette.h"
#include "picload.h"
#include "planet.h"
#include "shots.h"
#include "sprite.h"
#include "tyrian2.h"
#include "varz.h"
#include "video.h"
#include "vga256d.h"

#include "SDL_clipboard.h"

#include "itemmap.h"
#include "archipelago/apconnect.h"

typedef struct { 
	int count;
	struct {
		Sint16 x, y, w, h;
		unsigned int pointer_type;
	} items[16];
} mousetargets_t;

static int apmenu_mouseInTarget(const mousetargets_t *targets, int x, int y)
{
	if (!has_mouse || mouseInactive)
		return -1;

	mouseCursor = MOUSE_POINTER_NORMAL;
	for (int i = 0; i < targets->count; ++i)
	{
		const Sint16 xmin = targets->items[i].x;
		const Sint16 xmax = xmin + targets->items[i].w;
		const Sint16 ymin = targets->items[i].y;
		const Sint16 ymax = ymin + targets->items[i].h;

		if (x >= xmin && x <= xmax && y >= ymin && y <= ymax)
		{
			mouseCursor = targets->items[i].pointer_type;
			return i;
		}
	}
	return -1;
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
					difficultyLevel = initialDifficulty = APSeedSettings.Difficulty;
					player[0].cash = 0;
					sprites_loadMainShapeTables(APSeedSettings.Christmas);
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
				difficultyLevel = initialDifficulty = APSeedSettings.Difficulty;
				player[0].cash = 0;
				sprites_loadMainShapeTables(APSeedSettings.Christmas);
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
				&& selectedIndex == (unsigned)apmenu_mouseInTarget(&menuTargets, lastmouse_x, lastmouse_y))
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

// ============================================================================
// Item Screen / Main Archipelago Menu
// ============================================================================

static char string_buffer[32];

static void sidebarArchipelagoInfo(void);
static void sidebarPlanetNav(void);
static void sidebarSimulateShots(void);

static void nullinit(void);
static void nullexit(void);

static void submenuMain_Run(void);

static void submenuLevelSelect_Run(void);

static void submenuInShop_Init(void);
static void submenuInShop_Run(void);

static void submenuOptions_Run(void);
static void submenuOptions_Exit(void);

static void submenuUpgradeChoice_Init(void);
static void submenuUpgradeChoice_Run(void);

static void submenuUpgradeSelect_Run(void);

typedef enum {
	MENUNAV_NONE = 0,
	MENUNAV_SELECT,
	MENUNAV_LEFT,
	MENUNAV_RIGHT,
} menunavcode_t;

typedef enum
{
	SUBMENU_EXIT = -3, // Exit the game and return to title screen
	SUBMENU_LEVEL = -2, // Exit the item screen and go to level
	SUBMENU_RETURN = -1, // Return to previous submenu, as if ESC was pressed
	SUBMENU_NONE = 0, // No operation
	// ----- REAL MENUS START HERE -----
	SUBMENU_MAIN,
	SUBMENU_NEXT_LEVEL,
	SUBMENU_SELECT_SHOP,
	SUBMENU_IN_SHOP,
	SUBMENU_OPTIONS,
	SUBMENU_UPGRADE_CHOICE,
	SUBMENU_UPGRADE_SELECT,
	NUM_SUBMENU
} submenutype_t;

static const struct {
	const char *header;
	Uint8 palette;
	submenutype_t previous;
	void (*sidebarFunc)(void); // Runs every frame, before header is drawn, sets up sidebar contents
	void (*initFunc)(void); // Runs when menu is entered (not returned to), set up data for menu
	void (*exitFunc)(void); // Runs when menu is left, does other cleanup tasks
	void (*runFunc)(void); // Run every frame, handle drawing and input
} itemSubMenus[] = {
	{""}, // Dummy empty submenu for SUBMENU_NONE
	{"Archipelago", 0, SUBMENU_EXIT, sidebarArchipelagoInfo, nullinit, nullexit, submenuMain_Run},
	{"Next Level", 17, SUBMENU_MAIN, sidebarPlanetNav, nullinit, nullexit, submenuLevelSelect_Run},
	{"Visit Shop", 17, SUBMENU_MAIN, sidebarPlanetNav, nullinit, nullexit, submenuLevelSelect_Run},
	{"Shop", 0, SUBMENU_SELECT_SHOP, sidebarArchipelagoInfo, submenuInShop_Init, nullexit, submenuInShop_Run},
	{"Options", 0, SUBMENU_MAIN, sidebarArchipelagoInfo, nullinit, submenuOptions_Exit, submenuOptions_Run},
	{"Upgrade Ship", 0, SUBMENU_MAIN, sidebarSimulateShots, submenuUpgradeChoice_Init, nullexit, submenuUpgradeChoice_Run},
	{"Upgrade Ship", 0, SUBMENU_MAIN, sidebarSimulateShots, nullinit, nullexit, submenuUpgradeSelect_Run},
};

static submenutype_t currentSubMenu = SUBMENU_NONE;
static submenutype_t nextSubMenu = SUBMENU_NONE;
static int subMenuSelections[NUM_SUBMENU] = {0};

#define SELECTED(menuItem) (subMenuSelections[currentSubMenu] == menuItem)

// ------------------------------------------------------------------

// Basic init function, just sets submenu selection to topmost value.
static void nullinit(void)
{
	subMenuSelections[currentSubMenu] = 0;
}

// Basic exit function, does nothing
static void nullexit(void)
{
	// No operation
}

// Default menu option drawing.
static void defaultMenuOptionDraw(const char *msg, int y, bool disable, bool highlight)
{
	int brightness = (disable) ? -7 : -3;
	if (highlight)
		brightness += 2;

	draw_font_hv_shadow(VGAScreen, 166, y, msg, SMALL_FONT_SHAPES, left_aligned, 15, brightness, false, 2);
}

// Default menu navigation. Handles altering subMenuSelections for you, as well as mouse movement.
// Returns MENUNAV_SELECT if an item was selected,
// and MENUNAV_LEFT and MENUNAV_RIGHT for left or right actions respectively.
static menunavcode_t defaultMenuNavigation(const mousetargets_t *mouseTargets)
{
	int *selection = &subMenuSelections[currentSubMenu];

	int mouseTarget = apmenu_mouseInTarget(mouseTargets, mouse_x, mouse_y);
	bool buttonDown = (newmouse && lastmouse_but == SDL_BUTTON_LEFT);
	if (buttonDown)
		mouseTarget = apmenu_mouseInTarget(mouseTargets, lastmouse_x, lastmouse_y);

	if (mouseTarget >= 0 && *selection != mouseTarget)
	{
		JE_playSampleNum(S_CURSOR);
		*selection = mouseTarget;
	}
	else if (newkey && lastkey_scan == SDL_SCANCODE_UP)
	{
		JE_playSampleNum(S_CURSOR);
		if (--(*selection) < 0)
			*selection = mouseTargets->count - 1;
	}
	else if (newkey && lastkey_scan == SDL_SCANCODE_DOWN)
	{
		JE_playSampleNum(S_CURSOR);
		if (++(*selection) >= mouseTargets->count)
			*selection = 0;
	}

	if ((mouseTarget >= 0 && buttonDown) || (newkey && lastkey_scan == SDL_SCANCODE_RETURN))
		return MENUNAV_SELECT;
	if (newkey && lastkey_scan == SDL_SCANCODE_LEFT)
		return MENUNAV_LEFT;
	if (newkey && lastkey_scan == SDL_SCANCODE_RIGHT)
		return MENUNAV_RIGHT;
	return MENUNAV_NONE;
}

// ----------------------------------------------------------------------------
// Exit Game confirmation popup
// ----------------------------------------------------------------------------

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
// Main SubMenu
// ----------------------------------------------------------------------------

const mousetargets_t mainMenuTargets = {6, {
	{164,  38, 111, /* width "Play Next Level" */ 12},
	{164,  54,  77, /* width "Ship Specs"      */ 12},
	{164,  70,  71, /* width "Visit Shop"      */ 12},
	{164,  86,  97, /* width "Upgrade Ship"    */ 12},
	{164, 102,  53, /* width "Options"         */ 12},
	{164, 134,  71, /* width "Quit Game"       */ 12},
}};

const mousetargets_t mainMenuTargetsNoShop = {5, {
	{164,  38, 111, /* width "Play Next Level" */ 12},
	{164,  54,  77, /* width "Ship Specs"      */ 12},
	{164,  70,  97, /* width "Upgrade Ship"    */ 12},
	{164,  86,  53, /* width "Options"         */ 12},
	{164, 118,  71, /* width "Quit Game"       */ 12},
}};

static void submenuMain_Run(void)
{
	const mousetargets_t *mousenav = (APSeedSettings.ShopMenu == 0)
		? &mainMenuTargetsNoShop : &mainMenuTargets;

	if (defaultMenuNavigation(mousenav) == MENUNAV_SELECT)
	{
		JE_playSampleNum(S_CLICK);
		int localSelection = subMenuSelections[SUBMENU_MAIN];
		if (APSeedSettings.ShopMenu == 0 && localSelection > 1)
			++localSelection;

		switch (localSelection)
		{
			case 0: nextSubMenu = SUBMENU_NEXT_LEVEL;     break;
			case 1: JE_playSampleNum(S_CLINK);            break;
			case 2: nextSubMenu = SUBMENU_SELECT_SHOP;    break;
			case 3: nextSubMenu = SUBMENU_UPGRADE_CHOICE; break;
			case 4: nextSubMenu = SUBMENU_OPTIONS;        break;
			case 5: nextSubMenu = SUBMENU_EXIT;           break;
		}
	}

	defaultMenuOptionDraw("Play Next Level", 38 + 0,  false, SELECTED(0));
	defaultMenuOptionDraw("Ship Specs",      38 + 16,  true, SELECTED(1));
	if (APSeedSettings.ShopMenu == 0)
	{
		defaultMenuOptionDraw("Upgrade Ship",    38 + 32, false, SELECTED(2));
		defaultMenuOptionDraw("Options",         38 + 48, false, SELECTED(3));
		defaultMenuOptionDraw("Quit Game",       38 + 80, false, SELECTED(4));
	}
	else
	{
		defaultMenuOptionDraw("Visit Shop",      38 + 32, false, SELECTED(2));
		defaultMenuOptionDraw("Upgrade Ship",    38 + 48, false, SELECTED(3));
		defaultMenuOptionDraw("Options",         38 + 64, false, SELECTED(4));
		defaultMenuOptionDraw("Quit Game",       38 + 96, false, SELECTED(5));
	}
}

// ----------------------------------------------------------------------------
// Level Selection SubMenus
// ----------------------------------------------------------------------------

static int navEpisode = 1; // Currently selected episode
static int navLevel   = 0; // Sublevel in above episode
static int navScroll  = 0; // Level at the top
static bool navBack   = false;

static void levelselect_changeEpisode(int offset)
{
	int startEpisode = navEpisode;

	// Auto skip over episodes not played in the seed.
	do
	{
		navEpisode += offset;

		if (navEpisode == 0)
			navEpisode = 5;
		else if (navEpisode == 6)
			navEpisode = 1;
	} while (!(APSeedSettings.PlayEpisodes & (1 << (navEpisode-1))));

	// Nothing changed, suppress SFX and other behaviors
	if (navEpisode == startEpisode)
		return;

	JE_playSampleNum(S_CURSOR);
	navLevel = 0;
	navScroll = 0;
	navBack = false;
}

static void levelselect_changeLevel(int offset)
{
	if (navBack)
	{
		// Moving up off of BACK
		if (offset < 0)
		{
			JE_playSampleNum(S_CURSOR);
			navLevel = navScroll + 7;
			while (level_getByEpisode(navEpisode, navLevel) < 0)
				--navLevel; // Episode 5 nonsense
			navBack = false;
		}
		return;
	}

	if (navLevel + offset < 0)
		return; // Moving up off the top
	if (level_getByEpisode(navEpisode, navLevel + offset) < 0)
	{
		// Moving down onto BACK
		JE_playSampleNum(S_CURSOR);
		navBack = true;			
		return;
	}

	JE_playSampleNum(S_CURSOR);
	navLevel += offset;

	if (navLevel < navScroll)
		navScroll = navLevel;
	else if (navLevel > navScroll + 7)
		navScroll = navLevel - 7;
}

static void levelselect_scrollLevels(int offset)
{
	if (navScroll + offset < 0 || level_getByEpisode(navEpisode, navScroll + 7 + offset) < 0)
		return;

	JE_playSampleNum(S_CURSOR);
	navScroll += offset;

	if (navLevel < navScroll)
		navLevel = navScroll;
	else if (navLevel > navScroll + 7)
		navLevel = navScroll + 7;
}

static void levelselect_setLevel(int posFromScroll)
{
	if ((navLevel == navScroll + posFromScroll && !navBack)
		|| level_getByEpisode(navEpisode, navScroll + posFromScroll) < 0)
		return;

	JE_playSampleNum(S_CURSOR);
	navLevel = navScroll + posFromScroll;
	navBack = false;
}

static void levelselect_attemptSelect(void)
{
	if (navBack)
	{
		JE_playSampleNum(S_CLICK);
		nextSubMenu = SUBMENU_RETURN;
		return;
	}

	int globalID = level_getByEpisode(navEpisode, navLevel);
	if (globalID < 0)
		return;

	bool hasLevel = APItems.Levels[navEpisode - 1] & (1 << navLevel);
	bool hasClear = allCompletions[globalID];

	if (!hasLevel || (currentSubMenu == SUBMENU_SELECT_SHOP && !hasClear))
	{
		// Can't enter - do nothing
		JE_playSampleNum(S_CLINK);
		return;
	}

	subMenuSelections[currentSubMenu] = globalID;
	JE_playSampleNum(S_CLICK);
	nextSubMenu = (currentSubMenu == SUBMENU_SELECT_SHOP) ? SUBMENU_IN_SHOP : SUBMENU_LEVEL;
}

static void submenuLevelSelect_Run(void)
{
	mousetargets_t levelTargets = {13, {
		{164,  38,  80, 12},
		{164,  54,  80, 12},
		{164,  70,  80, 12},
		{164,  86,  80, 12},
		{164, 102,  80, 12},
		{164, 118,  80, 12},
		{164, 134,  80, 12},
		{164, 150,  80, 12},
		{264, 166,  40, 12}, // Back
		// Episode navigation
		{160,   0, 120,  34, MOUSE_POINTER_UP},
		{160, 182, 120,  18, MOUSE_POINTER_DOWN},
		{130,  38,  30, 144, MOUSE_POINTER_LEFT},
		{290,  38,  30, 144, MOUSE_POINTER_RIGHT},
	}};

	// Mousewheel actions go before mouse movement
	// so that what's behind the cursor gets updated immediately after scrolling.
	if (mousewheel == MOUSEWHEEL_UP)
		levelselect_scrollLevels(-1);
	else if (mousewheel == MOUSEWHEEL_DOWN)
		levelselect_scrollLevels(1);

	int mouseTarget = apmenu_mouseInTarget(&levelTargets, mouse_x, mouse_y);
	bool buttonDown = (newmouse && lastmouse_but == SDL_BUTTON_LEFT);
	if (buttonDown)
		mouseTarget = apmenu_mouseInTarget(&levelTargets, lastmouse_x, lastmouse_y);

	if (mouseTarget >= 0) switch (mouseTarget)
	{
		case 8:
			if (!navBack)
			{
				JE_playSampleNum(S_CURSOR);
				navBack = true;
			}
			if (buttonDown)
				levelselect_attemptSelect();
			break;
		case 9:
			if (buttonDown)
				levelselect_scrollLevels(-1);
			break;
		case 10:
			if (buttonDown)
				levelselect_scrollLevels(1);
			break;
		case 11:
			if (buttonDown)
				levelselect_changeEpisode(-1);
			break;
		case 12:
			if (buttonDown)
				levelselect_changeEpisode(1);
			break;
		default:
			levelselect_setLevel(mouseTarget);
			if (buttonDown)
				levelselect_attemptSelect();
			break;
	}
	else
	{
		// Keyboard / Joystick inputs
		if (newkey && lastkey_scan == SDL_SCANCODE_RIGHT)
			levelselect_changeEpisode(1);
		else if (newkey && lastkey_scan == SDL_SCANCODE_LEFT)
			levelselect_changeEpisode(-1);

		if (newkey && lastkey_scan == SDL_SCANCODE_UP)
			levelselect_changeLevel(-1);
		else if (newkey && lastkey_scan == SDL_SCANCODE_DOWN)
			levelselect_changeLevel(1);

		if (newkey && lastkey_scan == SDL_SCANCODE_RETURN)
			levelselect_attemptSelect();
	}

	snprintf(string_buffer, sizeof(string_buffer), "Episode %hu", navEpisode);
	draw_font_hv_full_shadow(VGAScreen, 234, 23, string_buffer, TINY_FONT, centered, 15, 4, true, 1);

	for (int i = 0; i < 8; ++i)
	{
		int levelID = level_getByEpisode(navEpisode, navScroll + i);
		if (levelID < 0)
			break;

		// For regular menu, can select if level unlocked, locked always shows "???"
		bool canSelect = APItems.Levels[navEpisode - 1] & (1<<(navScroll + i));
		bool canShow = false;
		bool highlighted = (!navBack && navScroll + i == navLevel);

		// For shop menu, can only select if level completed, but if unlocked level name is shown
		if (currentSubMenu == SUBMENU_SELECT_SHOP)
		{
			canShow = canSelect;
			canSelect = allCompletions[levelID];
		}

		if (canSelect)
			defaultMenuOptionDraw(allLevelData[levelID].prettyName, 38 + i*16, false, highlighted);
		else if (canShow)
			defaultMenuOptionDraw(allLevelData[levelID].prettyName, 38 + i*16, true, highlighted);
		else
			defaultMenuOptionDraw("???", 38 + i*16, true, highlighted);
	}
	draw_font_hv_shadow(VGAScreen, 304, 38 + 128, "Back", SMALL_FONT_SHAPES,
		right_aligned, 15, -3 + (navBack ? 2 : 0), false, 2);
}

// ----------------------------------------------------------------------------
// Upgrade Ship SubMenus
// ----------------------------------------------------------------------------

mousetargets_t upgradeChoiceTargets = {6};

mousetargets_t upgradeChoiceTargetsNoSpecial = {5};

static void submenuUpgradeChoice_Init(void)
{
	shots_initShotSim();
	subMenuSelections[currentSubMenu] = 0;
}

static void submenuUpgradeChoice_Run(void)
{
	const mousetargets_t *mousenav = (APSeedSettings.SpecialMenu == 0)
		? &upgradeChoiceTargetsNoSpecial : &upgradeChoiceTargets;

	if (defaultMenuNavigation(mousenav) == MENUNAV_SELECT)
	{
		JE_playSampleNum(S_CLICK);
		int localSelection = subMenuSelections[SUBMENU_UPGRADE_CHOICE];
		if (APSeedSettings.SpecialMenu == 0 && localSelection > 3)
			++localSelection;

		switch (localSelection)
		{
			case 0: nextSubMenu = SUBMENU_UPGRADE_SELECT;    break;
			case 1: JE_playSampleNum(S_CLINK);    break;
			case 2: JE_playSampleNum(S_CLINK);    break;
			case 3: JE_playSampleNum(S_CLINK);    break;
			case 4: JE_playSampleNum(S_CLINK);    break;
			case 5: nextSubMenu = SUBMENU_RETURN; break;
		}
	}

	defaultMenuOptionDraw("Front Weapon",   38 + 0,  true, SELECTED(0));
	defaultMenuOptionDraw("Rear Weapon",    38 + 16, true, SELECTED(1));
	defaultMenuOptionDraw("Left Sidekick",  38 + 32, true, SELECTED(2));
	defaultMenuOptionDraw("Right Sidekick", 38 + 48, true, SELECTED(3));
	if (APSeedSettings.SpecialMenu == 0)
		defaultMenuOptionDraw("Done",            38 + 80, false, SELECTED(4));
	else
	{
		defaultMenuOptionDraw("Special",         38 + 64, true, SELECTED(4));
		defaultMenuOptionDraw("Done",            38 + 96, false, SELECTED(5));
	}
}

static void submenuUpgradeSelect_Run(void)
{

	fill_rectangle_xy(VGAScreen, 164, 38+2, 300, 38+6, 227);
	blit_sprite2(VGAScreen, 298, 38-3, shopSpriteSheet, 247);

	for (int i = 0; i < 18; ++i)
	{
		const int shade = i ? 28 : 15;
		int portID = itemmap_FrontPorts[i];
		JE_textShade(VGAScreen, 171, 38 + 9*i, weaponPort[portID].name, shade / 16, shade % 16 - 8, DARKEN);
	}
}

// ----------------------------------------------------------------------------
// Shop SubMenu
// ----------------------------------------------------------------------------

// These fields are populated in init
static int shopItemCount;
static shopitem_t *shopItemList;

static void submenuInShop_Init(void)
{
	int shopLocationStart = allLevelData[subMenuSelections[SUBMENU_SELECT_SHOP]].shopStart;
	shopItemCount = Archipelago_GetShopItems(shopLocationStart, &shopItemList);

	// Start on "Done" - this may change later but I like how that flows for now
	subMenuSelections[SUBMENU_IN_SHOP] = shopItemCount;
}

static void submenuInShop_Run(void)
{
	mousetargets_t shopTargets = {shopItemCount + 1, {
		{164,  38, 120, shopItemCount > 0 ? 26 : 10},
		{164,  54, 120, shopItemCount > 1 ? 26 : 10},
		{164,  70, 120, shopItemCount > 2 ? 26 : 10},
		{164,  86, 120, shopItemCount > 3 ? 26 : 10},
		{164, 102, 120, shopItemCount > 4 ? 26 : 10},
		{164, 134, 120, 10},
	}};

	if (defaultMenuNavigation(&shopTargets) == MENUNAV_SELECT)
	{
		// Selected "Done", leave the shop
		if (subMenuSelections[SUBMENU_IN_SHOP] == shopItemCount)
		{
			JE_playSampleNum(S_ITEM);
			nextSubMenu = SUBMENU_RETURN;
			return;
		}

		// Attempt purchase?
		int buyItem = subMenuSelections[SUBMENU_IN_SHOP];
		if (Archipelago_WasChecked(shopItemList[buyItem].LocationID))
			JE_playSampleNum(S_CLICK); // Already purchased, just smile and nod
		else if (shopItemList[buyItem].Cost > APStats.Cash)
			JE_playSampleNum(S_CLINK); // Not enough money
		else
		{
			// We haven't bought this and we have the money for it, so buy it
			JE_playSampleNum(S_CLICK);
			Archipelago_SendCheck(shopItemList[buyItem].LocationID);
			APStats.Cash -= shopItemList[buyItem].Cost;
		}
	}

	for (int i = 0; i < shopItemCount; ++i)
	{
		const int y = 36 + i * 26;
		const int shade = subMenuSelections[SUBMENU_IN_SHOP] == i ? 15 : 28;
		int afford_shade = (shopItemList[i].Cost > APStats.Cash) ? 4 : 0;

		if (Archipelago_WasChecked(shopItemList[i].LocationID))
		{
			afford_shade = 0;

			fill_rectangle_xy(VGAScreen, 164, y+11, 300, y+15, 227);
			blit_sprite2(VGAScreen, 298, y+6, shopSpriteSheet, 247);

			strncpy(string_buffer, "Purchased", sizeof(string_buffer)-1);
		}
		else
			snprintf(string_buffer, sizeof(string_buffer), "Cost: %d", shopItemList[i].Cost);

		if (!shopItemList[i].PlayerName[0])
		{
			JE_textShade(VGAScreen, 185, y + 4, shopItemList[i].ItemName,
				shade / 16, shade % 16 - 8 - afford_shade, DARKEN);
		}
		else
		{
			JE_textShade(VGAScreen, 185, y, shopItemList[i].PlayerName,
				shade / 16, shade % 16 - 8 - afford_shade, DARKEN);
			JE_textShade(VGAScreen, 185, y+7, shopItemList[i].ItemName,
				shade / 16, shade % 16 - 8 - afford_shade, DARKEN);
		}
		JE_textShade(VGAScreen, 187, y+14, string_buffer,
			shade / 16, shade % 16 - 8 - afford_shade, DARKEN);

		if (shopItemList[i].Icon > 1000)
			blit_sprite2x2(VGAScreen, 160, y, archipelagoSpriteSheet, shopItemList[i].Icon - 1000);			
		else if (shopItemList[i].Icon > 0)
			blit_sprite2x2(VGAScreen, 160, y, shopSpriteSheet, shopItemList[i].Icon);
	}

	// Draw "Done"
	const int y = 40 + shopItemCount * 26;
	const int shade = subMenuSelections[SUBMENU_IN_SHOP] == shopItemCount ? 15 : 28;
	JE_textShade(VGAScreen, 185, y, "Done", shade / 16, shade % 16 - 8, DARKEN);
}

// ----------------------------------------------------------------------------
// Options SubMenus
// ----------------------------------------------------------------------------

static void drawOnOffOption(int y, bool value, bool disable, bool highlight)
{
	int brightness = (disable) ? -7 : -3;
	if (highlight)
		brightness += 2;

	draw_font_hv_shadow(VGAScreen, 304, y, value ? "On" : "Off", SMALL_FONT_SHAPES, right_aligned, 15, brightness, false, 2);
}

static void submenuOptions_Run(void)
{
	mousetargets_t optionTargets = {4, {
		{164,  38, JE_textWidth("Music", SMALL_FONT_SHAPES), 12},
		{164,  54, JE_textWidth("Sound", SMALL_FONT_SHAPES), 12},
		{164,  70, JE_textWidth("DeathLink", SMALL_FONT_SHAPES), 12},
		{164, 150, JE_textWidth("Exit to Main Menu", SMALL_FONT_SHAPES), 12}
	}};

	int menuResult;
	if ((menuResult = defaultMenuNavigation(&optionTargets)) != MENUNAV_NONE)
	{
		switch (subMenuSelections[SUBMENU_OPTIONS])
		{
			case 0:
				if (menuResult == MENUNAV_SELECT)
				{
					music_disabled = !music_disabled;
					if (!music_disabled)
						restart_song();
					JE_playSampleNum(S_SELECT);
				}
				else
				{
					if (music_disabled)
					{
						music_disabled = false;
						restart_song();
					}
					JE_changeVolume(&tyrMusicVolume, (menuResult == MENUNAV_RIGHT) ? 12 : -12, &fxVolume, 0);
					JE_playSampleNum(S_CURSOR);
				}
				break;
			case 1:
				if (menuResult == MENUNAV_SELECT)
				{
					samples_disabled = !samples_disabled;
					JE_playSampleNum(S_SELECT);
				}
				else
				{
					samples_disabled = false;
					JE_changeVolume(&tyrMusicVolume, 0, &fxVolume, (menuResult == MENUNAV_RIGHT) ? 12 : -12);
					JE_playSampleNum(S_CURSOR);
				}
				break;
			case 2:
				if (!APSeedSettings.DeathLink)
				{
					JE_playSampleNum(S_CLINK);
					break;
				}
				JE_playSampleNum((menuResult == MENUNAV_SELECT) ? S_SELECT : S_CURSOR);
				APOptions.EnableDeathLink = !APOptions.EnableDeathLink;
				break;
			case 3:
				if (menuResult != MENUNAV_SELECT)
					break;
				nextSubMenu = SUBMENU_RETURN;
				JE_playSampleNum(S_CLICK);
				break;
		}
	}

	JE_barDrawShadow(VGAScreen, 225, 38,    1, music_disabled ? 12 : 16, tyrMusicVolume / 12, 3, 13);
	JE_barDrawShadow(VGAScreen, 225, 38+16, 1, samples_disabled ? 12 : 16, fxVolume / 12, 3, 13);
	drawOnOffOption(38 + 32, APOptions.EnableDeathLink, !APSeedSettings.DeathLink, SELECTED(2));

	defaultMenuOptionDraw("Music",             38,       false,                     SELECTED(0));
	defaultMenuOptionDraw("Sound",             38 +  16, false,                     SELECTED(1));
	defaultMenuOptionDraw("DeathLink",         38 +  32, !APSeedSettings.DeathLink, SELECTED(2));
	defaultMenuOptionDraw("Exit to Main Menu", 38 + 112, false,                     SELECTED(3));
}

static void submenuOptions_Exit(void)
{
	// Update tags only when we leave options, to prevent spamming the server
	if (APSeedSettings.DeathLink)
		Archipelago_UpdateDeathLinkState();
}

// ------------------------------------------------------------------
// Item Screen Sidebars
// ------------------------------------------------------------------

static void sidebarArchipelagoInfo(void)
{
	// We must actually redraw these every frame, because an AP Item can arrive that upgrades them.
	JE_barDrawShadow(VGAScreen,  42, 152, 2, 14, APStats.ArmorLevel  - 8, 2, 13); // Armor
	JE_barDrawShadow(VGAScreen, 104, 152, 2, 14, APStats.ShieldLevel - 8, 2, 13); // Shield

	// Draw player ship
	blit_sprite2x2(VGAScreen, 66, 84, spriteSheet9, ships[1].shipgraphic);

	blit_sprite2x2(VGAScreen,  28,  26, shopSpriteSheet, weaponPort[5].itemgraphic);
	blit_sprite2x2(VGAScreen,  28,  54, shopSpriteSheet, weaponPort[1].itemgraphic);
	blit_sprite2x2(VGAScreen,  28, 120, shopSpriteSheet, powerSys[6].itemgraphic);
	blit_sprite2x2(VGAScreen,   3,  84, shopSpriteSheet, options[1].itemgraphic);
	blit_sprite2x2(VGAScreen, 129,  84, shopSpriteSheet, options[1].itemgraphic);

	JE_textShade(VGAScreen, 28, 26, "Zica Laser", 15, 6, DARKEN);
	JE_textShade(VGAScreen, 28, 54, "Rear Heavy Missile Launcher", 15, 6, DARKEN);
	JE_textShade(VGAScreen, 52, 120, "Gravitron Pulse-Wave", 15, 6, DARKEN);
}

// ------------------------------------------------------------------

static void sidebarPlanetNav(void)
{
	int levelID = level_getByEpisode(navEpisode, navLevel);

	tmp_setMapVars(allLevelData[levelID].planetNum);
	fill_rectangle_xy(VGAScreen, 19, 16, 135, 169, 2);
	JE_drawNavLines(true);
	JE_drawNavLines(false);

	for (int i = 0; i < 11; i++)
		JE_drawPlanet(i);
	if (allLevelData[levelID].planetNum >= 11)
		JE_drawPlanet(allLevelData[levelID].planetNum - 1);

	blit_sprite(VGAScreenSeg, 0, 0, OPTION_SHAPES, 28);  // navigation screen interface
}

// ------------------------------------------------------------------

static void sidebarSimulateShots(void)
{
	shots_runShotSim();
	blit_sprite2x2(VGAScreen, player[0].x - 5, player[0].y - 7, spriteSheet9, ships[1].shipgraphic);
}

// ----------------------------------------------------------------------------

int ap_itemScreen(void)
{
	bool updatePalette = true;

	if (shopSpriteSheet.data == NULL)
		JE_loadCompShapes(&shopSpriteSheet, '1');

	play_song(DEFAULT_SONG_BUY);

	VGAScreen = VGAScreenSeg;

	JE_loadPic(VGAScreen2, 1, false);
	memcpy(VGAScreen->pixels, VGAScreen2->pixels, VGAScreen->pitch * VGAScreen->h);

	JE_showVGA();

	if (shopSpriteSheet.data == NULL)
		JE_loadCompShapes(&shopSpriteSheet, '1');

	currentSubMenu = SUBMENU_MAIN;
	nextSubMenu = SUBMENU_NONE;
	itemSubMenus[SUBMENU_MAIN].initFunc();

	service_SDL_events(true); // Flush events before starting

	while (true)
	{
		if (updatePalette)
		{
			set_palette(palettes[itemSubMenus[currentSubMenu].palette], 0, 255);
			updatePalette = false;
		}

		push_joysticks_as_keyboard();
		service_SDL_events(false);

		itemSubMenus[currentSubMenu].sidebarFunc();
		draw_font_hv_shadow(VGAScreen, 234, 9, itemSubMenus[currentSubMenu].header,
			FONT_SHAPES, centered, 15, -3, false, 2);
		itemSubMenus[currentSubMenu].runFunc();

		// Always draw cash, over top everything
		snprintf(string_buffer, sizeof(string_buffer), "%lu", APStats.Cash);
		JE_textShade(VGAScreen, 65, 173, string_buffer, 1, 6, DARKEN);

		apmsg_manageQueue(false);

		// Back to previous menu
		if ((newmouse && lastmouse_but == SDL_BUTTON_RIGHT)
			|| (newkey && lastkey_scan == SDL_SCANCODE_ESCAPE))
		{
			JE_playSampleNum(S_SPRING);
			nextSubMenu = SUBMENU_RETURN;
		}

		// Submenu change wanted?
		switch (nextSubMenu)
		{
			case SUBMENU_NONE:
				break;

			case SUBMENU_RETURN:
				if (itemSubMenus[currentSubMenu].previous != SUBMENU_EXIT)
				{
					itemSubMenus[currentSubMenu].exitFunc();
					currentSubMenu = itemSubMenus[currentSubMenu].previous;
					updatePalette = true;
					// Do not call the init function on a submenu return
					break;
				}
				// Intentional fall through

			case SUBMENU_EXIT:
				if (!apmenu_quitRequest())
					break;

				itemSubMenus[currentSubMenu].exitFunc();
				fade_black(15);
				return -1;

			case SUBMENU_LEVEL:
				itemSubMenus[currentSubMenu].exitFunc();
				fade_black(15);
				return subMenuSelections[SUBMENU_NEXT_LEVEL];

			default:
				itemSubMenus[currentSubMenu].exitFunc();
				currentSubMenu = nextSubMenu;
				itemSubMenus[currentSubMenu].initFunc();
				updatePalette = true;
				break;
		}

		service_SDL_events(true);
		JE_mouseStart();
		JE_showVGA();

		if (itemSubMenus[currentSubMenu].sidebarFunc == sidebarSimulateShots)
		{
			// We have to use the regular delay func to make the simulator run at the proper speed.
			// This causes the mouse to look slightly more choppy than on other menus, but it's rather minor.
			// To not do this would require separating out shot movement and drawing, and that's a lot of
			// work for a relatively minor gain.
			setDelay(2);
			wait_delay();
		}
		else
			SDL_Delay(16);

		nextSubMenu = SUBMENU_NONE;
		memcpy(VGAScreen->pixels, VGAScreen2->pixels, VGAScreen->pitch * VGAScreen->h);
	}
}
