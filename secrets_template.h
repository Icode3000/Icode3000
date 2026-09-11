#pragma once

// Rename this file to secrets.h before building (keeps real creds out of
// version control if you push this project to GitHub).

// ---- WiFi ----
#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// ---- Spotify app credentials ----
// From https://developer.spotify.com/dashboard -> your app -> Settings
#define SPOTIFY_CLIENT_ID     "YOUR_SPOTIFY_CLIENT_ID"
#define SPOTIFY_CLIENT_SECRET "YOUR_SPOTIFY_CLIENT_SECRET"

// ---- Refresh token ----
// Obtained ONE TIME by running get_refresh_token.py on your computer.
// This is what lets the ESP32 act on your account without ever seeing
// your Spotify password. See README.md.
#define SPOTIFY_REFRESH_TOKEN "YOUR_REFRESH_TOKEN"
