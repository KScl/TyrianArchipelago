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

#include "archipelago/apconnect.h"
#include "archipelago/apitems.h"

typedef struct { 
	int count;
	struct {
		Sint16 x, y, w, h;
		unsigned int pointer_type;
	} items[20];
} mousetargets_t;

static int apmenu_mouseInTarget(const mousetargets_t *targets, int x, int y)
{
	if (!has_mouse || mouseActivity != MOUSE_ACTIVE)
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
	clearFileDropped();

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

	bool game_ready = false;
	while (!game_ready)
	{
		push_joysticks_as_keyboard();
		service_SDL_events(true);

		if (fileDropped)
		{
			FILE *f = fopen(fileDropped, "r");
			clearFileDropped();
			game_ready = Archipelago_StartLocalGame(f);
			fclose(f);

			if (game_ready)
				break;
		}

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
				break;

			case APCONN_CONNECTING:
				draw_font_hv_shadow(VGAScreen, 160, 100, "Connecting...", small_font, centered, 12, visual_pulse, false, 1);
				break;

			case APCONN_TENTATIVE:
				draw_font_hv_shadow(VGAScreen, 160, 100, "Authenticating...", small_font, centered, 12, visual_pulse, false, 1);
				break;

			case APCONN_READY:
				// Ready to start the game
				game_ready = true;
				break;
		}

		JE_showVGA();
		SDL_Delay(16);
		memcpy(VGAScreen->pixels, VGAScreen2->pixels, VGAScreen->pitch * VGAScreen->h);
	}

	JE_playSampleNum(S_SELECT);
	fade_black(15);
	player[0].cash = 0;
	onePlayerAction = false;
	twoPlayerMode = false;
	difficultyLevel = initialDifficulty = APSeedSettings.Difficulty;
	sprites_loadMainShapeTables(APSeedSettings.Christmas);
	JE_initEpisode(1); // Temporary: We need to init items before first menu
	return true;
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

static void submenuUpgrade_Init(void);
static void submenuUpgrade_Run(void);

static void submenuUpFrontPort_Init(void);
static void submenuUpRearPort_Init(void);
static void submenuUpLeftSide_Init(void);
static void submenuUpRightSide_Init(void);
static void submenuUpSpecial_Init(void);

static void submenuUpAll_Run(void);
static void submenuUpAll_Exit(void);

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
	SUBMENU_UPGRADE,
	SUBMENU_UP_FRONTPORT,
	SUBMENU_UP_REARPORT,
	SUBMENU_UP_LEFTSIDE,
	SUBMENU_UP_RIGHTSIDE,
	SUBMENU_UP_SPECIAL,
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

	{"Upgrade Ship", 0, SUBMENU_MAIN, sidebarSimulateShots, submenuUpgrade_Init, nullexit, submenuUpgrade_Run},
	{"Front Weapon", 0, SUBMENU_UPGRADE, sidebarSimulateShots, submenuUpFrontPort_Init, submenuUpAll_Exit, submenuUpAll_Run},
	{"Rear Weapon", 0, SUBMENU_UPGRADE, sidebarSimulateShots, submenuUpRearPort_Init, submenuUpAll_Exit, submenuUpAll_Run},
	{"Left Sidekick", 0, SUBMENU_UPGRADE, sidebarSimulateShots, submenuUpLeftSide_Init, submenuUpAll_Exit, submenuUpAll_Run},
	{"Right Sidekick", 0, SUBMENU_UPGRADE, sidebarSimulateShots, submenuUpRightSide_Init, submenuUpAll_Exit, submenuUpAll_Run},
	{"Special", 0, SUBMENU_UPGRADE, sidebarSimulateShots, submenuUpSpecial_Init, submenuUpAll_Exit, submenuUpAll_Run},
};

static submenutype_t currentSubMenu = SUBMENU_NONE;
static submenutype_t nextSubMenu = SUBMENU_NONE;
static int subMenuSelections[NUM_SUBMENU] = {0};

// Money subtractions (or additions) that have not been confirmed yet
// Upgrade menu uses this to show money accurately while you're choosing weapons
static Uint64 tempMoneySub = 0;

