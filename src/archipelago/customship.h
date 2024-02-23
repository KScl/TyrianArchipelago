#ifndef CPP_CUSTOMSHIP_H
#define CPP_CUSTOMSHIP_H

#include "SDL_types.h"

#ifndef SPRITE_H // Redefinition: needed because we output Sprite2_array
typedef struct {
	size_t size;
	Uint8 *data;
} Sprite2_array;
#endif

extern bool useCustomShips;
extern size_t currentCustomShip;

size_t CustomShip_SystemInit(const char *directories[], size_t num_directories);

size_t CustomShip_GetNumFromPath(const char *path);
size_t CustomShip_GetNumFromName(const char *name);

size_t CustomShip_Count(void);
bool CustomShip_Exists(size_t num);
const char *CustomShip_GetPath(size_t num);
const char *CustomShip_GetName(size_t num);
const char *CustomShip_GetAuthor(size_t num);
Sprite2_array *CustomShip_GetSprite(size_t num);

#endif