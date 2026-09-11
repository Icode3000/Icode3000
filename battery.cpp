#include "battery.h"
#include "config.h"
#include <stdlib.h>

// Heavy per-reading filtering: many samples, calibrated millivolt
// conversion, and outlier trimming BEFORE averaging. This matters far
// more here than on a typical battery gauge, because the usable window
// is only 0.55V (0.80-1.35V) — noise that would be trivial over a normal
// 4.2V LiPo range turns into several percentage points of error here.
#define BATTERY_ADC_SAMPLES 200
#define BATTERY_TRIM_PERCENT 15  // drop the highest/lowest 15% of samples before averaging

static uint16_t adcSamples[BATTERY_ADC_SAMPLES];

// The value actually shown on screen. Only ever moves ±1 at a time, and
// only in the currently "locked" direction — see the trend/hysteresis
// logic in batteryLoop() below.
static int displayedPercent = 50;
static unsigned long lastSampleMs = 0;

// Slow-moving average of the raw reading — this is what establishes the
// underlying trend without reacting to every little blip.
static float smoothedPercent = 50.0f;

// -1 = locked into counting down (discharging), +1 = locked into counting
// up (charging), 0 = not established yet (right after boot).
static int direction = 0;

static int cmpUint16(const void *a, const void *b) {
  return (int)(*(const uint16_t *)a) - (int)(*(const uint16_t *)b);
}

// Takes many samples, rejects the extreme high/low ones (glitches, not
// real signal), and converts using the ESP32's factory ADC calibration
// (analogReadMilliVolts) instead of a naive raw-count-to-voltage formula.
// Calibrated mV readings correct for the ADC's known non-linearity and
// per-chip variance — a big source of the boot-to-boot inconsistency,
// since two different ESP32s (or even the same one on different boots
// before calibration kicks in) can read the same real voltage as
// meaningfully different raw counts otherwise.
static float readRawPercent() {
  for (int i = 0; i < BATTERY_ADC_SAMPLES; i++) {
    adcSamples[i] = analogReadMilliVolts(BATTERY_ADC_PIN);
    delayMicroseconds(150);
  }
  qsort(adcSamples, BATTERY_ADC_SAMPLES, sizeof(uint16_t), cmpUint16);

  int trim = (BATTERY_ADC_SAMPLES * BATTERY_TRIM_PERCENT) / 100;
  long sum = 0;
  int count = 0;
  for (int i = trim; i < BATTERY_ADC_SAMPLES - trim; i++) {
    sum += adcSamples[i];
    count++;
  }
  float voltage = (sum / (float)count) / 1000.0f;  // mV -> V

  float pct = (voltage - BATTERY_EMPTY_V) / (BATTERY_FULL_V - BATTERY_EMPTY_V) * 100.0f;
  if (pct < 0.0f) pct = 0.0f;
  if (pct > 100.0f) pct = 100.0f;
  return pct;
}

void batteryBegin() {
  analogReadResolution(12);
  // Global attenuation, not per-pin: analogReadMilliVolts()'s calibration
  // math reads the global setting (a known ESP32 core quirk), so setting
  // only analogSetPinAttenuation() here would silently use the wrong
  // calibration curve.
  analogSetAttenuation(ADC_11db);
  delay(50);  // let the ADC/reference settle after reconfiguring attenuation

  // Seed with the average of several full (already heavily-filtered)
  // readings taken a few ms apart, rather than a single snapshot. This is
  // what keeps the starting percentage consistent from boot to boot,
  // instead of landing wherever one instant happened to read.
  const int seedReadings = 5;
  float sum = 0;
  for (int i = 0; i < seedReadings; i++) {
    sum += readRawPercent();
    delay(20);
  }
  float seeded = sum / seedReadings;

  smoothedPercent = seeded;
  displayedPercent = (int)(seeded + 0.5f);
  direction = 0;
  lastSampleMs = millis();
}

void batteryLoop() {
  unsigned long now = millis();
  if (now - lastSampleMs < BATTERY_SAMPLE_INTERVAL_MS) return;
  lastSampleMs = now;

  float raw = readRawPercent();

  // Slow exponential low-pass filter — the "true" trend, largely immune
  // to any single reading (BATTERY_TREND_ALPHA is small).
  smoothedPercent += BATTERY_TREND_ALPHA * (raw - smoothedPercent);

  // Establish an initial direction the first time the trend clearly
  // separates from the starting point.
  if (direction == 0) {
    if (smoothedPercent >= displayedPercent + 1.0f) direction = 1;
    else if (smoothedPercent <= displayedPercent - 1.0f) direction = -1;
  }

  // Only step the displayed number in the currently locked direction —
  // this is what makes discharge a clean descent and charge a clean
  // ascent, with no back-and-forth.
  if (direction == 1 && smoothedPercent >= displayedPercent + 1.0f) {
    displayedPercent++;
  } else if (direction == -1 && smoothedPercent <= displayedPercent - 1.0f) {
    displayedPercent--;
  }

  // Only flip direction once the trend has moved BATTERY_HYSTERESIS_PERCENT
  // points past the displayed value in the opposite direction — a real
  // charger plug/unplug, not noise.
  if (direction == 1 && smoothedPercent <= displayedPercent - BATTERY_HYSTERESIS_PERCENT) {
    direction = -1;
  } else if (direction == -1 && smoothedPercent >= displayedPercent + BATTERY_HYSTERESIS_PERCENT) {
    direction = 1;
  }
}

int batteryGetPercent() {
  return displayedPercent;
}
