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
#include "nortvars.h"
#include "player.h"
#include "varz.h"
#include "vga256d.h"
#include "video.h"

#include "archipelago/apitems.h"
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

void player_boostArmor(Player *this_player, int amount)
{
	if (twoPlayerLinked)
	{
		// share the armor evenly between linked players
		for (uint i = 0; i < COUNTOF(player); ++i)
		{
			player[i].armor += amount / COUNTOF(player);
			if (player[i].armor > 28)
				player[i].armor = 28;

			if (!APSeedSettings.ExcessArmor && player[i].armor > APStats.ArmorLevel)
				player[i].armor = APStats.ArmorLevel;
		}
	}
	else
	{
		this_player->armor += amount;
		if (this_player->armor > 28)
			this_player->armor = 28;

		if (!APSeedSettings.ExcessArmor && this_player->armor > APStats.ArmorLevel)
			this_player->armor = APStats.ArmorLevel;
	}
	player_drawArmor();
}

// ---------------------------------------------------------------------------

void player_updateItemChoices(void)
{
	// Default everything to "none"
	player[0].items.weapon[FRONT_WEAPON].id = 0;
	player[0].items.weapon[FRONT_WEAPON].power = 1;
	player[0].items.weapon[REAR_WEAPON].id = 0;
	player[0].items.weapon[REAR_WEAPON].power = 1;
	player[0].items.special = 0;
	player[0].items.sidekick[LEFT_SIDEKICK] = 0;
	player[0].items.sidekick[RIGHT_SIDEKICK] = 0;

	// Ensure power levels are within range that player has access to
	if (APItemChoices.FrontPort.PowerLevel > APStats.PowerMaxLevel)
		APItemChoices.FrontPort.PowerLevel = APStats.PowerMaxLevel;
	if (APItemChoices.RearPort.PowerLevel > APStats.PowerMaxLevel)
		APItemChoices.RearPort.PowerLevel = APStats.PowerMaxLevel;

	// Assign player's choices
	if (APItemChoices.FrontPort.Item)
	{
		if (APItemChoices.FrontPort.Item >= 500 && APItemChoices.FrontPort.Item < 500 + 64
			&& (APItems.FrontPorts & (1 << (APItemChoices.FrontPort.Item - 500))))
		{
			player[0].items.weapon[FRONT_WEAPON].id = apitems_FrontPorts[APItemChoices.FrontPort.Item - 500];
			player[0].items.weapon[FRONT_WEAPON].power = APItemChoices.FrontPort.PowerLevel + 1;
		}
		else
			APItemChoices.FrontPort.Item = 0;
	}

	if (APItemChoices.RearPort.Item)
	{
		if (APItemChoices.RearPort.Item >= 600 && APItemChoices.RearPort.Item < 600 + 64
			&& (APItems.RearPorts & (1 << (APItemChoices.RearPort.Item - 600))))
		{
			player[0].items.weapon[REAR_WEAPON].id = apitems_RearPorts[APItemChoices.RearPort.Item - 600];
			player[0].items.weapon[REAR_WEAPON].power = APItemChoices.RearPort.PowerLevel + 1;
			if (player_getPortConfigCount(0) == 1)
			{
				player[0].weapon_mode = 1;
				APItemChoices.RearMode2 = false;
			}
			else
				player[0].weapon_mode = APItemChoices.RearMode2 ? 2 : 1;
		}
		else
		{
			APItemChoices.RearPort.Item = 0;
			APItemChoices.RearMode2 = false;
		}
	}

	if (APItemChoices.Special.Item)
	{
		if (APItemChoices.Special.Item >= 700 && APItemChoices.Special.Item < 700 + 64
			&& (APItems.Specials & (1 << (APItemChoices.Special.Item - 700))))
		{
			player[0].items.special = apitems_Specials[APItemChoices.Special.Item - 700];
		}
		else
			APItemChoices.Special.Item = 0;
	}

	for (int i = 0; i < 2; ++i)
	{
		if (!APItemChoices.Sidekick[i].Item)
			continue;

		if (APItemChoices.Sidekick[i].Item >= 800 && APItemChoices.Sidekick[i].Item < 800 + 36
			&& APItems.Sidekicks[APItemChoices.Sidekick[i].Item - 800])
		{
			// Check for right-only sidekicks on left
			if (i == 0 && apitems_RightOnlySidekicks[APItemChoices.Sidekick[i].Item - 800])
				APItemChoices.Sidekick[i].Item = 0;
			else
				player[0].items.sidekick[i] = apitems_Sidekicks[APItemChoices.Sidekick[i].Item - 800];
		} 
		else
			APItemChoices.Sidekick[i].Item = 0;
	}

	// If both sidekicks are the same (and not none), confirm the player has two of that sidekick
	if (APItemChoices.Sidekick[0].Item
		&& APItemChoices.Sidekick[0].Item == APItemChoices.Sidekick[1].Item
		&& APItems.Sidekicks[APItemChoices.Sidekick[0].Item - 800] < 2)
	{
		APItemChoices.Sidekick[1].Item = 0;
		player[0].items.sidekick[RIGHT_SIDEKICK] = 0;
	}

	memset(shotMultiPos, 0, sizeof(shotMultiPos));
}

