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
#include "tyrian2.h"

#include "apmenu.h"
#include "apmsg.h"
#include "backgrnd.h"
#include "episodes.h"
#include "file.h"
#include "font.h"
#include "fonthand.h"
#include "joystick.h"
#include "keyboard.h"
#include "lds_play.h"
#include "levelend.h"
#include "loudness.h"
#include "mainint.h"
#include "mouse.h"
#include "mtrand.h"
#include "network.h"
#include "nortsong.h"
#include "nortvars.h"
#include "opentyr.h"
#include "params.h"
#include "pcxload.h"
#include "pcxmast.h"
#include "picload.h"
#include "shots.h"
#include "sprite.h"
#include "vga256d.h"
#include "video.h"

#include "archipelago/apconnect.h"

#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// TODO put this somewhere nicer
void tyrian_enemyDieItem(JE_byte enemyId);
void tyrian_setupAPItemSpawn(Sint16 item, Sint16 apItemNum);
#define ARCHIPELAGO_ITEM     900
#define ARCHIPELAGO_ITEM_MAX 931
static const Sint16 apBehaviorList[] = {512, 513, 628, 512, 513, 512, 513, 512, 513, 628};
static const Sint16 apEnemySection[] = { 25,  25,   0,   0,   0,  75,  75,  50,  50,  50};

struct {
	Uint16 location;
	Sint16 behavesAs;
	Sint16 alignX;
	Sint16 alignY;
} apCheckData[32]; // Data for each check that's been spawned
Uint8 apCheckCount; // Current number of checks spawned in level

inline static void blit_enemy(SDL_Surface *surface, unsigned int i, signed int x_offset, signed int y_offset, signed int sprite_offset);

boss_bar_t boss_bar[2];

/* Level Event Data */
JE_boolean quit, loadLevelOk;

JE_word levelEnemyMax;
JE_word levelEnemyFrequency;
JE_word levelEnemy[40]; /* [1..40] */

char tempStr[31];

/* Data used for ItemScreen procedure to indicate items available */
JE_byte itemAvail[9][10]; /* [1..9, 1..10] */
JE_byte itemAvailMax[9]; /* [1..9] */

void JE_starShowVGA(void)
{
	JE_byte *src;
	Uint8 *s = NULL; /* screen pointer, 8-bit specific */

	int x, y, lightx, lighty, lightdist;

	if (!playerEndLevel && !skipStarShowVGA)
	{

		s = VGAScreenSeg->pixels;

		src = game_screen->pixels;
		src += 24;

		if (smoothScroll != 0 /*&& thisPlayerNum != 2*/)
		{
			wait_delay();
			setDelay(frameCountMax);
		}

		if (starShowVGASpecialCode == 1)
		{
			src += game_screen->pitch * 183;
			for (y = 0; y < 184; y++)
			{
				memmove(s, src, 264);
				s += VGAScreenSeg->pitch;
				src -= game_screen->pitch;
			}
		}
		else if (starShowVGASpecialCode == 2 && processorType >= 2)
		{
			lighty = 172 - player[0].y;
			lightx = 281 - player[0].x;

			for (y = 184; y; y--)
			{
				if (lighty > y)
				{
					for (x = 320 - 56; x; x--)
					{
						*s = (*src & 0xf0) | ((*src >> 2) & 0x03);
						s++;
						src++;
					}
				}
				else
				{
					for (x = 320 - 56; x; x--)
					{
						lightdist = abs(lightx - x) + lighty;
						if (lightdist < y)
							*s = *src;
						else if (lightdist - y <= 5)
							*s = (*src & 0xf0) | (((*src & 0x0f) + (3 * (5 - (lightdist - y)))) / 4);
						else
							*s = (*src & 0xf0) | ((*src & 0x0f) >> 2);
						s++;
						src++;
					}
				}
				s += 56 + VGAScreenSeg->pitch - 320;
				src += 56 + VGAScreenSeg->pitch - 320;
			}
		}
		else
		{
			for (y = 0; y < 184; y++)
			{
				memmove(s, src, 264);
				s += VGAScreenSeg->pitch;
				src += game_screen->pitch;
			}
		}
		JE_showVGA();
	}

	quitRequested = false;
	skipStarShowVGA = false;
}

static Uint8 tyrian_scaleEnemyHealth(int baseHealth)
{
	if (baseHealth >= 255)
		return 255;

	switch (difficultyLevel)
	{
		default:
		case DIFFICULTY_WIMP:       baseHealth *= 0.5f + 1;  break;
		case DIFFICULTY_EASY:       baseHealth *= 0.75f + 1; break;
		case DIFFICULTY_NORMAL:     break;
		case DIFFICULTY_HARD:       baseHealth *= 1.2f;      break;
		case DIFFICULTY_IMPOSSIBLE: baseHealth *= 1.5f;      break;
		case DIFFICULTY_INSANITY:   baseHealth *= 1.8f;      break;
		case DIFFICULTY_SUICIDE:    baseHealth *= 2;         break;
		case DIFFICULTY_MANIACAL:   baseHealth *= 3;         break;
		case DIFFICULTY_ZINGLON:    baseHealth *= 4;         break;
		case DIFFICULTY_NORTANEOUS: baseHealth *= 8;         break;
		case DIFFICULTY_10:         baseHealth *= 8;         break;
	}

	if (baseHealth > 254)
		return 254;
	if (baseHealth <= 0)
		return 1;
	return (Uint8)baseHealth;
}

static void tyrian_blitAPRadar(SDL_Surface *surface, unsigned int i)
{
	if (enemy[i].sprite2s == NULL)
		return;

	const int baseX = enemy[i].ex + tempMapXOfs;
	const int baseY = enemy[i].ey;
	const unsigned int baseIndex = enemy[i].egr[enemy[i].enemycycle - 1];
	const int filter = 117;

	if (baseIndex == 999 || !baseIndex)
		return;

	if (enemy[i].size == 1) // 2x2 enemy
	{
		if (enemy[i].ey > -13)
		{
			blit_sprite2_filter(surface, baseX - 7, baseY - 7, *enemy[i].sprite2s, baseIndex,     filter);
			blit_sprite2_filter(surface, baseX - 8, baseY - 7, *enemy[i].sprite2s, baseIndex,     filter);
			blit_sprite2_filter(surface, baseX - 6, baseY - 8, *enemy[i].sprite2s, baseIndex,     filter);
			blit_sprite2_filter(surface, baseX - 6, baseY - 9, *enemy[i].sprite2s, baseIndex,     filter);

			blit_sprite2_filter(surface, baseX + 7, baseY - 7, *enemy[i].sprite2s, baseIndex + 1, filter);
			blit_sprite2_filter(surface, baseX + 8, baseY - 7, *enemy[i].sprite2s, baseIndex + 1, filter);
			blit_sprite2_filter(surface, baseX + 6, baseY - 8, *enemy[i].sprite2s, baseIndex + 1, filter);
			blit_sprite2_filter(surface, baseX + 6, baseY - 9, *enemy[i].sprite2s, baseIndex + 1, filter);
		}
		if (enemy[i].ey > -26 && enemy[i].ey < 182)
		{
			blit_sprite2_filter(surface, baseX - 7, baseY + 7, *enemy[i].sprite2s, baseIndex + 19, filter);
			blit_sprite2_filter(surface, baseX - 8, baseY + 7, *enemy[i].sprite2s, baseIndex + 19, filter);
			blit_sprite2_filter(surface, baseX - 6, baseY + 8, *enemy[i].sprite2s, baseIndex + 19, filter);
			blit_sprite2_filter(surface, baseX - 6, baseY + 9, *enemy[i].sprite2s, baseIndex + 19, filter);

			blit_sprite2_filter(surface, baseX + 7, baseY + 7, *enemy[i].sprite2s, baseIndex + 20, filter);
			blit_sprite2_filter(surface, baseX + 8, baseY + 7, *enemy[i].sprite2s, baseIndex + 20, filter);
			blit_sprite2_filter(surface, baseX + 6, baseY + 8, *enemy[i].sprite2s, baseIndex + 20, filter);
			blit_sprite2_filter(surface, baseX + 6, baseY + 9, *enemy[i].sprite2s, baseIndex + 20, filter);
		}
	}
	else
	{
		if (enemy[i].ey > -13)
		{
			blit_sprite2_filter(surface, baseX - 1, baseY, *enemy[i].sprite2s, baseIndex, filter);
			blit_sprite2_filter(surface, baseX - 2, baseY, *enemy[i].sprite2s, baseIndex, filter);
			blit_sprite2_filter(surface, baseX, baseY - 1, *enemy[i].sprite2s, baseIndex, filter);
			blit_sprite2_filter(surface, baseX, baseY - 2, *enemy[i].sprite2s, baseIndex, filter);

			blit_sprite2_filter(surface, baseX + 1, baseY, *enemy[i].sprite2s, baseIndex, filter);
			blit_sprite2_filter(surface, baseX + 2, baseY, *enemy[i].sprite2s, baseIndex, filter);
			blit_sprite2_filter(surface, baseX, baseY + 1, *enemy[i].sprite2s, baseIndex, filter);
			blit_sprite2_filter(surface, baseX, baseY + 2, *enemy[i].sprite2s, baseIndex, filter);
		}
	}
}

inline static void blit_enemy(SDL_Surface *surface, unsigned int i, signed int x_offset, signed int y_offset, signed int sprite_offset)
{
	if (enemy[i].sprite2s == NULL)
	{
		fprintf(stderr, "warning: enemy %d sprite missing\n", i);
		return;
	}
	
	const int x = enemy[i].ex + x_offset + tempMapXOfs,
	          y = enemy[i].ey + y_offset;
	const unsigned int index = enemy[i].egr[enemy[i].enemycycle - 1] + sprite_offset;

	if (enemy[i].filter != 0)
		blit_sprite2_filter(surface, x, y, *enemy[i].sprite2s, index, enemy[i].filter);
	else
		blit_sprite2(surface, x, y, *enemy[i].sprite2s, index);
}

void JE_drawEnemy(int enemyOffset) // actually does a whole lot more than just drawing
{
	player[0].x -= 25;

	// ------------------------------------------------------------------------
	// Draw AP Radar behind all enemies, to better highlight larger enemies
	for (int i = enemyOffset - 25; i < enemyOffset; i++)
	{
		if (enemyAvail[i] == 1)
			continue;

		if (enemy[i].ex + tempMapXOfs > -29 && enemy[i].ex + tempMapXOfs < 300)
		{
			if (enemy[i].enemydie == 533
				|| enemy[i].enemydie == 534
				|| enemy[i].enemydie == 512
				|| enemy[i].enemydie == 513
				|| (enemy[i].enemydie >= ARCHIPELAGO_ITEM && enemy[i].enemydie <= ARCHIPELAGO_ITEM_MAX))
			{
				tyrian_blitAPRadar(VGAScreen, i);
			}
		}
	}

	// ------------------------------------------------------------------------

	// Regular enemy routines
	for (int i = enemyOffset - 25; i < enemyOffset; i++)
	{
		if (enemyAvail[i] != 1)
		{
			enemy[i].mapoffset = tempMapXOfs;

			if (enemy[i].xaccel && enemy[i].xaccel - 89u > mt_rand() % 11)
			{
				if (player[0].x > enemy[i].ex)
				{
					if (enemy[i].exc < enemy[i].xaccel - 89)
						enemy[i].exc++;
				}
				else
				{
					if (enemy[i].exc >= 0 || -enemy[i].exc < enemy[i].xaccel - 89)
						enemy[i].exc--;
				}
			}

			if (enemy[i].yaccel && enemy[i].yaccel - 89u > mt_rand() % 11)
			{
				if (player[0].y > enemy[i].ey)
				{
					if (enemy[i].eyc < enemy[i].yaccel - 89)
						enemy[i].eyc++;
				}
				else
				{
					if (enemy[i].eyc >= 0 || -enemy[i].eyc < enemy[i].yaccel - 89)
						enemy[i].eyc--;
				}
			}

 			if (enemy[i].ex + tempMapXOfs > -29 && enemy[i].ex + tempMapXOfs < 300)
			{
				if (enemy[i].aniactive == 1)
				{
					enemy[i].enemycycle++;

					if (enemy[i].enemycycle == enemy[i].animax)
						enemy[i].aniactive = enemy[i].aniwhenfire;
					else if (enemy[i].enemycycle > enemy[i].ani)
						enemy[i].enemycycle = enemy[i].animin;
				}

				if (enemy[i].egr[enemy[i].enemycycle - 1] == 999)
					goto enemy_gone;

				if (enemy[i].size == 1) // 2x2 enemy
				{
					if (enemy[i].ey > -13)
					{
						blit_enemy(VGAScreen, i, -6, -7, 0);
						blit_enemy(VGAScreen, i,  6, -7, 1);
					}
					if (enemy[i].ey > -26 && enemy[i].ey < 182)
					{
						blit_enemy(VGAScreen, i, -6,  7, 19);
						blit_enemy(VGAScreen, i,  6,  7, 20);
					}
				}
				else
				{
					if (enemy[i].ey > -13)
						blit_enemy(VGAScreen, i, 0, 0, 0);
				}

				enemy[i].filter = 0;
			}

			if (enemy[i].excc)
			{
				if (--enemy[i].exccw <= 0)
				{
					if (enemy[i].exc == enemy[i].exrev)
					{
						enemy[i].excc = -enemy[i].excc;
						enemy[i].exrev = -enemy[i].exrev;
						enemy[i].exccadd = -enemy[i].exccadd;
					}
					else
					{
						enemy[i].exc += enemy[i].exccadd;
						enemy[i].exccw = enemy[i].exccwmax;
						if (enemy[i].exc == enemy[i].exrev)
						{
							enemy[i].excc = -enemy[i].excc;
							enemy[i].exrev = -enemy[i].exrev;
							enemy[i].exccadd = -enemy[i].exccadd;
						}
					}
				}
			}

			if (enemy[i].eycc)
			{
				if (--enemy[i].eyccw <= 0)
				{
					if (enemy[i].eyc == enemy[i].eyrev)
					{
						enemy[i].eycc = -enemy[i].eycc;
						enemy[i].eyrev = -enemy[i].eyrev;
						enemy[i].eyccadd = -enemy[i].eyccadd;
					}
					else
					{
						enemy[i].eyc += enemy[i].eyccadd;
						enemy[i].eyccw = enemy[i].eyccwmax;
						if (enemy[i].eyc == enemy[i].eyrev)
						{
							enemy[i].eycc = -enemy[i].eycc;
							enemy[i].eyrev = -enemy[i].eyrev;
							enemy[i].eyccadd = -enemy[i].eyccadd;
						}
					}
				}
			}

			enemy[i].ey += enemy[i].fixedmovey;

			enemy[i].ex += enemy[i].exc;
			if (enemy[i].ex < -80 || enemy[i].ex > 340)
				goto enemy_gone;

			enemy[i].ey += enemy[i].eyc;
			if (enemy[i].ey < -112 || enemy[i].ey > 190)
				goto enemy_gone;

			goto enemy_still_exists;

enemy_gone:
			/* enemy[i].egr[10] &= 0x00ff; <MXD> madness? */
			enemyAvail[i] = 1;
			goto draw_enemy_end;

enemy_still_exists:

			/*X bounce*/
			if (enemy[i].ex <= enemy[i].xminbounce || enemy[i].ex >= enemy[i].xmaxbounce)
				enemy[i].exc = -enemy[i].exc;

			/*Y bounce*/
			if (enemy[i].ey <= enemy[i].yminbounce || enemy[i].ey >= enemy[i].ymaxbounce)
				enemy[i].eyc = -enemy[i].eyc;

			/* Evalue != 0 - score item at boundary */
			if (enemy[i].scoreitem)
			{
				if (enemy[i].ex < -5)
					enemy[i].ex++;
				if (enemy[i].ex > 245)
					enemy[i].ex--;
			}

			enemy[i].ey += tempBackMove;

			if (enemy[i].ex <= -24 || enemy[i].ex >= 296)
				goto draw_enemy_end;

			tempX = enemy[i].ex;
			tempY = enemy[i].ey;

			temp = enemy[i].enemytype;

			/* Enemy Shots */
			if (enemy[i].edamaged == 1)
				goto draw_enemy_end;

			enemyOnScreen++;

			if (enemy[i].iced)
			{
				enemy[i].iced--;
				if (enemy[i].enemyground != 0)
				{
					enemy[i].filter = 0x09;
				}
				goto draw_enemy_end;
			}

			for (int j = 3; j > 0; j--)
			{
				if (enemy[i].freq[j-1])
				{
					temp3 = enemy[i].tur[j-1];

					if (--enemy[i].eshotwait[j-1] == 0 && temp3)
					{
						enemy[i].eshotwait[j-1] = enemy[i].freq[j-1];
						if (difficultyLevel > DIFFICULTY_NORMAL)
						{
							enemy[i].eshotwait[j-1] = (enemy[i].eshotwait[j-1] / 2) + 1;
							if (difficultyLevel > DIFFICULTY_MANIACAL)
								enemy[i].eshotwait[j-1] = (enemy[i].eshotwait[j-1] / 2) + 1;
						}

						if (galagaMode && (enemy[i].eyc == 0 || (mt_rand() % 400) >= galagaShotFreq))
							goto draw_enemy_end;

						switch (temp3)
						{
						case 252: /* Savara Boss DualMissile */
							if (enemy[i].ey > 20)
							{
								JE_setupExplosion(tempX - 8 + tempMapXOfs, tempY - 20 - backMove * 8, -2, 6, false, false);
								JE_setupExplosion(tempX + 4 + tempMapXOfs, tempY - 20 - backMove * 8, -2, 6, false, false);
							}
							break;
						case 251:; /* Suck-O-Magnet */
							const int attractivity = 4 - (abs(player[0].x - tempX) + abs(player[0].y - tempY)) / 100;
							player[0].x_velocity += (player[0].x > tempX) ? -attractivity : attractivity;
							break;
						case 253: /* Left ShortRange Magnet */
							if (abs(player[0].x + 25 - 14 - tempX) < 24 && abs(player[0].y - tempY) < 28)
							{
								player[0].x_velocity += 2;
							}
							if (twoPlayerMode &&
							   (abs(player[1].x - 14 - tempX) < 24 && abs(player[1].y - tempY) < 28))
							{
								player[1].x_velocity += 2;
							}
							break;
						case 254: /* Left ShortRange Magnet */
							if (abs(player[0].x + 25 - 14 - tempX) < 24 && abs(player[0].y - tempY) < 28)
							{
								player[0].x_velocity -= 2;
							}
							if (twoPlayerMode &&
							   (abs(player[1].x - 14 - tempX) < 24 && abs(player[1].y - tempY) < 28))
							{
								player[1].x_velocity -= 2;
							}
							break;
						case 255: /* Magneto RePulse!! */
							if (difficultyLevel != DIFFICULTY_EASY) /*DIF*/
							{
								if (j == 3)
								{
									enemy[i].filter = 0x70;
								}
								else
								{
									const int repulsivity = 4 - (abs(player[0].x - tempX) + abs(player[0].y - tempY)) / 20;
									if (repulsivity > 0)
										player[0].x_velocity += (player[0].x > tempX) ? repulsivity : -repulsivity;
								}
							}
							break;
						default:
						/*Rot*/
							for (int tempCount = weapons[temp3].multi; tempCount > 0; tempCount--)
							{
								for (b = 0; b < ENEMY_SHOT_MAX; b++)
								{
									if (enemyShotAvail[b] == 1)
										break;
								}
								if (b == ENEMY_SHOT_MAX)
									goto draw_enemy_end;

								enemyShotAvail[b] = !enemyShotAvail[b];

								if (weapons[temp3].sound > 0)
								{
									do
									{
										temp = mt_rand() % 8;
									} while (temp == 3);
									soundQueue[temp] = weapons[temp3].sound;
								}

								if (enemy[i].aniactive == 2)
									enemy[i].aniactive = 1;

								if (++enemy[i].eshotmultipos[j-1] > weapons[temp3].max)
									enemy[i].eshotmultipos[j-1] = 1;

								int tempPos = enemy[i].eshotmultipos[j-1] - 1;

								if (j == 1)
									temp2 = 4;

								enemyShot[b].sx = tempX + weapons[temp3].bx[tempPos] + tempMapXOfs;
								enemyShot[b].sy = tempY + weapons[temp3].by[tempPos];
								enemyShot[b].sdmg = weapons[temp3].attack[tempPos];
								enemyShot[b].tx = weapons[temp3].tx;
								enemyShot[b].ty = weapons[temp3].ty;
								enemyShot[b].duration = weapons[temp3].del[tempPos];
								enemyShot[b].animate = 0;
								enemyShot[b].animax = weapons[temp3].weapani;

								enemyShot[b].sgr = weapons[temp3].sg[tempPos];
								switch (j)
								{
								case 1:
									enemyShot[b].syc = weapons[temp3].acceleration;
									enemyShot[b].sxc = weapons[temp3].accelerationx;

									enemyShot[b].sxm = weapons[temp3].sx[tempPos];
									enemyShot[b].sym = weapons[temp3].sy[tempPos];
									break;
								case 3:
									enemyShot[b].sxc = -weapons[temp3].acceleration;
									enemyShot[b].syc = weapons[temp3].accelerationx;

									enemyShot[b].sxm = -weapons[temp3].sy[tempPos];
									enemyShot[b].sym = -weapons[temp3].sx[tempPos];
									break;
								case 2:
									enemyShot[b].sxc = weapons[temp3].acceleration;
									enemyShot[b].syc = -weapons[temp3].acceleration;

									enemyShot[b].sxm = weapons[temp3].sy[tempPos];
									enemyShot[b].sym = -weapons[temp3].sx[tempPos];
									break;
								}

								if (weapons[temp3].aim > 0)
								{
									int aim = weapons[temp3].aim;

									/*DIF*/
									if (difficultyLevel > DIFFICULTY_NORMAL)
									{
										aim += difficultyLevel - 2;
									}

									JE_word target_x = player[0].x;
									JE_word target_y = player[0].y;

									if (twoPlayerMode)
									{
										// fire at live player(s)
										if (player[0].is_alive && !player[1].is_alive)
											temp = 0;
										else if (player[1].is_alive && !player[0].is_alive)
											temp = 1;
										else
											temp = mt_rand() % 2;

										if (temp == 1)
										{
											target_x = player[1].x - 25;
											target_y = player[1].y;
										}
									}

									int relative_x = (target_x + 25) - tempX - tempMapXOfs - 4;
									if (relative_x == 0)
										relative_x = 1;
									int relative_y = target_y - tempY;
									if (relative_y == 0)
										relative_y = 1;
									const int longest_side = MAX(abs(relative_x), abs(relative_y));
									enemyShot[b].sxm = roundf((float)relative_x / longest_side * aim);
									enemyShot[b].sym = roundf((float)relative_y / longest_side * aim);
								}
							}
							break;
						}
					}
				}
			}

			/* Enemy Launch Routine */
			if (enemy[i].launchfreq)
			{
				if (--enemy[i].launchwait == 0)
				{
					enemy[i].launchwait = enemy[i].launchfreq;

					if (enemy[i].launchspecial != 0)
					{
						/*Type  1 : Must be inline with player*/
						if (abs(enemy[i].ey - player[0].y) > 5)
							goto draw_enemy_end;
					}

					if (enemy[i].aniactive == 2)
					{
						enemy[i].aniactive = 1;
					}

					if (enemy[i].launchtype == 0)
						goto draw_enemy_end;

					tempW = enemy[i].launchtype;
					b = JE_newEnemy(enemyOffset == 50 ? 75 : enemyOffset - 25, tempW, 0);

					/*Launch Enemy Placement*/
					if (b > 0)
					{
						struct JE_SingleEnemyType* e = &enemy[b-1];

						e->ex = tempX;
						e->ey = tempY + enemyDat[e->enemytype].startyc;
						if (e->size == 0)
							e->ey -= 7;

						if (e->launchtype > 0 && e->launchfreq == 0)
						{
							if (e->launchtype > 90)
							{
								e->ex += mt_rand() % ((e->launchtype - 90) * 4) - (e->launchtype - 90) * 2;
							}
							else
							{
								int target_x = (player[0].x + 25) - tempX - tempMapXOfs - 4;
								if (target_x == 0)
									target_x = 1;
								int tempI5 = player[0].y - tempY;
								if (tempI5 == 0)
									tempI5 = 1;
								const int longest_side = MAX(abs(target_x), abs(tempI5));
								e->exc = roundf(((float)target_x / longest_side) * e->launchtype);
								e->eyc = roundf(((float)tempI5 / longest_side) * e->launchtype);
							}
						}

						do
						{
							temp = mt_rand() % 8;
						} while (temp == 3);
						soundQueue[temp] = randomEnemyLaunchSounds[(mt_rand() % 3)];

						if (enemy[i].launchspecial == 1 &&
						    enemy[i].linknum < 100)
						{
							e->linknum = enemy[i].linknum;
						}
					}
				}
			}
		}
draw_enemy_end:
		;
	}

#if 0
	for (int i = enemyOffset - 25; i < enemyOffset; i++)
	{
		if (enemyAvail[i] != 0 || !enemy[i].armorleft)
			continue;

		const int baseX = enemy[i].ex + tempMapXOfs;
		const int baseY = enemy[i].ey;

		char buffer[6];
		sprintf(buffer, "%hd", enemy[i].armorleft);
		JE_textShade(VGAScreen, baseX, baseY, buffer, 14, 1, FULL_SHADE);

		if (!enemy[i].special)
			continue;
		sprintf(buffer, "%hd", enemy[i].flagnum);
		JE_textShade(VGAScreen, baseX, baseY+8, buffer, 7, 2, FULL_SHADE);
	}
#endif

	player[0].x += 25;
}

