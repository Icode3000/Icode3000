#pragma once
#include <Arduino.h>

// Tries to connect using saved credentials (falling back to secrets.h on
// first boot) for up to WIFI_CONNECT_TIMEOUT_MS (config.h).
//
// On success: returns normally with WiFi connected.
// On failure: puts the ESP32 into AP mode, shows a scannable QR code +
// setup instructions on the OLED, and runs a captive portal web server
// until the user submits new credentials — then saves them and reboots.
// This function does not return in that case.
void wifiSetupBegin();
