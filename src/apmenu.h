#ifndef APMENU_H
#define APMENU_H

// Basic init steps after an Archipelago game is loaded CPP side
void apmenu_initArchipelagoGame(void);

// Pre-game menus
bool ap_titleScreen(void);
bool ap_connectScreen(void);

// In-game menus
int apmenu_itemScreen(void);

// Post-game stats screen
void apmenu_RandomizerStats(void);

#endif