// Temporarily overrides player items in a given section.
// Used in the upgrade menus to show the results of changing / powering up weapons.
bool player_overrideItemChoice(int section, Uint16 itemID, Uint8 powerLevel)
{
	memset(shotMultiPos, 0, sizeof(shotMultiPos));

	if (itemID == 0) switch (section)
	{
		default:
		case 0: // Front Weapon - Not allowed
			return false;
		case 1: // Rear Weapon - Always allowed
			player[0].items.weapon[REAR_WEAPON].id = 0;
			return true;
		case 2: // Left Sidekick - Always allowed
		case 3: // Right Sidekick - Always allowed
			player[0].items.sidekick[section - 2] = 0;
			return true;
		case 4: // Special - Always allowed
			player[0].items.special = 0;
			return true;
	}

	if (section < 2 && powerLevel > APStats.PowerMaxLevel)
		return false;
	switch (section)
	{
		case 0: // Front Weapon
			if ((itemID >= 500 && itemID < 500 + 64)
				&& (APItems.FrontPorts & (1 << (itemID - 500))))
			{
				player[0].items.weapon[FRONT_WEAPON].id = apitems_FrontPorts[itemID - 500];
				player[0].items.weapon[FRONT_WEAPON].power = powerLevel + 1;
				return true;
			}
			return false;

		case 1: // Rear Weapon
			if ((itemID >= 600 && itemID < 600 + 64)
				&& (APItems.RearPorts & (1 << (itemID - 600))))
			{
				player[0].items.weapon[REAR_WEAPON].id = apitems_RearPorts[itemID - 600];
				player[0].items.weapon[REAR_WEAPON].power = powerLevel + 1;
				if (player_getPortConfigCount(0) == 1)
					player[0].weapon_mode = 1;
				else
					player[0].weapon_mode = APItemChoices.RearMode2 ? 2 : 1;
				return true;
			}
			return false;

		case 2: // Left Sidekick
		case 3: // Right Sidekick
			if ((itemID >= 800 && itemID < 800 + 36)
				&& APItems.Sidekicks[itemID - 800])
			{
				// Check for right-only on left
				if (section == 2 && apitems_RightOnlySidekicks[itemID - 800])
					return false;

				// Check for double sidekick with only one item
				const Uint8 sideID = apitems_Sidekicks[itemID - 800];
				if (player[0].items.sidekick[section == 2 ? RIGHT_SIDEKICK : LEFT_SIDEKICK] == sideID
					&& APItems.Sidekicks[itemID - 800] < 2)
				{
					return false;
				}

				player[0].items.sidekick[section - 2] = sideID;
				return true;
			}
			return false;

		case 4: // Special
			if ((itemID >= 700 && itemID < 700 + 64)
				&& (APItems.Specials & (1 << (itemID - 700))))
			{
				player[0].items.special = apitems_Specials[itemID - 700];
				return true;
			}
			return false;

		default:
			return false;
	}
}

// ---------------------------------------------------------------------------

Uint16 player_getPortConfigCount(uint player_index) // JE_portConfigs
{
	return weaponPort[player[player_index].items.weapon[REAR_WEAPON].id].opnum;
}

void player_drawPortConfigButtons(void) // JE_drawPortConfigButtons
{
	if (twoPlayerMode)
		return;

	if (player[0].weapon_mode == 1)
	{
		blit_sprite(VGAScreenSeg, 285, 44, OPTION_SHAPES, 18);  // lit
		blit_sprite(VGAScreenSeg, 302, 44, OPTION_SHAPES, 19);  // unlit
	}
	else // == 2
	{
		blit_sprite(VGAScreenSeg, 285, 44, OPTION_SHAPES, 19);  // unlit
		blit_sprite(VGAScreenSeg, 302, 44, OPTION_SHAPES, 18);  // lit
	}
}

