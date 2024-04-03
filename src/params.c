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
#include "params.h"

#include "apmenu.h"
#include "arg_parse.h"
#include "file.h"
#include "joystick.h"
#include "loudness.h"
#include "opentyr.h"
#include "varz.h"

#include "archipelago/apconnect.h"
#include "archipelago/customship.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

void JE_paramCheck(int argc, char *argv[])
{
	const Options options[] =
	{
		{ 'h', 'h', "help",        false },
		{ 's', 's', "no-sound",    false },
		{ 'j', 'j', "no-joystick", false },
		{ 't', 't', "data",         true },
		{ 'c', 'c', "connect",      true },

		// Debugging options
		{ 256, 0, "no-custom-ship", false },
		{ 257, 0, "damage-viewer",  false },
		
		{ 0, 0, NULL, false}
	};

	Option option;

	for (; ; )
	{
		option = parse_args(argc, (const char **)argv, options);

		if (option.value == NOT_OPTION)
			break;

		switch (option.value)
		{
		case INVALID_OPTION:
		case AMBIGUOUS_OPTION:
		case OPTION_MISSING_ARG:
			fprintf(stderr, "Try `%s --help' for more information.\n", argv[0]);
			exit(EXIT_FAILURE);
			break;

		case 'h':
			printf("Usage: %s [OPTION...]\n\n"
			       "Options:\n"
			       "  -h, --help                   Show help about options\n\n"
			       "  -s, --no-sound               Disable audio\n"
			       "  -j, --no-joystick            Disable joystick/gamepad input\n\n"
			       "  -t, --data=DIR               Set Tyrian data directory,\n"
			       "                               e.g. -t \"data\" or -t \"data2000\"\n"
			       "  -c, --connect=USER@ADDRESS   Immediately connect to Archipelago server,\n"
			       "                               e.g. -c \"myuser@archipelago.gg:38281\"\n", argv[0]);
			exit(0);
			break;

		// Debugging options
		case 256:
#ifdef DEBUG_OPTIONS
			useCustomShips = false;
#endif
			break;

		case 257:
#ifdef DEBUG_OPTIONS
			skipToGameplay = true;
			debugDamageViewer = true;
#endif
			break;

		// Regular options
		case 's':
			// Disables sound/music usage
			audio_disabled = true;
			break;

		case 'j':
			// Disables joystick detection
			ignore_joystick = true;
			break;

		// set custom Tyrian data directory
		case 't':
			custom_data_dir = option.arg;
			break;

		case 'c':
		{
			intptr_t temp = (intptr_t)strchr(option.arg, ':');
			if (!temp)
			{
				fprintf(stderr, "%s: error: %s: no port specified\n", argv[0], argv[option.argn]);
				goto connect_slot_error;
			}
			else
			{
				temp -= (intptr_t)option.arg;
				int temp_port = atoi(&option.arg[temp + 1]);
				if (!(temp_port > 0 && temp_port < 65536))
				{
					fprintf(stderr, "%s: error: %s: invalid port specified\n", argv[0], argv[option.argn]);
					goto connect_slot_error;
				}
			}
			temp = (intptr_t)strchr(option.arg, '@');
			if (!temp || temp == (intptr_t)option.arg)
			{
				fprintf(stderr, "%s: error: %s: no slot name specified\n", argv[0], argv[option.argn]);
				goto connect_slot_error;
			}

			temp -= (intptr_t)option.arg;
			strncpy(lastGoodServerAddr, &option.arg[temp + 1], sizeof(lastGoodServerAddr)-1);
			lastGoodServerAddr[sizeof(lastGoodServerAddr) - 1] = 0;
			strncpy(lastGoodSlotName, option.arg, sizeof(lastGoodSlotName)-1);
			lastGoodSlotName[temp] = 0;

			skipToGameplay = true;
			Archipelago_Connect(option.arg);
			break;

		connect_slot_error:
			fprintf(stderr, "Argument should be in the format \"slot name@address:port\"\n");
			exit(EXIT_FAILURE);
		}
#if 0
		case 'n':
			isNetworkGame = true;
			
			intptr_t temp = (intptr_t)strchr(option.arg, ':');
			if (temp)
			{
				temp -= (intptr_t)option.arg;
				
				int temp_port = atoi(&option.arg[temp + 1]);
				if (temp_port > 0 && temp_port < 49152)
					network_opponent_port = temp_port;
				else
				{
					fprintf(stderr, "%s: error: invalid network port number\n", argv[0]);
					exit(EXIT_FAILURE);
				}
				
				network_opponent_host = malloc(temp + 1);
				SDL_strlcpy(network_opponent_host, option.arg, temp + 1);
			}
			else
			{
				network_opponent_host = malloc(strlen(option.arg) + 1);
				strcpy(network_opponent_host, option.arg);
			}
			break;
			
		case 256: // --net-player-name
			network_player_name = malloc(strlen(option.arg) + 1);
			strcpy(network_player_name, option.arg);
			break;
			
		case 257: // --net-player-number
		{
			int temp = atoi(option.arg);
			if (temp >= 1 && temp <= 2)
				thisPlayerNum = temp;
			else
			{
				fprintf(stderr, "%s: error: invalid network player number\n", argv[0]);
				exit(EXIT_FAILURE);
			}
			break;
		}
		case 'p':
		{
			int temp = atoi(option.arg);
			if (temp > 0 && temp < 49152)
				network_player_port = temp;
			else
			{
				fprintf(stderr, "%s: error: invalid network port number\n", argv[0]);
				exit(EXIT_FAILURE);
			}
			break;
		}
		case 'd':
		{
			int temp;
			if (sscanf(option.arg, "%d", &temp) == 1)
				network_delay = 1 + temp;
			else
			{
				fprintf(stderr, "%s: error: invalid network delay value\n", argv[0]);
				exit(EXIT_FAILURE);
			}
			break;
		}
#endif

		default:
			assert(false);
			break;
		}
	}
}
