/*
 * OpenTyrian Classic: A moden cross-platform port of Tyrian
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
#include "opentyr.h"
#include "nortvars.h"
#include "error.h"

/* File constants for Saving ShapeFile */
const JE_byte NV_shapeactive   = 0x01;
const JE_byte NV_shapeinactive = 0x00;

JE_boolean ScanForJoystick;
JE_word z, y;
JE_boolean InputDetected;
JE_word LastMouseX, LastMouseY;

/*Mouse Data*/  /*Mouse_Installed is in VGA256d*/
JE_byte MouseCursor;
JE_boolean Mouse_ThreeButton;
JE_word MouseX, MouseY, MouseButton;


void JE_LoadShapeFile( JE_shapetype *Shapes, JE_char s )
{
    FILE *f;
    JE_word x;
    JE_boolean active;

    char buffer[12];
    sprintf(buffer, "SHAPES%c.DAT", s);

    JE_resetfile(&f, buffer);

    for(x = 0; x < 304; x++)
    {
        active = getc(f);

        if(active)
            fread((*Shapes)[x], 1, sizeof(*(*Shapes)[x]), f);
        else
            memset((*Shapes)[x], 0, sizeof(*(*Shapes)[x]));
    }

    fclose(f);

    /*fprintf(stderr, "Shapes%c completed.\n", s);*/
}

void JE_LoadNewShapeFile( JE_newshapetype *Shapes, JE_char s )
{
    FILE *f;
    JE_word x, y, z;
    JE_boolean active;
    JE_shapetypeone tempshape;
    JE_byte black, color;

    char buffer[12];
    sprintf(buffer, "SHAPES%c.DAT", s);

    JE_resetfile(&f, buffer);

    for(z = 0; z < 304; z++)
    {
        active = getc(f);

        if(active)
        {
            fread(tempshape, 1, sizeof(tempshape), f);

            for(y = 0; y <= 13; y++)
            {

                black = 0;
                color = 0;
                for(x = 0; x <= 11; x++)
                {
                    if(tempshape[x + y * 12] == 0)
                        black++;
                    else
                        color++;
                }

                if(black == 12)
                {  /* Compression Value 0 - All black */
                    (*Shapes)[z][y * 13] = 0;
                }
                else
                    if(color == 12)
                    {  /* Compression Value 1 - All color */
                        (*Shapes)[z][y * 13] = 1;
                        for(x = 0; x <= 11; x++)
                            (*Shapes)[z][x + 1 + y * 13] = tempshape[x + y * 12];
                    } else {
                        (*Shapes)[z][y * 13] = 2;
                        for(x = 0; x <= 11; x++)
                            (*Shapes)[z][x + 1 + y * 13] = tempshape[x + y * 12];
                    }
            }
        } else
            memset((*Shapes)[z], 0, sizeof((*Shapes)[z]));
    }

    fclose(f);

    /*fprintf(stderr, "Shapes%c completed.\n", s);*/
}

void JE_LoadCompShapes( JE_byte **Shapes, JE_word *ShapeSize, JE_char s, JE_byte **Shape )
{
    FILE *f;

    char buffer[11];
    sprintf(buffer, "NEWSH%c.SHP", s);

    if(*Shapes != NULL)
        free(*Shapes);

    JE_resetfile(&f, buffer);

    fseek(f, 0, SEEK_END);
    *ShapeSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    *Shapes = malloc(*ShapeSize);
    *Shape = *Shapes;

    fread(*Shapes, 1, *ShapeSize, f);

    fclose(f);
}

/* TODO */