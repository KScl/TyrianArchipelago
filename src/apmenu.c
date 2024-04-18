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
#include "pcxload.h"
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
#include "archipelago/customship.h"

// ============================================================================
// Mouse Targets (unified mouse handling)
// ============================================================================

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

// ============================================================================
// Text Input
// ============================================================================

typedef struct {
	Uint8  textColor;
	Uint8  errorColor;
	Uint8  textLenMax;

	Uint8  textLen;
	Uint8  flashTime;
	Uint8  isCaptured;
	char text[256];
} textinput_t;

// Display this text box.
static void apmenu_textInputRender(SDL_Surface *screen, textinput_t *input, Sint16 x, Sint16 y, Sint16 w)
{
	w -= 7; // Space for cursor

	const Sint16 textWidth = JE_textWidth(input->text, TINY_FONT);
	const Sint16 textEndX = (textWidth > w) ? x + w : x + textWidth; // Used for cursor placement
	const Sint16 textStartX = (textWidth > w) ? textEndX - textWidth : x; // Used to align text

	const int colorHue = input->textColor >> 4;
	const int colorVal = (input->textColor & 0xF) - 7;

	if (textWidth > 0)
		fonthand_outTextPartial(screen, textStartX, y, x, x + w, input->text, colorHue, colorVal, true);

	if (!input->isCaptured)
	{
		input->flashTime = 0;
		return;
	}
	input->isCaptured = false;

	// Cursor flash and draw
	if (!input->flashTime)
		input->flashTime = 49;
	else
		--input->flashTime;

	Uint8 baseColor = (input->textLen == input->textLenMax) ? input->errorColor : input->textColor;
	if (input->flashTime >= 25)
		baseColor += 4;

	JE_barShade(screen, textEndX + 2, y + 1, textEndX + 6, y + 6);
	fill_rectangle_xy(screen, textEndX + 1, y, textEndX + 5, y + 5, baseColor);
}

// Capture text input in this text box.
static bool apmenu_textInputCapture(textinput_t *input)
{
	input->isCaptured = true;
	if (newkey && lastkey_scan == SDL_SCANCODE_RETURN)
	{
		input->text[input->textLen] = 0; // Just absolutely ensure there's a null terminator
		return true;
	}
	else if (newkey && lastkey_scan == SDL_SCANCODE_BACKSPACE)
	{
		if (input->textLen)
			input->text[--input->textLen] = 0; // Erase character
	}
	else if (new_text)
	{
		for (char *input_p = last_text; *input_p; ++input_p)
		{
			if (input->textLen >= input->textLenMax)
				break; // Input box full
			else if (*input_p == ' ' || *input_p == '_') // Space and underscore excepted from below rule
				input->text[input->textLen++] = *input_p;
			else if ((unsigned char)*input_p < 127 && font_ascii[(unsigned char)*input_p] != -1)
				input->text[input->textLen++] = *input_p;
		}
		input->text[input->textLen] = 0;
	}
	return false;
}

// Update input box with new text.
static void apmenu_textInputUpdate(textinput_t *input, const char *newText)
{
	snprintf(input->text, 256, "%s", newText);
	input->text[input->textLenMax] = 0;
	input->textLen = (Uint8)strlen(input->text);
}

// ============================================================================
// Server Connection / Game Init Menus
// ============================================================================

// Last successful online server input
char lastGoodServerAddr[128] = "archipelago.gg:";
char lastGoodSlotName[20] = "";

// If true, aborts menu processing (fading everything out) to show connection progress instead.
static bool showGameInfo = false;
static int currentMenuState = 0; // 1 for online, 2 for offline, 0 for selection menu

// Used by local games to show error messages.
static const char *localErrorStr;

// Cursor locations for each submenu
static size_t connectSel_ModeChoice = 0;
static size_t connectSel_Online = 0;

// Text input for Online menu
static textinput_t inputServerAddr = { /* Text Color */ 250, /* Error Color */ 228, /* Max Length */ 127};
static textinput_t inputSlotName   = { /* Text Color */ 250, /* Error Color */ 228, /* Max Length */ 16};

// Does basic init steps as we initiate an Archipelago game.
void apmenu_initArchipelagoGame(void)
{
	player[0].cash = 0;
	onePlayerAction = false;
	twoPlayerMode = false;
	difficultyLevel = initialDifficulty = APSeedSettings.Difficulty;
	sprites_loadMainShapeTables(APSeedSettings.Christmas);
	JE_initEpisode(1); // Temporary: We need to init items before first menu
}

// ----------------------------------------------------------------------------

static void drawInputBox(SDL_Surface *screen, int x, int y, int w, int h, int c_outer, int c_inner)
{
	// Border
	JE_rectangle(screen, x-5, y-3, x+w+5 - 1, y+h+3 - 1, c_outer);
	JE_rectangle(screen, x-4, y-4, x+w+4 - 1, y+h+4 - 1, c_outer + 2);
	JE_rectangle(screen, x-3, y-5, x+w+3 - 1, y+h+5 - 1, c_outer);
	// Inner fill
	fill_rectangle_wh(screen, x-2, y-2, w+2+2, h+2+2, c_inner);
}

static void drawConnectOption(SDL_Surface *screen, const char *c, int y, bool highlighted)
{
	const int brightness = highlighted ? -1 : -3;
	draw_font_hv_shadow(screen, 30, y, c, SMALL_FONT_SHAPES, left_aligned, 15, brightness, false, 2);
}

// ----------------------------------------------------------------------------

static void ap_connectSubmenuModeChoice(void)
{
	if (currentMenuState < 0)
		; // View only -- No operation
	else if (fileDropped) // Handle file drops on this menu
	{
		FILE *f = fopen(fileDropped, "r");
		localErrorStr = Archipelago_StartLocalGame(f);
		fclose(f);

		showGameInfo = true;
		clearFileDropped();
	}
	else if (newkey && (lastkey_scan == SDL_SCANCODE_DOWN || lastkey_scan == SDL_SCANCODE_UP))
	{
		JE_playSampleNum(S_CURSOR);
		connectSel_ModeChoice ^= 1;
	}
	else if (newkey && lastkey_scan == SDL_SCANCODE_RETURN)
	{
		JE_playSampleNum(S_SELECT);
		if (connectSel_ModeChoice == 0)
			currentMenuState = 1;
		else
			JE_playSampleNum(S_SPRING);
	}

	draw_font_hv_shadow(VGAScreen, 160, 10, "Select Game Mode", large_font, centered, 15, -3, false, 2);

	drawConnectOption(VGAScreen, "Online via Archipelago Server", 60, (connectSel_ModeChoice == 0));
	drawConnectOption(VGAScreen, "Play Local Game",               120, (connectSel_ModeChoice == 1));

	draw_font_hv_shadow(VGAScreen, 40, 60+16, "Connect to an Archipelago Multiworld server, to", small_font, left_aligned, 15, 3, false, 1);
	draw_font_hv_shadow(VGAScreen, 40, 60+25, "play a randomized game with other players.", small_font, left_aligned, 15, 3, false, 1);

	draw_font_hv_shadow(VGAScreen, 40, 120+16, "Play a pre-generated randomized game offline.", small_font, left_aligned, 15, 3, false, 1);
}

// ----------------------------------------------------------------------------

