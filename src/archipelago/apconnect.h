
#include "SDL_types.h"

#include <stdio.h>

typedef enum {
	APCONN_NOT_CONNECTED = 0,
	APCONN_CONNECTING,
	APCONN_TENTATIVE,
	APCONN_READY
} archipelago_connectionstat_t;

typedef enum {
	APSFX_RECEIVE_ITEM = 1,
	APSFX_RECEIVE_MONEY,
	APSFX_CHAT,
} archipelago_sound_t;

typedef struct {
	int PlayEpisodes;
	int GoalEpisodes;
	int Difficulty;

	int ShopMenu;
	bool SpecialMenu;
	bool HardContact;
	bool ExcessArmor;
	bool TwiddleInputs;
	bool ArchipelagoRadar;
	bool Christmas;
	bool DeathLink;
} archipelago_settings_t;

extern archipelago_settings_t APSeedSettings;

typedef struct {
	bool EnableDeathLink;
} archipelago_options_t;

extern archipelago_options_t APOptions;

void Archipelago_GenerateUUID(void);
void Archipelago_SetUUID(const char *uuid);
const char *Archipelago_GetUUID(void);

void Archipelago_Save(void);

// ----------------------------------------------------------------------------
// Local Game
// ----------------------------------------------------------------------------

bool Archipelago_StartLocalGame(FILE *file);

// ----------------------------------------------------------------------------
// Remote Game
// ----------------------------------------------------------------------------

void Archipelago_SetDefaultConnectionDetails(const char *address);
void Archipelago_SetDefaultConnectionPassword(const char *connectionPassword);

void Archipelago_Connect(void);
void Archipelago_Poll(void);
void Archipelago_Disconnect(void);

archipelago_connectionstat_t Archipelago_ConnectionStatus(void);
const char* Archipelago_GetConnectionError(void);

// ----------------------------------------------------------------------------
// Items and Checks
// ----------------------------------------------------------------------------

#define AP_BITSET(var, bit) ((var) & (1 << (bit)))

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
} apitem_t;

// Player's item choices (see above)
typedef struct {
	apweapon_t FrontPort;
	apweapon_t RearPort;
	apweapon_t Special;
	apweapon_t Sidekick[2];
} apitemchoice_t;

// Stats of the player's game / ship
typedef struct {
	Uint32 Clears[5]; // One bit per level, per episode

	Uint8  PowerMaxLevel; // 0 - 10; +1 per "Maximum Power Up"
	Uint8  GeneratorLevel; // 1 - 6; +1 per "Progressive Generator"

	Uint8  ArmorLevel; // 10 - 28; +2 per "Armor Up"
	Uint8  ShieldLevel; // 10 - 28; +2 per "Shield Up"
	bool   SolarShield; // True if obtained

	Uint8  QueuedSuperBombs; // Will be given to the player next available opportunity
	Uint64 Cash; // Definitive cash total for the player
} apstat_t;

// Request C side to update / redraw
typedef struct {
	Uint8 Armor;
	Uint8 Shield;
} apupdatereq_t;

extern apitem_t APItems;
extern apstat_t APStats;
extern apitemchoice_t APItemChoices;

extern apupdatereq_t APUpdateRequest;

void Archipelago_SendCheck(int checkID);
bool Archipelago_WasChecked(int checkID);
bool Archipelago_CheckHasProgression(int checkID);

int Archipelago_GetRegionCheckCount(int firstCheckID);
Uint32 Archipelago_GetRegionWasCheckedList(int firstCheckID);

// ----------------------------------------------------------------------------
// Shops
// ----------------------------------------------------------------------------

typedef struct {
	Uint16 LocationID;
	Uint16 Cost;
	Uint16 Icon;
	char   ItemName[40];
	//char   PlayerName[40];
} shopitem_t;

// Returns count of items contained in *items
int Archipelago_GetShopItems(int shopStartID, shopitem_t **items);

// Scouts for shop items (only relevant for remote game)
void Archipelago_ScoutShopItems(int shopStartID);

unsigned int Archipelago_GetUpgradeCost(int itemID, int powerLevel);
unsigned int Archipelago_GetTotalUpgradeCost(int itemID, int powerLevel);

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
