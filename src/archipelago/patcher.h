
#include "SDL_types.h"

#ifndef LVLMAST_H // redefinitions
struct JE_EventRecType
{
	Uint16 eventtime;
	Uint8  eventtype;
	Sint16 eventdat, eventdat2;
	Sint8  eventdat3, eventdat5, eventdat6;
	Uint8  eventdat4;
};

extern struct JE_EventRecType eventRec[2500]; /* [1..eventMaximum] */
extern Uint16 maxEvent;
#endif

bool Patcher_SystemInit(void);

void Patcher_ReadyPatch(Uint32 crc, Uint8 levelNum);

// Returns number of events to be suppressed (seek 11*retval)
Uint16 Patcher_DoPatch(Uint16 *idx);
