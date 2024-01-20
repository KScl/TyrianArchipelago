
#include "SDL_types.h"

typedef enum {
	APCONN_NOT_CONNECTED = 0,
	APCONN_CONNECTING,
	APCONN_TENTATIVE,
	APCONN_READY
} archipelago_connectionstat_t;

typedef struct {
	bool tyrian_2000_support_wanted;
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

void Archipelago_SetDefaultConnectionDetails(const char *address);
void Archipelago_SetDefaultConnectionPassword(const char *connectionPassword);

void Archipelago_Connect(void);
void Archipelago_Poll(void);
void Archipelago_Disconnect(void);
void Archipelago_DisconnectWithError(const char *errorMsg);

archipelago_connectionstat_t Archipelago_ConnectionStatus(void);
const char* Archipelago_GetConnectionError(void);

// ----------------------------------------------------------------------------
// Checks
// ----------------------------------------------------------------------------

void Archipelago_SendCheck(int checkID);
bool Archipelago_WasChecked(int checkID);

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
