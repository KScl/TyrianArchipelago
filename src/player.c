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

#include "nortsong.h"
#include "player.h"
#include "varz.h"
#include "video.h"

#include "archipelago/apconnect.h"

Player player[2];

void calc_purple_balls_needed(Player *this_player)
{
	static const uint purple_balls_required[12] = { 1, 1, 2, 4, 8, 12, 16, 20, 25, 30, 40, 50 };
	
	this_player->purple_balls_needed = purple_balls_required[*this_player->lives];
}

bool power_up_weapon(Player *this_player, uint port)
{
	const bool can_power_up = this_player->items.weapon[port].id != 0 &&  // not None
	                          this_player->items.weapon[port].power < 11; // not at max power
	if (can_power_up)
	{
		++this_player->items.weapon[port].power;
		shotMultiPos[port] = 0; // TODO: should be part of Player structure
		
		calc_purple_balls_needed(this_player);
	}
	else  // cash consolation prize
	{
		this_player->cash += 1000;
	}
	
	return can_power_up;
}

void handle_got_purple_ball(Player *this_player)
{
	if (this_player->purple_balls_needed > 1)
		--this_player->purple_balls_needed;
	else
		power_up_weapon(this_player, this_player->is_dragonwing ? REAR_WEAPON : FRONT_WEAPON);
}

// ----------------------------------------------------------------------------

Uint8 player_takeDamage(Player *this_player, Uint8 damageAmount, damagetype_t damageType )
{
	// used to force velocity on the player if they take
	// a hit to their armor in certain situations
	int takenArmorDamage = 0;

	if (!this_player->is_alive)
		return 0; // shouldn't happen, but just making sure

	soundQueue[7] = S_SHIELD_HIT;

	if (damageAmount <= this_player->shield)
	{
		this_player->shield -= damageAmount;

		// Shield burst
		JE_setupExplosion(this_player->x - 17, this_player->y - 12, 0, 14, false, !twoPlayerMode);
		JE_setupExplosion(this_player->x - 5 , this_player->y - 12, 0, 15, false, !twoPlayerMode);
		JE_setupExplosion(this_player->x + 7 , this_player->y - 12, 0, 16, false, !twoPlayerMode);
		JE_setupExplosion(this_player->x + 19, this_player->y - 12, 0, 17, false, !twoPlayerMode);

		JE_setupExplosion(this_player->x - 17, this_player->y + 2, 0,  18, false, !twoPlayerMode);
		JE_setupExplosion(this_player->x + 19, this_player->y + 2, 0,  19, false, !twoPlayerMode);

		JE_setupExplosion(this_player->x - 17, this_player->y + 16, 0, 20, false, !twoPlayerMode);
		JE_setupExplosion(this_player->x - 5 , this_player->y + 16, 0, 21, false, !twoPlayerMode);
		JE_setupExplosion(this_player->x + 7 , this_player->y + 16, 0, 22, false, !twoPlayerMode);
	}
	else
	{
		// Through shields, now armor
		takenArmorDamage = damageAmount;
		damageAmount -= this_player->shield;
		this_player->shield = 0;

		if (damageAmount <= this_player->armor)
		{
			this_player->armor -= damageAmount;
			soundQueue[7] = S_HULL_HIT;
		}
		else
		{
			// Through armor, player is dead
			Archipelago_SendDeathLink(damageType);

			this_player->armor = 0;
			levelTimer = false;
			this_player->is_alive = false;
			this_player->exploding_ticks = 60;
			levelEnd = 40;
			tempVolume = tyrMusicVolume;
			soundQueue[1] = S_EXPLOSION_22;
		}
	}

	JE_wipeShieldArmorBars();
	VGAScreen = VGAScreenSeg; /* side-effect of game_screen */
	JE_drawShield();
	JE_drawArmor();
	VGAScreen = game_screen; /* side-effect of game_screen */

	return takenArmorDamage;
}

// Handle receiving a DeathLink request from someone else.
void player_handleDeathLink(Player *this_player)
{
	if (!this_player->is_alive || !apDeathLinkReceived)
		return;

	this_player->shield = 0;
	this_player->invulnerable_ticks = 0;
	player_takeDamage(this_player, 1, DAMAGE_DEATHLINK);
}

void player_debugCauseDeathLink(void)
{
	apDeathLinkReceived = true;
}

void player_resetDeathLink(void)
{
	apDeathLinkReceived = false;
}
