#pragma once

// ---- OLED (I2C) ----
#define OLED_SDA 21
#define OLED_SCL 22
// 1.3" 128x64 I2C OLEDs are almost always SH1106 controllers.

// ---- Playback buttons ----
// Wire each button between the GPIO and GND. Internal pull-ups are used,
// so pressing the button pulls the pin LOW.
#define BTN_PREV_PIN 25
#define BTN_PLAY_PIN 26
#define BTN_NEXT_PIN 27
#define BUTTON_DEBOUNCE_MS 120

// How long play/pause must be held to count as a "long press" — used both
// to skip the WiFi connect wait (wifi_setup.cpp) and to open the device
// picker from the home screen (spotify_oled_display.ino).
#define PLAY_LONG_PRESS_MS 1000

// How long a single play/pause press waits before confirming there's no
// second tap coming — i.e. that it should register as a plain 'S'
// (single press). If a second press starts within this window, it opens
// the Liked Songs list ('K') instead of toggling play/pause.
#define DOUBLE_PRESS_WINDOW_MS 350

// ---- Volume buttons ----
#define BTN_VOLUP_PIN   32
#define BTN_VOLDOWN_PIN 33
#define VOLUME_STEP_PERCENT 5   // how much each press changes Spotify volume by

// ---- Battery ADC ----
// GPIO34 is an ADC1, input-only pin — safe choice since ADC2 pins conflict
// with WiFi on the ESP32. Wire your battery-monitor analog output here.
#define BATTERY_ADC_PIN 34
#define BATTERY_EMPTY_V 0.80f   // voltage at 0%
#define BATTERY_FULL_V  1.35f   // voltage at 100%
#define BATTERY_SAMPLE_INTERVAL_MS 2000  // how often the displayed % is allowed to step by 1
#define BATTERY_TREND_ALPHA 0.02f        // smoothing factor for the underlying trend (smaller = slower/steadier)
#define BATTERY_HYSTERESIS_PERCENT 4.0f  // trend must move this many % points opposite the current direction to flip it

// ---- Polling ----
#define POLL_INTERVAL_MS       2000  // how often to ask Spotify what's playing
#define TOKEN_REFRESH_MARGIN_S 60    // refresh access token this many seconds early

// ---- Idle / fallback screen ----
// If nothing has been playing for this long, switch to the idle image.
#define IDLE_TIMEOUT_MS (5UL * 60UL * 1000UL)  // 5 minutes

// If the home screen has had no valid Spotify data at all (can't reach
// Spotify, token/API failures, etc.) for this long, show the same idle
// image as a "not connected" fallback instead of leaving the home screen
// showing an empty area with no track details or animation.
#define SPOTIFY_DATA_TIMEOUT_MS (2UL * 60UL * 1000UL)  // 2 minutes

// ---- Liked Songs list ----
#define LIKED_SONGS_LIST_TIMEOUT_MS 45000  // auto-close back to home after this long with no input

// ---- WiFi provisioning ----
// How long to try connecting with saved/secrets.h credentials before
// giving up and opening the setup portal.
#define WIFI_CONNECT_TIMEOUT_MS 60000  // 1 minute

// SoftAP the ESP32 broadcasts when it can't reach a known network. The QR
// code on the OLED encodes these so a phone can join with one scan.
#define AP_SSID     "SpotifyDisplay-Setup"
#define AP_PASSWORD "setup1234"   // WPA2 requires 8+ chars; use "" for an open network