#define SELECTED(menuItem) (subMenuSelections[currentSubMenu] == (menuItem))

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

	if ((mouseTarget >= 0 && buttonDown)
		|| (newkey && (lastkey_scan == SDL_SCANCODE_RETURN || lastkey_scan == SDL_SCANCODE_SPACE)))
	{
		return MENUNAV_SELECT;		
	}
	if (newkey && lastkey_scan == SDL_SCANCODE_LEFT)
		return MENUNAV_LEFT;
	if (newkey && lastkey_scan == SDL_SCANCODE_RIGHT)
		return MENUNAV_RIGHT;
	return MENUNAV_NONE;
}

static void blit_APIcon(int x, int y, Uint16 icon)
{
	if (icon > 9000)
		blit_sprite2x2(VGAScreen, x, y, archipelagoSpriteSheet, icon - 9000);
	else if (icon > 2000)
		blit_sprite2x2(VGAScreen, x, y, spriteSheet11, icon - 2000);
	else if (icon > 1000)
		blit_sprite2x2(VGAScreen, x, y, spriteSheet10, icon - 1000);
	else if (icon > 0)
		blit_sprite2x2(VGAScreen, x, y, shopSpriteSheet, icon);
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

	if (mouseActivity == MOUSE_ACTIVE)
		mouseActivity = MOUSE_DELAYED;
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
			case 0: nextSubMenu = SUBMENU_NEXT_LEVEL;  break;
			case 1: JE_playSampleNum(S_CLINK);         break;
			case 2: nextSubMenu = SUBMENU_SELECT_SHOP; break;
			case 3: nextSubMenu = SUBMENU_UPGRADE;     break;
			case 4: nextSubMenu = SUBMENU_OPTIONS;     break;
			case 5: nextSubMenu = SUBMENU_EXIT;        break;
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
	} while (!AP_BITSET(APSeedSettings.PlayEpisodes, navEpisode-1));

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
	bool hasClear = APStats.Clears[navEpisode - 1] & (1 << navLevel);

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

		if (newkey && (lastkey_scan == SDL_SCANCODE_RETURN || lastkey_scan == SDL_SCANCODE_SPACE))
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
		bool canSelect = AP_BITSET(APItems.Levels[navEpisode - 1], navScroll + i);
		bool canShow = false;
		bool highlighted = (!navBack && navScroll + i == navLevel);

		// For shop menu, can only select if level completed, but if unlocked level name is shown
		if (currentSubMenu == SUBMENU_SELECT_SHOP)
		{
			canShow = canSelect;
			canSelect = APStats.Clears[navEpisode - 1] & (1<<(navScroll + i));
		}

		if (canSelect)
			defaultMenuOptionDraw(allLevelData[levelID].prettyName, 38 + i*16, false, highlighted);
		else if (canShow)
			defaultMenuOptionDraw(allLevelData[levelID].prettyName, 38 + i*16, true, highlighted);
		else
			defaultMenuOptionDraw("???", 38 + i*16, true, highlighted);

		// Show check list next to level name
		const int region = currentSubMenu == SUBMENU_SELECT_SHOP
			? allLevelData[levelID].shopStart : allLevelData[levelID].locStart;

		const int checksAvail = Archipelago_GetRegionCheckCount(region);
		const Uint32 checksObtained = Archipelago_GetRegionWasCheckedList(region);
		for (int j = checksAvail; j > 0; --j)
		{
			blit_sprite2(VGAScreen, 245 + j*5, 37 + i*16, archipelagoSpriteSheet,
				AP_BITSET(checksObtained, j - 1) ? 38 : 37);
		}
	}
	draw_font_hv_shadow(VGAScreen, 304, 38 + 128, "Back", SMALL_FONT_SHAPES,
		right_aligned, 15, -3 + (navBack ? 2 : 0), false, 2);
}

// ----------------------------------------------------------------------------
// Upgrade Ship SubMenus
// ----------------------------------------------------------------------------

mousetargets_t upgradeChoiceTargets = {6};

mousetargets_t upgradeChoiceTargetsNoSpecial = {5};

static void submenuUpgrade_Init(void)
{
	shots_initShotSim();
	player_updateItemChoices();
	subMenuSelections[currentSubMenu] = 0;
}

