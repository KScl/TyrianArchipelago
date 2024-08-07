#ifndef CPP_PATCHER_H
#define CPP_PATCHER_H

#include "SDL_types.h"

#include <stdio.h>

typedef struct {
	const int major;
	const int minor;
	const int revision;
} patcher_version_t;

const char *Patcher_SystemInit(FILE *file);

void Patcher_ReadyPatch(const char *gameID, Uint8 episode, Uint8 levelNum);

// Returns number of events to be suppressed (seek 11*retval)
Uint16 Patcher_DoPatch(Uint16 *idx);

#endif
