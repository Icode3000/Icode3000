#include "buttons.h"
#include "config.h"

// Prev/next: simple debounced press-down events (fires immediately,
// auto-repeats while held).
static unsigned long lastPress[2] = {0, 0};  // index: 0=prev, 1=next

// Volume buttons: simple debounced press-down events (fires immediately,
// auto-repeats while held) — no combo logic anymore.
static unsigned long lastVolUpFireMs = 0, lastVolDownFireMs = 0;

// Play/pause: a small state machine distinguishing single press ('S'),
// double press ('K' — opens the Liked Songs list), and long press
// ('L' — opens the device picker). See pollPlayButton() below.
enum PlayButtonState {
  PLAY_IDLE,         // button up, nothing pending
  PLAY_PRESS1,       // first press currently held
  PLAY_LONG_HELD,    // already fired 'L' this hold; waiting for release
  PLAY_WAIT_SECOND,  // first press released as a tap; watching for a second
  PLAY_PRESS2,       // second press currently held
};
static PlayButtonState playState = PLAY_IDLE;
static unsigned long playStateChangedAt = 0;
static unsigned long lastPlayTransitionMs = 0;

void buttonsBegin() {
  pinMode(BTN_PREV_PIN, INPUT_PULLUP);
  pinMode(BTN_PLAY_PIN, INPUT_PULLUP);
  pinMode(BTN_NEXT_PIN, INPUT_PULLUP);
  pinMode(BTN_VOLUP_PIN, INPUT_PULLUP);
  pinMode(BTN_VOLDOWN_PIN, INPUT_PULLUP);
}

static bool debouncedPress(int pin, int idx) {
  if (digitalRead(pin) == LOW) {
    unsigned long now = millis();
    if (now - lastPress[idx] > BUTTON_DEBOUNCE_MS) {
      lastPress[idx] = now;
      return true;
    }
  }
  return false;
}

// Tracks BTN_PLAY_PIN through single-press / double-press / long-press.
// Debounced the same way as the other buttons (BUTTON_DEBOUNCE_MS) at
// every accepted transition, so contact bounce can't cause spurious
// events. See the PlayButtonState enum above for the state diagram.
static char pollPlayButton() {
  bool pressed = (digitalRead(BTN_PLAY_PIN) == LOW);
  unsigned long now = millis();

  switch (playState) {
    case PLAY_IDLE:
      if (pressed && now - lastPlayTransitionMs > BUTTON_DEBOUNCE_MS) {
        lastPlayTransitionMs = now;
        playState = PLAY_PRESS1;
        playStateChangedAt = now;
      }
      break;

    case PLAY_PRESS1:
      if (pressed) {
        if (now - playStateChangedAt >= PLAY_LONG_PRESS_MS) {
          lastPlayTransitionMs = now;
          playState = PLAY_LONG_HELD;
          return 'L';
        }
      } else if (now - lastPlayTransitionMs > BUTTON_DEBOUNCE_MS) {
        // Released before the long-press threshold -> one tap done.
        // Don't fire 'S' yet; wait to see if a second tap follows.
        lastPlayTransitionMs = now;
        playState = PLAY_WAIT_SECOND;
        playStateChangedAt = now;
      }
      break;

    case PLAY_LONG_HELD:
      // Already fired 'L' for this hold — just wait for release.
      if (!pressed && now - lastPlayTransitionMs > BUTTON_DEBOUNCE_MS) {
        lastPlayTransitionMs = now;
        playState = PLAY_IDLE;
      }
      break;

    case PLAY_WAIT_SECOND:
      if (pressed && now - lastPlayTransitionMs > BUTTON_DEBOUNCE_MS) {
        lastPlayTransitionMs = now;
        playState = PLAY_PRESS2;
        playStateChangedAt = now;
      } else if (!pressed && now - playStateChangedAt >= DOUBLE_PRESS_WINDOW_MS) {
        // No second tap arrived in time -> this was just a single press.
        playState = PLAY_IDLE;
        return 'S';
      }
      break;

    case PLAY_PRESS2:
      if (pressed) {
        if (now - playStateChangedAt >= PLAY_LONG_PRESS_MS) {
          // Second press turned into a hold -> treat as long-press,
          // discarding the earlier tap entirely.
          lastPlayTransitionMs = now;
          playState = PLAY_LONG_HELD;
          return 'L';
        }
      } else if (now - lastPlayTransitionMs > BUTTON_DEBOUNCE_MS) {
        lastPlayTransitionMs = now;
        playState = PLAY_IDLE;
        return 'K';  // confirmed double-press
      }
      break;
  }

  return 'N';
}

static char pollVolumeButtons() {
  bool upPressed = (digitalRead(BTN_VOLUP_PIN) == LOW);
  bool downPressed = (digitalRead(BTN_VOLDOWN_PIN) == LOW);
  unsigned long now = millis();

  if (upPressed && now - lastVolUpFireMs > BUTTON_DEBOUNCE_MS) {
    lastVolUpFireMs = now;
    return 'U';
  }
  if (downPressed && now - lastVolDownFireMs > BUTTON_DEBOUNCE_MS) {
    lastVolDownFireMs = now;
    return 'D';
  }

  return 'N';
}

char buttonsPoll() {
  if (debouncedPress(BTN_PREV_PIN, 0)) return 'P';
  if (debouncedPress(BTN_NEXT_PIN, 1)) return 'X';

  char volEvent = pollVolumeButtons();
  if (volEvent != 'N') return volEvent;

  char playEvent = pollPlayButton();
  if (playEvent != 'N') return playEvent;

  return 'N';
}
