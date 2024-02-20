/*
 * OpenTyrian: A modern cross-platform port of Tyrian
 * Copyright (C) 2007-2009  The OpenTyrian Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#ifndef MAININT_H
#define MAININT_H

#include "config.h"
#include "opentyr.h"
#include "palette.h"
#include "player.h"
#include "sprite.h"
#include "varz.h"

extern bool button[4]; // fire, left fire, right fire, mode swap

extern JE_shortint constantLastX;
extern JE_word textErase;
extern JE_boolean useLastBank;

extern bool pause_pressed, ingamemenu_pressed;

void JE_initPlayerData(void);

void JE_gammaCorrect_func(JE_byte *col, JE_real r);
void JE_gammaCorrect(Palette *colorBuffer, JE_byte gamma);
JE_boolean JE_gammaCheck(void);

void JE_doInGameSetup(void);
JE_boolean JE_inGameSetup(void);
void JE_inGameHelp(void);

int mainint_loadNextDemo(void);
bool replay_demo_keys(void);

void JE_SFCodes(JE_byte playerNum_, JE_integer PX_, JE_integer PY_, JE_integer mouseX_, JE_integer mouseY_);


void JE_outCharGlow(JE_word x, JE_word y, const char *s);

void JE_playCredits(void);

void JE_inGameDisplays(void);
void JE_mainKeyboardInput(void);
void JE_pauseGame(void);

void JE_playerMovement(Player *this_player, JE_byte inputDevice, JE_byte playerNum, JE_word shipGr, Sprite2_array *shipGrPtr_, JE_word *mouseX, JE_word *mouseY);
void JE_mainGamePlayerFunctions(void);
const char *JE_getName(JE_byte pnum);

void JE_playerCollide(Player *this_player, JE_byte playerNum);

void mainint_handleEnemyFlag(struct JE_SingleEnemyType *e);

#endif /* MAININT_H */
