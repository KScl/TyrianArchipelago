
#include "opentyr.h"
#include "planet.h"

#include "varz.h"
#include "vga256d.h"
#include "video.h"

static JE_integer tempNavX, tempNavY;
static JE_byte planetAni, planetAniWait;

static const JE_word PGR[21] /* [1..21] */ =
{
	4,
	1,2,3,
	41-21,57-21,73-21,89-21,105-21,
	121-21,137-21,153-21,
	151,151,151,151,73-21,73-21,1,2,4
	/*151,151,151*/
};

static const JE_byte PAni[21] /* [1..21] */ = {1,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,1};

static const JE_word planetX[21] = { 200, 150, 240, 300, 270, 280, 320, 260, 220, 150, 160, 210, 80, 240, 220, 180, 310, 330, 150, 240, 200 };
static const JE_word planetY[21] = {  40,  90,  90,  80, 170,  30,  50, 130, 120, 150, 220, 200, 80,  50, 160,  10,  55,  55,  90,  90,  40 };

void tmp_setMapVars(int planet)
{
	const unsigned int x_offset = sprite(PLANET_SHAPES, PGR[planet-1]-1)->width / 2,
	                   y_offset = sprite(PLANET_SHAPES, PGR[planet-1]-1)->height / 2;

	tempNavX = (planetX[planet-1] - x_offset);
	tempNavY = (planetY[planet-1] - y_offset);

	if (planetAniWait > 0)
	{
		planetAniWait--;
	}
	else
	{
		planetAni++;
		if (planetAni > 14)
			planetAni = 0;
		planetAniWait = 5;
	}
}

void JE_drawNavLines(JE_boolean dark)
{
	JE_byte x, y;
	JE_integer tempX, tempY;
	JE_integer tempX2, tempY2;
	JE_word tempW, tempW2;

	tempX2 = tempNavX >> 1;
	tempY2 = tempNavY >> 1;

	tempW = 0;
	for (x = 1; x <= 20; x++)
	{
		tempW += 15;
		tempX = tempW - tempX2;

		if (tempX > 18 && tempX < 135)
		{
			if (dark)
				JE_rectangle(VGAScreen, tempX + 1, 16, tempX + 1, 169, 1);
			else
				JE_rectangle(VGAScreen, tempX, 16, tempX, 169, 5);
		}
	}

	tempW = 0;
	for (y = 1; y <= 20; y++)
	{
		tempW += 15;
		tempY = tempW - tempY2;

		if (tempY > 15 && tempY < 169)
		{
			if (dark)
				JE_rectangle(VGAScreen, 19, tempY + 1, 135, tempY + 1, 1);
			else
				JE_rectangle(VGAScreen, 8, tempY, 160, tempY, 5);

			tempW2 = 0;

			for (x = 0; x < 20; x++)
			{
				tempW2 += 15;
				tempX = tempW2 - tempX2;
				if (tempX > 18 && tempX < 135)
					JE_pix3(VGAScreen, tempX, tempY, 7);
			}
		}
	}
}

void JE_drawPlanet(JE_byte planetNum)
{
	JE_integer tempZ = PGR[planetNum]-1,
	           tempX = planetX[planetNum] + 66 - tempNavX - sprite(PLANET_SHAPES, tempZ)->width / 2,
	           tempY = planetY[planetNum] + 85 - tempNavY - sprite(PLANET_SHAPES, tempZ)->height / 2;

	if (tempX > -7 && tempX + sprite(PLANET_SHAPES, tempZ)->width < 170 && tempY > 0 && tempY < 160)
	{
		if (PAni[planetNum])
			tempZ += planetAni;

		blit_sprite_dark(VGAScreenSeg, tempX + 3, tempY + 3, PLANET_SHAPES, tempZ, false);
		blit_sprite(VGAScreenSeg, tempX, tempY, PLANET_SHAPES, tempZ);  // planets
	}
}
