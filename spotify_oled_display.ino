/*
  Wireless Spotify "Now Playing" Display
  ESP32 + 1.3" I2C OLED (SH1106/SSD1306)
  + 5 buttons (prev / play-pause / next / volume+ / volume-)
  + 1 analog battery input
  + WiFi captive-portal provisioning (QR code setup on connection failure)

  UI: animated icon on the left (playing) / frozen first frame (paused),
  song name (bold) + play/pause icon + artist name on the right, all
  driven live from Spotify's currently-playing state. Switches to an idle
  screen after IDLE_TIMEOUT_MS of no playback, and to a WiFi setup QR
  screen if it can't connect to a known network within
  WIFI_CONNECT_TIMEOUT_MS.

  Long-pressing play/pause on the home screen opens a Spotify Connect
  device picker: skip-forward/back move the selection, play/pause selects
  (or goes Back), then it returns to the home screen with buttons back to
  normal playback control.

  Double-pressing play/pause on the home screen opens a Liked Songs list:
  vol-up/vol-down move the selection, play/pause selects (or goes Back),
  auto-closes after LIKED_SONGS_LIST_TIMEOUT_MS of no input.

  If Spotify has never successfully returned data (auth/network/API
  issues) and the home screen has been sitting empty for
  SPOTIFY_DATA_TIMEOUT_MS, it shows the same idle screen as a "not
  connected" fallback instead of leaving the home screen blank
  indefinitely. It jumps back to the home screen automatically the
  moment playback actually starts.

  Playback + volume buttons are wired: GPIO -> button -> GND, using
  internal pull-ups. Pressing a button pulls the pin LOW.

  Battery: an analog input reading 0.80V (empty) to 1.35V (full), mapped
  to a 0-100% reading that steps one percent at a time (see battery.cpp).

  See README.md for setup (Spotify app creation + getting a refresh token)
  and wiring instructions before you flash this.
*/

#include <WiFi.h>
#include "secrets.h"      // copy secrets_template.h -> secrets.h and fill in
#include "config.h"
#include "spotify_api.h"
#include "display_ui.h"
#include "buttons.h"
#include "battery.h"
#include "wifi_setup.h"

static unsigned long lastPoll = 0;
static unsigned long lastStatusUpdate = 0;
static bool lastIsPlaying = false;
static String lastTrackName = "", lastTrackArtist = "";

// 0 = currently playing (or we don't know yet); otherwise the millis()
// timestamp of when Spotify was first observed to be paused/stopped.
static unsigned long notPlayingSinceMs = 0;

// True once any poll has returned valid data from Spotify at all (even a
// "nothing playing" response) — distinct from notPlayingSinceMs, which
// only starts once we KNOW playback is paused. This one is about never
// having heard back successfully in the first place.
static bool everGotValidData = false;
static unsigned long noConnectionSinceMs = 0;

// True while the 5-minute auto-idle screen is showing. Gates button
// handling: the first playback-button press while idle only wakes the
// screen back up; it does NOT also act on Spotify. The press after that
// registers normally. (Long-press no longer triggers this screen — see
// inDeviceListMode below for what long-press does now.)
static bool inIdleMode = false;

// True while the Spotify Connect device picker is showing.
#define MAX_SPOTIFY_DEVICES 8
static SpotifyDevice deviceList[MAX_SPOTIFY_DEVICES];
static int deviceListCount = 0;
static bool inDeviceListMode = false;

// True while the Liked Songs picker is showing.
#define MAX_LIKED_SONGS 15
static LikedTrack likedSongs[MAX_LIKED_SONGS];
static int likedSongsCount = 0;
static bool inLikedSongsMode = false;
static unsigned long likedSongsLastInputMs = 0;

// Maps WiFi RSSI (dBm) to a 0-4 bar count for the status bar icon.
// Thresholds are the commonly used rule-of-thumb bands for 2.4/5GHz WiFi.
static int wifiRssiToBars(int rssi) {
  if (rssi >= -55) return 4;
  if (rssi >= -65) return 3;
  if (rssi >= -75) return 2;
  if (rssi >= -85) return 1;
  return 0;
}

