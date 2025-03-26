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
#include "jukebox.h"

#include "apmsg.h"
#include "font.h"
#include "joystick.h"
#include "keyboard.h"
#include "lds_play.h"
#include "loudness.h"
#include "mtrand.h"
#include "nortsong.h"
#include "opentyr.h"
#include "palette.h"
#include "sprite.h"
#include "starlib.h"
#include "vga_palette.h"
#include "video.h"

#include <stdio.h>

#define NUM_JUKEBOX_MESSAGES 8

static char msg_queue[NUM_JUKEBOX_MESSAGES][80];
static int msg_timer = 0;
static unsigned int msg_index = 0;

static void jbmsg_popMessage(void)
{
	if (msg_index > 0)
	{
		for (unsigned int i = 1; i < msg_index; ++i)
			memcpy(msg_queue[i-1], msg_queue[i], 80);
		msg_queue[--msg_index][0] = 0;
	}
	msg_timer = 0;
}

static void jbmsg_manageAndDraw(bool hide_text)
{
	if (msg_index > 1 && ++msg_timer >= 160)
		jbmsg_popMessage();
	else if (msg_index == 1 && ++msg_timer >= 240)
		jbmsg_popMessage();

	apmsg_manageQueueJukebox();

	if (hide_text)
		return;
	for (unsigned int i = 0; i < msg_index; ++i)
		draw_font_hv(VGAScreen, 10, 10+(10*i), msg_queue[i], small_font, left_aligned, 1, 0);
}

bool jbmsg_queueMessage(const char *str, int nudge_time)
{
	if (msg_index == 0)
		msg_timer = 0;

	// No room, try to nudge the topmost message off
	if (msg_index >= NUM_JUKEBOX_MESSAGES)
	{
		if (msg_timer >= nudge_time)
			jbmsg_popMessage();
	}

	// If still above, couldn't make room
	if (msg_index >= NUM_JUKEBOX_MESSAGES)
		return false;

	strncpy(msg_queue[msg_index], str, 80-1);
	msg_queue[msg_index][80-1] = 0;
	++msg_index;
	return true;
}

// ----- Original jukebox code -----

