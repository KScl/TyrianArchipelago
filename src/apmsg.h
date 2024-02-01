
#ifndef APMSG_H
#define APMSG_H

// apmsg_enqueue, apmsg_playSFX, and apmsg_setSpeed are exclusively for use with
// apconnect.cpp, and thus aren't defined here.

// Call to manage the queue and draw text on screen.
void apmsg_manageQueue(bool ingame);

// Call to draw to the in game text window directly.
// (Some events and actions still use this)
void apmsg_drawInGameText(const char *text); // fka JE_drawTextWindow

#endif
