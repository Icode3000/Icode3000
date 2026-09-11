#pragma once
#include <Arduino.h>

void buttonsBegin();

// Returns 'P' (previous), 'S' (play/pause single press), 'X' (next),
// 'U' (volume up), 'D' (volume down), 'L' (play/pause long press),
// 'K' (play/pause double press — opens the Liked Songs list), or 'N'
// (none).
//
// 'S' only fires after waiting up to DOUBLE_PRESS_WINDOW_MS (config.h)
// to confirm a second press isn't coming — this is the necessary
// tradeoff for reliable double-press detection on the same button.
// 'K' fires once when a second press starts within that window and is
// itself released as a tap (not held).
// 'L' fires once, immediately when play/pause has been held for
// PLAY_LONG_PRESS_MS (config.h), whether that's the first or second
// press of an attempted double-press.
char buttonsPoll();
