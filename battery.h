#pragma once
#include <Arduino.h>

void batteryBegin();

// Call periodically (e.g. every loop() iteration or on a timer). Internally
// rate-limited to config.h's BATTERY_SAMPLE_INTERVAL_MS, so calling it often
// is harmless.
void batteryLoop();

// Current displayed battery percentage (0-100). This is NOT the raw ADC
// reading — see battery.cpp for why.
int batteryGetPercent();