void JE_main(void)
{
#if SDL_VERSION_ATLEAST(2, 0, 18)
	Uint64 levelStartTime = 0;
#else
	Uint32 levelStartTime = 0;
#endif

	char buffer[256];

	int lastEnemyOnScreen;

	/* NOTE: BEGIN MAIN PROGRAM HERE AFTER LOADING A GAME OR STARTING A NEW ONE */

	/* ----------- GAME ROUTINES ------------------------------------- */
	/* We need to jump to the beginning to make space for the routines */
	/* --------------------------------------------------------------- */
	goto start_level_first;

	/*------------------------------GAME LOOP-----------------------------------*/

	/* Startlevel is called after a previous level is over.  If the first level
	   is started for a gaming session, startlevelfirst is called instead and
	   this code is skipped.  The code here finishes the level and prepares for
	   the loadmap function. */

start_level:

	mouseSetRelative(false);

	if (galagaMode)
		twoPlayerMode = false;

	JE_clearKeyboard();

	free_sprite2s(&enemySpriteSheets[0]);
	free_sprite2s(&enemySpriteSheets[1]);
	free_sprite2s(&enemySpriteSheets[2]);
	free_sprite2s(&enemySpriteSheets[3]);

	/* Normal speed */
	if (fastPlay != 0)
	{
		smoothScroll = true;
		Uint16 speed = 0x4300;
		setDelaySpeed(speed);
	}

	if (play_demo || record_demo)
	{
		if (demo_file)
		{
			fclose(demo_file);
			demo_file = NULL;
		}

		if (play_demo)
		{
			stop_song();
			fade_black(10);

			wait_noinput(true, true, true);
		}
	}

	difficultyLevel = oldDifficultyLevel;   /*Return difficulty to normal*/
	lastLevelCompleted = false;

	if (!play_demo)
	{
		if ((!all_players_dead() || normalBonusLevelCurrent || bonusLevelCurrent) && !playerEndLevel)
		{
			mainLevel = nextLevel;
			levelend_doLevelStats();

			fade_song();
		}
		else
		{
			fade_song();
			fade_black(10);

			//JE_loadGame(twoPlayerMode ? 22 : 11);
			if (doNotSaveBackup)
			{
				superTyrian = false;
				onePlayerAction = false;
				player[0].items.super_arcade_mode = SA_NONE;
			}
			if (bonusLevelCurrent && !playerEndLevel)
			{
				mainLevel = nextLevel;
			}
		}
	}
	doNotSaveBackup = false;

	if (play_demo)
		return;

	 // Player cash is cleared between levels (added to APStats.Cash on level completion)
	player[0].cash = 0;
	if (levelStartTime) // Increment total time spent in levels
	{
#if SDL_VERSION_ATLEAST(2, 0, 18)
		APPlayData.TimeInLevel += SDL_GetTicks64() - levelStartTime;
#else
		APPlayData.TimeInLevel += SDL_GetTicks() - levelStartTime;
#endif
	}


start_level_first:

	set_volume(tyrMusicVolume, fxVolume);

	endLevel = false;
	reallyEndLevel = false;
	playerEndLevel = false;
	extraGame = false;

	doNotSaveBackup = false;

	if (JE_loadMap() == 0)  // if quit itemscreen
		return;          // back to titlescreen

#if SDL_VERSION_ATLEAST(2, 0, 18)
	levelStartTime = SDL_GetTicks64();
#else
	levelStartTime = SDL_GetTicks();
#endif

	if (!play_demo)
		mouseSetRelative(true);

	fade_song();

	for (uint i = 0; i < COUNTOF(player); ++i)
		player[i].is_alive = true;

	oldDifficultyLevel = difficultyLevel;
	//if (episodeNum == EPISODE_AVAILABLE)
	//	difficultyLevel--;
	//if (difficultyLevel < DIFFICULTY_EASY)
	//	difficultyLevel = DIFFICULTY_EASY;

	player[0].x = 100;
	player[0].y = 180;

	player[1].x = 190;
	player[1].y = 180;

	assert(COUNTOF(player->old_x) == COUNTOF(player->old_y));

	for (uint i = 0; i < COUNTOF(player); ++i)
	{
		for (uint j = 0; j < COUNTOF(player->old_x); ++j)
		{
			player[i].old_x[j] = player[i].x - (19 - j);
			player[i].old_y[j] = player[i].y - 18;
		}
		
		player[i].last_x_shot_move = player[i].x;
		player[i].last_y_shot_move = player[i].y;
	}
	
	JE_loadPic(VGAScreen, twoPlayerMode ? 6 : 3, false);

	player_updateItemChoices(); // sync AP items with internal items
	JE_drawOptions();

	JE_outText(VGAScreen, 268, twoPlayerMode ? 76 : 118, levelName, 12, 4);

	JE_showVGA();
	JE_gammaCorrect(&colors, gammaCorrection);
	fade_palette(colors, 50, 0, 255);

	if (explosionSpriteSheet.data == NULL)
		JE_loadCompShapes(&explosionSpriteSheet, '6');

	/* MAPX will already be set correctly */
	mapY = 300 - 8;
	mapY2 = 600 - 8;
	mapY3 = 600 - 8;
	mapYPos = &megaData1.mainmap[mapY][0] - 1;
	mapY2Pos = &megaData2.mainmap[mapY2][0] - 1;
	mapY3Pos = &megaData3.mainmap[mapY3][0] - 1;
	mapXPos = 0;
	mapXOfs = 0;
	mapX2Pos = 0;
	mapX3Pos = 0;
	mapX3Ofs = 0;
	mapXbpPos = 0;
	mapX2bpPos = 0;
	mapX3bpPos = 0;

	map1YDelay = 1;
	map1YDelayMax = 1;
	map2YDelay = 1;
	map2YDelayMax = 1;

	musicFade = false;

	backPos = 0;
	backPos2 = 0;
	backPos3 = 0;
	generatorPower = 0;
	starfield_speed = 1;

	player_updateShipData(); // Setup player ship data
	player_resetDeathLink();
	memset(&APUpdateRequest, 0, sizeof(APUpdateRequest));

	for (uint i = 0; i < COUNTOF(player); ++i)
	{
		player[i].x_velocity = 0;
		player[i].y_velocity = 0;

		player[i].invulnerable_ticks = 100;
	}

	newkey = newmouse = false;

	/* Initialize Level Data and Debug Mode */
	levelEnd = 255;
	levelEndWarp = -4;
	levelEndFxWait = 0;
	warningCol = 120;
	warningColChange = 1;
	warningSoundDelay = 0;
	armorShipDelay = 50;

	bonusLevel = false;
	readyToEndLevel = false;
	firstGameOver = true;
	eventLoc = 1;
	curLoc = 0;
	backMove = 1;
	backMove2 = 2;
	backMove3 = 3;
	explodeMove = 2;
	enemiesActive = true;
	for (temp = 0; temp < 3; temp++)
	{
		button[temp] = false;
	}
	stopBackgrounds = false;
	stopBackgroundNum = 0;
	background3x1   = false;
	background3x1b  = false;
	background3over = 0;
	background2over = 1;
	topEnemyOver = false;
	skyEnemyOverAll = false;
	smallEnemyAdjust = false;
	starActive = true;
	enemyContinualDamage = false;
	levelEnemyFrequency = 96;
	quitRequested = false;

	for (unsigned int i = 0; i < COUNTOF(boss_bar); i++)
		boss_bar[i].link_num = 0;

	forceEvents = false;  /*Force events to continue if background movement = 0*/

	superEnemy254Jump = 0;   /*When Enemy with PL 254 dies*/

	/* Filter Status */
	filterActive = true;
	filterFade = true;
	filterFadeStart = false;
	levelFilter = -99;
	levelBrightness = -14;
	levelBrightnessChg = 1;

	background2notTransparent = false;

	uint old_weapon_bar[2] = { 0, 0 };  // only redrawn when they change

	/* Initially erase power bars */
	lastGenPower = generatorPower / 10;

	/* Initial Text */
	apmsg_drawInGameText(miscText[20]);

	/* Setup Armor/Shield Data */
	shieldWait = 1;

	player_drawShield();
	player_drawArmor();

	/* Set cubes to 0 */
	cubeMax = 0;

	/* Secret Level Display */
	flash = 0;
	flashChange = 1;
	displayTime = 0;

	play_song(levelSong - 1);

	player_drawPortConfigButtons();

	/* --- MAIN LOOP --- */

	newkey = false;

#ifdef WITH_NETWORK
	if (isNetworkGame)
	{
		JE_clearSpecialRequests();
		mt_srand(32402394);
	}
#endif

	initialize_starfield();

	JE_setNewGameSpeed();

	set_volume(tyrMusicVolume, fxVolume);

#if 0
	/*Save backup game*/
	if (!play_demo && !doNotSaveBackup)
	{
		temp = twoPlayerMode ? 22 : 11;
		JE_saveGame(temp, "LAST LEVEL    ");
	}
#endif

	if (!play_demo && record_demo)
	{
		Uint8 new_demo_num = 0;

		do
		{
			sprintf(tempStr, "demorec.%d", new_demo_num++);
		} while (dir_file_exists(get_user_directory(), tempStr)); // until file doesn't exist

		demo_file = dir_fopen_warn(get_user_directory(), tempStr, "wb");
		if (!demo_file)
			exit(1);

		fwrite_u8_die(&episodeNum, 1, demo_file);

		// Pad string buffer with NULs.
		for (size_t i = 1; i < 10; ++i)
			if (levelName[i - 1] == '\0')
				levelName[i] = '\0';
		fwrite_u8_die((Uint8 *)levelName, 10, demo_file);

		fwrite_u8_die(&lvlFileNum, 1, demo_file);

		fwrite_u8_die(&player[0].items.weapon[FRONT_WEAPON].id,  1, demo_file);
		fwrite_u8_die(&player[0].items.weapon[REAR_WEAPON].id,   1, demo_file);
		fwrite_u8_die(&player[0].items.super_arcade_mode,        1, demo_file);
		fwrite_u8_die(&player[0].items.sidekick[LEFT_SIDEKICK],  1, demo_file);
		fwrite_u8_die(&player[0].items.sidekick[RIGHT_SIDEKICK], 1, demo_file);
		fwrite_u8_die(&player[0].items.generator,                1, demo_file);

		fwrite_u8_die(&player[0].items.sidekick_level,           1, demo_file);
		fwrite_u8_die(&player[0].items.sidekick_series,          1, demo_file);

		fwrite_u8_die(&initial_episode_num,                      1, demo_file);

		fwrite_u8_die(&player[0].items.shield,                   1, demo_file);
		fwrite_u8_die(&player[0].items.special,                  1, demo_file);
		fwrite_u8_die(&player[0].items.ship,                     1, demo_file);

		for (uint i = 0; i < 2; ++i)
			fwrite_u8_die(&player[0].items.weapon[i].power,      1, demo_file);

		Uint8 unused[3] = { 0, 0, 0 };
		fwrite_u8_die(unused, 3, demo_file);

		fwrite_u8_die(&levelSong, 1, demo_file);

		demo_keys = 0;
		demo_keys_wait = 0;
	}

	twoPlayerLinked = false;
	linkGunDirec = M_PI;

	for (uint i = 0; i < COUNTOF(player); ++i)
		calc_purple_balls_needed(&player[i]);

	damageRate = 2;  /*Normal Rate for Collision Damage*/

	chargeWait   = 5;
	chargeLevel  = 0;
	chargeMax    = 5;
	chargeGr     = 0;
	chargeGrWait = 3;

	portConfigChange = false;

	/*Destruction Ratio*/
	totalEnemy = 0;
	enemyKilled = 0;

	astralDuration = 0;

	superArcadePowerUp = 1;

	yourInGameMenuRequest = false;

	constantLastX = -1;

	for (uint i = 0; i < COUNTOF(player); ++i)
		player[i].exploding_ticks = 0;

#if 0
	if (isNetworkGame)
	{
		JE_loadItemDat();
	}
#endif

	// ------------------------------------------------------------------------
	// Archipelago Data Init
	apCheckCount = 0;
	memset(apCheckData, 0, sizeof(apCheckData));
	// ------------------------------------------------------------------------

	memset(enemyAvail,       1, sizeof(enemyAvail));
	for (uint i = 0; i < COUNTOF(enemyShotAvail); i++)
		enemyShotAvail[i] = 1;

	/*Initialize Shots*/
	memset(playerShotData,   0, sizeof(playerShotData));
	memset(shotAvail,        0, sizeof(shotAvail));
	memset(shotMultiPos,     0, sizeof(shotMultiPos));
	memset(shotRepeat,       1, sizeof(shotRepeat));

	memset(button,           0, sizeof(button));

	memset(globalFlags,      0, sizeof(globalFlags));

	memset(explosions,       0, sizeof(explosions));
	memset(rep_explosions,   0, sizeof(rep_explosions));

	/* --- Clear Sound Queue --- */
	memset(soundQueue,       0, sizeof(soundQueue));
	nortsong_playVoice(V_GOOD_LUCK);

	memset(enemySpriteSheetIds, 0, sizeof(enemySpriteSheetIds));
	memset(enemy,               0, sizeof(enemy));

	memset(SFCurrentCode,    0, sizeof(SFCurrentCode));
	memset(SFExecuted,       0, sizeof(SFExecuted));

	zinglonDuration = 0;
	specialWait = 0;
	nextSpecialWait = 0;
	optionAttachmentMove  = 0;    /*Launch the Attachments!*/
	optionAttachmentLinked = true;

	//editShip1 = false;
	//editShip2 = false;

	memset(smoothies, 0, sizeof(smoothies));

	levelTimer = false;
	randomExplosions = false;

	last_superpixel = 0;
	memset(superpixels, 0, sizeof(superpixels));

	returnActive = false;

	galagaShotFreq = 0;

	if (galagaMode)
	{
		difficultyLevel = DIFFICULTY_NORMAL;
	}
	galagaLife = 10000;

	JE_drawOptionLevel();

	// keeps map from scrolling past the top
	BKwrap1 = BKwrap1to = &megaData1.mainmap[1][0];
	BKwrap2 = BKwrap2to = &megaData2.mainmap[1][0];
	BKwrap3 = BKwrap3to = &megaData3.mainmap[1][0];

#ifdef DEBUG_OPTIONS
	damagePerSecondTime = 0;
	damagePerSecondHist[0] = 0;
	damagePerSecondHist[1] = 0;
	damagePerSecondHist[2] = 0;
	damagePerSecondAvg = 0.0f;
#endif

level_loop:

#ifdef DEBUG_OPTIONS
	if (++damagePerSecondTime >= 35)
	{
		damagePerSecondHist[2] = damagePerSecondHist[1];
		damagePerSecondHist[1] = damagePerSecondHist[0];
		damagePerSecondHist[0] = 0;
		damagePerSecondAvg = (float)(damagePerSecondHist[0] + damagePerSecondHist[1] + damagePerSecondHist[2]) / 3;
		damagePerSecondTime = 0;
	}
#endif

	//tempScreenSeg = game_screen; /* side-effect of game_screen */

	if (isNetworkGame)
	{
		smoothies[9-1] = false;
		smoothies[6-1] = false;
	}
	else
	{
		starShowVGASpecialCode = smoothies[9-1] + (smoothies[6-1] << 1);
	}

	/*Background Wrapping*/
	if (mapYPos <= BKwrap1)
		mapYPos = BKwrap1to;
	if (mapY2Pos <= BKwrap2)
		mapY2Pos = BKwrap2to;
	if (mapY3Pos <= BKwrap3)
		mapY3Pos = BKwrap3to;

	allPlayersGone = all_players_dead() &&
	                 ((*player[0].lives == 1 && player[0].exploding_ticks == 0) || (!onePlayerAction && !twoPlayerMode)) &&
	                 ((*player[1].lives == 1 && player[1].exploding_ticks == 0) || !twoPlayerMode);

	/*-----MUSIC FADE------*/
	if (musicFade)
	{
		if (tempVolume > 10)
		{
			tempVolume--;
			set_volume(tempVolume, fxVolume);
		}
		else
		{
			musicFade = false;
		}
	}

	if (!allPlayersGone && levelEnd > 0 && endLevel)
	{
		play_song(9);
		musicFade = false;
	}
	else if (!playing && firstGameOver)
	{
		play_song(levelSong - 1);
	}

	// Message bar moved into apmsg_manageQueueInGame
	// We also always draw it, even in endLevel
	apmsg_manageQueueInGame();

	if (!endLevel) // draw HUD
	{
		VGAScreen = VGAScreenSeg; /* side-effect of game_screen */

		// Give player queued superbombs if we can.
		if (player[0].superbombs < 10 && APStats.QueuedSuperBombs)
		{
			--APStats.QueuedSuperBombs;
			++player[0].superbombs;
		}

		if (APUpdateRequest.Armor)
			player_boostArmor(&player[0], APUpdateRequest.Armor);
		if (APUpdateRequest.Shield)
			player_drawShield();
		memset(&APUpdateRequest, 0, sizeof(APUpdateRequest));

		/*------------------------Shield Gen-------------------------*/
		if (galagaMode)
		{
			for (uint i = 0; i < COUNTOF(player); ++i)
				player[i].shield = 0;

			// spawned dragonwing died :(
			if (*player[1].lives == 0 || player[1].armor == 0)
				twoPlayerMode = false;

			if (player[0].cash >= (unsigned)galagaLife)
			{
				soundQueue[6] = S_EXPLOSION_11;
				soundQueue[7] = S_SOUL_OF_ZINGLON;

				if (*player[0].lives < 11)
					++(*player[0].lives);
				else
					player[0].cash += 1000;

				if (galagaLife == 10000)
					galagaLife = 20000;
				else
					galagaLife += 25000;
			}
		}
		else // not galagaMode
		{
			// For reference: Default shields are 6*20
			uint shieldT = APStats.SolarShield ? 20 : 140;
			// shieldT = shields[player[0].items.shield].tpwr * 20;

			if (twoPlayerMode)
			{
				if (--shieldWait == 0)
				{
					shieldWait = 15;

					for (uint i = 0; i < COUNTOF(player); ++i)
					{
						if (player[i].shield < APStats.ShieldLevel && player[i].is_alive)
							++player[i].shield;
					}

					player_drawShield();
				}
			}
			else if (player[0].is_alive && player[0].shield < APStats.ShieldLevel && generatorPower > shieldT)
			{
				if (--shieldWait == 0)
				{
					shieldWait = 15;

					generatorPower -= shieldT;

					++player[0].shield;
					if (player[1].shield < APStats.ShieldLevel)
						++player[1].shield;

					player_drawShield();
				}
			}
		}

		/*---------------------Weapon Display-------------------------*/
		for (uint i = 0; i < 2; ++i)
		{
			uint item_power = player[twoPlayerMode ? i : 0].items.weapon[i].power;

			if (old_weapon_bar[i] != item_power)
			{
				old_weapon_bar[i] = item_power;

				int x = twoPlayerMode ? 286 : 289,
				    y = (i == 0) ? (twoPlayerMode ? 6 : 17) : (twoPlayerMode ? 100 : 38);

				fill_rectangle_xy(VGAScreenSeg, x, y, x + 1 + 10 * 2, y + 2, 0);

				for (uint j = 1; j <= item_power; ++j)
				{
					JE_rectangle(VGAScreen, x, y, x + 1, y + 2, 115 + j); /* SEGa000 */
					x += 2;
				}
			}
		}

		/*------------------------Power Bar-------------------------*/
		if (twoPlayerMode || onePlayerAction)
		{
			generatorPower = 900;
		}
		else
		{
			generatorPower += powerSys[APStats.GeneratorLevel].power;
			if (generatorPower > 900)
				generatorPower = 900;

			temp = generatorPower / 10;

			if (temp != lastGenPower)
			{
				if (temp > lastGenPower)
					fill_rectangle_xy(VGAScreenSeg, 269, 113 - 11 - temp, 276, 114 - 11 - lastGenPower, 113 + temp / 7);
				else
					fill_rectangle_xy(VGAScreenSeg, 269, 113 - 11 - lastGenPower, 276, 114 - 11 - temp, 0);
				
				lastGenPower = temp;
			}
		}

		oldMapX3Ofs = mapX3Ofs;

		enemyOnScreen = 0;
	}

	/* use game_screen for all the generic drawing functions */
	VGAScreen = game_screen;

	/*---------------------------EVENTS-------------------------*/
	while (eventRec[eventLoc-1].eventtime <= curLoc && eventLoc <= maxEvent)
		JE_eventSystem();

	if (isNetworkGame && reallyEndLevel)
		goto start_level;

	/* SMOOTHIES! */
	JE_checkSmoothies();
	if (anySmoothies)
		VGAScreen = VGAScreen2;  // this makes things complicated, but we do it anyway :(

	/* --- BACKGROUNDS --- */
	/* --- BACKGROUND 1 --- */

	if (forceEvents && !backMove)
		curLoc++;

	if (map1YDelayMax > 1 && backMove < 2)
		backMove = (map1YDelay == 1) ? 1 : 0;

	/*Draw background*/
	if (astralDuration == 0)
		draw_background_1(VGAScreen);
	else
		JE_clr256(VGAScreen);

	/*Set Movement of background 1*/
	if (--map1YDelay == 0)
	{
		map1YDelay = map1YDelayMax;

		curLoc += backMove;

		backPos += backMove;

		if (backPos > 27)
		{
			backPos -= 28;
			mapY--;
			mapYPos -= 14;  /*Map Width*/
		}
	}

	if (starActive || astralDuration > 0)
		update_and_draw_starfield(VGAScreen, starfield_speed);

	if (processorType > 1 && smoothies[5-1])
	{
		iced_blur_filter(game_screen, VGAScreen);
		VGAScreen = game_screen;
	}

	/*-----------------------BACKGROUNDS------------------------*/
	/*-----------------------BACKGROUND 2------------------------*/
	if (background2over == 3)
	{
		draw_background_2(VGAScreen);
		background2 = true;
	}

	if (background2over == 0)
	{
		if (!(smoothies[2-1] && processorType < 4) && !(smoothies[1-1] && processorType == 3))
		{
			if (wild && !background2notTransparent)
				draw_background_2_blend(VGAScreen);
			else
				draw_background_2(VGAScreen);
		}
	}

	if (smoothies[0] && processorType > 2 && smoothie_data[0] == 0)
	{
		lava_filter(game_screen, VGAScreen);
		VGAScreen = game_screen;
	}
	if (smoothies[2-1] && processorType > 2)
	{
		water_filter(game_screen, VGAScreen);
		VGAScreen = game_screen;
	}

	/*-----------------------Ground Enemy------------------------*/
	lastEnemyOnScreen = enemyOnScreen;

	tempMapXOfs = mapXOfs;
	tempBackMove = backMove;
	JE_drawEnemy(50);
	JE_drawEnemy(100);

	if (enemyOnScreen == 0 || enemyOnScreen == lastEnemyOnScreen)
	{
		if (stopBackgroundNum == 1)
			stopBackgroundNum = 9;
	}

	if (smoothies[0] && processorType > 2 && smoothie_data[0] > 0)
	{
		lava_filter(game_screen, VGAScreen);
		VGAScreen = game_screen;
	}

	if (superWild)
	{
		neat += 3;
		JE_darkenBackground(neat);
	}

	/*-----------------------BACKGROUNDS------------------------*/
	/*-----------------------BACKGROUND 2------------------------*/
	if (!(smoothies[2-1] && processorType < 4) &&
	    !(smoothies[1-1] && processorType == 3))
	{
		if (background2over == 1)
		{
			if (wild && !background2notTransparent)
				draw_background_2_blend(VGAScreen);
			else
				draw_background_2(VGAScreen);
		}
	}

	if (superWild)
	{
		neat++;
		JE_darkenBackground(neat);
	}

	if (background3over == 2)
		draw_background_3(VGAScreen);

	/* New Enemy */
	if (enemiesActive && mt_rand() % 100 > levelEnemyFrequency)
	{
		tempW = levelEnemy[mt_rand() % levelEnemyMax];
		if (tempW == 2)
			soundQueue[3] = S_WEAPON_7;
		b = JE_newEnemy(0, tempW, 0);
	}

	if (processorType > 1 && smoothies[3-1])
	{
		iced_blur_filter(game_screen, VGAScreen);
		VGAScreen = game_screen;
	}
	if (processorType > 1 && smoothies[4-1])
	{
		blur_filter(game_screen, VGAScreen);
		VGAScreen = game_screen;
	}

	/* Draw Sky Enemy */
	if (!skyEnemyOverAll)
	{
		lastEnemyOnScreen = enemyOnScreen;

		tempMapXOfs = mapX2Ofs;
		tempBackMove = 0;
		JE_drawEnemy(25);

		if (enemyOnScreen == lastEnemyOnScreen)
		{
			if (stopBackgroundNum == 2)
				stopBackgroundNum = 9;
		}
	}

	if (background3over == 0)
		draw_background_3(VGAScreen);

	/* Draw Top Enemy */
	if (!topEnemyOver)
	{
		tempMapXOfs = (background3x1 == 0) ? oldMapX3Ofs : mapXOfs;
		tempBackMove = backMove3;
		JE_drawEnemy(75);
	}

	/* Player Shot Images */
	for (int z = 0; z < MAX_PWEAPON; z++)
	{
		if (shotAvail[z] != 0)
		{
			bool is_special = false;
			int tempShotX = 0, tempShotY = 0;
			JE_byte chain;
			JE_byte playerNum;
			JE_word tempX2, tempY2;
			JE_integer damage;
			
			if (!player_shot_move_and_draw(z, &is_special, &tempShotX, &tempShotY, &damage, &temp2, &chain, &playerNum, &tempX2, &tempY2))
			{
				goto draw_player_shot_loop_end;
			}

			for (b = 0; b < 100; b++)
			{
				if (enemyAvail[b] == 0)
				{
					bool collided;

					if (z == MAX_PWEAPON - 1)
					{
						temp = 25 - abs(zinglonDuration - 25);
						collided = abs(enemy[b].ex + enemy[b].mapoffset - (player[0].x + 7)) < temp;
						temp2 = 9;
						chain = 0;
						damage = 10;
					}
					else if (is_special)
					{
						collided = ((enemy[b].enemycycle == 0) &&
						            (abs(enemy[b].ex + enemy[b].mapoffset - tempShotX - tempX2) < (25 + tempX2)) &&
						            (abs(enemy[b].ey - tempShotY - 12 - tempY2)                 < (29 + tempY2))) ||
						           ((enemy[b].enemycycle > 0) &&
						            (abs(enemy[b].ex + enemy[b].mapoffset - tempShotX - tempX2) < (13 + tempX2)) &&
						            (abs(enemy[b].ey - tempShotY - 6 - tempY2)                  < (15 + tempY2)));
					}
					else
					{
						collided = ((enemy[b].enemycycle == 0) &&
						            (abs(enemy[b].ex + enemy[b].mapoffset - tempShotX) < 25) && (abs(enemy[b].ey - tempShotY - 12) < 29)) ||
						           ((enemy[b].enemycycle > 0) &&
						            (abs(enemy[b].ex + enemy[b].mapoffset - tempShotX) < 13) && (abs(enemy[b].ey - tempShotY - 6) < 15));
					}

					if (collided)
					{
						if (chain > 0)
						{
							shotMultiPos[SHOT_MISC] = 0;
							b = player_shot_create(0, SHOT_MISC, tempShotX, tempShotY, mouseX, mouseY, chain, playerNum);
							shotAvail[z] = 0;
							goto draw_player_shot_loop_end;
						}

						infiniteShot = false;

						if (damage == 99)
						{
							damage = 0;
							doIced = 40;
							enemy[b].iced = 40;
						}
						else
						{
							doIced = 0;
							if (damage >= 250)
							{
								damage = damage - 250;
								infiniteShot = true;
							}
						}

#ifdef DEBUG_OPTIONS
						if (debugDamageViewer && enemy[b].linknum == 254)
							damagePerSecondHist[0] += damage;
#endif

						int armorleft = enemy[b].armorleft;

						temp = enemy[b].linknum;
						if (temp == 0)
							temp = 255;

						if (enemy[b].armorleft < 255)
						{
							for (unsigned int i = 0; i < COUNTOF(boss_bar); i++)
								if (temp == boss_bar[i].link_num)
									boss_bar[i].color = 6;

							if (enemy[b].enemyground)
								enemy[b].filter = temp2;

							for (unsigned int e = 0; e < COUNTOF(enemy); e++)
							{
								if (enemy[e].linknum == temp &&
								    enemyAvail[e] != 1 &&
								    enemy[e].enemyground != 0)
								{
									if (doIced)
										enemy[e].iced = doIced;
									enemy[e].filter = temp2;
								}
							}
						}

						if (armorleft > damage)
						{
							if (z != MAX_PWEAPON - 1)
							{
								if (enemy[b].armorleft != 255)
								{
									enemy[b].armorleft -= damage;
									JE_setupExplosion(tempShotX, tempShotY, 0, 0, false, false);
								}
								else
								{
									JE_doSP(tempShotX + 6, tempShotY + 6, damage / 2 + 3, damage / 4 + 2, temp2);
								}
							}

							soundQueue[5] = S_ENEMY_HIT;

							if ((armorleft - damage <= enemy[b].edlevel) &&
							    ((!enemy[b].edamaged) ^ (enemy[b].edani < 0)))
							{

								for (temp3 = 0; temp3 < 100; temp3++)
								{
									if (enemyAvail[temp3] != 1)
									{
										int linknum = enemy[temp3].linknum;
										if (
										     (temp3 == b) ||
										     (
										       (temp != 255) &&
										       (
										         ((enemy[temp3].edlevel > 0) && (linknum == temp)) ||
										         (
										           (enemyContinualDamage && (temp - 100 == linknum)) ||
										           ((linknum > 40) && (linknum / 20 == temp / 20) && (linknum <= temp))
										         )
										       )
										     )
										   )
										{
											enemy[temp3].enemycycle = 1;

											enemy[temp3].edamaged = !enemy[temp3].edamaged;

											if (enemy[temp3].edani != 0)
											{
												enemy[temp3].ani = abs(enemy[temp3].edani);
												enemy[temp3].aniactive = 1;
												enemy[temp3].animax = 0;
												enemy[temp3].animin = enemy[temp3].edgr;
												enemy[temp3].enemycycle = enemy[temp3].animin - 1;

											}
											else if (enemy[temp3].edgr > 0)
											{
												enemy[temp3].egr[1-1] = enemy[temp3].edgr;
												enemy[temp3].ani = 1;
												enemy[temp3].aniactive = 0;
												enemy[temp3].animax = 0;
												enemy[temp3].animin = 1;
											}
											else
											{
												enemyAvail[temp3] = 1;
												enemyKilled++;
											}

											enemy[temp3].aniwhenfire = 0;

											if (enemy[temp3].armorleft > (unsigned char)enemy[temp3].edlevel)
												enemy[temp3].armorleft = enemy[temp3].edlevel;

											tempX = enemy[temp3].ex + enemy[temp3].mapoffset;
											tempY = enemy[temp3].ey;

											if (enemyDat[enemy[temp3].enemytype].esize != 1)
												JE_setupExplosion(tempX, tempY - 6, 0, 1, false, false);
											else
												JE_setupExplosionLarge(enemy[temp3].enemyground, enemy[temp3].explonum / 2, tempX, tempY);
										}
									}
								}
							}
						}
						else
						{

							if ((temp == 254) && (superEnemy254Jump > 0))
								JE_eventJump(superEnemy254Jump);

							for (int other_enemy = 0; other_enemy < 100; other_enemy++)
							{
								if (enemyAvail[other_enemy] == 1)
									continue;

								const int other_linknum = enemy[other_enemy].linknum;
								if ((other_enemy == b) || (temp == 254)
									|| ((temp != 255) && ((temp == other_linknum) || (temp - 100 == other_linknum)
									|| ((other_linknum > 40) && (other_linknum / 20 == temp / 20) && (other_linknum <= temp)))))
								{

									int enemy_screen_x = enemy[other_enemy].ex + enemy[other_enemy].mapoffset;

									// Bugfix: Don't destroy scoreitems (most often happens when linknum 254 gets killed)
									if (enemyAvail[other_enemy] == 2 && enemy[other_enemy].scoreitem)
										continue;

									if (enemy[other_enemy].special)
										mainint_handleEnemyFlag(&enemy[other_enemy]);

									if (enemy[other_enemy].enemydie > 0)
										tyrian_enemyDieItem(other_enemy);

									// Note: > 1 because == 1 used to silently award a data cube (why???)
									if (enemy[other_enemy].evalue > 1 && enemy[other_enemy].evalue < 10000)
									{
										// in galaga mode player 2 is sidekick, so give cash to player 1
										player[galagaMode ? 0 : playerNum - 1].cash += enemy[other_enemy].evalue;
									}

									if ((enemy[other_enemy].edlevel == -1) && (temp == other_linknum))
									{
										enemy[other_enemy].edlevel = 0;
										enemyAvail[other_enemy] = 2;
										enemy[other_enemy].egr[1-1] = enemy[other_enemy].edgr;
										enemy[other_enemy].ani = 1;
										enemy[other_enemy].aniactive = 0;
										enemy[other_enemy].animax = 0;
										enemy[other_enemy].animin = 1;
										enemy[other_enemy].edamaged = true;
										enemy[other_enemy].enemycycle = 1;
									}
									else
									{
										enemyAvail[other_enemy] = 1;
										enemyKilled++;
									}

									if (enemyDat[enemy[other_enemy].enemytype].esize == 1)
									{
										JE_setupExplosionLarge(enemy[other_enemy].enemyground, enemy[other_enemy].explonum, enemy_screen_x, enemy[other_enemy].ey);
										soundQueue[6] = S_EXPLOSION_9;
									}
									else
									{
										JE_setupExplosion(enemy_screen_x, enemy[other_enemy].ey, 0, 1, false, false);
										soundQueue[6] = S_EXPLOSION_8;
									}
								}
							}
						}

						if (infiniteShot)
						{
							damage += 250;
						}
						else if (z != MAX_PWEAPON - 1)
						{
							if (damage <= armorleft)
							{
								shotAvail[z] = 0;
								goto draw_player_shot_loop_end;
							}
							else
							{
								playerShotData[z].shotDmg -= armorleft;
							}
						}
					}
				}
			}

draw_player_shot_loop_end:
			;
		}
	}

	/* Player movement indicators for shots that track your ship */
	for (uint i = 0; i < COUNTOF(player); ++i)
	{
		player[i].last_x_shot_move = player[i].x;
		player[i].last_y_shot_move = player[i].y;
	}
	
	/*=================================*/
	/*=======Collisions Detection======*/
	/*=================================*/
	
	for (uint i = 0; i < (twoPlayerMode ? 2 : 1); ++i)
		if (player[i].is_alive && !endLevel)
			JE_playerCollide(&player[i], i + 1);
	
	if (firstGameOver)
		JE_mainGamePlayerFunctions();      /*--------PLAYER DRAW+MOVEMENT---------*/

	if (!endLevel)
	{    /*MAIN DRAWING IS STOPPED STARTING HERE*/

		/* Draw Enemy Shots */
		for (int z = 0; z < ENEMY_SHOT_MAX; z++)
		{
			if (enemyShotAvail[z] == 0)
			{
				enemyShot[z].sxm += enemyShot[z].sxc;
				enemyShot[z].sx += enemyShot[z].sxm;

				if (enemyShot[z].tx != 0)
				{
					if (enemyShot[z].sx > player[0].x)
					{
						if (enemyShot[z].sxm > -enemyShot[z].tx)
							enemyShot[z].sxm--;
					}
					else
					{
						if (enemyShot[z].sxm < enemyShot[z].tx)
							enemyShot[z].sxm++;
					}
				}

				enemyShot[z].sym += enemyShot[z].syc;
				enemyShot[z].sy += enemyShot[z].sym;

				if (enemyShot[z].ty != 0)
				{
					if (enemyShot[z].sy > player[0].y)
					{
						if (enemyShot[z].sym > -enemyShot[z].ty)
							enemyShot[z].sym--;
					}
					else
					{
						if (enemyShot[z].sym < enemyShot[z].ty)
							enemyShot[z].sym++;
					}
				}

				if (enemyShot[z].duration-- == 0 || enemyShot[z].sy > 190 || enemyShot[z].sy <= -14 || enemyShot[z].sx > 275 || enemyShot[z].sx <= 0)
				{
					enemyShotAvail[z] = true;
				}
				else  // check if shot collided with player
				{
					for (uint i = 0; i < (twoPlayerMode ? 2 : 1); ++i)
					{
						if (player[i].is_alive &&
						    enemyShot[z].sx > player[i].x - (signed)player[i].shot_hit_area_x &&
						    enemyShot[z].sx < player[i].x + (signed)player[i].shot_hit_area_x &&
						    enemyShot[z].sy > player[i].y - (signed)player[i].shot_hit_area_y &&
						    enemyShot[z].sy < player[i].y + (signed)player[i].shot_hit_area_y)
						{
							tempX = enemyShot[z].sx;
							tempY = enemyShot[z].sy;
							temp = enemyShot[z].sdmg;

							enemyShotAvail[z] = true;

							JE_setupExplosion(tempX, tempY, 0, 0, false, false);

							if (player[i].invulnerable_ticks == 0)
							{
								if ((temp = player_takeDamage(&player[i], temp, DAMAGE_BULLET)) > 0)
								{
									player[i].x_velocity += (enemyShot[z].sxm * temp) / 2;
									player[i].y_velocity += (enemyShot[z].sym * temp) / 2;
								}
							}

							break;
						}
					}

					if (enemyShotAvail[z] == false)
					{
						if (enemyShot[z].animax != 0)
						{
							if (++enemyShot[z].animate >= enemyShot[z].animax)
								enemyShot[z].animate = 0;
						}

						if (enemyShot[z].sgr >= 500)
							blit_sprite2(VGAScreen, enemyShot[z].sx, enemyShot[z].sy, spriteSheet12, enemyShot[z].sgr + enemyShot[z].animate - 500);
						else
							blit_sprite2(VGAScreen, enemyShot[z].sx, enemyShot[z].sy, spriteSheet8, enemyShot[z].sgr + enemyShot[z].animate);
					}
				}

			}
		}
	}

	if (background3over == 1)
		draw_background_3(VGAScreen);

	/* Draw Top Enemy */
	if (topEnemyOver)
	{
		tempMapXOfs = (background3x1 == 0) ? oldMapX3Ofs : oldMapXOfs;
		tempBackMove = backMove3;
		JE_drawEnemy(75);
	}

	/* Draw Sky Enemy */
	if (skyEnemyOverAll)
	{
		lastEnemyOnScreen = enemyOnScreen;

		tempMapXOfs = mapX2Ofs;
		tempBackMove = 0;
		JE_drawEnemy(25);

		if (enemyOnScreen == lastEnemyOnScreen)
		{
			if (stopBackgroundNum == 2)
				stopBackgroundNum = 9;
		}
	}

	/*-------------------------- Sequenced Explosions -------------------------*/
	enemyStillExploding = false;
	for (int i = 0; i < MAX_REPEATING_EXPLOSIONS; i++)
	{
		if (rep_explosions[i].ttl != 0)
		{
			enemyStillExploding = true;

			if (rep_explosions[i].delay > 0)
			{
				rep_explosions[i].delay--;
				continue;
			}

			rep_explosions[i].y += backMove2 + 1;
			tempX = rep_explosions[i].x + (mt_rand() % 24) - 12;
			tempY = rep_explosions[i].y + (mt_rand() % 27) - 24;

			if (rep_explosions[i].big)
			{
				JE_setupExplosionLarge(false, 2, tempX, tempY);

				if (rep_explosions[i].ttl == 1 || mt_rand() % 5 == 1)
					soundQueue[7] = S_EXPLOSION_11;
				else
					soundQueue[6] = S_EXPLOSION_9;

				rep_explosions[i].delay = 4 + (mt_rand() % 3);
			}
			else
			{
				JE_setupExplosion(tempX, tempY, 0, 1, false, false);

				soundQueue[5] = S_EXPLOSION_4;

				rep_explosions[i].delay = 3;
			}

			rep_explosions[i].ttl--;
		}
	}

	/*---------------------------- Draw Explosions ----------------------------*/
	for (int j = 0; j < MAX_EXPLOSIONS; j++)
	{
		if (explosions[j].ttl != 0)
		{
			if (explosions[j].fixed_position != true)
			{
				explosions[j].sprite++;
				explosions[j].y += explodeMove;
			}
			else if (explosions[j].follow_player == true)
			{
				explosions[j].x += explosionFollowAmountX;
				explosions[j].y += explosionFollowAmountY;
			}
			explosions[j].y += explosions[j].delta_y;
			explosions[j].x += explosions[j].delta_x;

			if (explosions[j].y > 200 - 14)
			{
				explosions[j].ttl = 0;
			}
			else
			{
				if (explosionTransparent)
					blit_sprite2_blend(VGAScreen, explosions[j].x, explosions[j].y, *explosions[j].sheet, explosions[j].sprite + 1);
				else
					blit_sprite2(VGAScreen, explosions[j].x, explosions[j].y, *explosions[j].sheet, explosions[j].sprite + 1);

				explosions[j].ttl--;
			}
		}
	}

	if (!portConfigChange)
		portConfigDone = true;

	/*-----------------------BACKGROUNDS------------------------*/
	/*-----------------------BACKGROUND 2------------------------*/
	if (!(smoothies[2-1] && processorType < 4) &&
	    !(smoothies[1-1] && processorType == 3))
	{
		if (background2over == 2)
		{
			if (wild && !background2notTransparent)
				draw_background_2_blend(VGAScreen);
			else
				draw_background_2(VGAScreen);
		}
	}

	/*-------------------------Warning---------------------------*/
	if ((player[0].is_alive && player[0].armor < 6) ||
	    (twoPlayerMode && !galagaMode && player[1].is_alive && player[1].armor < 6))
	{
		int armor_amount = (player[0].is_alive && player[0].armor < 6) ? player[0].armor : player[1].armor;

		if (armorShipDelay > 0)
		{
			armorShipDelay--;
		}
		else
		{
			tempW = 560;
			b = JE_newEnemy(50, tempW, 0);
			if (b > 0)
			{
				enemy[b-1].enemydie = 560 + (mt_rand() % 3) + 1;
				enemy[b-1].eyc -= backMove3;
				enemy[b-1].armorleft = 4;
			}
			armorShipDelay = 500;
		}

		if ((player[0].is_alive && player[0].armor < 6 && (!isNetworkGame || thisPlayerNum == 1)) ||
		    (twoPlayerMode && player[1].is_alive && player[1].armor < 6 && (!isNetworkGame || thisPlayerNum == 2)))
		{

			tempW = armor_amount * 4 + 8;
			if (warningSoundDelay > tempW)
				warningSoundDelay = tempW;

			if (warningSoundDelay > 1)
			{
				warningSoundDelay--;
			}
			else
			{
				soundQueue[7] = S_WARNING;
				warningSoundDelay = tempW;
			}

			warningCol += warningColChange;
			if (warningCol > 113 + (14 - (armor_amount * 2)))
			{
				warningColChange = -warningColChange;
				warningCol = 113 + (14 - (armor_amount * 2));
			}
			else if (warningCol < 113)
			{
				warningColChange = -warningColChange;
			}
			fill_rectangle_xy(VGAScreen, 24, 181, 138, 183, warningCol);
			fill_rectangle_xy(VGAScreen, 175, 181, 287, 183, warningCol);
			fill_rectangle_xy(VGAScreen, 24, 0, 287, 3, warningCol);

			JE_outText(VGAScreen, 140, 178, "WARNING", 7, (warningCol % 16) / 2);

		}
	}

	/*------- Random Explosions --------*/
	if (randomExplosions && mt_rand() % 10 == 1)
		JE_setupExplosionLarge(false, 20, mt_rand() % 280, mt_rand() % 180);

	/*=================================*/
	/*=======The Sound Routine=========*/
	/*=================================*/
	if (firstGameOver)
	{
		temp = 0;
		for (temp2 = 0; temp2 < COUNTOF(soundQueue); temp2++)
		{
			if (soundQueue[temp2] != S_NONE)
			{
				temp = soundQueue[temp2];
				if (temp2 == 3)
					temp3 = fxPlayVol;
				else if (temp == 15)
					temp3 = fxPlayVol / 4;
				else   /*Lightning*/
					temp3 = fxPlayVol / 2;

				multiSamplePlay(soundSamples[temp-1], soundSampleCount[temp-1], temp2, temp3);

				soundQueue[temp2] = S_NONE;
			}
		}
	}

	if (returnActive && enemyOnScreen == 0)
	{
		JE_eventJump(65535);
		returnActive = false;
	}

	/*-------      DEbug      ---------*/
	debugTime = SDL_GetTicks();
	tempW = lastmouse_but;
	tempX = mouse_x;
	tempY = mouse_y;

	if (debug)
	{
		for (size_t i = 0; i < 9; i++)
		{
			tempStr[i] = '0' + smoothies[i];
		}
		tempStr[9] = '\0';
		sprintf(buffer, "SM = %s", tempStr);
		JE_outText(VGAScreen, 30, 70, buffer, 4, 0);

		sprintf(buffer, "Memory left = %d", -1);
		JE_outText(VGAScreen, 30, 80, buffer, 4, 0);
		sprintf(buffer, "Enemies onscreen = %d", enemyOnScreen);
		JE_outText(VGAScreen, 30, 90, buffer, 6, 0);

		debugHist = debugHist + abs((JE_longint)debugTime - (JE_longint)lastDebugTime);
		debugHistCount++;
		sprintf(tempStr, "%2.3f", 1000.0f / roundf(debugHist / debugHistCount));
		sprintf(buffer, "X:%d Y:%-5d  %s FPS  %d %d %d %d", (mapX - 1) * 12 + player[0].x, curLoc, tempStr, player[0].x_velocity, player[0].y_velocity, player[0].x, player[0].y);
		JE_outText(VGAScreen, 45, 175, buffer, 15, 3);
		lastDebugTime = debugTime;
	}

	if (displayTime > 0)
	{
		displayTime--;
		JE_outTextAndDarken(VGAScreen, 90, 10, miscText[59], 15, (JE_byte)flash - 8, FONT_SHAPES);
		flash += flashChange;
		if (flash > 4 || flash == 0)
			flashChange = -flashChange;
	}

	/*Pentium Speed Mode?*/
	if (pentiumMode)
	{
		frameCountMax = (frameCountMax == 2) ? 3 : 2;
	}

	/*--------  Level Timer    ---------*/
	if (levelTimer && levelTimerCountdown > 0)
	{
		levelTimerCountdown--;
		if (levelTimerCountdown == 0)
			JE_eventJump(levelTimerJumpTo);

		if (levelTimerCountdown > 200)
		{
			if (levelTimerCountdown % 100 == 0)
				soundQueue[7] = S_WARNING;

			if (levelTimerCountdown % 10 == 0)
				soundQueue[6] = S_CLICK;
		}
		else if (levelTimerCountdown % 20 == 0)
		{
			soundQueue[7] = S_WARNING;
		}

		JE_textShade (VGAScreen, 140, 6, miscText[66], 7, (levelTimerCountdown % 20) / 3, FULL_SHADE);

		// Don't use floats due to rounding.
		sprintf(buffer, "%d.%d", levelTimerCountdown / 100, (levelTimerCountdown / 10) % 10);
		JE_dString (VGAScreen, 100, 2, buffer, SMALL_FONT_SHAPES);
	}

	/*GAME OVER*/
	if (allPlayersGone)
	{
		if (player[0].exploding_ticks > 0 || player[1].exploding_ticks > 0)
		{
			if (galagaMode)
				player[1].exploding_ticks = 0;

			musicFade = true;
		}
		else
		{
			if (play_demo || normalBonusLevelCurrent || bonusLevelCurrent)
				reallyEndLevel = true;
			else
				JE_dString(VGAScreen, 120, 60, miscText[21], FONT_SHAPES); // game over

			if (firstGameOver)
			{
				if (!play_demo)
				{
					play_song(SONG_GAMEOVER);
					// Make the game over solo a bit louder, but not full volume.
					set_volume((tyrMusicVolume/4) + (tyrMusicVolume/2), fxVolume);
					musicFade = false;
				}
				firstGameOver = false;
			}

			if (!play_demo)
			{
				push_joysticks_as_keyboard();
				service_SDL_events(true);
				if ((newkey || button[0] || button[1] || button[2]) || newmouse)
				{
					reallyEndLevel = true;
				}
			}

			if (isNetworkGame)
				reallyEndLevel = true;
		}
	}

	if (play_demo) // input kills demo
	{
		push_joysticks_as_keyboard();
		service_SDL_events(false);

		if (newkey || newmouse)
		{
			reallyEndLevel = true;

			stopped_demo = true;
		}
	}
	else // input handling for pausing, menu, cheats
	{
		service_SDL_events(false);

		if (newkey)
		{
			skipStarShowVGA = false;
			JE_mainKeyboardInput();
			newkey = false;
			if (skipStarShowVGA)
				goto level_loop;
		}

		if (pause_pressed || !windowHasFocus)
		{
			pause_pressed = false;

			if (isNetworkGame)
				pauseRequest = true;
			else
				JE_pauseGame();
		}

		if (ingamemenu_pressed)
		{
			ingamemenu_pressed = false;

			if (isNetworkGame)
			{
				inGameMenuRequest = true;
			}
			else
			{
				yourInGameMenuRequest = true;
				JE_doInGameSetup();
				skipStarShowVGA = true;
			}
		}
	}

	/*Network Update*/
#if 0
	if (isNetworkGame)
	{
		if (!reallyEndLevel)
		{
			Uint16 requests = (pauseRequest == true) |
			                  (inGameMenuRequest == true) << 1 |
			                  (skipLevelRequest == true) << 2 |
			                  (nortShipRequest == true) << 3;
			SDLNet_Write16(requests,        &packet_state_out[0]->data[14]);

			SDLNet_Write16(difficultyLevel, &packet_state_out[0]->data[16]);
			SDLNet_Write16(player[0].x,     &packet_state_out[0]->data[18]);
			SDLNet_Write16(player[1].x,     &packet_state_out[0]->data[20]);
			SDLNet_Write16(player[0].y,     &packet_state_out[0]->data[22]);
			SDLNet_Write16(player[1].y,     &packet_state_out[0]->data[24]);
			SDLNet_Write16(curLoc,          &packet_state_out[0]->data[26]);

			network_state_send();

			if (network_state_update())
			{
				assert(SDLNet_Read16(&packet_state_in[0]->data[26]) == SDLNet_Read16(&packet_state_out[network_delay]->data[26]));

				requests = SDLNet_Read16(&packet_state_in[0]->data[14]) ^ SDLNet_Read16(&packet_state_out[network_delay]->data[14]);
				if (requests & 1)
				{
					JE_pauseGame();
				}
				if (requests & 2)
				{
					yourInGameMenuRequest = SDLNet_Read16(&packet_state_out[network_delay]->data[14]) & 2;
					JE_doInGameSetup();
					yourInGameMenuRequest = false;
					if (haltGame)
						reallyEndLevel = true;
				}
				if (requests & 4)
				{
					levelTimer = true;
					levelTimerCountdown = 0;
					endLevel = true;
					levelEnd = 40;
				}
				if (requests & 8) // nortship
				{
					player[0].items.ship = 12;                     // Nort Ship
					player[0].items.special = 13;                  // Astral Zone
					player[0].items.weapon[FRONT_WEAPON].id = 36;  // NortShip Super Pulse
					player[0].items.weapon[REAR_WEAPON].id = 37;   // NortShip Spreader
					shipGr = 1;
				}

				for (int i = 0; i < 2; i++)
				{
					if (SDLNet_Read16(&packet_state_in[0]->data[18 + i * 2]) != SDLNet_Read16(&packet_state_out[network_delay]->data[18 + i * 2]) || SDLNet_Read16(&packet_state_in[0]->data[20 + i * 2]) != SDLNet_Read16(&packet_state_out[network_delay]->data[20 + i * 2]))
					{
						char temp[64];
						sprintf(temp, "Player %d is unsynchronized!", i + 1);

						JE_textShade(game_screen, 40, 110 + i * 10, temp, 9, 2, FULL_SHADE);
					}
				}
			}
		}

		JE_clearSpecialRequests();
	}
#endif

	/** Test **/
	JE_drawSP();

	/*Filtration*/
	if (filterActive)
	{
		JE_filterScreen(levelFilter, levelBrightness);
	}

	draw_boss_bar();

	JE_inGameDisplays();

	VGAScreen = VGAScreenSeg; /* side-effect of game_screen */

	JE_starShowVGA();

	/*Start backgrounds if no enemies on screen
	  End level if number of enemies left to kill equals 0.*/
	if (stopBackgroundNum == 9 && backMove == 0 && !enemyStillExploding)
	{
		backMove = 1;
		backMove2 = 2;
		backMove3 = 3;
		explodeMove = 2;
		stopBackgroundNum = 0;
		stopBackgrounds = false;
		if (waitToEndLevel)
		{
			endLevel = true;
			levelEnd = 40;
		}
		if (allPlayersGone)
		{
			reallyEndLevel = true;
		}
	}

	if (!endLevel && enemyOnScreen == 0)
	{
		if (readyToEndLevel && !enemyStillExploding)
		{
			if (levelTimerCountdown > 0)
			{
				levelTimer = false;
			}
			readyToEndLevel = false;
			endLevel = true;
			levelEnd = 40;
			if (allPlayersGone)
			{
				reallyEndLevel = true;
			}
		}
		if (stopBackgrounds)
		{
			stopBackgrounds = false;
			backMove = 1;
			backMove2 = 2;
			backMove3 = 3;
			explodeMove = 2;
		}
	}

	if (reallyEndLevel)
	{
		goto start_level;
	}
	goto level_loop;
}

