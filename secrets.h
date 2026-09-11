#pragma once

// Rename this file to secrets.h before building (keeps real creds out of
// version control if you push this project to GitHub).

// ---- WiFi ----
#define WIFI_SSID     "Airtel_ashi_6910"
#define WIFI_PASSWORD "air13126"

// ---- Spotify app credentials ----
// From https://developer.spotify.com/dashboard -> your app -> Settings
#define SPOTIFY_CLIENT_ID     "f7f86a4563d343ac9ed4703bead7fc1f"
#define SPOTIFY_CLIENT_SECRET "88af551988e9444988c603f3b70003ba"

// ---- Refresh token ----
// Obtained ONE TIME by running get_refresh_token.py on your computer.
// This is what lets the ESP32 act on your account without ever seeing
// your Spotify password. See README.md.
#define SPOTIFY_REFRESH_TOKEN "AQCaq5KABhGkit0nZzdVpB5i9FtAxlICvoOCCYhSqWZOHR9gSQPeYWBkKWqOi8txtAtrvvnLIV1rdTOMdEO_X-RCZGI3UDQ57JPuNBM_fyBHzivB7-sEScrNhr2cp6Yt9Xs"