static void submenuUpgrade_Run(void)
{
	const mousetargets_t *mousenav = (APSeedSettings.SpecialMenu == 0)
		? &upgradeChoiceTargetsNoSpecial : &upgradeChoiceTargets;

	// Hide power level interface
	blit_sprite(VGAScreenSeg, 20, 146, OPTION_SHAPES, 17);

	if (defaultMenuNavigation(mousenav) == MENUNAV_SELECT)
	{
		JE_playSampleNum(S_CLICK);
		int localSelection = subMenuSelections[SUBMENU_UPGRADE];
		if (APSeedSettings.SpecialMenu == 0 && localSelection > 3)
			++localSelection;

		switch (localSelection)
		{
			case 0: nextSubMenu = SUBMENU_UP_FRONTPORT; break;
			case 1: nextSubMenu = SUBMENU_UP_REARPORT;  break;
			case 2: nextSubMenu = SUBMENU_UP_LEFTSIDE;  break;
			case 3: nextSubMenu = SUBMENU_UP_RIGHTSIDE; break;
			case 4: nextSubMenu = SUBMENU_UP_SPECIAL;   break;
			case 5: nextSubMenu = SUBMENU_RETURN;       break;
		}
	}

	defaultMenuOptionDraw("Front Weapon",   38 + 0,  false, SELECTED(0));
	defaultMenuOptionDraw("Rear Weapon",    38 + 16, false, SELECTED(1));
	defaultMenuOptionDraw("Left Sidekick",  38 + 32, false, SELECTED(2));
	defaultMenuOptionDraw("Right Sidekick", 38 + 48, false, SELECTED(3));
	if (APSeedSettings.SpecialMenu == 0)
		defaultMenuOptionDraw("Done",            38 + 80, false, SELECTED(4));
	else
	{
		defaultMenuOptionDraw("Special",         38 + 64, false, SELECTED(4));
		defaultMenuOptionDraw("Done",            38 + 96, false, SELECTED(5));
	}
}

// ------------------------------------------------------------------

static int upgradeScroll = 0;

// This is the structure in APItemChoices that we're wanting to change.
apweapon_t *mainChoice;

int itemCount = 0;
apweapon_t itemList[64]; // All possible item IDs, and power levels for each
bool currentSelectionValid = false;

static void upgrade_changeWeapon(int offset)
{
	if (subMenuSelections[currentSubMenu] == 99) // On "done"
	{
		if (offset > 0)
			return;
		JE_playSampleNum(S_CURSOR);
		subMenuSelections[currentSubMenu] = upgradeScroll + 13;
	}
	else if (subMenuSelections[currentSubMenu] + offset >= itemCount)
	{
		JE_playSampleNum(S_CURSOR);
		subMenuSelections[currentSubMenu] = 99;
	}
	else if (subMenuSelections[currentSubMenu] + offset >= 0)
	{
		JE_playSampleNum(S_CURSOR);
		subMenuSelections[currentSubMenu] += offset;
	}

	if (subMenuSelections[currentSubMenu] != 99)
	{
		if (subMenuSelections[currentSubMenu] < upgradeScroll)
			upgradeScroll = subMenuSelections[currentSubMenu];
		else if (subMenuSelections[currentSubMenu] > upgradeScroll + 13)
			upgradeScroll = subMenuSelections[currentSubMenu] - 13;

		apweapon_t *tmp = &itemList[subMenuSelections[currentSubMenu]];
		tempMoneySub = Archipelago_GetTotalUpgradeCost(tmp->Item, tmp->PowerLevel);
		currentSelectionValid = player_overrideItemChoice(currentSubMenu - SUBMENU_UP_FRONTPORT,
			tmp->Item, tmp->PowerLevel);
	}
}