/* --- Load Level/Map Data --- */
bool JE_loadMap(void)
{
	lastCubeMax = cubeMax;

	/* Load LEVELS.DAT - Section = MAINLEVEL */
	saveLevel = mainLevel;

	galagaMode  = false;
	extraGame   = false;
	haltGame = false;
	gameLoaded = false;

	nextLevel = 1;

	useLastBank = false;

	int levelID;
	if (play_demo)
	{
		levelID = mainint_loadNextDemo();
	}
	else
	{
		// If the last level completed was the end level of the episode, play the cutscene corresponding to it.
		// This may also clear the player's goal and play the credits.
		if (lastLevelCompleted && allLevelData[currentLevelID].endEpisode)
			levelend_playEndScene(allLevelData[currentLevelID].episodeNum);

		levelID = apmenu_itemScreen();
	}

	if (levelID < 0)
	{
		Archipelago_Disconnect();
		return 0;
	}		
	level_loadFromLevelID(levelID);

	loadLevelOk = true;
	gameJustLoaded = false;
	bonusLevelCurrent = false;
	normalBonusLevelCurrent = false;
	return 1;
}

void JE_loadMapData(FILE *level_f)
{
	fread_u16_die(&mapX,  1, level_f);
	fread_u16_die(&mapX2, 1, level_f);
	fread_u16_die(&mapX3, 1, level_f);

	fread_u16_die(&levelEnemyMax, 1, level_f);
	fread_u16_die(levelEnemy, levelEnemyMax, level_f);
}

