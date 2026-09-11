#include "spotify_api.h"
#include "secrets.h"
#include "config.h"

#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <base64.h>  // built into the ESP32 Arduino core

static String accessToken;
static unsigned long accessTokenExpiresAtMs = 0;

void spotifyBegin() {
  // Nothing to do yet — token is fetched lazily on first use.
}

// NOTE: client.setInsecure() skips TLS certificate validation. That's the
// simplest way to get this running, but it means you're trusting the
// network path. For production use, pin Spotify's/Accounts' root CA
// certificates instead with client.setCACert(...).

bool spotifyEnsureAccessToken() {
  if (accessToken.length() && millis() < accessTokenExpiresAtMs) {
    return true;
  }

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  if (!https.begin(client, "https://accounts.spotify.com/api/token")) {
    Serial.println("Unable to begin token request");
    return false;
  }

  String creds = String(SPOTIFY_CLIENT_ID) + ":" + String(SPOTIFY_CLIENT_SECRET);
  String b64Creds = base64::encode(creds);

  https.addHeader("Authorization", "Basic " + b64Creds);
  https.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String body = "grant_type=refresh_token&refresh_token=" + String(SPOTIFY_REFRESH_TOKEN);
  int code = https.POST(body);
  String resp = https.getString();
  https.end();

  if (code != 200) {
    Serial.printf("Token refresh failed, HTTP %d: %s\n", code, resp.c_str());
    return false;
  }

  DynamicJsonDocument doc(1024);
  if (deserializeJson(doc, resp)) {
    Serial.println("Token JSON parse failed");
    return false;
  }

  accessToken = doc["access_token"].as<String>();
  int expiresIn = doc["expires_in"] | 3600;
  long marginMs = (long)(expiresIn - TOKEN_REFRESH_MARGIN_S) * 1000L;
  if (marginMs < 0) marginMs = 0;
  accessTokenExpiresAtMs = millis() + (unsigned long)marginMs;

  // Spotify only issues a new refresh_token sometimes; if it's rotated,
  // you'd need to persist the new one. For this project we keep using the
  // original refresh token from secrets.h, which stays valid indefinitely
  // as long as you don't revoke the app's access.
  return true;
}

bool spotifyGetCurrentlyPlaying(TrackInfo &out) {
  out = TrackInfo();
  if (!spotifyEnsureAccessToken()) return false;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  if (!https.begin(client, "https://api.spotify.com/v1/me/player/currently-playing")) {
    return false;
  }
  https.addHeader("Authorization", "Bearer " + accessToken);
  int code = https.GET();

  if (code == 204) {
    // No content = nothing currently playing
    https.end();
    out.valid = true;
    out.isPlaying = false;
    out.name = "Nothing playing";
    out.artist = "";
    return true;
  }

  if (code != 200) {
    Serial.printf("currently-playing HTTP %d\n", code);
    https.end();
    return false;
  }

  String payload = https.getString();
  https.end();

  DynamicJsonDocument doc(6144);
  if (deserializeJson(doc, payload)) {
    Serial.println("currently-playing JSON parse failed");
    return false;
  }

  JsonObject item = doc["item"];
  if (item.isNull()) {
    out.valid = true;
    out.name = "Nothing playing";
    return true;
  }

  out.name = item["name"].as<String>();

  String artistStr;
  for (JsonObject a : item["artists"].as<JsonArray>()) {
    if (artistStr.length()) artistStr += ", ";
    artistStr += a["name"].as<String>();
  }
  out.artist = artistStr;
  out.isPlaying = doc["is_playing"] | false;
  out.valid = true;
  return true;
}

static void spotifyPlaybackRequest(const char *method, const char *path) {
  if (!spotifyEnsureAccessToken()) return;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  String url = String("https://api.spotify.com/v1") + path;
  if (!https.begin(client, url)) return;

  https.addHeader("Authorization", "Bearer " + accessToken);
  https.addHeader("Content-Length", "0");

  int code = (strcmp(method, "PUT") == 0) ? https.PUT("") : https.POST("");
  Serial.printf("%s %s -> %d\n", method, path, code);
  https.end();
}

void spotifyPlay()     { spotifyPlaybackRequest("PUT",  "/me/player/play"); }
void spotifyPause()    { spotifyPlaybackRequest("PUT",  "/me/player/pause"); }
void spotifyNext()     { spotifyPlaybackRequest("POST", "/me/player/next"); }
void spotifyPrevious() { spotifyPlaybackRequest("POST", "/me/player/previous"); }

// ---- Volume control ----
// -1 means "not yet synced with the real device".
static int currentVolume = -1;

static void spotifySetVolume(int percent) {
  if (!spotifyEnsureAccessToken()) return;
  percent = constrain(percent, 0, 100);

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  String url = "https://api.spotify.com/v1/me/player/volume?volume_percent=" + String(percent);
  if (!https.begin(client, url)) return;

  https.addHeader("Authorization", "Bearer " + accessToken);
  https.addHeader("Content-Length", "0");
  int code = https.PUT("");
  Serial.printf("PUT volume=%d -> %d\n", percent, code);
  https.end();
}