static void ap_connectSubmenuOnline(void)
{
	if (currentMenuState < 0)
		; // View only -- No operation
	else if (newkey && lastkey_scan == SDL_SCANCODE_DOWN)
	{
		JE_playSampleNum(S_CURSOR);
		connectSel_Online = (connectSel_Online >= 2) ? 0 : connectSel_Online + 1;
	}
	else if (newkey && lastkey_scan == SDL_SCANCODE_UP)
	{
		JE_playSampleNum(S_CURSOR);
		connectSel_Online = (connectSel_Online == 0) ? 2 : connectSel_Online - 1;
	}
	else if (newkey && lastkey_scan == SDL_SCANCODE_RETURN)
	{
		JE_playSampleNum(S_SELECT);
		if (connectSel_Online < 2)
			++connectSel_Online;
		else
		{
			char connectAddress[160];
			snprintf(connectAddress, 160, "%s@%s", inputSlotName.text, inputServerAddr.text);

			Archipelago_Connect(connectAddress);			
			showGameInfo = true;
		}
	}

	if (currentMenuState >= 0)
	{
		switch (connectSel_Online)
		{
			case 0:  apmenu_textInputCapture(&inputServerAddr); break;
			case 1:  apmenu_textInputCapture(&inputSlotName); break;
			default: break;
		}		
	}

	draw_font_hv_shadow(VGAScreen, 160, 10, "Archipelago Server", large_font, centered, 15, -3, false, 2);

	drawInputBox(VGAScreen, 160, 60, 120, 12, (connectSel_Online == 0) ? 248 : 226, 224);
	drawInputBox(VGAScreen, 160, 90, 120, 12, (connectSel_Online == 1) ? 248 : 226, 224);
	apmenu_textInputRender(VGAScreen, &inputServerAddr, 160 + 2, 60 + 2, 120 - 4);
	apmenu_textInputRender(VGAScreen, &inputSlotName,   160 + 2, 90 + 2, 120 - 4);

	drawConnectOption(VGAScreen, "Address",            60, (connectSel_Online == 0));
	drawConnectOption(VGAScreen, "Slot Name",          90, (connectSel_Online == 1));
	drawConnectOption(VGAScreen, "Connect to Server", 160, (connectSel_Online == 2));
}


