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
#include "lvlmast.h"

#include "file.h"
#include "opentyr.h"

#include "archipelago/patcher.h"

const JE_char shapeFile[34] = /* [1..34] */
{
	'2', '4', '7', '8', 'A', 'B', 'C', 'D', 'E', 'F',
	'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
	'Q', 'R', 'S', 'T', 'U', '5', '#', 'V', '0', '@',  // [25] should be '&' rather than '5'
	'3', '^', '5', '9'
};

// ----------------------------------------------------------------------------

struct JE_EventRecType eventRec[EVENT_MAXIMUM]; /* [1..eventMaximum] */
JE_word maxEvent;

void level_loadEvents(FILE *level_f, JE_byte levelNum)
{
	fread_u16_die(&maxEvent, 1, level_f);
	Patcher_ReadyPatch(0x06ce2efe, levelNum);

	Uint16 x = 0;
	while (x < maxEvent)
	{
		int skips;
		if ((skips = Patcher_DoPatch(&x)))
		{
			fseek(level_f, 11*skips, SEEK_CUR);
			continue;
		}

		fread_u16_die(&eventRec[x].eventtime, 1, level_f);
		fread_u8_die( &eventRec[x].eventtype, 1, level_f);
		fread_s16_die(&eventRec[x].eventdat,  1, level_f);
		fread_s16_die(&eventRec[x].eventdat2, 1, level_f);
		fread_s8_die( &eventRec[x].eventdat3, 1, level_f);
		fread_s8_die( &eventRec[x].eventdat5, 1, level_f);
		fread_s8_die( &eventRec[x].eventdat6, 1, level_f);
		fread_u8_die( &eventRec[x].eventdat4, 1, level_f);
		++x;
	}
	eventRec[x].eventtime = 65500;  /*Not needed but just in case*/
}