static void upgrade_changePower(apweapon_t *selWeapon, int offset)
{
	if (!currentSelectionValid)
		return;

	if ((selWeapon->PowerLevel == 0 && offset < 0) // Attempting to go below 1
		|| (selWeapon->PowerLevel + offset > APStats.PowerMaxLevel)) // Attempting to go over max power
	{
		JE_playSampleNum(S_CLINK);
		return;
	}

	if (offset > 0)
	{
		if (Archipelago_GetTotalUpgradeCost(selWeapon->Item, selWeapon->PowerLevel + 1) > APStats.Cash)
		{
			// Can't afford upgrade
			JE_playSampleNum(S_CLINK);
			return;
		}
	}

	JE_playSampleNum(S_CURSOR);
	selWeapon->PowerLevel += offset;
	tempMoneySub = Archipelago_GetTotalUpgradeCost(selWeapon->Item, selWeapon->PowerLevel);
	currentSelectionValid = player_overrideItemChoice(currentSubMenu - SUBMENU_UP_FRONTPORT,
		selWeapon->Item, selWeapon->PowerLevel);
}

static void upgrade_confirm(apweapon_t *selWeapon)
{
	if (subMenuSelections[currentSubMenu] == 99)
	{
		JE_playSampleNum(S_ITEM);
		nextSubMenu = SUBMENU_RETURN;
		return;
	}

	if (!player_overrideItemChoice(currentSubMenu - SUBMENU_UP_FRONTPORT,
		selWeapon->Item, selWeapon->PowerLevel))
		JE_playSampleNum(S_CLINK);
	else
	{
		JE_playSampleNum(S_CLICK);
		mainChoice->Item = selWeapon->Item;
		mainChoice->PowerLevel = selWeapon->PowerLevel;
		subMenuSelections[currentSubMenu] = 99;		
	}
}

