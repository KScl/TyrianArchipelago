
#include "opentyr.h"
#include "apmsg.h"

#include "fonthand.h"
#include "mainint.h"
#include "nortsong.h"
#include "sprite.h"
#include "video.h"

#include "archipelago/apconnect.h"

static char messageQueue[256][80];

static Uint8 mq_cur = 0;
static Uint8 mq_head = 0;
static Uint8 mq_speed = 70;

void apmsg_enqueue(const char *msg)
{
	const char *src_p;
	char *dst_p;
	char *dst_p_begin;

	char text_hue = 'Z';
	char text_lum = 'Z';
	bool hanging_indent = false;

	int cur_width = 0;

	while (true)
	{
		src_p = msg;
		dst_p_begin = messageQueue[mq_head++];

		if (hanging_indent)
			*dst_p_begin++ = ' ';
		if (text_hue != 'Z')
		{
			*dst_p_begin++ = '<';
			*dst_p_begin++ = text_hue;
			*dst_p_begin++ = text_lum;
		}
		dst_p = dst_p_begin;

		cur_width = 0;
		while (*src_p && cur_width < 228 && dst_p - dst_p_begin < 73)
		{
			if (*src_p == '\n')
				break;

			// Received color code?
			if (*src_p == '<' && *(src_p + 1) && *(src_p + 2))
			{
				// Skip these three characters for length calculation.
				*dst_p++ = *src_p++;
				// Technically setting these here is incorrect in rare cases, but I can't be bothered
				text_hue = *src_p++;
				*dst_p++ = text_hue;
				text_lum = *src_p++;
				*dst_p++ = text_lum;
				continue;
			}

			// Received color end?
			if (*src_p == '>')
			{
				text_hue = 'Z';
				*dst_p++ = *src_p++;
				continue;
			}


			int sprite_id;
			if (*src_p == ' ')
				cur_width += 6;
			else if ((sprite_id = font_ascii[(unsigned char)*src_p]) != -1)
				cur_width += sprite(TINY_FONT, sprite_id)->width + 1;

			*dst_p++ = *src_p++;
		}

		// Always add null terminator -- the string will always be this long (or shorter)
		*dst_p = 0;

		// If we reached a null terminator from the source string, we're done
		if (!*src_p)
			break;

		hanging_indent = true;

		// If we reached a newline, we don't need to iterate backward, we're already at a good place
		if (*src_p == '\n')
		{
			dst_p_begin[src_p - msg] = 0;
			msg = src_p + 1;
			continue;
		}

		// If we didn't, iterate backwards to find the last space
		while (src_p > msg && *src_p != ' ')
			--src_p;

		// If we did find a space (we're not back at the start of the message), insert a null terminator there.
		// Set msg to beyond the space, we'll resume from that point.
		if (src_p > msg)
		{
			dst_p_begin[src_p - msg] = 0;
			msg = src_p + 1;
		}

		// If we didn't find a space, then the break in the message is whatever character we stopped at
		// We just advance msg by however many characters we took, and resume from there
		else
			msg += (dst_p - dst_p_begin);
	}

	Uint8 dist = mq_head - mq_cur;

	// If there are many queued up messages, start displaying them increasingly quickly.
	Uint8 new_speed;
	if      (dist < 4)  new_speed = 70;
	else if (dist < 8)  new_speed = 52;
	else if (dist < 15) new_speed = 35;
	else if (dist < 30) new_speed = 17;
	else                new_speed = 10;
	if (mq_speed > new_speed)
		mq_speed = new_speed;
}

void apmsg_playSFX(archipelago_sound_t apSoundID)
{
	Uint8 tyrSample;
	switch (apSoundID)
	{
		case APSFX_RECEIVE_ITEM:  tyrSample = S_POWERUP; break;
		case APSFX_RECEIVE_MONEY: tyrSample = S_ITEM;    break;
		case APSFX_CHAT:          tyrSample = S_CLICK;   break;
		default: return;
	}
	nortsong_playPrioritySound(tyrSample);
}

// Force the queue to go faster. Used exclusively for countdowns
void apmsg_setSpeed(Uint8 speed)
{
	if (mq_speed > speed)
		mq_speed = speed;
}

// Initialize the message queue.
void apmsg_cleanQueue(void)
{
	memset(messageQueue, 0, sizeof(messageQueue));
	mq_cur = 0;
	mq_head = 0;
	mq_speed = 70;
}

// ----------------------------------------------------------------------------

void apmsg_drawScrollBack(Uint8 dist, Uint8 count)
{
	Uint8 current = mq_head - dist;
	int y = 172;

	for (; current != mq_head && count; --count, --current)
	{
		fonthand_outTextColorize(VGAScreen, 10, y, messageQueue[current], 14, 1, true);
		y -= 9;
	}
}

void apmsg_manageQueueMenu(bool stripColor)
{
	mq_cur = mq_head - 1;
	mq_speed = 70;
	if (stripColor)
		JE_outTextAndDarken(VGAScreen, 10, 187, messageQueue[mq_cur], 14, 1, TINY_FONT);
	else
		fonthand_outTextColorize(VGAScreen, 10, 187, messageQueue[mq_cur], 14, 1, true);
	mq_cur = mq_head;
}

void apmsg_manageQueueInGame(void)
{
	if (textErase > 0 && --textErase == 0)
		blit_sprite(VGAScreenSeg, 16, 189, OPTION_SHAPES, 36);  // in-game message area		

	if (mq_cur == mq_head)
		return;

	if (textErase < 100 - mq_speed)
		apmsg_drawInGameText(messageQueue[mq_cur++]);

	if (mq_cur == mq_head)
		mq_speed = 70;
}

void apmsg_drawInGameText(const char *text) // fka JE_drawTextWindow
{
	if (textErase > 0) // erase current text
		blit_sprite(VGAScreenSeg, 16, 189, OPTION_SHAPES, 36);  // in-game text area

	textErase = 100;
	fonthand_outTextColorize(VGAScreenSeg, 20, 190, text, 0, 4, false);
}
