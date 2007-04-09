/* vim: set noet:
 *
 * OpenTyrian Classic: A modern cross-platform port of Tyrian
 * Copyright (C) 2007  The OpenTyrian Team
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
#ifndef LOUDNESS_H
#define LOUDNESS_H

#include "opentyr.h"

#include "SDL.h"

typedef JE_byte JE_MusicType [20000];

#ifndef NO_EXTERNS
extern JE_MusicType musicData;
extern JE_boolean repeated;
extern JE_boolean playing;
#endif

void JE_initialize(JE_word soundblaster, JE_word midi); /* SYN: The arguments to this function are probably meaningless now */
void JE_deinitialize( void );
void JE_play( void );
/* SYN: selectSong is called with 0 to disable the current song. Calling it with 1 will start the current song if not playing,
   or restart it if it is. */
void JE_selectSong( JE_word value ); 
void JE_samplePlay(JE_word addlo, JE_word addhi, JE_word size, JE_word freq);
void setVol(JE_word volume, JE_word sample); /* Call with 0x1-0x100 for music volume, and 0x10 to 0xf0 for sample volume. */
JE_word getVol( void );
JE_word getSampleVol( void );

#endif /* LOUDNESS_H */