static void submenuUpAll_Run(void)
{
	apweapon_t *selWeapon = mainChoice;

	if (newkey && lastkey_scan == SDL_SCANCODE_UP)
		upgrade_changeWeapon(-1);
	else if (newkey && lastkey_scan == SDL_SCANCODE_DOWN)
		upgrade_changeWeapon(1);

	if (subMenuSelections[currentSubMenu] != 99)
	{
		selWeapon = &itemList[subMenuSelections[currentSubMenu]];

		if (currentSubMenu == SUBMENU_UP_FRONTPORT || currentSubMenu == SUBMENU_UP_REARPORT)
		{
			if (newkey && lastkey_scan == SDL_SCANCODE_LEFT)
				upgrade_changePower(selWeapon, -1);
			else if (newkey && lastkey_scan == SDL_SCANCODE_RIGHT)
				upgrade_changePower(selWeapon, 1);			
		}
	}

	if (newkey && (lastkey_scan == SDL_SCANCODE_RETURN || lastkey_scan == SDL_SCANCODE_SPACE))
		upgrade_confirm(selWeapon);

	if (subMenuSelections[currentSubMenu] == 99 // Over "Done"
		|| selWeapon->Item < 500 // Item ID too low (probably "None")
		|| selWeapon->Item > 664 // Item ID too high (wrong menu?)
		|| !currentSelectionValid) // Current hovered choice is invalid
	{
		// Hide power level interface
		blit_sprite(VGAScreenSeg, 20, 146, OPTION_SHAPES, 17);
	}
	else
	{
		const unsigned int downCost = Archipelago_GetUpgradeCost(selWeapon->Item, selWeapon->PowerLevel);
		const unsigned int upCost = Archipelago_GetUpgradeCost(selWeapon->Item, selWeapon->PowerLevel + 1);

		// Weapon downgrading
		if (selWeapon->PowerLevel == 0)
			blit_sprite(VGAScreenSeg, 24, 149, OPTION_SHAPES, 13);  // downgrade disabled
		else
		{
			sprintf(string_buffer, "%d", downCost);
			JE_outText(VGAScreen, 26, 137, string_buffer, 1, 4);
		}

		// Weapon upgrading
		if (selWeapon->PowerLevel >= APStats.PowerMaxLevel)
			blit_sprite(VGAScreenSeg, 119, 149, OPTION_SHAPES, 14);  // upgrade disabled
		else
		{
			sprintf(string_buffer, "%d", upCost);

			if (upCost + tempMoneySub > APStats.Cash)
				blit_sprite(VGAScreenSeg, 119, 149, OPTION_SHAPES, 14);  // upgrade disabled
			JE_outText(VGAScreen, 108, 137, string_buffer, (upCost + tempMoneySub > APStats.Cash) ? 7 : 1, 4);
		}

		// Show weapon power
		// TODO Show maximum available power in some way
		for (int i = 0; i <= selWeapon->PowerLevel; ++i)
		{
			fill_rectangle_xy(VGAScreen, 45 + i * 6, 151, 45 + i * 6 + 4, 151, 251);
			JE_pix(VGAScreen, 45 + i * 6, 151, 252);
			fill_rectangle_xy(VGAScreen, 45 + i * 6, 152, 45 + i * 6 + 4, 164, 250);
			fill_rectangle_xy(VGAScreen, 45 + i * 6, 165, 45 + i * 6 + 4, 165, 249);
		}

		sprintf(string_buffer, "POWER: %d", selWeapon->PowerLevel + 1);
		JE_outText(VGAScreen, 58, 137, string_buffer, 15, 4);
	}

	int ypos = 38;
	for (int i = upgradeScroll; i < upgradeScroll + 14; ++i)
	{
		const int shade = subMenuSelections[currentSubMenu] == i ? 15 : 28;

		bool enabled = false;
		bool visible = false;

		if (itemList[i].Item == 0)
			enabled = visible = true;
		else if (itemList[i].Item >= 500 && itemList[i].Item < 500 + 64)
			enabled = visible = AP_BITSET(APItems.FrontPorts, itemList[i].Item - 500);
		else if (itemList[i].Item >= 600 && itemList[i].Item < 600 + 64)
			enabled = visible = AP_BITSET(APItems.RearPorts, itemList[i].Item - 600);
		else if (itemList[i].Item >= 700 && itemList[i].Item < 700 + 64)
			enabled = visible = AP_BITSET(APItems.Specials, itemList[i].Item - 700);
		else if (itemList[i].Item >= 800 && itemList[i].Item < 800 + 36)
		{
			const int sideID = itemList[i].Item - 800;
			visible = (APItems.Sidekicks[sideID] > 0);

			if (currentSubMenu == SUBMENU_UP_LEFTSIDE && APItemChoices.Sidekick[1].Item == itemList[i].Item)
				enabled = (APItems.Sidekicks[sideID] > 1);
			else if (currentSubMenu == SUBMENU_UP_RIGHTSIDE && APItemChoices.Sidekick[0].Item == itemList[i].Item)
				enabled = (APItems.Sidekicks[sideID] > 1);
			else
				enabled = visible;
		}

		if (mainChoice->Item == itemList[i].Item)
		{
			fill_rectangle_xy(VGAScreen, 164, ypos+2, 300, ypos+6, 227);
			blit_sprite2(VGAScreen, 298, ypos-3, shopSpriteSheet, 247);
		}

		if (itemList[i].Item == 0)
		{
			JE_textShade(VGAScreen, 171, ypos, "None",
				shade / 16, shade % 16 - 8, DARKEN);
		}
		else if (enabled)
		{
			JE_textShade(VGAScreen, 171, ypos, apitems_AllNames[itemList[i].Item],
				shade / 16, shade % 16 - 8, DARKEN);
		}
		else if (visible)
		{
			JE_textShade(VGAScreen, 171, ypos, apitems_AllNames[itemList[i].Item],
				shade / 16, shade % 16 - 8 - 4, DARKEN);
		}
		else
		{
			JE_textShade(VGAScreen, 171, ypos, "???",
				shade / 16, shade % 16 - 8 - 4, DARKEN);
		}

		// Show acquired item count for sidekicks
		if (visible && itemList[i].Item >= 800 && itemList[i].Item < 800 + 36)
		{
			snprintf(string_buffer, sizeof(string_buffer), "%u", APItems.Sidekicks[itemList[i].Item - 800]);
			JE_textShade(VGAScreen, 164, ypos, string_buffer, shade / 16, shade % 16 - 8 - 5, DARKEN);
		}
		ypos += 9;
	}

	draw_font_hv_shadow(VGAScreen, 304, 38 + 128, "Done", SMALL_FONT_SHAPES,
		right_aligned, 15, -3 + (subMenuSelections[currentSubMenu] == 99 ? 2 : 0), false, 2);
}

// ------------------------------------------------------------------