// Fetches the active device's current volume once, so the first press
// steps from where playback actually is rather than an arbitrary default.
static void spotifySyncVolumeIfNeeded() {
  if (currentVolume >= 0) return;
  if (!spotifyEnsureAccessToken()) return;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  if (!https.begin(client, "https://api.spotify.com/v1/me/player")) return;
  https.addHeader("Authorization", "Bearer " + accessToken);
  int code = https.GET();

  if (code == 200) {
    String payload = https.getString();
    DynamicJsonDocument doc(2048);
    if (!deserializeJson(doc, payload)) {
      int vol = doc["device"]["volume_percent"] | -1;
      if (vol >= 0) currentVolume = vol;
    }
  } else {
    Serial.printf("volume sync HTTP %d\n", code);
  }
  https.end();

  // No active device / request failed — fall back to a sane default so
  // volume buttons still work instead of doing nothing.
  if (currentVolume < 0) currentVolume = 50;
}

void spotifyVolumeUp() {
  spotifySyncVolumeIfNeeded();
  currentVolume = min(100, currentVolume + VOLUME_STEP_PERCENT);
  spotifySetVolume(currentVolume);
}

void spotifyVolumeDown() {
  spotifySyncVolumeIfNeeded();
  currentVolume = max(0, currentVolume - VOLUME_STEP_PERCENT);
  spotifySetVolume(currentVolume);
}

// ---- Device list / transfer playback ----

int spotifyGetDevices(SpotifyDevice *outDevices, int maxCount) {
  if (!spotifyEnsureAccessToken()) return 0;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  if (!https.begin(client, "https://api.spotify.com/v1/me/player/devices")) return 0;
  https.addHeader("Authorization", "Bearer " + accessToken);
  int code = https.GET();

  if (code != 200) {
    Serial.printf("get devices HTTP %d\n", code);
    https.end();
    return 0;
  }

  String payload = https.getString();
  https.end();

  DynamicJsonDocument doc(2048);
  if (deserializeJson(doc, payload)) {
    Serial.println("devices JSON parse failed");
    return 0;
  }

  int n = 0;
  for (JsonObject d : doc["devices"].as<JsonArray>()) {
    if (n >= maxCount) break;
    outDevices[n].id = d["id"].as<String>();
    outDevices[n].name = d["name"].as<String>();
    outDevices[n].isActive = d["is_active"] | false;
    n++;
  }
  return n;
}

void spotifyTransferPlayback(const String &deviceId) {
  if (!spotifyEnsureAccessToken()) return;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  if (!https.begin(client, "https://api.spotify.com/v1/me/player")) return;
  https.addHeader("Authorization", "Bearer " + accessToken);
  https.addHeader("Content-Type", "application/json");

  DynamicJsonDocument doc(256);
  JsonArray ids = doc.createNestedArray("device_ids");
  ids.add(deviceId);
  doc["play"] = true;

  String body;
  serializeJson(doc, body);

  int code = https.PUT(body);
  Serial.printf("PUT transfer playback -> %d\n", code);
  https.end();
}

int spotifyGetLikedSongs(LikedTrack *outTracks, int maxCount) {
  if (!spotifyEnsureAccessToken()) return 0;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  String url = "https://api.spotify.com/v1/me/tracks?limit=" + String(maxCount);
  if (!https.begin(client, url)) return 0;
  https.addHeader("Authorization", "Bearer " + accessToken);
  int code = https.GET();

  if (code != 200) {
    Serial.printf("get liked songs HTTP %d\n", code);
    https.end();
    return 0;
  }

  String payload = https.getString();
  https.end();

  DynamicJsonDocument doc(6144);
  if (deserializeJson(doc, payload)) {
    Serial.println("liked songs JSON parse failed");
    return 0;
  }

  int n = 0;
  for (JsonObject item : doc["items"].as<JsonArray>()) {
    if (n >= maxCount) break;
    JsonObject track = item["track"];

    outTracks[n].uri = track["uri"].as<String>();
    outTracks[n].name = track["name"].as<String>();

    String artistStr;
    for (JsonObject a : track["artists"].as<JsonArray>()) {
      if (artistStr.length()) artistStr += ", ";
      artistStr += a["name"].as<String>();
    }
    outTracks[n].artist = artistStr;
    n++;
  }
  return n;
}

void spotifyPlayTrack(const String &uri) {
  if (!spotifyEnsureAccessToken()) return;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  if (!https.begin(client, "https://api.spotify.com/v1/me/player/play")) return;
  https.addHeader("Authorization", "Bearer " + accessToken);
  https.addHeader("Content-Type", "application/json");

  DynamicJsonDocument doc(256);
  JsonArray uris = doc.createNestedArray("uris");
  uris.add(uri);

  String body;
  serializeJson(doc, body);

  int code = https.PUT(body);
  Serial.printf("PUT play track -> %d\n", code);
  https.end();
}