void JE_loadMapShapes(FILE *level_f, JE_char char_shapeFile)
{
	JE_DanCShape shape;

	JE_word x, y;
	JE_integer yy;
	JE_word mapSh[3][128]; /* [1..3, 0..127] */
	JE_byte *ref[3][128]; /* [1..3, 0..127] */
	JE_byte mapBuf[15 * 600]; /* [1..15 * 600] */
	JE_word bufLoc;

	/* MAP SHAPE LOOKUP TABLE - Each map is directly after level */
	for (temp = 0; temp < 3; temp++)
	{
		fread_u16_die(mapSh[temp], sizeof(*mapSh) / sizeof(JE_word), level_f);
		for (temp2 = 0; temp2 < 128; temp2++)
		{
			mapSh[temp][temp2] = SDL_Swap16(mapSh[temp][temp2]);
		}
	}

	/* Read Shapes.DAT */
	sprintf(tempStr, "shapes%c.dat", tolower((unsigned char)char_shapeFile));
	FILE *shpFile = dir_fopen_die(data_dir(), tempStr, "rb");

	for (int z = 0; z < 600; z++)
	{
		JE_boolean shapeBlank;
		fread_bool_die(&shapeBlank, shpFile);

		if (shapeBlank)
			memset(shape, 0, sizeof(shape));
		else
			fread_u8_die(shape, sizeof(shape), shpFile);

		/* Match 1 */
		for (int x = 0; x <= 71; ++x)
		{
			if (mapSh[0][x] == z+1)
			{
				memcpy(megaData1.shapes[x].sh, shape, sizeof(JE_DanCShape));

				ref[0][x] = megaData1.shapes[x].sh;
			}
		}

		/* Match 2 */
		for (int x = 0; x <= 71; ++x)
		{
			if (mapSh[1][x] == z+1)
			{
				if (x != 71 && !shapeBlank)
				{
					memcpy(megaData2.shapes[x].sh, shape, sizeof(JE_DanCShape));

					y = 1;
					for (yy = 0; yy < (24 * 28) >> 1; yy++)
						if (shape[yy] == 0)
							y = 0;

					megaData2.shapes[x].fill = y;
					ref[1][x] = megaData2.shapes[x].sh;
				}
				else
				{
					ref[1][x] = NULL;
				}
			}
		}

		/*Match 3*/
		for (int x = 0; x <= 71; ++x)
		{
			if (mapSh[2][x] == z+1)
			{
				if (x < 70 && !shapeBlank)
				{
					memcpy(megaData3.shapes[x].sh, shape, sizeof(JE_DanCShape));

					y = 1;
					for (yy = 0; yy < (24 * 28) >> 1; yy++)
						if (shape[yy] == 0)
							y = 0;

					megaData3.shapes[x].fill = y;
					ref[2][x] = megaData3.shapes[x].sh;
				}
				else
				{
					ref[2][x] = NULL;
				}
			}
		}
	}

	fclose(shpFile);

	fread_u8_die(mapBuf, 14 * 300, level_f);
	bufLoc = 0;              /* MAP NUMBER 1 */
	for (y = 0; y < 300; y++)
	{
		for (x = 0; x < 14; x++)
		{
			megaData1.mainmap[y][x] = ref[0][mapBuf[bufLoc]];
			bufLoc++;
		}
	}

	fread_u8_die(mapBuf, 14 * 600, level_f);
	bufLoc = 0;              /* MAP NUMBER 2 */
	for (y = 0; y < 600; y++)
	{
		for (x = 0; x < 14; x++)
		{
			megaData2.mainmap[y][x] = ref[1][mapBuf[bufLoc]];
			bufLoc++;
		}
	}

	fread_u8_die(mapBuf, 15 * 600, level_f);
	bufLoc = 0;              /* MAP NUMBER 3 */
	for (y = 0; y < 600; y++)
	{
		for (x = 0; x < 15; x++)
		{
			megaData3.mainmap[y][x] = ref[2][mapBuf[bufLoc]];
			bufLoc++;
		}
	}

	/* Note: The map data is automatically calculated with the correct mapsh
	value and then the pointer is calculated using the formula (MAPSH-1)*168.
	Then, we'll automatically add S2Ofs to get the exact offset location into
	the shape table! This makes it VERY FAST! */

	/*debuginfo('Map file done.');*/
	/* End of find loop for LEVEL??.DAT */
}

