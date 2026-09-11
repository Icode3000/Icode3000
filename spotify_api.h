#pragma once
#include <Arduino.h>

struct TrackInfo {
  String name;
  String artist;
  bool isPlaying = false;
  bool valid = false;
};

// One entry from GET /v1/me/player/devices.
struct SpotifyDevice {
  String id;
  String name;
  bool isActive = false;
};

void spotifyBegin();
bool spotifyEnsureAccessToken();
bool spotifyGetCurrentlyPlaying(TrackInfo &out);

void spotifyPlay();
void spotifyPause();
void spotifyNext();
void spotifyPrevious();

// Steps Spotify's playback volume up/down by VOLUME_STEP_PERCENT (config.h).
// Volume is tracked locally (synced from the active device on first use)
// since Spotify's API only takes an absolute volume_percent, not deltas.
void spotifyVolumeUp();
void spotifyVolumeDown();

// Fetches available Spotify Connect devices into outDevices (caller-owned
// array of at least maxCount entries). Returns how many were filled in
// (0 on failure or if there are none).
int spotifyGetDevices(SpotifyDevice *outDevices, int maxCount);

// Transfers playback to the given device ID, keeping it playing.
void spotifyTransferPlayback(const String &deviceId);

// One entry from GET /v1/me/tracks (the user's Liked Songs).
struct LikedTrack {
  String uri;   // e.g. "spotify:track:XXXX" — what playback needs
  String name;
  String artist;
};

// Fetches up to maxCount of the user's most recently liked songs into
// outTracks. Returns how many were filled in (0 on failure).
// Uses GET /me/tracks — confirmed unaffected by Spotify's February 2026
// Web API Dev Mode changes (only the save/remove/"contains" write
// endpoints moved to the generic /me/library; reading is unchanged).
int spotifyGetLikedSongs(LikedTrack *outTracks, int maxCount);

// Starts playback of a specific track by URI (PUT /me/player/play).
void spotifyPlayTrack(const String &uri);