// ---------------------------------------------------------------------------

void player_wipeShieldArmorBars(void)
{
	if (!twoPlayerMode || galagaMode)
	{
		fill_rectangle_xy(VGAScreenSeg, 270, 137, 278, 194 - player[0].shield * 2, 0);
	}
	else
	{
		fill_rectangle_xy(VGAScreenSeg, 270, 60 - 44, 278, 60, 0);
		fill_rectangle_xy(VGAScreenSeg, 270, 194 - 44, 278, 194, 0);
	}
	if (!twoPlayerMode || galagaMode)
	{
		fill_rectangle_xy(VGAScreenSeg, 307, 137, 315, 194 - player[0].armor * 2, 0);
	}
	else
	{
		fill_rectangle_xy(VGAScreenSeg, 307, 60 - 44, 315, 60, 0);
		fill_rectangle_xy(VGAScreenSeg, 307, 194 - 44, 315, 194, 0);
	}
}

void player_drawShield(void)
{
	if (twoPlayerMode && !galagaMode)
	{
		for (uint i = 0; i < COUNTOF(player); ++i)
			JE_dBar3(VGAScreenSeg, 270, 60 + 134 * i, roundf(player[i].shield * 0.8f), 144);
	}
	else
	{
		JE_dBar3(VGAScreenSeg, 270, 194, player[0].shield, 144);
		if (player[0].shield != APStats.ShieldLevel)
		{
			const uint y = 193 - (APStats.ShieldLevel * 2);
			JE_rectangle(VGAScreenSeg, 270, y, 278, y, 68); /* <MXD> SEGa000 */
		}
	}
}

void player_drawArmor(void)
{
	if (twoPlayerMode && !galagaMode)
	{
		for (uint i = 0; i < COUNTOF(player); ++i)
			JE_dBar3(VGAScreenSeg, 307, 60 + 134 * i, roundf(player[i].armor * 0.8f), 224);
	}
	else
	{
		JE_dBar3(VGAScreenSeg, 307, 194, player[0].armor, 224);
		if (!APSeedSettings.ExcessArmor && player[0].armor < APStats.ArmorLevel)
		{
			const uint y = 193 - (APStats.ArmorLevel * 2);
			JE_rectangle(VGAScreenSeg, 307, y, 315, y, 68); /* <MXD> SEGa000 */
		}
	}
}

// ----------------------------------------------------------------------------

Uint8 player_takeDamage(Player *this_player, Uint8 damageAmount, damagetype_t damageType )
{
	// used to force velocity on the player if they take
	// a hit to their armor in certain situations
	int takenArmorDamage = 0;

#ifdef LEVEL_CHEATS
	if (!this_player->is_alive || youAreCheating)
#else
	if (!this_player->is_alive)
#endif
		return 0;

	soundQueue[7] = S_SHIELD_HIT;
	if (APSeedSettings.HardContact && damageType == DAMAGE_CONTACT)
	{
		this_player->shield = 0;
		damageAmount = 1;
	}

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
			++APPlayData.Deaths;

			this_player->armor = 0;
			levelTimer = false;
			this_player->is_alive = false;
			this_player->exploding_ticks = 60;
			levelEnd = 40;
			tempVolume = tyrMusicVolume;
			soundQueue[1] = S_EXPLOSION_22;
		}
	}

	player_wipeShieldArmorBars();
	player_drawShield();
	player_drawArmor();

	return takenArmorDamage;
}

// Handle receiving a DeathLink request from someone else.
void player_handleDeathLink(Player *this_player)
{
	if (!this_player->is_alive
		|| !APSeedSettings.DeathLink // DeathLink off in seed
		|| !APOptions.EnableDeathLink // DeathLink off in options menu
		|| !APDeathLinkReceived) // Haven't received one
		return;

	this_player->shield = 0;
	this_player->invulnerable_ticks = 0;
	player_takeDamage(this_player, 1, DAMAGE_DEATHLINK);
}

void player_debugCauseDeathLink(void)
{
	APDeathLinkReceived = true;
}

void player_resetDeathLink(void)
{
	APDeathLinkReceived = false;
}