#ifdef WITH_NETWORK
void networkStartScreen(void)
{
	JE_loadPic(VGAScreen, 2, false);
	memcpy(VGAScreen2->pixels, VGAScreen->pixels, VGAScreen2->pitch * VGAScreen2->h);
	JE_dString(VGAScreen, JE_fontCenter("Waiting for other player.", SMALL_FONT_SHAPES), 140, "Waiting for other player.", SMALL_FONT_SHAPES);
	JE_showVGA();
	fade_palette(colors, 10, 0, 255);

	network_connect();

	twoPlayerMode = true;
	if (thisPlayerNum == 1)
	{
		fade_black(10);

		if (episodeSelect() && difficultySelect())
		{
			initialDifficulty = difficultyLevel;

			difficultyLevel++;  /*Make it one step harder for 2-player mode!*/

			network_prepare(PACKET_DETAILS);
			SDLNet_Write16(episodeNum, &packet_out_temp->data[4]);
			SDLNet_Write16(difficultyLevel, &packet_out_temp->data[6]);
			network_send(8);  // PACKET_DETAILS
		}
		else
		{
			network_prepare(PACKET_QUIT);
			network_send(4);  // PACKET QUIT

			network_tyrian_halt(0, true);
		}
	}
	else
	{
		memcpy(VGAScreen->pixels, VGAScreen2->pixels, VGAScreen->pitch * VGAScreen->h);
		JE_dString(VGAScreen, JE_fontCenter(networkText[4 - 1], SMALL_FONT_SHAPES), 140, networkText[4 - 1], SMALL_FONT_SHAPES);
		JE_showVGA();

		// until opponent sends details packet
		while (true)
		{
			service_SDL_events(false);
			JE_showVGA();

			if (packet_in[0] && SDLNet_Read16(&packet_in[0]->data[0]) == PACKET_DETAILS)
				break;

			network_update();
			network_check();

			SDL_Delay(16);
		}

		JE_initEpisode(SDLNet_Read16(&packet_in[0]->data[4]));
		difficultyLevel = SDLNet_Read16(&packet_in[0]->data[6]);
		initialDifficulty = difficultyLevel - 1;
		fade_black(10);

		network_update();
	}

	for (uint i = 0; i < COUNTOF(player); ++i)
		player[i].cash = 0;

	player[0].items.ship = 11;  // Silver Ship

	while (!network_is_sync())
	{
		service_SDL_events(false);
		JE_showVGA();

		network_check();
		SDL_Delay(16);
	}
}
#endif /* WITH_NETWORK */

void intro_logos(void)
{
	SDL_FillRect(VGAScreen, NULL, 0);

	fade_white(25);

	JE_loadPic(VGAScreen, 10, false);
	JE_showVGA();

	fade_palette(colors, 25, 0, 255);

	setDelay(200);
	wait_delayorinput();

	fade_black(10);

	JE_loadPic(VGAScreen, 12, false);
	JE_showVGA();

	fade_palette(colors, 10, 0, 255);

	setDelay(200);
	wait_delayorinput();

	fade_black(10);
}

Sint16 JE_newEnemy(int enemyOffset, Uint16 eDatI, Sint16 uniqueShapeTableI)
{
	for (int i = enemyOffset; i < enemyOffset + 25; ++i)
	{
		if (enemyAvail[i] == 1)
		{
			enemyAvail[i] = JE_makeEnemy(&enemy[i], eDatI, uniqueShapeTableI);
			return i + 1;
		}
	}
	
	return 0;
}

