#pragma once
#include <Arduino.h>

void displayBegin();

// Push new track data + playing state to the screen. Call whenever a
// Spotify poll returns fresh data.
void displayUpdate(const String &name, const String &artist, bool isPlaying);

// Updates the top-right status bar. wifiBars: 0-4, batteryPercent: 0-100.
// Uses placeholder values until real WiFi/battery logic is wired in — call
// this whenever that logic is ready to report real readings.
void displaySetStatus(int wifiBars, int batteryPercent);

// Shows a WiFi captive-portal QR code + AP name + portal IP on the OLED.
// Used only during first-boot / failed WiFi provisioning — see wifi_setup.cpp.
void displayShowWifiSetup(const String &qrPayload, const String &apSsid, const String &portalIp);

// Switches to a full-screen "nothing playing" idle image (the Spotify
// logo + starfield). Call once when playback has been stopped for
// IDLE_TIMEOUT_MS (config.h). Automatically cleared the next time
// displayUpdate() is called with isPlaying == true.
void displayShowIdle();

// Device picker screen: shown on a play/pause long-press. `names` is the
// list of Spotify Connect device names (a "Back" entry is appended
// automatically); `activeIndex` (or -1) marks which one is currently
// playing, shown with a marker. Selection starts on the first entry.
void displayShowDeviceList(const String *names, int count, int activeIndex);

// Moves the highlighted row by `delta` (+1 down, -1 up), wrapping around.
void displayDeviceListMove(int delta);

// Currently highlighted row index. A value == the `count` passed to
// displayShowDeviceList() means the "Back" entry is selected.
int displayDeviceListGetSelection();

void displayHideDeviceList();

// Liked Songs picker screen, opened by holding vol-up + vol-down together.
// `names` is the list of song names (a "Back" entry is appended
// automatically). Selection starts on the first entry.
void displayShowSongList(const String *names, int count);

// Moves the highlighted row by `delta` (+1 down, -1 up), wrapping around.
void displaySongListMove(int delta);

// Currently highlighted row index. A value == the `count` passed to
// displayShowSongList() means the "Back" entry is selected.
int displaySongListGetSelection();

void displayHideSongList();

// Diagnostic only — prints animation frame data status to Serial.
// Call once from setup(), after displayBegin(), then check Serial Monitor.
void displayDebugAnimation();

void displayShowMessage(const String &line1, const String &line2 = "");

// Call every loop() iteration: advances the animation and scrolling text.
void displayLoop();
