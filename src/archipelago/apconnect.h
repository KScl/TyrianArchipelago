#ifndef CPP_APCONNECT_H
#define CPP_APCONNECT_H

#include "SDL_types.h"

#include <stdio.h>

typedef enum {
	APCONN_NOT_CONNECTED = 0,
	APCONN_CONNECTING,
	APCONN_NEED_PASSWORD,
	APCONN_TENTATIVE,
	APCONN_READY,
	APCONN_FATAL_ERROR
} archipelago_connectionstat_t;

typedef enum {
	APSFX_RECEIVE_ITEM = 1,
	APSFX_RECEIVE_MONEY,
	APSFX_CHAT,
} archipelago_sound_t;

typedef enum {
	SHOP_MODE_NONE = 0,
	SHOP_MODE_STANDARD,
	SHOP_MODE_HIDDEN,
	SHOP_MODE_SHOPS_ONLY,
} archipelago_shopmode_t;

typedef struct {
	bool Tyrian2000Mode;

	unsigned int PlayEpisodes; // One bit per episode
	unsigned int GoalEpisodes; // e.g. 0b11111 for all 5 episodes
	int Difficulty;
	int DataCubesNeeded; // 0 if off, otherwise count of data cubes to open goal levels

	int ShopMode;
	bool SpecialMenu;
	bool HardContact;
	bool ExcessArmor;

	int ForceGameSpeed;
	bool Christmas;
	bool DeathLink;
} archipelago_settings_t;

extern archipelago_settings_t APSeedSettings;

typedef struct {
	bool ArchipelagoRadar;
	bool EnableDeathLink;

	bool PracticeMode; // aka Debug Mode, possibly used for things later
} archipelago_options_t;

extern archipelago_options_t APOptions;

void Archipelago_GenerateUUID(void);
void Archipelago_SetUUID(const char *uuid);
const char *Archipelago_GetUUID(void);

void Archipelago_Save(void);

// ----------------------------------------------------------------------------
// Local Game
// ----------------------------------------------------------------------------

// These return NULL on success or error string on failure.
const char *Archipelago_StartDebugGame(void);
const char *Archipelago_StartLocalGame(FILE *file);

// ----------------------------------------------------------------------------
// Remote Game
// ----------------------------------------------------------------------------

void Archipelago_SetPassword(const char *password);
void Archipelago_Connect(const char *slot_name, const char *address, const char *password);
void Archipelago_Poll(void);
void Archipelago_Disconnect(void);

archipelago_connectionstat_t Archipelago_ConnectionStatus(void);
const char* Archipelago_GetConnectionError(void);

void Archipelago_ChatMessage(const char *userMessage);
void Archipelago_GoalComplete(void);

bool Archipelago_IsRacing(void);

// ----------------------------------------------------------------------------
// Items and Checks
// ----------------------------------------------------------------------------

#define AP_BITSET(var, bit) ((var) & (1ull << (bit)))

typedef struct {
	Uint16 Item;
	Uint8  PowerLevel;
} apweapon_t;

// Items that have been collected, that the player can choose from
typedef struct {
	Uint64 FrontPorts; // One bit per weapon, 1 enables
	Uint64 RearPorts; // One bit per weapon, 1 enables
	Uint64 Specials; // One bit per weapon, 1 enables
	Uint32 Levels[5]; // One bit per level, per episode
	Uint8  Sidekicks[36]; // Total number obtained, per sidekick
	Uint8  BonusGames; // One bit per bonus game
} apitem_t;

// Player's item choices (see above)
typedef struct {
	apweapon_t FrontPort;
	apweapon_t RearPort;
	apweapon_t Special;
	apweapon_t Sidekick[2];
	bool       RearMode2;
} apitemchoice_t;

// Stats of the player's game / ship
typedef struct {
	Uint32 Clears[5]; // One bit per level, per episode
	Uint8  RestGoalEpisodes; // See APSeedSettings.GoalEpisodes - when 0, game is complete

	Uint16 DataCubes; // For data cube goals, total collected
	bool   CubeRewardGiven; // True if goal levels have been given to the player

	Uint8  PowerMaxLevel; // 0 - 10; +1 per "Maximum Power Up"
	Uint8  GeneratorLevel; // 1 - 6; +1 per "Progressive Generator"

	Uint8  ArmorLevel; // 10 - 28; +2 per "Armor Up"
	Uint8  ShieldLevel; // 10 - 28; +2 per "Shield Up"
	bool   SolarShield; // True if obtained

	Uint8  QueuedSuperBombs; // Will be given to the player next available opportunity
	Uint64 Cash; // Definitive cash total for the player
} apstat_t;

// Player's time in game/deaths/etc
typedef struct {
	Uint64 TimeInLevel; // Tracked with SDL_GetTicks/SDL_GetTicks64
	Uint64 TimeInMenu; // Tracked with SDL_GetTicks/SDL_GetTicks64
	Uint64 TimeInBonus; // Tracked with SDL_GetTicks/SDL_GetTicks64

	Uint16 Deaths;
	Uint16 ExitedLevels;
} applaydata_t;

// Request C side to update / redraw
typedef struct {
	Uint8 Armor;
	Uint8 Shield;
} apupdatereq_t;

extern apitem_t APItems;
extern apstat_t APStats;
extern applaydata_t APPlayData;
extern apitemchoice_t APItemChoices;

extern apupdatereq_t APUpdateRequest;

void Archipelago_SendCheck(int checkID);
bool Archipelago_WasChecked(int checkID);
bool Archipelago_CheckHasProgression(int checkID);

int Archipelago_GetRegionCheckCount(int firstCheckID);
Uint32 Archipelago_GetRegionWasCheckedList(int firstCheckID);
int Archipelago_GetTotalCheckCount(void);
int Archipelago_GetTotalAnyoneCheckedCount(void);
int Archipelago_GetTotalWeCheckedCount(void);

// ----------------------------------------------------------------------------
// Twiddles / SF Codes
// ----------------------------------------------------------------------------

typedef struct {
	char Name[32]; // Displayed in menus
	Uint8 Command[8]; // Max 7 inputs, end with (100 + special_id)
	Uint8 Cost; // Uses same formula as original game
} aptwiddle_t;

typedef struct {
	int Count;
	aptwiddle_t Code[3];
} aptwiddlelist_t;

extern aptwiddlelist_t APTwiddles;

// ----------------------------------------------------------------------------
// Shops
// ----------------------------------------------------------------------------

typedef struct {
	Uint16 LocationID;
	Uint16 Cost;
	Uint16 Icon;
	char   ItemName[128];
	char   PlayerName[40]; // Extra room for aliases
} shopitem_t;

// Returns count of items contained in *items
int Archipelago_GetShopItems(int shopStartID, shopitem_t **items);

// Scouts for shop items (only relevant for remote game)
void Archipelago_ScoutShopItems(int shopStartID);

unsigned int Archipelago_GetUpgradeCost(Uint16 localItemID, int powerLevel);
unsigned int Archipelago_GetTotalUpgradeCost(Uint16 localItemID, int powerLevel);

// ----------------------------------------------------------------------------
// DeathLink
// ----------------------------------------------------------------------------

#ifndef PLAYER_H
typedef enum { // redefinition
	DAMAGE_DEATHLINK = 0,
	DAMAGE_BULLET,
	DAMAGE_CONTACT,
	COUNT_DEATH
} damagetype_t;
#endif

extern bool APDeathLinkReceived;

void Archipelago_SendDeathLink(damagetype_t source);
void Archipelago_UpdateDeathLinkState(void);

#endif