static void openDeviceList() {
  deviceListCount = spotifyGetDevices(deviceList, MAX_SPOTIFY_DEVICES);

  String names[MAX_SPOTIFY_DEVICES];
  int activeIdx = -1;
  for (int i = 0; i < deviceListCount; i++) {
    names[i] = deviceList[i].name;
    if (deviceList[i].isActive) activeIdx = i;
  }

  displayShowDeviceList(names, deviceListCount, activeIdx);
  inDeviceListMode = true;
}

static void openLikedSongsList() {
  likedSongsCount = spotifyGetLikedSongs(likedSongs, MAX_LIKED_SONGS);

  String names[MAX_LIKED_SONGS];
  for (int i = 0; i < likedSongsCount; i++) {
    names[i] = likedSongs[i].name;
  }

  displayShowSongList(names, likedSongsCount);
  inLikedSongsMode = true;
  likedSongsLastInputMs = millis();
}

void handleButtons() {
  char b = buttonsPoll();
  if (b == 'N') return;

  // Liked Songs list: vol-up/down navigate, play/pause selects. Any
  // input resets the 45-second no-input auto-close timer.
  if (inLikedSongsMode) {
    likedSongsLastInputMs = millis();
    switch (b) {
      case 'U':
        displaySongListMove(-1);
        break;
      case 'D':
        displaySongListMove(1);
        break;
      case 'S': {
        int sel = displaySongListGetSelection();
        if (sel >= 0 && sel < likedSongsCount) {
          spotifyPlayTrack(likedSongs[sel].uri);
        }
        // sel == likedSongsCount means "Back" was selected — no API call.
        inLikedSongsMode = false;
        displayHideSongList();
        lastPoll = 0;  // refresh track info soon
        break;
      }
      // 'P'/'X'/'L'/'K' while this list is open are ignored.
    }
    return;
  }

  // Device picker: skip-forward/back AND volume up/down all navigate the
  // list here — volume does not adjust playback volume while this is
  // open. Play/pause selects. Checked before the global volume handling
  // below specifically so it can intercept 'U'/'D' this way.
  if (inDeviceListMode) {
    switch (b) {
      case 'X':  // skip forward -> move down the list
      case 'D':  // volume down -> move down the list
        displayDeviceListMove(1);
        break;
      case 'P':  // skip backward -> move up the list
      case 'U':  // volume up -> move up the list
        displayDeviceListMove(-1);
        break;
      case 'S': {
        int sel = displayDeviceListGetSelection();
        if (sel >= 0 && sel < deviceListCount) {
          spotifyTransferPlayback(deviceList[sel].id);
        }
        // sel == deviceListCount means "Back" was selected — no API call.
        inDeviceListMode = false;
        displayHideDeviceList();
        lastPoll = 0;  // refresh track/device info soon
        break;
      }
      // 'L' (another long-press while already in the list) is ignored.
    }
    return;
  }

  // Volume works normally everywhere else (home screen, idle screen).
  if (b == 'U') { spotifyVolumeUp(); return; }
  if (b == 'D') { spotifyVolumeDown(); return; }

  // If the auto-idle screen is showing, ANY button press just wakes it
  // back to the home screen. That press is fully consumed by waking up —
  // it does not also act. The next press after this behaves normally.
  // 'K' is included here (not treated as a bypass like volume) since it
  // comes from the same physical button as 'S'/'L'.
  if (inIdleMode) {
    if (b == 'P' || b == 'S' || b == 'X' || b == 'L' || b == 'K') {
      inIdleMode = false;
      displayUpdate(lastTrackName, lastTrackArtist, lastIsPlaying);
      lastPoll = 0;  // refresh from Spotify soon in case anything changed while idle
    }
    return;
  }

  // Normal home-screen behavior.
  switch (b) {
    case 'P':
      spotifyPrevious();
      lastPoll = 0;  // force an immediate re-poll so the screen updates fast
      break;
    case 'X':
      spotifyNext();
      lastPoll = 0;
      break;
    case 'S':
      if (lastIsPlaying) spotifyPause();
      else spotifyPlay();
      lastPoll = 0;
      break;
    case 'L':
      openDeviceList();
      break;
    case 'K':
      openLikedSongsList();
      break;
  }
}