uint JE_makeEnemy(struct JE_SingleEnemyType *enemy, Uint16 eDatI, Sint16 uniqueShapeTableI)
{
	uint avail;

	JE_byte shapeTableI;

	if (superArcadeMode != SA_NONE && eDatI == 534)
		eDatI = 533;

	if (uniqueShapeTableI > 0)
	{
		shapeTableI = uniqueShapeTableI;
	}
	else
	{
		shapeTableI = enemyDat[eDatI].shapebank;
	}

	Sprite2_array *sprite2s = NULL;
	if (shapeTableI == 21)
	{
		sprite2s = &spriteSheet11;  // Coins&Gems
	}
	else if (shapeTableI == 26)
	{
		sprite2s = &spriteSheet10;  // Two-Player Stuff
	}
	else
	{
		for (size_t i = 0; i < COUNTOF(enemySpriteSheetIds); ++i)
			if (shapeTableI == enemySpriteSheetIds[i])
				sprite2s = &enemySpriteSheets[i];
	}
	
	if (sprite2s != NULL)
		enemy->sprite2s = sprite2s;
	else
		// Use shape table value from previous enemy that occupied the enemy slot. (Ex. APPROACH.)
		fprintf(stderr, "warning: ignoring sprite from unloaded shape table %d\n", shapeTableI);

	enemy->enemydatofs = &enemyDat[eDatI];

	enemy->mapoffset = 0;

	for (uint i = 0; i < 3; ++i)
	{
		enemy->eshotmultipos[i] = 0;
	}

	enemy->enemyground = (enemyDat[eDatI].explosiontype & 1) == 0;
	enemy->explonum = enemyDat[eDatI].explosiontype >> 1;

	enemy->launchfreq = enemyDat[eDatI].elaunchfreq;
	enemy->launchwait = enemyDat[eDatI].elaunchfreq;

	// Account for the second enemy bank only if we're creating something from it
	if (eDatI > 1000)
	{
		enemy->launchtype = enemyDat[eDatI].elaunchtype;
		enemy->launchspecial = 0;
	}
	else
	{
		enemy->launchtype = enemyDat[eDatI].elaunchtype % 1000;
		enemy->launchspecial = enemyDat[eDatI].elaunchtype / 1000;
	}

	enemy->xaccel = enemyDat[eDatI].xaccel;
	enemy->yaccel = enemyDat[eDatI].yaccel;

	enemy->xminbounce = -10000;
	enemy->xmaxbounce = 10000;
	enemy->yminbounce = -10000;
	enemy->ymaxbounce = 10000;
	/*Far enough away to be impossible to reach*/

	for (uint i = 0; i < 3; ++i)
	{
		enemy->tur[i] = enemyDat[eDatI].tur[i];
	}

	enemy->ani = enemyDat[eDatI].ani;
	enemy->animin = 1;

	switch (enemyDat[eDatI].animate)
	{
	case 0:
		enemy->enemycycle = 1;
		enemy->aniactive = 0;
		enemy->animax = 0;
		enemy->aniwhenfire = 0;
		break;
	case 1:
		enemy->enemycycle = 0;
		enemy->aniactive = 1;
		enemy->animax = 0;
		enemy->aniwhenfire = 0;
		break;
	case 2:
		enemy->enemycycle = 1;
		enemy->aniactive = 2;
		enemy->animax = enemy->ani;
		enemy->aniwhenfire = 2;
		break;
	}

	if (enemyDat[eDatI].startxc != 0)
		enemy->ex = enemyDat[eDatI].startx + (mt_rand() % (enemyDat[eDatI].startxc * 2)) - enemyDat[eDatI].startxc + 1;
	else
		enemy->ex = enemyDat[eDatI].startx + 1;

	if (enemyDat[eDatI].startyc != 0)
		enemy->ey = enemyDat[eDatI].starty + (mt_rand() % (enemyDat[eDatI].startyc * 2)) - enemyDat[eDatI].startyc + 1;
	else
		enemy->ey = enemyDat[eDatI].starty + 1;

	enemy->exc = enemyDat[eDatI].xmove;
	enemy->eyc = enemyDat[eDatI].ymove;
	enemy->excc = enemyDat[eDatI].xcaccel;
	enemy->eycc = enemyDat[eDatI].ycaccel;
	enemy->exccw = abs(enemy->excc);
	enemy->exccwmax = enemy->exccw;
	enemy->eyccw = abs(enemy->eycc);
	enemy->eyccwmax = enemy->eyccw;
	enemy->exccadd = (enemy->excc > 0) ? 1 : -1;
	enemy->eyccadd = (enemy->eycc > 0) ? 1 : -1;
	enemy->special = ENEMYFLAG_NONE;
	enemy->iced = 0;

	if (enemyDat[eDatI].xrev == 0)
		enemy->exrev = 100;
	else if (enemyDat[eDatI].xrev == -99)
		enemy->exrev = 0;
	else
		enemy->exrev = enemyDat[eDatI].xrev;

	if (enemyDat[eDatI].yrev == 0)
		enemy->eyrev = 100;
	else if (enemyDat[eDatI].yrev == -99)
		enemy->eyrev = 0;
	else
		enemy->eyrev = enemyDat[eDatI].yrev;

	enemy->exca = (enemy->xaccel > 0) ? 1 : -1;
	enemy->eyca = (enemy->yaccel > 0) ? 1 : -1;

	enemy->enemytype = eDatI;

	for (uint i = 0; i < 3; ++i)
	{
		if (enemy->tur[i] == 252)
			enemy->eshotwait[i] = 1;
		else if (enemy->tur[i] > 0)
			enemy->eshotwait[i] = 20;
		else
			enemy->eshotwait[i] = 255;
	}
	for (uint i = 0; i < 20; ++i)
		enemy->egr[i] = enemyDat[eDatI].egraphic[i];
	enemy->size = enemyDat[eDatI].esize;
	enemy->linknum = 0;
	enemy->edamaged = enemyDat[eDatI].dani < 0;
	enemy->enemydie = enemyDat[eDatI].eenemydie;

	enemy->freq[1-1] = enemyDat[eDatI].freq[1-1];
	enemy->freq[2-1] = enemyDat[eDatI].freq[2-1];
	enemy->freq[3-1] = enemyDat[eDatI].freq[3-1];

	enemy->edani   = enemyDat[eDatI].dani;
	enemy->edgr    = enemyDat[eDatI].dgr;
	enemy->edlevel = enemyDat[eDatI].dlevel;

	enemy->fixedmovey = 0;

	enemy->filter = 0x00;

#if 0
	// Scale pickup point values by difficulty
	// I don't really like this behavior, so it's commented out
	int tempValue = 0;
	if (enemyDat[eDatI].value > 1 && enemyDat[eDatI].value < 10000)
	{
		switch (difficultyLevel)
		{
		case -1:
		case DIFFICULTY_WIMP:
			tempValue = enemyDat[eDatI].value * 0.75f;
			break;
		case DIFFICULTY_EASY:
		case DIFFICULTY_NORMAL:
			tempValue = enemyDat[eDatI].value;
			break;
		case DIFFICULTY_HARD:
			tempValue = enemyDat[eDatI].value * 1.125f;
			break;
		case DIFFICULTY_IMPOSSIBLE:
			tempValue = enemyDat[eDatI].value * 1.5f;
			break;
		case DIFFICULTY_INSANITY:
			tempValue = enemyDat[eDatI].value * 2;
			break;
		case DIFFICULTY_SUICIDE:
			tempValue = enemyDat[eDatI].value * 2.5f;
			break;
		case DIFFICULTY_MANIACAL:
		case DIFFICULTY_ZINGLON:
			tempValue = enemyDat[eDatI].value * 4;
			break;
		case DIFFICULTY_NORTANEOUS:
		case DIFFICULTY_10:
			tempValue = enemyDat[eDatI].value * 8;
			break;
		}
		if (tempValue > 10000)
			tempValue = 10000;
		enemy->evalue = tempValue;
	}
	else
	{
		enemy->evalue = enemyDat[eDatI].value;
	}
#else
	enemy->evalue = enemyDat[eDatI].value;
#endif

	if (enemyDat[eDatI].armor > 0)
	{
		enemy->armorleft = tyrian_scaleEnemyHealth(enemyDat[eDatI].armor);

		avail = 0;
		enemy->scoreitem = false;
	}
	else
	{
		avail = 2;
		enemy->armorleft = 255;
		// Bugfix (original game): Make sure scoreitem is always set
		enemy->scoreitem = (enemy->evalue != 0);
	}

	if (!enemy->scoreitem)
	{
		totalEnemy++;  /*Destruction ratio*/
	}

	/* indicates what to set ENEMYAVAIL to */
	return avail;
}

void JE_createNewEventEnemy(JE_byte enemyTypeOfs, JE_word enemyOffset, Sint16 uniqueShapeTableI)
{
	int i;

	b = 0;

	for (i = enemyOffset; i < enemyOffset + 25; i++)
	{
		if (enemyAvail[i] == 1)
		{
			b = i + 1;
			break;
		}
	}

	if (b == 0)
		return;

	tempW = eventRec[eventLoc-1].eventdat + enemyTypeOfs;

	enemyAvail[b-1] = JE_makeEnemy(&enemy[b-1], tempW, uniqueShapeTableI);

	// Tyrian 2000 random X position
	if (eventRec[eventLoc-1].eventdat2 == -200)
		eventRec[eventLoc-1].eventdat2 = (mt_rand() % 208) + 24;

	if (eventRec[eventLoc-1].eventdat2 != -99)
	{
		switch (enemyOffset)
		{
		case 0:
			enemy[b-1].ex = eventRec[eventLoc-1].eventdat2 - (mapX - 1) * 24;
			enemy[b-1].ey -= backMove2;
			break;
		case 25:
		case 75:
			enemy[b-1].ex = eventRec[eventLoc-1].eventdat2 - (mapX - 1) * 24 - 12;
			enemy[b-1].ey -= backMove;
			break;
		case 50:
			if (background3x1)
				enemy[b-1].ex = eventRec[eventLoc-1].eventdat2 - (mapX - 1) * 24 - 12;
			else
				enemy[b-1].ex = eventRec[eventLoc-1].eventdat2 - mapX3 * 24 - 24 * 2 + 6;
			enemy[b-1].ey -= backMove3;

			if (background3x1b)
				enemy[b-1].ex -= 6;
			break;
		}
		enemy[b-1].ey = -28;
		if (background3x1b && enemyOffset == 50)
			enemy[b-1].ey += 4;
	}

	if (smallEnemyAdjust && enemy[b-1].size == 0)
	{
		enemy[b-1].ex -= 10;
		enemy[b-1].ey -= 7;
	}

	enemy[b-1].ey += eventRec[eventLoc-1].eventdat5;
	enemy[b-1].eyc += eventRec[eventLoc-1].eventdat3;
	enemy[b-1].linknum = eventRec[eventLoc-1].eventdat4;
	enemy[b-1].fixedmovey = eventRec[eventLoc-1].eventdat6;
}

void JE_eventJump(JE_word jump)
{
	JE_word tempW;

	if (jump == 65535)
	{
		curLoc = returnLoc;
	}
	else
	{
		returnLoc = curLoc + 1;
		curLoc = jump;
	}
	tempW = 0;
	do
	{
		tempW++;
	} while (!(eventRec[tempW-1].eventtime >= curLoc));
	eventLoc = tempW - 1;
}

bool JE_searchFor/*enemy*/(JE_byte PLType, JE_byte* out_index)
{
	int found_id = -1;

	for (int i = 0; i < 100; i++)
	{
		if (enemyAvail[i] == 0 && enemy[i].linknum == PLType)
		{
			found_id = i;
			if (galagaMode)
				enemy[i].evalue += enemy[i].evalue;
		}
	}

	if (found_id != -1)
	{
		if (out_index)
			*out_index = found_id;
		return true;
	}
	else
	{
		return false;
	}
}

