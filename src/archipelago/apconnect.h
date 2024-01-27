
#include "SDL_types.h"

#include <stdio.h>

typedef enum {
	APCONN_NOT_CONNECTED = 0,
	APCONN_CONNECTING,
	APCONN_TENTATIVE,
	APCONN_READY
} archipelago_connectionstat_t;

typedef struct {
	int goal;
	bool shops_exist;

	int game_difficulty;
	bool hard_contact;
	bool excess_armor;
	bool show_twiddle_inputs;

	bool archipelago_radar;
	bool christmas;

	bool deathlink;
} archipelago_settings_t;

extern archipelago_settings_t ArchipelagoOpts;

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
// Checks
// ----------------------------------------------------------------------------

typedef struct {
	Uint64 FrontPorts; // One bit per weapon, 1 enables
	Uint64 RearPorts; // One bit per weapon, 1 enables
	Uint64 Specials; // One bit per weapon, 1 enables
	Uint32 Levels[5]; // One bit per level, per episode
	Uint8  Sidekicks[36]; // Total number obtained, per sidekick
} apitem_t;

typedef struct {
	Uint8  PowerMaxLevel; // 0 - 10; +1 per "Maximum Power Up"
	Uint8  GeneratorLevel; // 0 - 5; +1 per "Progressive Generator"

	Uint8  ArmorLevel; // 10 - 28; +2 per "Armor Up"
	Uint8  ShieldLevel; // 10 - 28; +2 per "Shield Up"
	bool   SolarShield; // True if obtained

	Uint8  QueuedSuperBombs; // Will be given to the player next available opportunity
	Uint32 Cash;
} apstat_t;

extern apitem_t APItems;
extern apstat_t APStats;

void Archipelago_SendCheck(int checkID);
bool Archipelago_WasChecked(int checkID);
bool Archipelago_CheckHasProgression(int checkID);

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

extern bool apDeathLinkReceived;

void Archipelago_SendDeathLink(damagetype_t source);