static void submenuUpAll_Init(void)
{
	// Scroll the item the player currently has to the center.
	upgradeScroll = subMenuSelections[currentSubMenu] - 7;
	if (upgradeScroll < 0)
		upgradeScroll = 0;
	else if (upgradeScroll > itemCount - 14)
		upgradeScroll = itemCount - 14;

	currentSelectionValid = player_overrideItemChoice(currentSubMenu - SUBMENU_UP_FRONTPORT,
		mainChoice->Item, mainChoice->PowerLevel);

	// Money only for front and rear ports
	if (currentSubMenu == SUBMENU_UP_FRONTPORT || currentSubMenu == SUBMENU_UP_REARPORT)
	{
		tempMoneySub = Archipelago_GetTotalUpgradeCost(mainChoice->Item, mainChoice->PowerLevel);
		APStats.Cash += tempMoneySub;
	}
}

static void submenuUpAll_Exit(void)
{
	// Money only for front and rear ports
	if (currentSubMenu == SUBMENU_UP_FRONTPORT || currentSubMenu == SUBMENU_UP_REARPORT)
	{
		APStats.Cash -= Archipelago_GetTotalUpgradeCost(mainChoice->Item, mainChoice->PowerLevel);
		tempMoneySub = 0;		
	}
	player_updateItemChoices();
}

// ------------------------------------------------------------------

static void submenuUpFrontPort_Init(void)
{
	subMenuSelections[currentSubMenu] = 0;

	mainChoice = &APItemChoices.FrontPort;

	itemCount = 25;
	memset(itemList, 0, sizeof(itemList));
	for (int i = 0; i < itemCount; ++i)
	{
		itemList[i].Item = 500 + i;

		if (itemList[i].Item == mainChoice->Item)
		{
			subMenuSelections[currentSubMenu] = i;
			itemList[i].PowerLevel = mainChoice->PowerLevel;
		}
	}
	submenuUpAll_Init();
}

static void submenuUpRearPort_Init(void)
{
	subMenuSelections[currentSubMenu] = 0;

	mainChoice = &APItemChoices.RearPort;
	tempMoneySub = Archipelago_GetTotalUpgradeCost(mainChoice->Item, mainChoice->PowerLevel);
	APStats.Cash += tempMoneySub;

	itemCount = 17;
	memset(itemList, 0, sizeof(itemList));
	for (int i = 1; i < itemCount; ++i)
	{
		itemList[i].Item = (600 + i) - 1;

		if (itemList[i].Item == mainChoice->Item)
		{
			subMenuSelections[currentSubMenu] = i;
			itemList[i].PowerLevel = mainChoice->PowerLevel;
		}
	}
	submenuUpAll_Init();
}

static void submenuUpLeftSide_Init(void)
{
	subMenuSelections[currentSubMenu] = 0;

	mainChoice = &APItemChoices.Sidekick[0];

	itemCount = 1;
	const int trueItemCount = 31;

	memset(itemList, 0, sizeof(itemList));
	for (int i = 1; i < trueItemCount; ++i)
	{
		if (apitems_RightOnlySidekicks[i - 1])
			continue;
		itemList[itemCount].Item = (800 + i) - 1;

		if (itemList[itemCount].Item == mainChoice->Item)
			subMenuSelections[currentSubMenu] = itemCount;
		++itemCount;
	}
	submenuUpAll_Init();
}

static void submenuUpRightSide_Init(void)
{
	subMenuSelections[currentSubMenu] = 0;

	mainChoice = &APItemChoices.Sidekick[1];

	itemCount = 31;
	memset(itemList, 0, sizeof(itemList));
	for (int i = 1; i < itemCount; ++i)
	{
		itemList[i].Item = (800 + i) - 1;

		if (itemList[i].Item == mainChoice->Item)
			subMenuSelections[currentSubMenu] = i;
	}
	submenuUpAll_Init();
}