void JE_eventSystem(void)
{
#ifdef PRINT_EVENTS
	printf("%d : %d %d (%d %d %d %d %d %d)\n",
		eventLoc,
		eventRec[eventLoc-1].eventtime,
		eventRec[eventLoc-1].eventtype,
		eventRec[eventLoc-1].eventdat,
		eventRec[eventLoc-1].eventdat2,
		eventRec[eventLoc-1].eventdat3,
		eventRec[eventLoc-1].eventdat5,
		eventRec[eventLoc-1].eventdat6,
		eventRec[eventLoc-1].eventdat4);
#endif
	switch (eventRec[eventLoc-1].eventtype)
	{
	case 0:
		// ignore no-op
		break;

	case 1:
		starfield_speed = eventRec[eventLoc-1].eventdat;
		break;

	case 2:
		map1YDelay = 1;
		map1YDelayMax = 1;
		map2YDelay = 1;
		map2YDelayMax = 1;

		backMove = eventRec[eventLoc-1].eventdat;
		backMove2 = eventRec[eventLoc-1].eventdat2;

		if (backMove2 > 0)
			explodeMove = backMove2;
		else
			explodeMove = backMove;

		backMove3 = eventRec[eventLoc-1].eventdat3;

		if (backMove > 0)
			stopBackgroundNum = 0;
		break;

	case 3:
		backMove = 1;
		map1YDelay = 3;
		map1YDelayMax = 3;
		backMove2 = 1;
		map2YDelay = 2;
		map2YDelayMax = 2;
		backMove3 = 1;
		break;

	case 4:
	case 83: // Tyrian 2000: duplicate MapStop event
		stopBackgrounds = true;
		switch (eventRec[eventLoc-1].eventdat)
		{
		case 0:
		case 1:
			stopBackgroundNum = 1;
			break;
		case 2:
			stopBackgroundNum = 2;
			break;
		case 3:
			stopBackgroundNum = 3;
			break;
		}
		break;

	case 5:  // load enemy shape banks
		{
			const Uint8 newEnemyShapeTables[] =
			{
				eventRec[eventLoc-1].eventdat > 0 ? eventRec[eventLoc-1].eventdat : 0,
				eventRec[eventLoc-1].eventdat2 > 0 ? eventRec[eventLoc-1].eventdat2 : 0,
				eventRec[eventLoc-1].eventdat3 > 0 ? eventRec[eventLoc-1].eventdat3 : 0,
				eventRec[eventLoc-1].eventdat4 > 0 ? eventRec[eventLoc-1].eventdat4 : 0,
			};
			
			for (unsigned int i = 0; i < COUNTOF(newEnemyShapeTables); ++i)
			{
				if (enemySpriteSheetIds[i] != newEnemyShapeTables[i])
				{
					if (newEnemyShapeTables[i] > 0)
					{
						assert(newEnemyShapeTables[i] <= COUNTOF(shapeFile));
						JE_loadCompShapes(&enemySpriteSheets[i], shapeFile[newEnemyShapeTables[i] - 1]);
					}
					else
						free_sprite2s(&enemySpriteSheets[i]);

					enemySpriteSheetIds[i] = newEnemyShapeTables[i];
				}
			}
		}
		break;

	case 6: /* Ground Enemy */
		JE_createNewEventEnemy(0, 25, 0);
		break;

	case 7: /* Top Enemy */
		JE_createNewEventEnemy(0, 50, 0);
		break;

	case 8:
		starActive = false;
		break;

	case 9:
		starActive = true;
		break;

	case 10: /* Ground Enemy 2 */
		JE_createNewEventEnemy(0, 75, 0);
		break;

	case 11:
		if (allPlayersGone || eventRec[eventLoc-1].eventdat == 1)
		{
			reallyEndLevel = true;
		}
		else if (!endLevel)
		{
			readyToEndLevel = false;
			endLevel = true;
			levelEnd = 40;
		}
		break;

	case 12: /* Custom 4x4 Ground Enemy */
		{
			uint temp = 0;
			switch (eventRec[eventLoc-1].eventdat6)
			{
			case 0:
			case 1:
				temp = 25;
				break;
			case 2:
				temp = 0;
				break;
			case 3:
				temp = 50;
				break;
			case 4:
				temp = 75;
				break;
			}
			eventRec[eventLoc-1].eventdat6 = 0;   /* We use EVENTDAT6 for the background */
			JE_createNewEventEnemy(0, temp, 0);
			JE_createNewEventEnemy(1, temp, 0);
			if (b > 0)
				enemy[b-1].ex += 24;
			JE_createNewEventEnemy(2, temp, 0);
			if (b > 0)
				enemy[b-1].ey -= 28;
			JE_createNewEventEnemy(3, temp, 0);
			if (b > 0)
			{
				enemy[b-1].ex += 24;
				enemy[b-1].ey -= 28;
			}
			break;
		}
	case 13:
		enemiesActive = false;
		break;

	case 14:
		enemiesActive = true;
		break;

	case 15: /* Sky Enemy */
		JE_createNewEventEnemy(0, 0, 0);
		break;

	case 16: // VoicedCallout
	callout_rejoin:
		if (eventRec[eventLoc-1].eventdat > 9)
		{
			fprintf(stderr, "warning: event 16: bad event data\n");
		}
		else
		{
			apmsg_drawInGameText(outputs[eventRec[eventLoc-1].eventdat-1]);
			nortsong_playVoice(windowTextSamples[eventRec[eventLoc-1].eventdat-1]);
		}
		break;

	case 17: /* Ground Bottom */
		JE_createNewEventEnemy(0, 25, 0);
		if (b > 0)
			enemy[b-1].ey = 190 + eventRec[eventLoc-1].eventdat5;
		break;

	case 18: /* Sky Enemy on Bottom */
		JE_createNewEventEnemy(0, 0, 0);
		if (b > 0)
			enemy[b-1].ey = 190 + eventRec[eventLoc-1].eventdat5;
		break;

	case 19: /* Enemy Global Move */
	{
		int initial_i = 0, max_i = 0;
		bool all_enemies = false;

		if (eventRec[eventLoc-1].eventdat3 > 79 && eventRec[eventLoc-1].eventdat3 < 90)
		{
			initial_i = 0;
			max_i = 100;
			all_enemies = false;
			eventRec[eventLoc-1].eventdat4 = newPL[eventRec[eventLoc-1].eventdat3 - 80];
		}
		else
		{
			switch (eventRec[eventLoc-1].eventdat3)
			{
			case 0:
				initial_i = 0;
				max_i = 100;
				all_enemies = false;
				break;
			case 2:
				initial_i = 0;
				max_i = 25;
				all_enemies = true;
				break;
			case 1:
				initial_i = 25;
				max_i = 50;
				all_enemies = true;
				break;
			case 3:
				initial_i = 50;
				max_i = 75;
				all_enemies = true;
				break;
			case 99:
				initial_i = 0;
				max_i = 100;
				all_enemies = true;
				break;
			}
		}

		for (int i = initial_i; i < max_i; i++)
		{
			// AP: If affecting all enemies, ignore scoreitems (so they don't behave erratically)
			if (all_enemies && enemy[temp].scoreitem)
				continue;

			if (all_enemies || enemy[i].linknum == eventRec[eventLoc-1].eventdat4)
			{
				if (eventRec[eventLoc-1].eventdat != -99)
					enemy[i].exc = eventRec[eventLoc-1].eventdat;

				if (eventRec[eventLoc-1].eventdat2 != -99)
					enemy[i].eyc = eventRec[eventLoc-1].eventdat2;

				if (eventRec[eventLoc-1].eventdat6 != 0)
					enemy[i].fixedmovey = eventRec[eventLoc-1].eventdat6;

				if (eventRec[eventLoc-1].eventdat6 == -99)
					enemy[i].fixedmovey = 0;

				if (eventRec[eventLoc-1].eventdat5 > 0)
					enemy[i].enemycycle = eventRec[eventLoc-1].eventdat5;
			}
		}
		break;
	}

	case 20: /* Enemy Global Accel */
		if (eventRec[eventLoc-1].eventdat3 > 79 && eventRec[eventLoc-1].eventdat3 < 90)
			eventRec[eventLoc-1].eventdat4 = newPL[eventRec[eventLoc-1].eventdat3 - 80];

		for (temp = 0; temp < 100; temp++)
		{
			if (enemyAvail[temp] == 1)
				continue;

			// AP: If linknum isn't set, ignore scoreitems (so they don't behave erratically)
			if (eventRec[eventLoc-1].eventdat4 == 0 && enemy[temp].scoreitem)
				continue;

			if (eventRec[eventLoc-1].eventdat4 == 0 || enemy[temp].linknum == eventRec[eventLoc-1].eventdat4)
			{
				if (eventRec[eventLoc-1].eventdat != -99)
				{
					enemy[temp].excc = eventRec[eventLoc-1].eventdat;
					enemy[temp].exccw = abs(eventRec[eventLoc-1].eventdat);
					enemy[temp].exccwmax = abs(eventRec[eventLoc-1].eventdat);
					if (eventRec[eventLoc-1].eventdat > 0)
						enemy[temp].exccadd = 1;
					else
						enemy[temp].exccadd = -1;
				}

				if (eventRec[eventLoc-1].eventdat2 != -99)
				{
					enemy[temp].eycc = eventRec[eventLoc-1].eventdat2;
					enemy[temp].eyccw = abs(eventRec[eventLoc-1].eventdat2);
					enemy[temp].eyccwmax = abs(eventRec[eventLoc-1].eventdat2);
					if (eventRec[eventLoc-1].eventdat2 > 0)
						enemy[temp].eyccadd = 1;
					else
						enemy[temp].eyccadd = -1;
				}

				if (eventRec[eventLoc-1].eventdat5 > 0)
				{
					enemy[temp].enemycycle = eventRec[eventLoc-1].eventdat5;
				}
				if (eventRec[eventLoc-1].eventdat6 > 0)
				{
					enemy[temp].ani = eventRec[eventLoc-1].eventdat6;
					enemy[temp].animin = eventRec[eventLoc-1].eventdat5;
					enemy[temp].animax = 0;
					enemy[temp].aniactive = 1;
				}
			}
		}
		break;

	case 21:
		background3over = 1;
		break;

	case 22:
		background3over = 0;
		break;

	case 23: /* Sky Enemy on Bottom */
		JE_createNewEventEnemy(0, 50, 0);
		if (b > 0)
			enemy[b-1].ey = 180 + eventRec[eventLoc-1].eventdat5;
		break;

	case 24: /* Enemy Global Animate */
		for (temp = 0; temp < 100; temp++)
		{
			if (enemy[temp].linknum == eventRec[eventLoc-1].eventdat4)
			{
				enemy[temp].aniactive = 1;
				enemy[temp].aniwhenfire = 0;
				if (eventRec[eventLoc-1].eventdat2 > 0)
				{
					enemy[temp].enemycycle = eventRec[eventLoc-1].eventdat2;
					enemy[temp].animin = enemy[temp].enemycycle;
				}
				else
				{
					enemy[temp].enemycycle = 0;
				}

				if (eventRec[eventLoc-1].eventdat > 0)
					enemy[temp].ani = eventRec[eventLoc-1].eventdat;

				if (eventRec[eventLoc-1].eventdat3 == 1)
				{
					enemy[temp].animax = enemy[temp].ani;
				}
				else if (eventRec[eventLoc-1].eventdat3 == 2)
				{
					enemy[temp].aniactive = 2;
					enemy[temp].animax = enemy[temp].ani;
					enemy[temp].aniwhenfire = 2;
				}
			}
		}
		break;

	case 25: /* Enemy Global Damage change */
		for (temp = 0; temp < 100; temp++)
		{
			if (eventRec[eventLoc-1].eventdat4 == 0 || enemy[temp].linknum == eventRec[eventLoc-1].eventdat4)
			{
				if (eventRec[eventLoc-1].eventdat6 == 99)
					enemy[temp].armorleft = tyrian_scaleEnemyHealth(eventRec[eventLoc-1].eventdat);
				else if (galagaMode)
					enemy[temp].armorleft = roundf(eventRec[eventLoc-1].eventdat * (difficultyLevel / 2));
				else
					enemy[temp].armorleft = eventRec[eventLoc-1].eventdat;
			}
		}
		break;

	case 26:
		smallEnemyAdjust = eventRec[eventLoc-1].eventdat;
		break;

	case 27: /* Enemy Global AccelRev */
		if (eventRec[eventLoc-1].eventdat3 > 79 && eventRec[eventLoc-1].eventdat3 < 90)
			eventRec[eventLoc-1].eventdat4 = newPL[eventRec[eventLoc-1].eventdat3 - 80];

		for (temp = 0; temp < 100; temp++)
		{
			if (eventRec[eventLoc-1].eventdat4 == 0 || enemy[temp].linknum == eventRec[eventLoc-1].eventdat4)
			{
				if (eventRec[eventLoc-1].eventdat != -99)
					enemy[temp].exrev = eventRec[eventLoc-1].eventdat;
				if (eventRec[eventLoc-1].eventdat2 != -99)
					enemy[temp].eyrev = eventRec[eventLoc-1].eventdat2;
				if (eventRec[eventLoc-1].eventdat3 != 0 && eventRec[eventLoc-1].eventdat3 < 17)
					enemy[temp].filter = eventRec[eventLoc-1].eventdat3;
			}
		}
		break;

	case 28:
		topEnemyOver = false;
		break;

	case 29:
		topEnemyOver = true;
		break;

	case 30:
		map1YDelay = 1;
		map1YDelayMax = 1;
		map2YDelay = 1;
		map2YDelayMax = 1;

		backMove = eventRec[eventLoc-1].eventdat;
		backMove2 = eventRec[eventLoc-1].eventdat2;
		explodeMove = backMove2;
		backMove3 = eventRec[eventLoc-1].eventdat3;
		break;

	case 31: /* Enemy Fire Override */
		for (temp = 0; temp < 100; temp++)
		{
			if (eventRec[eventLoc-1].eventdat4 == 99 || enemy[temp].linknum == eventRec[eventLoc-1].eventdat4)
			{
				enemy[temp].freq[1-1] = eventRec[eventLoc-1].eventdat ;
				enemy[temp].freq[2-1] = eventRec[eventLoc-1].eventdat2;
				enemy[temp].freq[3-1] = eventRec[eventLoc-1].eventdat3;
				for (temp2 = 0; temp2 < 3; temp2++)
				{
					enemy[temp].eshotwait[temp2] = 1;
				}
				if (enemy[temp].launchtype > 0)
				{
					enemy[temp].launchfreq = eventRec[eventLoc-1].eventdat5;
					enemy[temp].launchwait = 1;
				}
			}
		}
		break;

	case 32:  // create enemy
		JE_createNewEventEnemy(0, 50, 0);
		if (b > 0)
			enemy[b-1].ey = 190;
		break;

	case 33: /* Enemy From other Enemies */
#if 1
		for (temp = 0; temp < 100; temp++)
		{
			if (enemy[temp].linknum == eventRec[eventLoc-1].eventdat4)
				enemy[temp].enemydie = eventRec[eventLoc-1].eventdat;
		}
#else
		if (!((eventRec[eventLoc-1].eventdat == 512 || eventRec[eventLoc-1].eventdat == 513) && (twoPlayerMode || onePlayerAction || superTyrian)))
		{
			if (superArcadeMode != SA_NONE)
			{
				if (eventRec[eventLoc-1].eventdat == 534)
					eventRec[eventLoc-1].eventdat = 827;
			}
			else if (!superTyrian)
			{
				const Uint8 lives = *player[0].lives;

				if (eventRec[eventLoc-1].eventdat == 533 && (lives == 11 || (mt_rand() % 15) < lives))
				{
					// enemy will drop random special weapon
					eventRec[eventLoc-1].eventdat = 829 + (mt_rand() % 6);
				}
			}
			if (eventRec[eventLoc-1].eventdat == 534 && superTyrian)
				eventRec[eventLoc-1].eventdat = 828 + superTyrianSpecials[mt_rand() % 4];

			for (temp = 0; temp < 100; temp++)
			{
				if (enemy[temp].linknum == eventRec[eventLoc-1].eventdat4)
					enemy[temp].enemydie = eventRec[eventLoc-1].eventdat;
			}
		}
#endif
		break;

	case 34: /* Start Music Fade */
		if (firstGameOver)
		{
			musicFade = true;
			tempVolume = tyrMusicVolume;
		}
		break;

	case 35: /* Play new song */
		if (firstGameOver)
		{
			play_song(eventRec[eventLoc-1].eventdat - 1);
			set_volume(tyrMusicVolume, fxVolume);
		}
		musicFade = false;
		break;

	case 36:
		readyToEndLevel = true;
		break;

	case 37:
		levelEnemyFrequency = eventRec[eventLoc-1].eventdat;
		break;

	case 38:
		curLoc = eventRec[eventLoc-1].eventdat;
		int new_event_loc = 1;
		for (tempW = 0; tempW < maxEvent; tempW++)
		{
			if (eventRec[tempW].eventtime <= curLoc)
				new_event_loc = tempW+1 - 1;
		}
		eventLoc = new_event_loc;
		break;

	case 39: /* Enemy Global Linknum Change */
		for (temp = 0; temp < 100; temp++)
		{
			if (enemy[temp].linknum == eventRec[eventLoc-1].eventdat)
				enemy[temp].linknum = eventRec[eventLoc-1].eventdat2;
		}
		break;

	case 40: /* Enemy Continual Damage */
		enemyContinualDamage = true;
		break;

	case 41:
		if (eventRec[eventLoc-1].eventdat == 0)
		{
			memset(enemyAvail, 1, sizeof(enemyAvail));
		}
		else
		{
			for (x = 0; x <= 24; x++)
				enemyAvail[x] = 1;
		}
		break;

	case 42:
		background3over = 2;
		break;

	case 43:
		background2over = eventRec[eventLoc-1].eventdat;
		break;

	case 44:
		filterActive       = (eventRec[eventLoc-1].eventdat > 0);
		filterFade         = (eventRec[eventLoc-1].eventdat == 2);
		levelFilter        = eventRec[eventLoc-1].eventdat2;
		levelBrightness    = eventRec[eventLoc-1].eventdat3;
		levelFilterNew     = eventRec[eventLoc-1].eventdat4;
		levelBrightnessChg = eventRec[eventLoc-1].eventdat5;
		filterFadeStart    = (eventRec[eventLoc-1].eventdat6 == 0);
		break;

	case 45: /* arcade-only enemy from other enemies */
	case 85: // T2K_TimeBattle_EnemyFromOtherEnemy
#if 0
		for (temp = 0; temp < 100; temp++)
		{
			if (enemy[temp].linknum == eventRec[eventLoc-1].eventdat4)
			{
				if (enemy[temp].enemydie != 533
					&& enemy[temp].enemydie != 534
					&& enemy[temp].enemydie != 512
					&& enemy[temp].enemydie != 513
					&& (enemy[temp].enemydie < ARCHIPELAGO_ITEM || enemy[temp].enemydie > ARCHIPELAGO_ITEM_MAX))
				enemy[temp].enemydie = eventRec[eventLoc-1].eventdat;
			}
		}
#else
		if (!superTyrian)
		{
			const Uint8 lives = *player[0].lives;

			if (eventRec[eventLoc-1].eventdat == 533 && (lives == 11 || (mt_rand() % 15) < lives))
			{
				eventRec[eventLoc-1].eventdat = 829 + (mt_rand() % 6);
			}
			if (twoPlayerMode || onePlayerAction)
			{
				for (temp = 0; temp < 100; temp++)
				{
					if (enemy[temp].linknum == eventRec[eventLoc-1].eventdat4)
						enemy[temp].enemydie = eventRec[eventLoc-1].eventdat;
				}
			}
		}
#endif
		break;

	case 46:  // change difficulty
		if (eventRec[eventLoc-1].eventdat3 != 0)
			damageRate = eventRec[eventLoc-1].eventdat3;

		if (eventRec[eventLoc-1].eventdat2 == 0 || twoPlayerMode || onePlayerAction)
		{
			difficultyLevel += eventRec[eventLoc-1].eventdat;
			if (difficultyLevel < DIFFICULTY_EASY)
				difficultyLevel = DIFFICULTY_EASY;
			if (difficultyLevel > DIFFICULTY_10)
				difficultyLevel = DIFFICULTY_10;
		}
		break;

	case 47: /* Enemy Global AccelRev */
		for (temp = 0; temp < 100; temp++)
		{
			if (eventRec[eventLoc-1].eventdat4 == 0 || enemy[temp].linknum == eventRec[eventLoc-1].eventdat4)
				enemy[temp].armorleft = eventRec[eventLoc-1].eventdat;
		}
		break;

	case 48: /* Background 2 Cannot be Transparent */
		background2notTransparent = true;
		break;

	case 49:
	case 50:
	case 51:
	case 52:
		tempDat2 = eventRec[eventLoc-1].eventdat;
		eventRec[eventLoc-1].eventdat = 0;
		tempDat = eventRec[eventLoc-1].eventdat3;
		eventRec[eventLoc-1].eventdat3 = 0;
		tempDat3 = eventRec[eventLoc-1].eventdat6;
		eventRec[eventLoc-1].eventdat6 = 0;
		enemyDat[0].armor = tempDat3;
		enemyDat[0].egraphic[1-1] = tempDat2;
		switch (eventRec[eventLoc-1].eventtype - 48)
		{
		case 1:
			temp = 25;
			break;
		case 2:
			temp = 0;
			break;
		case 3:
			temp = 50;
			break;
		case 4:
			temp = 75;
			break;
		}
		JE_createNewEventEnemy(0, temp, tempDat);
		eventRec[eventLoc-1].eventdat = tempDat2;
		eventRec[eventLoc-1].eventdat3 = tempDat;
		eventRec[eventLoc-1].eventdat6 = tempDat3;
		break;

	case 53:
		forceEvents = (eventRec[eventLoc-1].eventdat != 99);
		break;

	case 54:
		JE_eventJump(eventRec[eventLoc-1].eventdat);
		break;

	case 55: /* Enemy Global AccelRev */
		if (eventRec[eventLoc-1].eventdat3 > 79 && eventRec[eventLoc-1].eventdat3 < 90)
			eventRec[eventLoc-1].eventdat4 = newPL[eventRec[eventLoc-1].eventdat3 - 80];

		for (temp = 0; temp < 100; temp++)
		{
			if (eventRec[eventLoc-1].eventdat4 == 0 || enemy[temp].linknum == eventRec[eventLoc-1].eventdat4)
			{
				if (eventRec[eventLoc-1].eventdat != -99)
					enemy[temp].xaccel = eventRec[eventLoc-1].eventdat;
				if (eventRec[eventLoc-1].eventdat2 != -99)
					enemy[temp].yaccel = eventRec[eventLoc-1].eventdat2;
			}
		}
		break;

	case 56: /* Ground2 Bottom */
		JE_createNewEventEnemy(0, 75, 0);
		if (b > 0)
			enemy[b-1].ey = 190;
		break;

	case 57:
		superEnemy254Jump = eventRec[eventLoc-1].eventdat;
		break;

	case 60: /*Assign Special Enemy*/
		for (temp = 0; temp < 100; temp++)
		{
			if (enemy[temp].linknum == eventRec[eventLoc-1].eventdat4)
			{
				enemy[temp].special = ENEMYFLAG_SET;
				enemy[temp].flagnum = eventRec[eventLoc-1].eventdat;
				enemy[temp].setto  = (eventRec[eventLoc-1].eventdat2 == 1);
			}
		}
		break;

	case 61:  // if specific flag set to specific value, skip events
		if (globalFlags[eventRec[eventLoc-1].eventdat-1] == eventRec[eventLoc-1].eventdat2)
			eventLoc += eventRec[eventLoc-1].eventdat3;
		break;

	case 62: /*Play sound effect*/
		soundQueue[3] = eventRec[eventLoc-1].eventdat;
		break;

	case 63:  // skip events if not in 2-player mode
		if (!twoPlayerMode && !onePlayerAction)
			eventLoc += eventRec[eventLoc-1].eventdat;
		break;

	case 64:
		if (!(eventRec[eventLoc-1].eventdat == 6 && twoPlayerMode && difficultyLevel > DIFFICULTY_NORMAL))
		{
			smoothies[eventRec[eventLoc-1].eventdat-1] = eventRec[eventLoc-1].eventdat2;
			temp = eventRec[eventLoc-1].eventdat;
			if (temp == 5)
				temp = 3;
			smoothie_data[temp-1] = eventRec[eventLoc-1].eventdat3;
		}
		break;

	case 65:
		background3x1 = (eventRec[eventLoc-1].eventdat == 0);
		break;

	case 66: /*If not on this difficulty level or higher then...*/
		if (initialDifficulty <= eventRec[eventLoc-1].eventdat)
			eventLoc += eventRec[eventLoc-1].eventdat2;
		break;

	case 67:
		levelTimer = (eventRec[eventLoc-1].eventdat == 1);
		levelTimerCountdown = eventRec[eventLoc-1].eventdat3 * 100;
		levelTimerJumpTo   = eventRec[eventLoc-1].eventdat2;
		break;

	// 68: Random Explosions -- that's event 99 in Tyrian 2000!

	case 69:
		for (uint i = 0; i < COUNTOF(player); ++i)
			player[i].invulnerable_ticks = eventRec[eventLoc-1].eventdat;
		break;

	case 70:
		if (eventRec[eventLoc-1].eventdat2 == 0)
		{  /*1-10*/
			bool found = false;

			for (temp = 1; temp <= 19; temp++)
				found = found || JE_searchFor(temp, NULL);

			if (!found)
				JE_eventJump(eventRec[eventLoc-1].eventdat);
		}
		else if (!JE_searchFor(eventRec[eventLoc-1].eventdat2, NULL) &&
		         (eventRec[eventLoc-1].eventdat3 == 0 || !JE_searchFor(eventRec[eventLoc-1].eventdat3, NULL)) &&
		         (eventRec[eventLoc-1].eventdat4 == 0 || !JE_searchFor(eventRec[eventLoc-1].eventdat4, NULL)))
		{
			JE_eventJump(eventRec[eventLoc-1].eventdat);
		}
		break;

	case 71:
		if (((((intptr_t)mapYPos - (intptr_t)&megaData1.mainmap) / sizeof(JE_byte *)) * 2) <= (unsigned)eventRec[eventLoc-1].eventdat2)
			JE_eventJump(eventRec[eventLoc-1].eventdat);
		break;

	case 72:
		background3x1b = (eventRec[eventLoc-1].eventdat == 1);
		break;

	case 73:
		skyEnemyOverAll = (eventRec[eventLoc-1].eventdat == 1);
		break;

	case 74: /* Enemy Global BounceParams */
		for (temp = 0; temp < 100; temp++)
		{
			if (eventRec[eventLoc-1].eventdat4 == 0 || enemy[temp].linknum == eventRec[eventLoc-1].eventdat4)
			{
				if (eventRec[eventLoc-1].eventdat5 != -99)
					enemy[temp].xminbounce = eventRec[eventLoc-1].eventdat5;

				if (eventRec[eventLoc-1].eventdat6 != -99)
					enemy[temp].yminbounce = eventRec[eventLoc-1].eventdat6;

				if (eventRec[eventLoc-1].eventdat != -99)
					enemy[temp].xmaxbounce = eventRec[eventLoc-1].eventdat;

				if (eventRec[eventLoc-1].eventdat2 != -99)
					enemy[temp].ymaxbounce = eventRec[eventLoc-1].eventdat2;
			}
		}
		break;

	case 75:;
		bool temp_no_clue = false; // TODO: figure out what this is doing

		for (temp = 0; temp < 100; temp++)
		{
			if (enemyAvail[temp] == 0 &&
			    enemy[temp].eyc == 0 &&
			    enemy[temp].linknum >= eventRec[eventLoc-1].eventdat &&
			    enemy[temp].linknum <= eventRec[eventLoc-1].eventdat2)
			{
				temp_no_clue = true;
			}
		}

		if (temp_no_clue)
		{
			JE_byte enemy_i;
			do
			{
				temp = (mt_rand() % (eventRec[eventLoc-1].eventdat2 + 1 - eventRec[eventLoc-1].eventdat)) + eventRec[eventLoc-1].eventdat;
			} while (!(JE_searchFor(temp, &enemy_i) && enemy[enemy_i].eyc == 0));

			newPL[eventRec[eventLoc-1].eventdat3 - 80] = temp;
		}
		else
		{
			newPL[eventRec[eventLoc-1].eventdat3 - 80] = 255;
			if (eventRec[eventLoc-1].eventdat4 > 0)
			{ /*Skip*/
				curLoc = eventRec[eventLoc-1 + eventRec[eventLoc-1].eventdat4].eventtime - 1;
				eventLoc += eventRec[eventLoc-1].eventdat4 - 1;
			}
		}

		break;

	case 76:
		returnActive = true;
		break;

	case 77:
		mapYPos = &megaData1.mainmap[0][0];
		mapYPos += eventRec[eventLoc-1].eventdat / 2;
		if (eventRec[eventLoc-1].eventdat2 > 0)
		{
			mapY2Pos = &megaData2.mainmap[0][0];
			mapY2Pos += eventRec[eventLoc-1].eventdat2 / 2;
		}
		else
		{
			mapY2Pos = &megaData2.mainmap[0][0];
			mapY2Pos += eventRec[eventLoc-1].eventdat / 2;
		}
		break;

	case 78:
		if (galagaShotFreq < 10)
			galagaShotFreq++;
		break;

	case 79:
		boss_bar[0].link_num = eventRec[eventLoc-1].eventdat;
		boss_bar[1].link_num = eventRec[eventLoc-1].eventdat2;
		break;

	case 80:  // skip events if in 2-player mode
		if (twoPlayerMode)
			eventLoc += eventRec[eventLoc-1].eventdat;
		break;

	case 81: /*WRAP2*/
		BKwrap2   = &megaData2.mainmap[0][0];
		BKwrap2   += eventRec[eventLoc-1].eventdat / 2;
		BKwrap2to = &megaData2.mainmap[0][0];
		BKwrap2to += eventRec[eventLoc-1].eventdat2 / 2;
		break;

	case 82: /*Give SPECIAL WEAPON*/
		player[0].items.special = eventRec[eventLoc-1].eventdat;
		shotMultiPos[SHOT_SPECIAL] = 0;
		shotRepeat[SHOT_SPECIAL] = 0;
		shotMultiPos[SHOT_SPECIAL2] = 0;
		shotRepeat[SHOT_SPECIAL2] = 0;
		break;

	// ===== Tyrian 2000 ======================================================

	case 58: // Set enemy launch type
		for (temp = 0; temp < 100; temp++)
		{
			if (eventRec[eventLoc-1].eventdat4 == 99 || enemy[temp].linknum == eventRec[eventLoc-1].eventdat4)
				enemy[temp].launchtype = eventRec[eventLoc-1].eventdat;
		}
		break;

	case 59: // Replace enemy
	case 68: // Duplicate event, replaces original Random Explosions event
		for (temp = 0; temp < 100; temp++)
		{
			if (!(eventRec[eventLoc-1].eventdat4 == 0 || enemy[temp].linknum == eventRec[eventLoc-1].eventdat4))
				continue;

			const int enemy_offset = temp - (temp % 25);
			b = JE_newEnemy(enemy_offset, eventRec[eventLoc-1].eventdat, 0);
			if (b != 0)
			{
				enemy[b-1].ex = enemy[temp].ex;
				enemy[b-1].ey = enemy[temp].ey;
			}

			enemyAvail[temp] = 1;
		}			
		break;

	// 83: T2K_MapStop: See case 4

	case 84: // T2K_TimeBattleParams
		// Not unknown, but we ingore timed battle events.
		break;

	case 99: // Random Explosions (post-T2K)
		randomExplosions = (eventRec[eventLoc-1].eventdat == 1);
		break;

	// ===== Archipelago Tyrian ===============================================

	case 100: // Skip_IfFlagNotEquals
		if (globalFlags[eventRec[eventLoc-1].eventdat-1] != eventRec[eventLoc-1].eventdat2)
			eventLoc += eventRec[eventLoc-1].eventdat3;
		break;

	case 101: // Skip_IfFlagLessThan
		if (globalFlags[eventRec[eventLoc-1].eventdat-1] < eventRec[eventLoc-1].eventdat2)
			eventLoc += eventRec[eventLoc-1].eventdat3;
		break;

	case 102: // Skip_IfFlagGreaterThan
		if (globalFlags[eventRec[eventLoc-1].eventdat-1] > eventRec[eventLoc-1].eventdat2)
			eventLoc += eventRec[eventLoc-1].eventdat3;
		break;

	case 110: // EnemyGlobal_IncrementFlag
		for (temp = 0; temp < 100; temp++)
		{
			if (enemy[temp].linknum == eventRec[eventLoc-1].eventdat4)
			{
				enemy[temp].special = ENEMYFLAG_INCREMENT;
				enemy[temp].flagnum = eventRec[eventLoc-1].eventdat;
				enemy[temp].setto   = eventRec[eventLoc-1].eventdat2;
			}
		}
		break;

	case 200:; // AP_CheckFreestanding
		bool using_backup = false;

		if (ARCHIPELAGO_ITEM + apCheckCount > ARCHIPELAGO_ITEM_MAX)
			break; // ???

		if (Archipelago_WasChecked(eventRec[eventLoc-1].eventdat))
		{
			// Check for backup item spawn
			if (!eventRec[eventLoc-1].eventdat6)
				break;

			// We have a backup item, so spawn it instead
			temp = JE_newEnemy(
				apEnemySection[eventRec[eventLoc-1].eventdat3],
				eventRec[eventLoc-1].eventdat6 + 450,
				0);

			using_backup = true;
		}
		else
		{
			temp = JE_newEnemy(
				apEnemySection[eventRec[eventLoc-1].eventdat3],
				apBehaviorList[eventRec[eventLoc-1].eventdat3],
				0);			
		}

		if (temp != 0)
		{
			switch (apEnemySection[eventRec[eventLoc-1].eventdat3])
			{
				default:
				case 0:
					enemy[temp-1].ex = eventRec[eventLoc-1].eventdat2 - (mapX - 1) * 24;
					enemy[temp-1].ey -= backMove2;
					break;
				case 25:
				case 75:
					enemy[temp-1].ex = eventRec[eventLoc-1].eventdat2 - (mapX - 1) * 24 - 12;
					enemy[temp-1].ey -= backMove;
					break;
				case 50:
					enemy[temp-1].ex = eventRec[eventLoc-1].eventdat2 - mapX3 * 24 - 24 * 2 + 6;
					enemy[temp-1].ey -= backMove3;
					break;
			}
			enemy[temp-1].ey += eventRec[eventLoc-1].eventdat5;
			enemy[temp-1].linknum = eventRec[eventLoc-1].eventdat4;

			if (!using_backup)
			{
				apCheckData[apCheckCount].location = eventRec[eventLoc-1].eventdat;
				apCheckData[apCheckCount].behavesAs = 0; // Already handled
				tyrian_setupAPItemSpawn(temp, apCheckCount++);				
			}
		}
		break;

	case 201: // AP_CheckFromEnemy
		if (Archipelago_WasChecked(eventRec[eventLoc-1].eventdat))
		{
			// Check for backup item spawn
			if (!eventRec[eventLoc-1].eventdat2)
				break;

			// We have a backup item, so spawn it instead
			for (temp = 0; temp < 100; temp++)
			{
				if (enemy[temp].linknum == eventRec[eventLoc-1].eventdat4)
					enemy[temp].enemydie = eventRec[eventLoc-1].eventdat2;
			}
			break;
		}

		if (ARCHIPELAGO_ITEM + apCheckCount <= ARCHIPELAGO_ITEM_MAX)
		{
			// Get the average position of every enemy with the given linknum
			Sint16 xmin = INT16_MAX, ymin = INT16_MAX;
			Sint16 xmax = INT16_MIN, ymax = INT16_MIN;
			Uint8 assigned = 0xFF;

			for (temp = 0; temp < 100; temp++)
			{
				if (enemyAvail[temp])
					continue;

				if (enemy[temp].linknum == eventRec[eventLoc-1].eventdat4)
				{
					if (assigned == 0xFF)
					{
						enemy[temp].enemydie = ARCHIPELAGO_ITEM + apCheckCount;
						assigned = temp;
					}

					//printf("enemy%3d: %d, %d\n", temp, enemy[temp].ex, enemy[temp].ey);
					xmin = MIN(xmin, enemy[temp].ex);
					xmax = MAX(xmax, enemy[temp].ex);
					ymin = MIN(ymin, enemy[temp].ey);
					ymax = MAX(ymax, enemy[temp].ey);
				}
			}
			if (assigned == 0xFF) // If still unset, no enemies had that linknum, so ignore
				break;

			apCheckData[apCheckCount].alignX = ((xmax + xmin) / 2) - enemy[assigned].ex;
			apCheckData[apCheckCount].alignY = ((ymax + ymin) / 2) - enemy[assigned].ey;
			//printf("x: %d, %d\n", xmin, xmax);
			//printf("y: %d, %d\n", ymin, ymax);
			//printf("align: %d, %d\n", apCheckData[apCheckCount].alignX, apCheckData[apCheckCount].alignY);
			apCheckData[apCheckCount].location = eventRec[eventLoc-1].eventdat;
			apCheckData[apCheckCount].behavesAs = apBehaviorList[eventRec[eventLoc-1].eventdat3];
			++apCheckCount;
		}
		break;

	case 202: // AP_CheckFromLastNewEnemy
		// Used to insert a check on an enemy without a linknum.
		// Must be appended after the NewEnemy event, with the same time.
		if (ARCHIPELAGO_ITEM + apCheckCount > ARCHIPELAGO_ITEM_MAX)
			break; // ???

		if (b > 0)
		{
			if (Archipelago_WasChecked(eventRec[eventLoc-1].eventdat))
			{
				// Check for backup item spawn
				if (!eventRec[eventLoc-1].eventdat2)
					break;

				// We have a backup item, so spawn it instead
				enemy[b-1].enemydie = eventRec[eventLoc-1].eventdat2;
				break;
			}

			enemy[b-1].enemydie = ARCHIPELAGO_ITEM + apCheckCount;
			apCheckData[apCheckCount].location = eventRec[eventLoc-1].eventdat;
			apCheckData[apCheckCount].behavesAs = apBehaviorList[eventRec[eventLoc-1].eventdat3];
			apCheckData[apCheckCount].alignX = eventRec[eventLoc-1].eventdat5;
			apCheckData[apCheckCount].alignY = eventRec[eventLoc-1].eventdat6;
			++apCheckCount;
		}
		break;

	case 216: // HardContactCallout
		if ((eventRec[eventLoc-1].eventdat6 == 1 && APSeedSettings.HardContact)
			|| (eventRec[eventLoc-1].eventdat6 == 2 && !APSeedSettings.HardContact))
		{
			break;
		}
		goto callout_rejoin;

	default:
		fprintf(stderr, "warning: ignoring unknown event %d\n", eventRec[eventLoc-1].eventtype);
		break;
	}

	eventLoc++;
}