void pollSpotify() {
  TrackInfo info;
  if (!spotifyGetCurrentlyPlaying(info) || !info.valid) return;

  everGotValidData = true;  // we successfully heard back from Spotify, however briefly

  lastIsPlaying = info.isPlaying;
  lastTrackName = info.name;
  lastTrackArtist = info.artist;
  displayUpdate(info.name, info.artist, info.isPlaying);

  if (info.isPlaying) {
    notPlayingSinceMs = 0;
    inIdleMode = false;  // playback resumed (e.g. from your phone) cancels idle too
  } else if (notPlayingSinceMs == 0) {
    notPlayingSinceMs = millis();
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  displayBegin();
  displayDebugAnimation();  // TEMPORARY: check Serial Monitor, remove once animation is confirmed working
  displayShowMessage("Connecting WiFi...");

  // Tries saved/secrets.h credentials for WIFI_CONNECT_TIMEOUT_MS, or
  // until play/pause is long-pressed, whichever comes first; if that
  // fails, shows a QR setup screen and blocks in a captive portal until
  // new credentials are submitted, then reboots. Only returns on success.
  wifiSetupBegin();

  buttonsBegin();
  batteryBegin();
  spotifyBegin();

  displayShowMessage("Connecting to", "Spotify...");
  noConnectionSinceMs = millis();  // starts the SPOTIFY_DATA_TIMEOUT_MS clock below
}

void loop() {
  handleButtons();
  batteryLoop();  // rate-limited internally; safe to call every iteration

  // Skip the regular Spotify poll while a menu is open — its blocking
  // HTTPS call was the actual cause of laggy scrolling/selection, since
  // it freezes button handling for however long that request takes.
  // Nothing about now-playing needs to update while browsing a menu.
  if (!inDeviceListMode && !inLikedSongsMode && millis() - lastPoll > POLL_INTERVAL_MS) {
    lastPoll = millis();
    pollSpotify();
  }

  // Switch to the idle screen once playback has been stopped continuously
  // for IDLE_TIMEOUT_MS. displayUpdate() cancels it automatically the
  // moment playback resumes.
  if (!lastIsPlaying && notPlayingSinceMs != 0 && !inIdleMode && !inDeviceListMode && !inLikedSongsMode &&
      millis() - notPlayingSinceMs >= IDLE_TIMEOUT_MS) {
    displayShowIdle();
    inIdleMode = true;
  }

  // Same idle screen, different trigger: Spotify has never successfully
  // returned data at all (bad token, network, no active session yet,
  // etc.), so the home screen would otherwise just sit empty forever.
  // Reverts to the home screen automatically once playback actually
  // starts (handled by the same displayUpdate()/inIdleMode logic above).
  if (!everGotValidData && !inIdleMode && !inDeviceListMode && !inLikedSongsMode &&
      millis() - noConnectionSinceMs >= SPOTIFY_DATA_TIMEOUT_MS) {
    displayShowIdle();
    inIdleMode = true;
  }

  // Auto-close the Liked Songs list after LIKED_SONGS_LIST_TIMEOUT_MS of
  // no input, back to the home screen.
  if (inLikedSongsMode && millis() - likedSongsLastInputMs >= LIKED_SONGS_LIST_TIMEOUT_MS) {
    inLikedSongsMode = false;
    displayHideSongList();
  }

  // Push real-time WiFi signal strength + current battery reading to the
  // status bar once a second.
  if (millis() - lastStatusUpdate > 1000) {
    lastStatusUpdate = millis();
    int bars = WiFi.isConnected() ? wifiRssiToBars(WiFi.RSSI()) : 0;
    displaySetStatus(bars, batteryGetPercent());
  }

  displayLoop();  // drives animation + text scrolling
  delay(10);
}
