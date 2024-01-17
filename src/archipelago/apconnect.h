
#include "SDL_types.h"

typedef enum {
	APCONN_NOT_CONNECTED = 0,
	APCONN_CONNECTING,
	APCONN_TENTATIVE,
	APCONN_READY
} archipelago_connectionstat_t;


typedef struct {
	// We don't need to know shop_mode, price_scale, money_pool_scale, or base_weapon_costs.

	// We only need to know if specials are item pickups or not.
	bool specials_are_items;

	bool show_twiddle_inputs;
	Uint8 difficulty;
	bool contact_bypasses_shields;
	bool allow_excess_armor;

	bool christmas;

	bool deathlink;
} archipelago_settings_t;

extern archipelago_settings_t ArchipelagoOpts;

void Archipelago_SetDefaultConnectionDetails(const char *address);
void Archipelago_SetDefaultConnectionPassword(const char *connectionPassword);

void Archipelago_Connect(void);
void Archipelago_Poll(void);
void Archipelago_Disconnect(void);

archipelago_connectionstat_t Archipelago_ConnectionStatus(void);
const char* Archipelago_GetConnectionError(void);

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