bool ap_connectScreen(void)
{
	bool game_ready = false;

	clearFileDropped();

	JE_loadPic(VGAScreen2, 2, false);
	memcpy(VGAScreen->pixels, VGAScreen2->pixels, VGAScreen->pitch * VGAScreen->h);

	currentMenuState = -1; // View only until loaded
	connectSel_ModeChoice = 0;
	connectSel_Online = 0;
	apmenu_textInputUpdate(&inputServerAddr, lastGoodServerAddr);
	apmenu_textInputUpdate(&inputSlotName, lastGoodSlotName);

	if (skipToGameplay)
	{
		showGameInfo = true;
		skipToGameplay = false; // Clear flag so we don't get stuck in loops later
		ap_connectSubmenuOnline(); // To draw it once
		currentMenuState = 1;
	}
	else
	{
		ap_connectSubmenuModeChoice(); // To draw it once
		currentMenuState = 0;
	}

	fade_palette(colors, 10, 0, 255);

	while (!game_ready && currentMenuState != -1)
	{
		memcpy(VGAScreen->pixels, VGAScreen2->pixels, VGAScreen->pitch * VGAScreen->h);
		push_joysticks_as_keyboard();
		service_SDL_events(true);

		if (newkey && lastkey_scan == SDL_SCANCODE_ESCAPE)
		{
			JE_playSampleNum(S_SPRING);
			currentMenuState = (currentMenuState > 0) ? 0 : -1;
		}

		switch (currentMenuState)
		{
			default:
			case 0:  ap_connectSubmenuModeChoice(); break;
			case 1:  ap_connectSubmenuOnline(); break;
			case 2:  break;
		}
		if (showGameInfo)
		{
			int visual_time = 0;
			int visual_pulse = 0;

			JE_barShade(VGAScreen, 0, 0, 319, 199);

			while (showGameInfo)
			{
				push_joysticks_as_keyboard();
				service_SDL_events(true);

				++visual_time;
				visual_time &= 0x1F;

				if (visual_time & 0x10)
					visual_pulse = -2 + ((visual_time ^ 0x1F) >> 2);
				else
					visual_pulse = -2 + ( visual_time         >> 2);

				if (currentMenuState != 1)
				{
					if (!localErrorStr)
					{
						showGameInfo = false;
						game_ready = true;
					}
					else
					{
						drawInputBox(VGAScreen, 40, 100-11, 240, 22, 232, 224);
						draw_font_hv_shadow(VGAScreen, 160,  91, "Can't start local game:", small_font, centered, 14, 2, false, 1);
						draw_font_hv_shadow(VGAScreen, 160, 101, localErrorStr, small_font, centered, 14, 2, false, 1);

						if (newkey && (lastkey_scan == SDL_SCANCODE_RETURN || lastkey_scan == SDL_SCANCODE_ESCAPE))
						{
							JE_playSampleNum((lastkey_scan == SDL_SCANCODE_RETURN) ? S_SELECT : S_SPRING);
							showGameInfo = false;
						}
					}
				}
				else switch (Archipelago_ConnectionStatus())
				{
					default:
						drawInputBox(VGAScreen, 40, 100-11, 240, 22, 232, 224);
						draw_font_hv_shadow(VGAScreen, 160,  91, "Failed to connect to the Archipelago server:", small_font, centered, 14, 2, false, 1);
						draw_font_hv_shadow(VGAScreen, 160, 101, Archipelago_GetConnectionError(), small_font, centered, 14, 2, false, 1);

						if (newkey && (lastkey_scan == SDL_SCANCODE_RETURN || lastkey_scan == SDL_SCANCODE_ESCAPE))
						{
							JE_playSampleNum((lastkey_scan == SDL_SCANCODE_RETURN) ? S_SELECT : S_SPRING);
							showGameInfo = false;
						}
						break;
					case APCONN_CONNECTING:
						drawInputBox(VGAScreen, 120, 100-6, 80, 12, 200, 224);
						draw_font_hv_shadow(VGAScreen, 160, 96, "Connecting...", small_font, centered, 12, visual_pulse, false, 1);
						break;
					case APCONN_TENTATIVE:
						drawInputBox(VGAScreen, 120, 100-6, 80, 12, 200, 224);
						draw_font_hv_shadow(VGAScreen, 160, 96, "Authenticating...", small_font, centered, 12, visual_pulse, false, 1);
						break;
					case APCONN_READY:
						memcpy(lastGoodServerAddr, inputServerAddr.text, 128);
						memcpy(lastGoodSlotName, inputSlotName.text, 20);
						lastGoodServerAddr[127] = 0;
						lastGoodSlotName[19] = 0;
						showGameInfo = false;
						game_ready = true;
						break;
				}

				JE_showVGA();
				SDL_Delay(16);
			}
		}
		else
		{
			JE_showVGA();
			SDL_Delay(16);
		}
	}

	if (currentMenuState < 0)
	{
		fade_black(15);
		return false;
	}

	JE_playSampleNum(S_SELECT);
	fade_black(15);
	apmenu_initArchipelagoGame();
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
		if (tyrian2000detected)
			blit_sprite(VGAScreenSeg, 155, 41, PLANET_SHAPES, 151); // 2000(tm)
		fade_palette(colors, 10, 0, 255 - 16);

		for (int yLogo = 60, y2K = 45; yLogo >= 4; yLogo -= 2, ++y2K)
		{
			setDelay(2);

			memcpy(VGAScreen->pixels, VGAScreen2->pixels, VGAScreen->pitch * VGAScreen->h);
			blit_sprite(VGAScreenSeg, 11, yLogo, PLANET_SHAPES, 146); // tyrian logo
			if (tyrian2000detected)
				blit_sprite(VGAScreenSeg, 155, y2K, PLANET_SHAPES, 151); // 2000(tm)
			JE_showVGA();

			service_wait_delay();
		}
	}
	else
	{
		blit_sprite(VGAScreenSeg, 11, 4, PLANET_SHAPES, 146); // tyrian logo
		if (tyrian2000detected)
			blit_sprite(VGAScreenSeg, 155, 73, PLANET_SHAPES, 151); // 2000(tm)
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

	if (skipToGameplay)
	{
		play_song(SONG_TITLE);
		if (ap_connectScreen())
			return true;
	}

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
// Results / Stats Screen
// ============================================================================

static const char* ticksToString(Uint64 ticks)
{
	static char local_buf[64];
	unsigned int seconds = ((ticks %   60000) /    1000);
	unsigned int minutes = ((ticks % 3600000) /   60000);
	unsigned int hours   = ((ticks)           / 3600000);

	if (hours) snprintf(local_buf, sizeof(local_buf), "%u:%02u:%02u", hours, minutes, seconds);
	else       snprintf(local_buf, sizeof(local_buf), "%u:%02u", minutes, seconds);
	return local_buf;
}

static const char* getCheckCount(void)
{
	static char local_buf[64];

	const int checked = Archipelago_GetTotalWasCheckedCount();
	const int total = Archipelago_GetTotalCheckCount();
	snprintf(local_buf, sizeof(local_buf), "%3d/%3d", checked, total);
	return local_buf;
}

static const char* getIntString(int value)
{
	static char local_buf[64];
	snprintf(local_buf, sizeof(local_buf), "%d", value);
	return local_buf;
}

void drawRandomizerStat(int y, const char *desc, const char *stat)
{
	draw_font_hv_shadow(VGAScreen2, 48, y, desc, SMALL_FONT_SHAPES, left_aligned, 15, -3, false, 2);
	draw_font_hv_shadow(VGAScreen2, 320-48, y, stat, SMALL_FONT_SHAPES, right_aligned, 15, -3, false, 2);
}

void apmenu_RandomizerStats(void)
{
	play_song(SONG_ZANAC);

	JE_loadPic(VGAScreen2, 2, false);

	draw_font_hv_shadow(VGAScreen2, 160, 8, "Results", FONT_SHAPES, centered, 15, -3, false, 2);

	Uint64 totalTime = APPlayData.TimeInMenu + APPlayData.TimeInLevel;
	drawRandomizerStat(48, "Locations Checked", getCheckCount());
	drawRandomizerStat(80, "Total Time", ticksToString(totalTime));
	drawRandomizerStat(96, "Time in Menus", ticksToString(APPlayData.TimeInMenu));
	drawRandomizerStat(112, "Time in Levels", ticksToString(APPlayData.TimeInLevel));
	drawRandomizerStat(144, "Deaths", getIntString(APPlayData.Deaths));
	memcpy(VGAScreen->pixels, VGAScreen2->pixels, VGAScreen->pitch * VGAScreen->h);

	fade_palette(colors, 10, 0, 255);

	service_SDL_events(true); // Flush events before loop starts
	while (true)
	{
		push_joysticks_as_keyboard();
		service_SDL_events(false);

		if (newkey && lastkey_scan == SDL_SCANCODE_ESCAPE)
		{
			JE_playSampleNum(S_SPRING);
			break;
		}
		else if (newkey && (lastkey_scan == SDL_SCANCODE_RETURN || lastkey_scan == SDL_SCANCODE_SPACE))
		{
			JE_playSampleNum(S_SELECT);
			break;
		}

		service_SDL_events(true);
		JE_mouseStart();
		JE_showVGA();
		JE_mouseReplace();

		SDL_Delay(16);
	}
	fade_black(15);
}

// ============================================================================
// Item Screen / Main Archipelago Menu
// ============================================================================

static char string_buffer[32];

static void sidebarShipSprite(void);
static void sidebarArchipelagoInfo(void);
static void sidebarPlanetNav(void);
static void sidebarSimulateShots(void);

static void nullinit(void);
static void nullexit(void);

static void submenuMain_Run(void);

static void submenuLevelSelect_Init(void);
static void submenuLevelSelect_Run(void);

static void submenuInShop_Init(void);
static void submenuInShop_Run(void);

static void submenuOptions_Run(void);
static void submenuOptions_Exit(void);

static void submenuOptCustomShip_Init(void);
static void submenuOptCustomShip_Run(void);

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
	SUBMENU_OPTIONS_CUSTOMSHIP,
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
	{"Next Level", 17, SUBMENU_MAIN, sidebarPlanetNav, submenuLevelSelect_Init, nullexit, submenuLevelSelect_Run},
	{"Visit Shop", 17, SUBMENU_MAIN, sidebarPlanetNav, submenuLevelSelect_Init, nullexit, submenuLevelSelect_Run},
	{"Shop", 0, SUBMENU_SELECT_SHOP, sidebarArchipelagoInfo, submenuInShop_Init, nullexit, submenuInShop_Run},
	{"Options", 0, SUBMENU_MAIN, sidebarArchipelagoInfo, nullinit, submenuOptions_Exit, submenuOptions_Run},
	{"Ship Sprite", 0, SUBMENU_OPTIONS, sidebarShipSprite, submenuOptCustomShip_Init, nullexit, submenuOptCustomShip_Run},

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

// A few different menus use this
static Uint32 itemHoverTime = 0;

#define GETSELECTED() (subMenuSelections[currentSubMenu])
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

	++itemHoverTime;

	if (mouseTarget >= 0 && *selection != mouseTarget)
	{
		JE_playSampleNum(S_CURSOR);
		*selection = mouseTarget;
		itemHoverTime = 0;
	}
	else if (newkey && lastkey_scan == SDL_SCANCODE_UP)
	{
		JE_playSampleNum(S_CURSOR);
		if (--(*selection) < 0)
			*selection = mouseTargets->count - 1;
		itemHoverTime = 0;
	}
	else if (newkey && lastkey_scan == SDL_SCANCODE_DOWN)
	{
		JE_playSampleNum(S_CURSOR);
		if (++(*selection) >= mouseTargets->count)
			*selection = 0;
		itemHoverTime = 0;
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

// Returns item name without disambiguating parentheticals
static const char *getCleanName(Uint16 itemID)
{
	switch (itemID)
	{
		case 0:             return "None";
		case 501: case 601: return "Multi-Cannon";
		case 508: case 603: return "Protron";
		case 506: case 605: return "Vulcan Cannon";
		case 511: case 607: return "Heavy Missile Launcher";
		case 510: case 608: return "Mega Pulse";
		case 512: case 609: return "Banana Blast";
		case 513: case 610: return "HotDog";
		default:            return apitems_AllNames[itemID];		
	}
}

// Scrolls text on screen if it is too long.
void scrollingTinyTextItem(SDL_Surface *screen, int x, int y, int width, const char *s, Uint8 hue, Uint8 value, bool doScroll)
{
	x += 1;
	y += 1;

	const int textWidth = JE_textWidth(s, TINY_FONT);
	if (textWidth <= width)
		JE_outTextAndDarken(screen, x, y, s, hue, value, TINY_FONT);
	else if (!doScroll)
		fonthand_outTextPartial(screen, x, y, x, x+width, s, hue, value, true);
	else
	{
		const int scrollX = (itemHoverTime <= 60) ? x : x - ((signed)itemHoverTime - 60);
		if (scrollX + textWidth < x)
			itemHoverTime = 0;

		fonthand_outTextPartial(screen, scrollX, y, x, x+width, s, hue, value, true);			
	}
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

	service_SDL_events(true); // Flush events before loop starts
	while (true)
	{
		push_joysticks_as_keyboard();
		service_SDL_events(false);
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

		service_SDL_events(true);
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

static const mousetargets_t mainMenuTargets = {6, {
	{164,  38, 111, /* width "Play Next Level" */ 12},
	{164,  54,  77, /* width "Ship Specs"      */ 12},
	{164,  70,  71, /* width "Visit Shop"      */ 12},
	{164,  86,  97, /* width "Upgrade Ship"    */ 12},
	{164, 102,  53, /* width "Options"         */ 12},
	{164, 134,  71, /* width "Quit Game"       */ 12},
}};

static const mousetargets_t mainMenuTargetsNoShop = {5, {
	{164,  38, 111, /* width "Play Next Level" */ 12},
	{164,  54,  77, /* width "Ship Specs"      */ 12},
	{164,  70,  97, /* width "Upgrade Ship"    */ 12},
	{164,  86,  53, /* width "Options"         */ 12},
	{164, 118,  71, /* width "Quit Game"       */ 12},
}};

static void submenuMain_Run(void)
{
	const mousetargets_t *mousenav = (APSeedSettings.ShopMode == SHOP_MODE_NONE)
		? &mainMenuTargetsNoShop : &mainMenuTargets;

	if (defaultMenuNavigation(mousenav) == MENUNAV_SELECT)
	{
		JE_playSampleNum(S_CLICK);
		int localSelection = subMenuSelections[SUBMENU_MAIN];
		if (APSeedSettings.ShopMode == SHOP_MODE_NONE && localSelection > 1)
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

#ifdef DEBUG_OPTIONS
		// If in damage viewer mode, immediately go to Soh Jin (Episode 2).
		if (debugDamageViewer && nextSubMenu == SUBMENU_NEXT_LEVEL)
		{
			subMenuSelections[SUBMENU_NEXT_LEVEL] = 24;
			nextSubMenu = SUBMENU_LEVEL;
		}
#endif
	}

	defaultMenuOptionDraw("Play Next Level", 38 + 0,  false, SELECTED(0));
	defaultMenuOptionDraw("Ship Specs",      38 + 16,  true, SELECTED(1));
	if (APSeedSettings.ShopMode == SHOP_MODE_NONE)
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

	apmsg_manageQueueMenu(false);
	JE_outTextAndDarken(VGAScreen, 286, 187, "[TAB]", 14, 1, TINY_FONT);
}

// ----------------------------------------------------------------------------
// Level Selection SubMenus
// ----------------------------------------------------------------------------

static const mousetargets_t levelTargets = {13, {
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

	// ----- Help Text --------------------------------------------------------
	const char *helpText;
	if (navBack)
		helpText = "Go back to the previous menu.";
	else if (currentSubMenu == SUBMENU_SELECT_SHOP)
		helpText = "You can visit the shop of any completed level.";
	else
		helpText = "Select which level to play next.";
	JE_outTextAndDarken(VGAScreen, 10, 187, helpText, 14, 1, TINY_FONT);
}

void submenuLevelSelect_Init(void)
{
	// Default to last played level.
	if (currentLevelID >= 0)
	{
		navLevel = allLevelData[currentLevelID].episodeLevelID;
		navEpisode = allLevelData[currentLevelID].episodeNum;
	}
	else
	{
		navLevel = 0;
		navEpisode = 1;
	}

	while (!AP_BITSET(APSeedSettings.PlayEpisodes, navEpisode-1))
	{
		++navEpisode;
		navLevel = 0;

		if (navEpisode == 6)
			JE_tyrianHalt(1); // Impossible
	}
	navBack = false;

	// Center scroll on the default level.
	for (navScroll = navLevel - 4; navScroll > 0; --navScroll)
	{
		// Move up until the bottom-most entry isn't empty.
		// Or until we're at the top, because Episode 5 exists.
		if (level_getByEpisode(navEpisode, navScroll + 7) != -1)
			break;
	}
	// If above the top, set to the top.
	if (navScroll < 0)
		navScroll = 0;

}

// ----------------------------------------------------------------------------
// Upgrade Ship SubMenus
// ----------------------------------------------------------------------------

static const mousetargets_t upgradeChoiceTargets = {6, {
	{164,  38, 104, /* width "Front Weapon"   */ 12},
	{164,  54,  97, /* width "Rear Weapon"    */ 12},
	{164,  70,  93, /* width "Left Sidekick"  */ 12},
	{164,  86,  99, /* width "Right Sidekick" */ 12},
	{164, 102,  50, /* width "Special"        */ 12},
	{164, 134,  35, /* width "Done"           */ 12},
}};

static const mousetargets_t upgradeChoiceTargetsNoSpecial = {5, {
	{164,  38, 104, /* width "Front Weapon"   */ 12},
	{164,  54,  97, /* width "Rear Weapon"    */ 12},
	{164,  70,  93, /* width "Left Sidekick"  */ 12},
	{164,  86,  99, /* width "Right Sidekick" */ 12},
	{164, 118,  35, /* width "Done"           */ 12},
}};

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

	// ----- Help Text --------------------------------------------------------
	const char *helpText;
	switch (GETSELECTED())
	{
		case 0:  helpText = "Change or upgrade your front weapon."; break;
		case 1:  helpText = "Change, upgrade, or remove your rear weapon."; break;
		case 2:  // fall through
		case 3:  helpText = "Change or remove your sidekicks."; break;
		case 4:
			if (APSeedSettings.SpecialMenu)
			{
				helpText = "Change or remove your special weapon.";
				break;
			}
			// fall through
		default: helpText = "Go back to the previous menu."; break;
	}
	JE_outTextAndDarken(VGAScreen, 10, 187, helpText, 14, 1, TINY_FONT);
}

// ------------------------------------------------------------------

static int upgradeScroll = 0;

// This is the structure in APItemChoices that we're wanting to change.
apweapon_t *mainChoice;

int itemCount = 0;
apweapon_t itemList[64]; // All possible item IDs, and power levels for each
bool currentSelectionValid = false;

// ------------------------------------------------------------------
// Mouse Controls Specific Behavior
// ------------------------------------------------------------------

static const mousetargets_t upgradeItemListTargets = {20, {
	{171,  38, 129,  8},
	{171,  47, 129,  8},
	{171,  56, 129,  8},
	{171,  65, 129,  8},
	{171,  74, 129,  8},
	{171,  83, 129,  8},
	{171,  92, 129,  8},
	{171, 101, 129,  8},
	{171, 110, 129,  8},
	{171, 119, 129,  8},
	{171, 128, 129,  8},
	{171, 137, 129,  8},
	{171, 146, 129,  8},
	{171, 155, 129,  8},
	{269, 166,  35, 12}, // Done
	// Menu scrolling
	{160,   0, 120,  34, MOUSE_POINTER_UP},
	{160, 182, 120,  18, MOUSE_POINTER_DOWN},
	// Power up/down
	{ 24, 149,  12,  19, MOUSE_POINTER_LEFT},
	{120, 149,  12,  19, MOUSE_POINTER_RIGHT},
	// Mode change
	{  3,  56,  14,  16},
}};

static int upgradeLastHover = -1;

static void upgrade_scrollWeapons(int offset)
{
	if (upgradeScroll + offset < 0 || upgradeScroll + 13 + offset >= itemCount)
		return;

	JE_playSampleNum(S_CURSOR);
	upgradeScroll += offset;
}

static void upgrade_setWeapon(int posFromScroll)
{
	if (upgradeScroll + posFromScroll >= itemCount)
		return;

	apweapon_t *newWeapon = &itemList[upgradeScroll + posFromScroll];

	if (!player_overrideItemChoice(currentSubMenu - SUBMENU_UP_FRONTPORT,
		newWeapon->Item, newWeapon->PowerLevel))
	{
		JE_playSampleNum(S_CLINK);		
	}
	else
	{
		JE_playSampleNum(S_CLICK);
		mainChoice->Item = newWeapon->Item;
		mainChoice->PowerLevel = newWeapon->PowerLevel;
		tempMoneySub = Archipelago_GetTotalUpgradeCost(mainChoice->Item, mainChoice->PowerLevel);
		subMenuSelections[currentSubMenu] = upgradeScroll + posFromScroll;
	}
}

// ------------------------------------------------------------------
// Keyboard Specific Behavior
// ------------------------------------------------------------------

static void upgrade_changeWeapon(int offset)
{
	if (subMenuSelections[currentSubMenu] < 0) // On "done"
	{
		if (offset > 0)
			return;
		JE_playSampleNum(S_CURSOR);
		subMenuSelections[currentSubMenu] = upgradeScroll + 13;
	}
	else if (subMenuSelections[currentSubMenu] + offset >= itemCount)
	{
		JE_playSampleNum(S_CURSOR);
		subMenuSelections[currentSubMenu] = -9999;
	}
	else if (subMenuSelections[currentSubMenu] + offset >= 0)
	{
		JE_playSampleNum(S_CURSOR);
		subMenuSelections[currentSubMenu] += offset;
	}

	if (subMenuSelections[currentSubMenu] >= 0)
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

static void upgrade_changeMode(apweapon_t *selWeapon)
{
	if (!currentSelectionValid || !(selWeapon->Item >= 600 && selWeapon->Item < 664))
		return;

	APItemChoices.RearMode2 = !APItemChoices.RearMode2;
	currentSelectionValid = player_overrideItemChoice(currentSubMenu - SUBMENU_UP_FRONTPORT,
		selWeapon->Item, selWeapon->PowerLevel);
}

static void upgrade_confirm(apweapon_t *selWeapon)
{
	if (subMenuSelections[currentSubMenu] < 0)
	{
		JE_playSampleNum(S_ITEM);
		nextSubMenu = SUBMENU_RETURN;
		return;
	}

	if (!player_overrideItemChoice(currentSubMenu - SUBMENU_UP_FRONTPORT,
		selWeapon->Item, selWeapon->PowerLevel))
	{
		JE_playSampleNum(S_CLINK);		
	}
	else
	{
		JE_playSampleNum(S_CLICK);
		mainChoice->Item = selWeapon->Item;
		mainChoice->PowerLevel = selWeapon->PowerLevel;
		subMenuSelections[currentSubMenu] = -9999;
	}
}

// ------------------------------------------------------------------

static void submenuUpAll_Run(void)
{
	apweapon_t *selWeapon = mainChoice;

	// Mousewheel actions go before mouse movement
	// so that what's behind the cursor gets updated immediately after scrolling.
	if (mousewheel == MOUSEWHEEL_UP)
		upgrade_scrollWeapons(-1);
	else if (mousewheel == MOUSEWHEEL_DOWN)
		upgrade_scrollWeapons(1);

	// Separate mouse hover from mouse down for this menu only
	int mouseHoverTarget = apmenu_mouseInTarget(&upgradeItemListTargets, mouse_x, mouse_y);
	if (upgradeLastHover != mouseHoverTarget)
	{
		upgradeLastHover = mouseHoverTarget;
		if (mouseHoverTarget >= 0 && mouseHoverTarget <= 14)
			JE_playSampleNum(S_CURSOR);
	}

	// Menu behavior for upgrading and choosing weapons works differently because the natural
	// flow for keyboard doesn't convey well for a mouse. You'd have to dodge menu items to
	// upgrade the weapon you wanted if we just auto-selected what you're highlighted over,
	// like the keyboard controls do.
	//
	// The other prime difference is that changes are committed immediately, instead of
	// requiring a selection to confirm, because hitting a narrow menu target to confirm
	// your changes is not as easy as just pressing the "enter" button. This does make for
	// some weird interactions if you switch between mouse and keyboard, but oh well.
	int mouseDownTarget = (newmouse && lastmouse_but == SDL_BUTTON_LEFT) ?
		apmenu_mouseInTarget(&upgradeItemListTargets, lastmouse_x, lastmouse_y) : -1;
	if (mouseDownTarget >= 0)
	{
		switch (mouseDownTarget)
		{
			case 14:
				JE_playSampleNum(S_ITEM);
				nextSubMenu = SUBMENU_RETURN;
				break;
			case 15:
				upgrade_scrollWeapons(-1);
				break;
			case 16:
				upgrade_scrollWeapons(1);
				break;
			case 17:
				if (currentSubMenu == SUBMENU_UP_FRONTPORT || currentSubMenu == SUBMENU_UP_REARPORT)
					upgrade_changePower(mainChoice, -1);
				break;
			case 18:
				if (currentSubMenu == SUBMENU_UP_FRONTPORT || currentSubMenu == SUBMENU_UP_REARPORT)
					upgrade_changePower(mainChoice, 1);
				break;
			case 19:
				upgrade_changeMode(mainChoice);
				break;
			default:
				upgrade_setWeapon(mouseDownTarget);
				break;
		}
		selWeapon = &itemList[subMenuSelections[currentSubMenu]];
		selWeapon->PowerLevel = mainChoice->PowerLevel;
	}
	else
	{
		if (newkey && lastkey_scan == SDL_SCANCODE_UP)
			upgrade_changeWeapon(-1);
		else if (newkey && lastkey_scan == SDL_SCANCODE_DOWN)
			upgrade_changeWeapon(1);

		if (subMenuSelections[currentSubMenu] >= 0)
		{
			selWeapon = &itemList[subMenuSelections[currentSubMenu]];

			if (currentSubMenu == SUBMENU_UP_FRONTPORT || currentSubMenu == SUBMENU_UP_REARPORT)
			{
				if (newkey && lastkey_scan == SDL_SCANCODE_LEFT)
					upgrade_changePower(selWeapon, -1);
				else if (newkey && lastkey_scan == SDL_SCANCODE_RIGHT)
					upgrade_changePower(selWeapon, 1);
				else if (newkey && lastkey_scan == SDL_SCANCODE_SLASH)
					upgrade_changeMode(selWeapon);
			}
		}
	}

	if (newkey && (lastkey_scan == SDL_SCANCODE_RETURN || lastkey_scan == SDL_SCANCODE_SPACE))
		upgrade_confirm(selWeapon);

	if (subMenuSelections[currentSubMenu] < 0 // Over "Done"
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
		bool highlight = (subMenuSelections[currentSubMenu] == i
			|| (upgradeLastHover >= 0 && i == upgradeScroll + upgradeLastHover));
		const int shade = highlight ? 15 : 28;

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

		const char *itemName = getCleanName(itemList[i].Item);
		if (enabled)
			JE_textShade(VGAScreen, 171, ypos, itemName, shade / 16, shade % 16 - 8, DARKEN);
		else if (visible)
			JE_textShade(VGAScreen, 171, ypos, itemName, shade / 16, shade % 16 - 8 - 4, DARKEN);
		else
			JE_textShade(VGAScreen, 171, ypos, "???", shade / 16, shade % 16 - 8 - 4, DARKEN);

		// Show acquired item count for sidekicks
		if (visible && itemList[i].Item >= 800 && itemList[i].Item < 800 + 36)
		{
			snprintf(string_buffer, sizeof(string_buffer), "%u", APItems.Sidekicks[itemList[i].Item - 800]);
			JE_textShade(VGAScreen, 164, ypos, string_buffer, shade / 16, shade % 16 - 8 - 5, DARKEN);
		}
		ypos += 9;
	}

	const bool doneSelected = ((upgradeLastHover <= 0 && subMenuSelections[currentSubMenu] < 0)
		|| upgradeLastHover == 14);
	draw_font_hv_shadow(VGAScreen, 304, 38 + 128, "Done", SMALL_FONT_SHAPES,
		right_aligned, 15, -3 + (doneSelected ? 2 : 0), false, 2);

	// ----- Help Text --------------------------------------------------------
	const char *helpText;
	if (subMenuSelections[currentSubMenu] < 0)
		helpText = "Go back to the previous menu.";
	else if (!currentSelectionValid)
	{
		if (selWeapon->Item >= 800 && selWeapon->Item < 836
			&& APItems.Sidekicks[selWeapon->Item - 800] == 1)
		{
			helpText = "You only have one of this item.";
		}
		else
			helpText = "You haven't found this item yet.";
	}
	else if (selWeapon->Item == 0)
		helpText = "Remove the item in this slot.";
	else if (selWeapon->Item >= 500 && selWeapon->Item < 664)
		helpText = "Change power level with left/right, select to confirm.";
	else
		helpText = "Select the item you want.";

	JE_outTextAndDarken(VGAScreen, 10, 187, helpText, 14, 1, TINY_FONT);
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

	upgradeLastHover = -1;
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

static const Uint16 baseItemList_Front[] = {
	500, 501, 506, 508, 518, 509, 515, 513, 522, 523, 524, 504, 502, 507, 511, 512,
	519, 514, 510, 505, 520, 516, 503, 517, 521,
	// Tyrian 2000
	525, 526, 527, 528,
	0xFFFF // End
};

static const Uint16 baseItemList_Rear[] = {
	0, // "None" available for this menu
	601, 605, 600, 603, 611, 612, 610, 613, 602, 604, 606, 607, 609, 608, 614, 615,
	// Tyrian 2000
	616,
	0xFFFF // End
};

static const Uint16 baseItemList_Special[] = {
	0, // "None" available for this menu
	700, 703, 704, 702, 701, 705, 706, 707, 708, 709, 710, 711, 712, 713, 714, 715,
	716, 717, 718, 719, 720, 721,
	// Tyrian 2000
	723, 724,
	0xFFFF // End
};

static const Uint16 baseItemList_Sidekick[] = {
	0, // "None" available for this menu
	800, 801, 803, 829, 804, 802, 816, 811, 817, 812, 813, 814, 809, 810, 805, 806,
	807, 808, 815, 828, 818, 820, 823, 819, 821, 825, 826, 827, 822, 824,
	// Tyrian 2000
	830, 831, 
	0xFFFF // End
};

static void assignItemList(const Uint16 *baseList)
{
	memset(itemList, 0, sizeof(itemList));
	itemCount = 0;
	for (; *baseList != 0xFFFF; ++baseList)
	{
		if (currentSubMenu == SUBMENU_UP_LEFTSIDE && *baseList != 0
			&& apitems_RightOnlySidekicks[*baseList - 800])
		{
			continue;
		}

		if (!APSeedSettings.Tyrian2000Mode && *baseList != 0
			&& apitems_Tyrian2000Only[*baseList])
		{
			continue;
		}

		itemList[itemCount].Item = *baseList;

		if (*baseList == mainChoice->Item)
		{
			subMenuSelections[currentSubMenu] = itemCount;
			itemList[itemCount].PowerLevel = mainChoice->PowerLevel;
		}
		++itemCount;
	}
}

static void submenuUpFrontPort_Init(void)
{
	subMenuSelections[currentSubMenu] = 0;

	mainChoice = &APItemChoices.FrontPort;
	assignItemList(baseItemList_Front);
	submenuUpAll_Init();
}

static void submenuUpRearPort_Init(void)
{
	subMenuSelections[currentSubMenu] = 0;

	mainChoice = &APItemChoices.RearPort;
	assignItemList(baseItemList_Rear);
	submenuUpAll_Init();
}

static void submenuUpLeftSide_Init(void)
{
	subMenuSelections[currentSubMenu] = 0;

	mainChoice = &APItemChoices.Sidekick[0];
	assignItemList(baseItemList_Sidekick);
	submenuUpAll_Init();
}

static void submenuUpRightSide_Init(void)
{
	subMenuSelections[currentSubMenu] = 0;

	mainChoice = &APItemChoices.Sidekick[1];
	assignItemList(baseItemList_Sidekick);
	submenuUpAll_Init();
}

static void submenuUpSpecial_Init(void)
{
	subMenuSelections[currentSubMenu] = 0;

	mainChoice = &APItemChoices.Special;
	assignItemList(baseItemList_Special);
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
		}
		else
		{
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

		scrollingTinyTextItem(VGAScreen, 185, y+4, 126, shopItemList[i].ItemName,
			shade / 16, shade % 16 - 8 - afford_shade, (shade == 15));
		JE_textShade(VGAScreen, 187, y+14, string_buffer,
			shade / 16, shade % 16 - 8 - afford_shade, DARKEN);

		sprites_blitArchipelagoIcon(VGAScreen, 160, y, shopItemList[i].Icon);
	}

	// Draw "Done"
	const int y = 40 + shopItemCount * 26;
	const int shade = subMenuSelections[SUBMENU_IN_SHOP] == shopItemCount ? 15 : 28;
	JE_textShade(VGAScreen, 185, y, "Done", shade / 16, shade % 16 - 8, DARKEN);

	// ----- Help Text --------------------------------------------------------
	char helpText[128];
	if (subMenuSelections[SUBMENU_IN_SHOP] == shopItemCount)
		strcpy(helpText, "Go back to the previous menu.");
	else if (shopItemList[subMenuSelections[SUBMENU_IN_SHOP]].PlayerName[0] != '\0')
	{
		snprintf(helpText, sizeof(helpText) - 1, "This item is for ~%s~.",
			shopItemList[subMenuSelections[SUBMENU_IN_SHOP]].PlayerName);
		helpText[127] = 0;
	}
	else
		strcpy(helpText, "Select the item you want.");

	JE_outTextAndDarken(VGAScreen, 10, 187, helpText, 14, 1, TINY_FONT);
}

// ----------------------------------------------------------------------------
// Custom Ship Choice SubMenu
// ----------------------------------------------------------------------------

static const mousetargets_t customShipTargets = {17, {
	{171,  38, 129,  8},
	{171,  47, 129,  8},
	{171,  56, 129,  8},
	{171,  65, 129,  8},
	{171,  74, 129,  8},
	{171,  83, 129,  8},
	{171,  92, 129,  8},
	{171, 101, 129,  8},
	{171, 110, 129,  8},
	{171, 119, 129,  8},
	{171, 128, 129,  8},
	{171, 137, 129,  8},
	{171, 146, 129,  8},
	{171, 155, 129,  8},
	{269, 166,  35, 12}, // Done
	// Menu scrolling
	{160,   0, 120,  34, MOUSE_POINTER_UP},
	{160, 182, 120,  18, MOUSE_POINTER_DOWN},
}};

static int customShipScroll = 0;

static void ship_set(int posFromScroll)
{
	if (customShipScroll + posFromScroll == subMenuSelections[currentSubMenu]
		|| !CustomShip_Exists(customShipScroll + posFromScroll))
	{
		return;		
	}

	JE_playSampleNum(S_CURSOR);
	subMenuSelections[currentSubMenu] = customShipScroll + posFromScroll;
}

static void ship_scroll(int offset)
{
	const int customShipTotal = (signed)CustomShip_Count();
	if (customShipScroll + offset < 0 || customShipScroll + 13 + offset >= customShipTotal)
		return;

	JE_playSampleNum(S_CURSOR);
	customShipScroll += offset;

	if (subMenuSelections[currentSubMenu] < customShipScroll)
		subMenuSelections[currentSubMenu] = customShipScroll;
	else if (subMenuSelections[currentSubMenu] > customShipScroll + 13)
		subMenuSelections[currentSubMenu] = customShipScroll + 13;
}

static void ship_navigate(int offset)
{
	const int customShipTotal = (signed)CustomShip_Count();

	if (subMenuSelections[currentSubMenu] < 0) // On "done"
	{
		if (offset > 0)
			return;
		JE_playSampleNum(S_CURSOR);
		subMenuSelections[currentSubMenu] = customShipScroll + 13;

		if (subMenuSelections[currentSubMenu] >= customShipTotal)
			subMenuSelections[currentSubMenu] = customShipTotal - 1;
	}
	else if (subMenuSelections[currentSubMenu] + offset >= customShipTotal)
	{
		JE_playSampleNum(S_CURSOR);
		subMenuSelections[currentSubMenu] = -9999;
	}
	else if (subMenuSelections[currentSubMenu] + offset >= 0)
	{
		JE_playSampleNum(S_CURSOR);
		subMenuSelections[currentSubMenu] += offset;
	}

	if (subMenuSelections[currentSubMenu] >= 0)
	{
		if (subMenuSelections[currentSubMenu] < customShipScroll)
			customShipScroll = subMenuSelections[currentSubMenu];
		else if (subMenuSelections[currentSubMenu] > customShipScroll + 13)
			customShipScroll = subMenuSelections[currentSubMenu] - 13;
	}
}

static void ship_confirm(void)
{
	JE_playSampleNum(S_CLICK);
	if (subMenuSelections[currentSubMenu] < 0)
		nextSubMenu = SUBMENU_RETURN;
	else
	{
		currentCustomShip = subMenuSelections[currentSubMenu];
		subMenuSelections[currentSubMenu] = -9999;
	}
}

static void submenuOptCustomShip_Run(void)
{
	// Mousewheel actions go before mouse movement
	// so that what's behind the cursor gets updated immediately after scrolling.
	if (mousewheel == MOUSEWHEEL_UP)
		ship_scroll(-1);
	else if (mousewheel == MOUSEWHEEL_DOWN)
		ship_scroll(1);

	int mouseTarget = apmenu_mouseInTarget(&customShipTargets, mouse_x, mouse_y);
	bool buttonDown = (newmouse && lastmouse_but == SDL_BUTTON_LEFT);
	if (buttonDown)
		mouseTarget = apmenu_mouseInTarget(&customShipTargets, lastmouse_x, lastmouse_y);

	if (mouseTarget >= 0) switch (mouseTarget)
	{
		case 14:
			if (subMenuSelections[currentSubMenu] >= 0)
			{
				JE_playSampleNum(S_CURSOR);
				subMenuSelections[currentSubMenu] = -9999;
			}
			if (buttonDown)
				ship_confirm();
			break;
		case 15:
			if (buttonDown)
				ship_scroll(-1);
			break;
		case 16:
			if (buttonDown)
				ship_scroll(1);
			break;
		default:
			ship_set(mouseTarget);
			if (buttonDown)
			{
				ship_confirm();
				subMenuSelections[currentSubMenu] = currentCustomShip;
			}
			break;
	}
	else
	{
		if (newkey && lastkey_scan == SDL_SCANCODE_UP)
			ship_navigate(-1);
		else if (newkey && lastkey_scan == SDL_SCANCODE_DOWN)
			ship_navigate(1);

		if (newkey && (lastkey_scan == SDL_SCANCODE_RETURN || lastkey_scan == SDL_SCANCODE_SPACE))
			ship_confirm();		
	}

	int ypos = 38;
	for (int i = customShipScroll; i < customShipScroll + 14; ++i)
	{
		const int shade = subMenuSelections[currentSubMenu] == i ? 15 : 28;

		if (!CustomShip_Exists(i))
			break;

		if (i == (signed)currentCustomShip)
		{
			fill_rectangle_xy(VGAScreen, 164, ypos+2, 300, ypos+6, 227);
			blit_sprite2(VGAScreen, 298, ypos-3, shopSpriteSheet, 247);
		}

		const char *shipName = CustomShip_GetName(i);
		JE_textShade(VGAScreen, 171, ypos, shipName, shade / 16, shade % 16 - 8, DARKEN);
		ypos += 9;
	}

	draw_font_hv_shadow(VGAScreen, 304, 38 + 128, "Done", SMALL_FONT_SHAPES,
		right_aligned, 15, -3 + (subMenuSelections[currentSubMenu] < 0 ? 2 : 0), false, 2);

	// ----- Help Text --------------------------------------------------------
	const char *helpText;
	if (subMenuSelections[currentSubMenu] < 0)
		helpText = "Go back to the previous menu.";
	else
		helpText = "Select the ship sprite you want to use.";
	JE_outTextAndDarken(VGAScreen, 10, 187, helpText, 14, 1, TINY_FONT);
}

static void submenuOptCustomShip_Init(void)
{
	const size_t customShipTotal = CustomShip_Count();

	// Scroll the ship the player currently is using to the center.
	subMenuSelections[currentSubMenu] = (CustomShip_Exists(currentCustomShip) ? currentCustomShip : 0);
	customShipScroll = subMenuSelections[currentSubMenu] - 7;

	if (customShipScroll < 0)
		customShipScroll = 0;
	else if (customShipScroll > (signed)customShipTotal - 14)
		customShipScroll = (signed)customShipTotal - 14;
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
	mousetargets_t optionTargets = {5, {
		{164,  38, JE_textWidth("Music", SMALL_FONT_SHAPES), 12},
		{164,  54, JE_textWidth("Sound", SMALL_FONT_SHAPES), 12},
		{164,  70, JE_textWidth("DeathLink", SMALL_FONT_SHAPES), 12},
		{164,  86, JE_textWidth("Change Sprite...", SMALL_FONT_SHAPES), 12},
		{164, 150, JE_textWidth("Done", SMALL_FONT_SHAPES), 12}
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
				if (!useCustomShips)
				{
					JE_playSampleNum(S_CLINK);
					break;
				}
				nextSubMenu = SUBMENU_OPTIONS_CUSTOMSHIP;
				JE_playSampleNum(S_CLICK);
				break;
			case 4:
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

	defaultMenuOptionDraw("Music",            38,       false,                     SELECTED(0));
	defaultMenuOptionDraw("Sound",            38 +  16, false,                     SELECTED(1));
	defaultMenuOptionDraw("DeathLink",        38 +  32, !APSeedSettings.DeathLink, SELECTED(2));
	defaultMenuOptionDraw("Change Sprite...", 38 +  48, !useCustomShips,           SELECTED(3));
	defaultMenuOptionDraw("Done",             38 + 112, false,                     SELECTED(4));


	// ----- Help Text --------------------------------------------------------
	const char *helpText;
	switch (GETSELECTED())
	{
		case 0:  // fall through
		case 1:  helpText = "Use left/right to adjust volume, select for on/off."; break;
		case 2:  helpText = "Enable or disable DeathLink."; break;
		case 3:  helpText = "Change the sprite that your ship uses."; break;
		default: helpText = "Go back to the previous menu."; break;
	}
	JE_outTextAndDarken(VGAScreen, 10, 187, helpText, 14, 1, TINY_FONT);
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

static void sidebarShipSprite(void)
{
	JE_barDrawShadow(VGAScreen,  42, 152, 2, 14, APStats.ArmorLevel  - 8, 2, 13); // Armor
	JE_barDrawShadow(VGAScreen, 104, 152, 2, 14, APStats.ShieldLevel - 8, 2, 13); // Shield
	if (APItemChoices.Sidekick[0].Item)
		sprites_blitArchipelagoItem(VGAScreen,   3, 84, APItemChoices.Sidekick[0].Item);
	if (APItemChoices.Sidekick[1].Item)
		sprites_blitArchipelagoItem(VGAScreen, 129, 84, APItemChoices.Sidekick[1].Item);

	int customShipID = subMenuSelections[SUBMENU_OPTIONS_CUSTOMSHIP];
	if (customShipID < 0)
		customShipID = currentCustomShip;

	Sprite2_array *customShipGr = customShipGr = CustomShip_GetSprite(customShipID);
	if (customShipGr)
	{
		blit_sprite2x2(VGAScreen, 66-32, 84,    *customShipGr, 1);
		blit_sprite2x2(VGAScreen, 66-16, 84-28, *customShipGr, 3);
		blit_sprite2x2(VGAScreen, 66,    84-56, *customShipGr, 5);
		blit_sprite2x2(VGAScreen, 66+16, 84-28, *customShipGr, 7);
		blit_sprite2x2(VGAScreen, 66+32, 84,    *customShipGr, 9);
	}

	JE_textShade(VGAScreen, 24, 118, CustomShip_GetName(customShipID), 15, 6, DARKEN);
	JE_textShade(VGAScreen, 24, 128, "Author:", 15, 4, DARKEN);
	JE_textShade(VGAScreen, 24, 136, CustomShip_GetAuthor(customShipID), 15, 6, DARKEN);
}

static void sidebarArchipelagoInfo(void)
{
	JE_barDrawShadow(VGAScreen,  42, 152, 2, 14, APStats.ArmorLevel  - 8, 2, 13); // Armor
	JE_barDrawShadow(VGAScreen, 104, 152, 2, 14, APStats.ShieldLevel - 8, 2, 13); // Shield

	// Draw player ship
	if (useCustomShips)
	{
		Sprite2_array *customShipGr = CustomShip_GetSprite(currentCustomShip);
		if (customShipGr)
			blit_sprite2x2(VGAScreen, 66, 84, *customShipGr, 5);
	}
	else // Fallback to USP Talon
		blit_sprite2x2(VGAScreen, 66, 84, spriteSheet9, 233);


	if (APItemChoices.FrontPort.Item)
	{
		const int apItem = APItemChoices.FrontPort.Item;
		sprites_blitArchipelagoItem(VGAScreen, 108, 30, apItem);
		JE_textShade(VGAScreen, 28, 38, "Front Weapon:", 15, 4, DARKEN);
		JE_textShade(VGAScreen, 28, 46, getCleanName(apItem), 15, 6, DARKEN);
	}
	if (APItemChoices.RearPort.Item)
	{
		const int apItem = APItemChoices.RearPort.Item;
		sprites_blitArchipelagoItem(VGAScreen, 108, 54, apItem);
		JE_textShade(VGAScreen, 28, 62, "Rear Weapon:", 15, 4, DARKEN);
		JE_textShade(VGAScreen, 28, 70, getCleanName(apItem), 15, 6, DARKEN);
	}
	if (APItemChoices.Sidekick[0].Item)
		sprites_blitArchipelagoItem(VGAScreen,   3, 84, APItemChoices.Sidekick[0].Item);
	if (APItemChoices.Sidekick[1].Item)
		sprites_blitArchipelagoItem(VGAScreen, 129, 84, APItemChoices.Sidekick[1].Item);

	if (APStats.GeneratorLevel > 0 && APStats.GeneratorLevel <= 6)
	{
		// We don't use sprites_blitArchipelagoItem for these,
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
	blit_sprite2x2(VGAScreen, player[0].x - 5, player[0].y - 7, *shipGrPtr, shipGr);

	// Show weapon mode type
	blit_sprite(VGAScreenSeg, 3, 56, OPTION_SHAPES, (player[0].weapon_mode == 1) ? 18 : 19);
	blit_sprite(VGAScreenSeg, 3, 64, OPTION_SHAPES, (player[0].weapon_mode == 1) ? 19 : 18);

#ifdef ITEM_CHEATS
	if (newkey && lastkey_scan == SDL_SCANCODE_F2 && APStats.GeneratorLevel > 1)
		--APStats.GeneratorLevel;
	if (newkey && lastkey_scan == SDL_SCANCODE_F3 && APStats.GeneratorLevel < 6)
		++APStats.GeneratorLevel;
#endif
}

// ------------------------------------------------------------------
// Chat Box
// ------------------------------------------------------------------

static textinput_t chatTextEntry = { /* Text Color */ 232, /* Error Color */ 72, /* Max Length */ 255};

static void apmenu_chatbox(void)
{
	Uint8 scrollbackPos = 1;
	int lines = 1;

	JE_playSampleNum(S_SPRING);

	// Save old screen contents in VGAScreen2
	// (we'll need to restore VGAScreen2 to base menu background after we're done)
	memcpy(VGAScreen2->pixels, VGAScreen->pixels, VGAScreen->pitch * VGAScreen->h);

	JE_barShade(VGAScreen, 0, 172, 319, 175);
	pcxload_renderChatBox(VGAScreen, 174, 0, 26);
	JE_mouseStart();
	JE_showVGA();
	JE_mouseReplace();
	SDL_Delay(16);

	// Animate upwards
	for (; lines <= 16; lines++)
	{
		JE_barShade(VGAScreen, 0, 172 - (lines * 9), 319, 175 - (lines * 9));
		pcxload_renderChatBox(VGAScreen, 174 - (lines * 9), 0, 6);
		fill_rectangle_xy(VGAScreen, 0, 180 - (lines * 9), 319, 180, 228);
		fill_rectangle_xy(VGAScreen, 307, 180 - (lines * 9), 310, 180, 227);
		apmsg_drawScrollBack(scrollbackPos, lines);

		JE_mouseStart();
		JE_showVGA();
		JE_mouseReplace();
		SDL_Delay(16);
	}
	lines = 16;

	chatTextEntry.flashTime = 0;
	service_SDL_events(true); // Get fresh events
	while (true)
	{
		service_SDL_events(false);

		if ((newkey && lastkey_scan == SDL_SCANCODE_PAGEUP) || mousewheel == MOUSEWHEEL_UP)
			++scrollbackPos;
		else if ((newkey && lastkey_scan == SDL_SCANCODE_PAGEDOWN) || mousewheel == MOUSEWHEEL_DOWN)
			--scrollbackPos;
		else if (newkey && lastkey_scan == SDL_SCANCODE_HOME)
			scrollbackPos = 254 - lines;
		else if (newkey && lastkey_scan == SDL_SCANCODE_END)
			scrollbackPos = 1;

		if (scrollbackPos >= 255 - lines)
			scrollbackPos = 254 - lines;
		else if (scrollbackPos == 0)
			scrollbackPos = 1;

		if (newkey && (lastkey_scan == SDL_SCANCODE_TAB || lastkey_scan == SDL_SCANCODE_ESCAPE))
			break;

		if (apmenu_textInputCapture(&chatTextEntry) && chatTextEntry.textLen)
		{
			Archipelago_ChatMessage(chatTextEntry.text);
			apmenu_textInputUpdate(&chatTextEntry, "");
			scrollbackPos = 1;
		}

		fill_rectangle_xy(VGAScreen, 0, 180 - (lines * 9), 319, 180, 228);
		fill_rectangle_xy(VGAScreen, 307, 180 - (lines * 9), 310, 180, 227);
		apmsg_drawScrollBack(scrollbackPos, lines);
		blit_sprite2(VGAScreen, 303, 160 - (scrollbackPos >> 1), shopSpriteSheet, 247);

		fill_rectangle_xy(VGAScreen, 9, 187, 267, 187+9, 228);
		apmenu_textInputRender(VGAScreen, &chatTextEntry, 10, 187, 257);

		// Draw online/offline marker
		fill_rectangle_xy(VGAScreen, 282, 187, 312, 187+9, 228);
		if (Archipelago_ConnectionStatus() == APCONN_READY)
			JE_outTextAndDarken(VGAScreen, 284, 187, "Online", 12, 3, TINY_FONT);
		else
			JE_outTextAndDarken(VGAScreen, 282, 187, "Offline", 4, 3, TINY_FONT);

		service_SDL_events(true);
		JE_mouseStart();
		JE_showVGA();
		JE_mouseReplace();
		SDL_Delay(16);
	}

	JE_playSampleNum(S_SPRING);

	// Animate downwards
	for (lines = 15; lines >= 0; lines--)
	{
		memcpy(VGAScreen->pixels, VGAScreen2->pixels, VGAScreen->pitch * VGAScreen->h);
		pcxload_renderChatBox(VGAScreen, 174, 0, 26);
		JE_barShade(VGAScreen, 0, 172 - (lines * 9), 319, 175 - (lines * 9));
		pcxload_renderChatBox(VGAScreen, 174 - (lines * 9), 0, 6);
		fill_rectangle_xy(VGAScreen, 0, 180 - (lines * 9), 319, 180, 228);
		fill_rectangle_xy(VGAScreen, 307, 180 - (lines * 9), 310, 180, 227);
		if (lines)
			apmsg_drawScrollBack(scrollbackPos, lines);

		JE_mouseStart();
		JE_showVGA();
		JE_mouseReplace();
		SDL_Delay(16);
	}

	service_SDL_events(true); // Get fresh events upon exiting
	JE_loadPic(VGAScreen2, 1, false);
	pcxload_renderChatBox(VGAScreen2, 185, 11, 15);
}

// ----------------------------------------------------------------------------

int apmenu_itemScreen(void)
{
#if SDL_VERSION_ATLEAST(2, 0, 18)
	Uint64 menuStartTime = SDL_GetTicks64();
#else
	Uint32 menuStartTime = SDL_GetTicks();
#endif

	bool updatePalette = true;

	play_song(DEFAULT_SONG_BUY);
	pcxload_prepChatBox();

	VGAScreen = VGAScreenSeg;

	JE_loadPic(VGAScreen2, 1, false);
	pcxload_renderChatBox(VGAScreen2, 185, 11, 15);
	memcpy(VGAScreen->pixels, VGAScreen2->pixels, VGAScreen->pitch * VGAScreen->h);

	JE_showVGA();

	if (shopSpriteSheet.data == NULL)
		JE_loadCompShapes(&shopSpriteSheet, '1');

	currentSubMenu = SUBMENU_MAIN;
	nextSubMenu = SUBMENU_NONE;
	itemSubMenus[SUBMENU_MAIN].initFunc();

	// Clear chat box when entering menu
	apmenu_textInputUpdate(&chatTextEntry, "");

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
		snprintf(string_buffer, sizeof(string_buffer), "%llu", (unsigned long long)(APStats.Cash - tempMoneySub));
		JE_textShade(VGAScreen, 65, 173, string_buffer, 1, 6, DARKEN);

		if (newkey && lastkey_scan == SDL_SCANCODE_TAB && currentSubMenu == SUBMENU_MAIN)
			apmenu_chatbox(); // Blocks until exited

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

#if SDL_VERSION_ATLEAST(2, 0, 18)
				APPlayData.TimeInMenu += SDL_GetTicks64() - menuStartTime;
#else
				APPlayData.TimeInMenu += SDL_GetTicks() - menuStartTime;
#endif
				return -1;

			case SUBMENU_LEVEL:
				itemSubMenus[currentSubMenu].exitFunc();
				fade_black(15);

#if SDL_VERSION_ATLEAST(2, 0, 18)
				APPlayData.TimeInMenu += SDL_GetTicks64() - menuStartTime;
#else
				APPlayData.TimeInMenu += SDL_GetTicks() - menuStartTime;
#endif
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
