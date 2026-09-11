#pragma once
#include <Arduino.h>

// 64x64 1-bit "now playing" animation, PackBits-compressed.
// THIS IS A PLACEHOLDER — 2 blank frames, just so the sketch compiles.
//
// To use your real animation:
//   1. Save your original frame array into raw_frames.h (see
//      compress_frames.py's instructions at the top of that file).
//   2. Run: python compress_frames.py
//   3. Replace this file with the animation_data.h it generates.

#define FRAME_WIDTH  64
#define FRAME_HEIGHT 64
#define FRAME_DELAY_MS 42
#define FRAME_COUNT 2

const uint16_t PROGMEM frame_offsets[FRAME_COUNT] = {0, 2};
const uint16_t PROGMEM frame_lengths[FRAME_COUNT] = {2, 2};

// Each frame here is PackBits header 0xFF (256-255=... i.e. 255 -> 257-255=2
// repeats) followed by value 0x00 -> two zero bytes is NOT enough for a
// 512-byte frame; the decoder below stops once it fills the 512-byte
// output buffer regardless, so a short/placeholder stream just yields a
// blank (all-zero) frame. That's fine — it's only here so the project
// builds before you generate the real file.
const uint8_t PROGMEM frame_data[4] = {
  255, 0,
  255, 0,
};