static void JE_barX(JE_word x1, JE_word y1, JE_word x2, JE_word y2, JE_byte col)
{
	fill_rectangle_xy(VGAScreen, x1, y1,     x2, y1,     col + 1);
	fill_rectangle_xy(VGAScreen, x1, y1 + 1, x2, y2 - 1, col    );
	fill_rectangle_xy(VGAScreen, x1, y2,     x2, y2,     col - 1);
}

void draw_boss_bar(void)
{
	for (unsigned int b = 0; b < COUNTOF(boss_bar); b++)
	{
		if (boss_bar[b].link_num == 0)
			continue;

		unsigned int armor = 256;  // higher than armor max

		for (unsigned int e = 0; e < COUNTOF(enemy); e++)  // find most damaged
		{
			if (enemyAvail[e] != 1 && enemy[e].linknum == boss_bar[b].link_num)
				if (enemy[e].armorleft < armor)
					armor = enemy[e].armorleft;
		}

		if (armor > 255 || armor == 0)  // boss dead?
			boss_bar[b].link_num = 0;
		else
			boss_bar[b].armor = (armor == 255) ? 254 : armor;  // 255 would make the bar too long
	}

	unsigned int bars = (boss_bar[0].link_num != 0 ? 1 : 0)
	                  + (boss_bar[1].link_num != 0 ? 1 : 0);

	// if only one bar left, make it the first one
	if (bars == 1 && boss_bar[0].link_num == 0)
	{
		memcpy(&boss_bar[0], &boss_bar[1], sizeof(boss_bar_t));
		boss_bar[1].link_num = 0;
	}

	for (unsigned int b = 0; b < bars; b++)
	{
		unsigned int x = (bars == 2)
		               ? ((b == 0) ? 125 : 185)
		               : ((levelTimer) ? 250 : 155);  // level timer and boss bar would overlap

		JE_barX(x - 25, 7, x + 25, 12, 115);
		JE_barX(x - (boss_bar[b].armor / 10), 7, x + (boss_bar[b].armor + 5) / 10, 12, 118 + boss_bar[b].color);

		if (boss_bar[b].color > 0)
			boss_bar[b].color--;
	}
}

// ----------------------------------------------------------------------------

void tyrian_setupAPItemSpawn(Sint16 item, Sint16 apItemNum)
{
	// This is unlikely to happen, but _can_ because of other players on the same seed, collects, etc.
	// If we've spawned an AP Item and as we're setting it up, we notice it's already been collected,
	// we just immediately despawn it.
	if (Archipelago_WasChecked(apCheckData[apItemNum].location))
	{
		enemyAvail[item-1] = 1;
		return;
	}

	enemy[item-1].scoreitem = true;
	enemy[item-1].evalue = 28000 + apCheckData[apItemNum].location;

	int animStart = Archipelago_CheckHasProgression(apCheckData[apItemNum].location) ? 7 : 3;
	enemy[item-1].sprite2s = &archipelagoSpriteSheet;
	enemy[item-1].ani = 6;
	enemy[item-1].egr[0] = enemy[item-1].egr[1] = enemy[item-1].egr[2] = animStart + 2;
	enemy[item-1].egr[3] = enemy[item-1].egr[4] = enemy[item-1].egr[5] = animStart;
}

void tyrian_enemyDieItem(JE_byte enemyId)
{
	Sint16 item;
	int newItemId = enemy[enemyId].enemydie;

	// This is to hide the AP Radar on dead ground enemies.
	enemy[enemyId].enemydie = 0;

	int enemy_offset = enemyId - (enemyId % 25);
	if (enemyDat[newItemId].value > 30000)
		enemy_offset = 0;

	if (newItemId >= ARCHIPELAGO_ITEM && newItemId <= ARCHIPELAGO_ITEM_MAX)
	{
		int item_num = newItemId - ARCHIPELAGO_ITEM;
		item = JE_newEnemy(enemy_offset, apCheckData[item_num].behavesAs, 0);
		if (item != 0)
		{
			tyrian_setupAPItemSpawn(item, newItemId - ARCHIPELAGO_ITEM);
			enemy[item-1].ex = enemy[enemyId].ex + apCheckData[item_num].alignX;
			enemy[item-1].ey = enemy[enemyId].ey + apCheckData[item_num].alignY;
		}
		return;
	}

	// Don't spawn purple balls in Super Arcade modes
	if (superArcadeMode != SA_NONE && enemyDat[enemy[enemyId].enemydie].value == 30000)
		return;

	item = JE_newEnemy(enemy_offset, newItemId, 0);
	if (item != 0)
	{
		if ((superArcadeMode != SA_NONE) && (enemy[item-1].evalue > 30000))
		{
			superArcadePowerUp++;
			if (superArcadePowerUp > 5)
				superArcadePowerUp = 1;
			enemy[item-1].egr[1-1] = 5 + superArcadePowerUp * 2;
			enemy[item-1].evalue = 30000 + superArcadePowerUp;
		}

		// Bugfix (original game): Don't set scoreitem here
		// JE_makeEnemy already does a good enough job of that
		// All we do by setting it here is ruin it and mess up stuff
		//if (enemy[item-1].evalue != 0)
		//	enemy[item-1].scoreitem = true;
		//else
		//	enemy[item-1].scoreitem = false;

		enemy[item-1].ex = enemy[enemyId].ex;
		enemy[item-1].ey = enemy[enemyId].ey;
	}
}