static void submenuUpSpecial_Init(void)
{
	subMenuSelections[currentSubMenu] = 0;

	mainChoice = &APItemChoices.Special;

	itemCount = 24;
	memset(itemList, 0, sizeof(itemList));
	for (int i = 1; i < itemCount; ++i)
	{
		itemList[i].Item = (700 + i) - 1;

		if (itemList[i].Item == mainChoice->Item)
			subMenuSelections[currentSubMenu] = i;
	}
	submenuUpAll_Init();
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
		{164,  36, 120, shopItemCount > 0 ? 24 : 10},
		{164,  62, 120, shopItemCount > 1 ? 24 : 10},
		{164,  88, 120, shopItemCount > 2 ? 24 : 10},
		{164, 114, 120, shopItemCount > 3 ? 24 : 10},
		{164, 140, 120, shopItemCount > 4 ? 24 : 10},
		{164, 166, 120, 10},
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

		JE_textShade(VGAScreen, 185, y+4, shopItemList[i].ItemName,
			shade / 16, shade % 16 - 8 - afford_shade, DARKEN);
		JE_textShade(VGAScreen, 187, y+14, string_buffer,
			shade / 16, shade % 16 - 8 - afford_shade, DARKEN);

		/* I couldn't find a way to make this look good
		else
		{
			JE_textShade(VGAScreen, 185, y+2, shopItemList[i].ItemName,
				shade / 16, shade % 16 - 8 - afford_shade, DARKEN);
			JE_textShade(VGAScreen, 185, y+9, shopItemList[i].PlayerName,
				shade / 16, shade % 16 - 8 - afford_shade, DARKEN);
			JE_textShade(VGAScreen, 187, y+17, string_buffer,
				shade / 16, shade % 16 - 8 - afford_shade, DARKEN);
		}
		*/

		blit_APIcon(160, y, shopItemList[i].Icon);
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
	JE_barDrawShadow(VGAScreen,  42, 152, 2, 14, APStats.ArmorLevel  - 8, 2, 13); // Armor
	JE_barDrawShadow(VGAScreen, 104, 152, 2, 14, APStats.ShieldLevel - 8, 2, 13); // Shield

	// Draw player ship
	blit_sprite2x2(VGAScreen, 66, 84, spriteSheet9, ships[1].shipgraphic);

	if (APItemChoices.FrontPort.Item)
	{
		const int apItem = APItemChoices.FrontPort.Item;
		blit_APIcon(108, 30, apitems_AllIcons[apItem]);
		JE_textShade(VGAScreen, 28, 38, "Front Weapon:", 15, 4, DARKEN);
		JE_textShade(VGAScreen, 28, 46, apitems_AllNames[apItem], 15, 6, DARKEN);
	}
	if (APItemChoices.RearPort.Item)
	{
		const int apItem = APItemChoices.RearPort.Item;
		blit_APIcon(108, 54, apitems_AllIcons[apItem]);
		JE_textShade(VGAScreen, 28, 62, "Rear Weapon:", 15, 4, DARKEN);
		JE_textShade(VGAScreen, 28, 70, apitems_AllNames[apItem], 15, 6, DARKEN);
	}
	if (APItemChoices.Sidekick[0].Item)
		blit_APIcon(  3, 84, apitems_AllIcons[APItemChoices.Sidekick[0].Item]);
	if (APItemChoices.Sidekick[1].Item)
		blit_APIcon(129, 84, apitems_AllIcons[APItemChoices.Sidekick[1].Item]);

	if (APStats.GeneratorLevel > 0 && APStats.GeneratorLevel <= 6)
	{
		// We don't use the apitems lists for these,
		// APStats.GeneratorLevel maps directly to the internal items anyway.
		const int item = APStats.GeneratorLevel;
		blit_sprite2x2(VGAScreen, 108, 120, shopSpriteSheet, powerSys[item].itemgraphic);
		JE_textShade(VGAScreen, 24, 128, "Generator:", 15, 4, DARKEN);
		JE_textShade(VGAScreen, 24, 136, powerSys[item].name, 15, 6, DARKEN);
	}
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
	Archipelago_Save();

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
		snprintf(string_buffer, sizeof(string_buffer), "%lu", APStats.Cash - tempMoneySub);
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

					if (mouseActivity == MOUSE_ACTIVE)
						mouseActivity = MOUSE_DELAYED;
					updatePalette = true;
					// Do not call the init function on a submenu return
					break;
				}
				// Intentional fall through

			case SUBMENU_EXIT:
				if (mouseActivity == MOUSE_ACTIVE)
					mouseActivity = MOUSE_DELAYED;
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

				if (mouseActivity == MOUSE_ACTIVE)
					mouseActivity = MOUSE_DELAYED;
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