void jukebox(void)  // FKA Setup.jukeboxGo
{
	bool trigger_quit = false,  // true when user wants to quit
	     quitting = false;
	
	bool hide_text = false;

	bool fade_looped_songs = true, fading_song = false;
	bool stopped = false;

	bool fx = false;
	int fx_num = 0;

	int palette_fade_steps = 15;

	int diff[256][3];
	init_step_fade_palette(diff, vga_palette, 0, 255);

	JE_starlib_init();

	int fade_volume = tyrMusicVolume;

	// Init message queue
	memset(msg_queue, 0, sizeof(msg_queue));
	msg_timer = 0;
	msg_index = 0;

	// Don't instantly fade out song if we've been in the menu a while
	songlooped = false;

	for (; ; )
	{
		if (!stopped && !audio_disabled)
		{
			bool play_new_song = !playing;

			if (songlooped && fade_looped_songs)
				fading_song = true;

			if (fading_song)
			{
				if (fade_volume > 2)
				{
					--fade_volume;
				}
				else
				{
					fade_volume = tyrMusicVolume;

					fading_song = false;
					play_new_song = true;
				}

				set_volume(fade_volume, fxVolume);
			}

			// If the music faded out or stopped, restart it.
			if (play_new_song)
			{
				if (fade_looped_songs) // Shuffle
				{
					unsigned int next_song = song_playing;
					while (next_song == song_playing)
						next_song = mt_rand() % MUSIC_NUM;
					play_song(next_song);
				}
				else // Loop forever
				{
					restart_song();
				}
			}
		}

		setDelay(1);

		SDL_FillRect(VGAScreenSeg, NULL, 0);

		// starlib input needs to be rewritten
		JE_starlib_main();

		push_joysticks_as_keyboard();
		service_SDL_events(true);

		if (!hide_text)
		{
			char buffer[60];
			
			if (fx)
				snprintf(buffer, sizeof(buffer), "%d %s", fx_num + 1, soundTitle[fx_num]);
			else
				snprintf(buffer, sizeof(buffer), "%d %s", song_playing + 1, musicTitle[song_playing]);
			
			const int x = VGAScreen->w / 2;
			
			draw_font_hv(VGAScreen, x, 170, "Press ESC to quit the jukebox.",           small_font, centered, 1, 0);
			draw_font_hv(VGAScreen, x, 180, "Arrow keys change the song being played.", small_font, centered, 1, 0);
			draw_font_hv(VGAScreen, x, 190, buffer,                                     small_font, centered, 1, 4);
		}
		jbmsg_manageAndDraw(hide_text);

		if (palette_fade_steps > 0)
			step_fade_palette(diff, palette_fade_steps--, 0, 255);

		JE_showVGA();

		wait_delay();

		// quit on mouse click
		Uint16 x, y;
		if (JE_mousePosition(&x, &y) > 0)
			trigger_quit = true;

		if (newkey)
		{
			switch (lastkey_scan)
			{
			case SDL_SCANCODE_ESCAPE: // quit jukebox
			case SDL_SCANCODE_Q:
				trigger_quit = true;
				break;

			case SDL_SCANCODE_SPACE:
				hide_text = !hide_text;
				break;

			case SDL_SCANCODE_F:
				fading_song = !fading_song;
				break;
			case SDL_SCANCODE_N:
				fade_looped_songs = !fade_looped_songs;
				if (fade_looped_songs)
					jbmsg_queueMessage("Play mode: Shuffle", 0);
				else
					jbmsg_queueMessage("Play mode: Loop forever", 0);
				break;

			case SDL_SCANCODE_SLASH: // switch to sfx mode
				fx = !fx;
				if (fx)
					jbmsg_queueMessage("Sound effects mode: Enabled (use ~comma~, ~period~, ~semicolon~)", 0);
				else
					jbmsg_queueMessage("Sound effects mode: Disabled", 0);
				break;
			case SDL_SCANCODE_COMMA:
				if (!fx)
					break;
				do
				{
					if (--fx_num < 0)
						fx_num = SOUND_COUNT - 1;
				} while (!soundSampleCount[fx_num]);
				break;
			case SDL_SCANCODE_PERIOD:
				if (!fx)
					break;
				do
				{
					if (++fx_num >= SOUND_COUNT)
						fx_num = 0;					
				} while (!soundSampleCount[fx_num]);
				break;
			case SDL_SCANCODE_SEMICOLON:
				if (fx)
					JE_playSampleNum(fx_num + 1);
				break;

			case SDL_SCANCODE_LEFT:
			case SDL_SCANCODE_UP:
				play_song((song_playing > 0 ? song_playing : MUSIC_NUM) - 1);
				stopped = false;
				break;
			case SDL_SCANCODE_RETURN:
			case SDL_SCANCODE_RIGHT:
			case SDL_SCANCODE_DOWN:
				play_song((song_playing + 1) % MUSIC_NUM);
				stopped = false;
				break;
			case SDL_SCANCODE_S: // stop song
				stop_song();
				stopped = true;
				break;
			case SDL_SCANCODE_R: // restart song
				restart_song();
				stopped = false;
				break;

			case SDL_SCANCODE_F1:
				jbmsg_queueMessage("~Jukebox Hotkeys~", 0);
				jbmsg_queueMessage("  ~space~: Toggle text displays", 0);
				jbmsg_queueMessage("  ~/~: Toggle sound effects mode", 0);
				jbmsg_queueMessage("  ~N~: Change play mode (shuffle/loop)", 0);
				jbmsg_queueMessage("  ~F~: Fade out song", 0);
				jbmsg_queueMessage("  ~R~: Restart song", 0);
				jbmsg_queueMessage("  ~S~: Stop all music", 0);
				break;

			default:
				break;
			}
		}
		
		// user wants to quit, start fade-out
		if (trigger_quit && !quitting)
		{
			palette_fade_steps = 15;
			
			SDL_Color black = { 0, 0, 0 };
			init_step_fade_solid(diff, black, 0, 255);
			
			quitting = true;
		}
		
		// if fade-out finished, we can finally quit
		if (quitting && palette_fade_steps == 0)
			break;
	}

	set_volume(tyrMusicVolume, fxVolume);
}
