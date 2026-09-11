# Wireless Spotify Now-Playing Display (ESP32)

Shows the current track, artist, and album art from your Spotify account on
a 1.3" I2C OLED, with physical previous / play-pause / next buttons.

## Why not just "log in with username and password"?

Spotify retired that option (the "password grant") for third-party apps
years ago — it's a security risk and no longer works. The supported way in
is **OAuth**: you approve access once in a browser, and Spotify gives you a
**refresh token**. The ESP32 stores that token and uses it to silently mint
new short-lived access tokens forever, without ever touching your password.
`get_refresh_token.py` in this folder does that one-time approval for you.

## 1. Create a Spotify app

1. Go to https://developer.spotify.com/dashboard and log in.
2. Click **Create app**. Any name/description is fine.
3. In **Settings**, add this Redirect URI exactly: `http://127.0.0.1:8888/callback`
4. Copy the **Client ID** and **Client Secret** — you'll need them twice
   (once in the Python script, once in `secrets.h`).

## 2. Get your refresh token (one time, on your computer)

```bash
pip install requests
python get_refresh_token.py
```

Edit `CLIENT_ID` / `CLIENT_SECRET` at the top of the script first. It opens
a browser tab, you approve access, and it prints a refresh token in the
terminal.

## 3. Configure the firmware

1. Rename `secrets_template.h` to `secrets.h`.
2. Fill in: WiFi SSID/password, Spotify Client ID, Client Secret, and the
   refresh token from step 2.

`secrets.h` is the only file with real credentials — keep it out of git if
you publish this project.

## 4. Wiring

| Component        | ESP32 Pin |
|-------------------|-----------|
| OLED SDA          | GPIO 21   |
| OLED SCL          | GPIO 22   |
| OLED VCC           | 3.3V      |
| OLED GND           | GND       |
| Previous button    | GPIO 25 → other leg to GND |
| Play/Pause button  | GPIO 26 → other leg to GND |
| Next button        | GPIO 27 → other leg to GND |
| Volume+ button     | GPIO 32 → other leg to GND |
| Volume- button     | GPIO 33 → other leg to GND |
| Battery analog input | GPIO 34 (ADC1, input-only) |

Buttons use the ESP32's internal pull-ups (set in `buttons.cpp`), so no
external resistors are needed — just a plain momentary switch to GND. Pin
numbers are all in `config.h` if you want to move them.

**Battery input:** GPIO 34 expects an analog voltage between 0.80V (empty)
and 1.35V (full) from your battery monitoring circuit — wire whatever
produces that range directly to this pin. `battery.cpp` maps it to 0-100%
and only ever steps the displayed percentage by 1 point at a time (see
"Battery behavior" below), so it won't flicker even if the raw reading is
a bit noisy. GPIO 34 is an ADC1, input-only pin — deliberately chosen
since ADC2 pins can conflict with WiFi on the ESP32.

**Display controller:** most 1.3" 128x64 I2C OLEDs use the **SH1106**
driver (0.96" ones are usually SSD1306). This code defaults to SH1106. If
your text/graphics look shifted or garbled, open `display_ui.cpp` and swap
the constructor line to the SSD1306 variant (commented right above it).

## 5. Arduino IDE setup

Board: **ESP32 Dev Module** (or your specific board) via the ESP32 board
package (esp32 by Espressif Systems, installed through Boards Manager).

Install these libraries via Library Manager:
- **U8g2** by olikraus
- **ArduinoJson** (v6.x) by Benoit Blanchon
- **QRCode** by Richard Moore (for the WiFi setup screen)

`WiFi`, `WebServer`, `DNSServer`, `Preferences`, and `HTTPClient` come
bundled with the ESP32 core, no separate install needed.

`WiFi`, `HTTPClient`, `WiFiClientSecure`, and `base64` come bundled with the
ESP32 core, no separate install needed.

Open `spotify_oled_display.ino` (keep all files in the same folder — the
Arduino IDE will show them as tabs), select your board/port, and upload.

## How it works

- Polls `GET /v1/me/player/currently-playing` every 2 seconds (`POLL_INTERVAL_MS`
  in `config.h`). Spotify has no push/webhook API, so polling is the only option.
- Refreshes the access token automatically ~1 minute before it expires
  using the stored refresh token — no user interaction ever needed again.
- Buttons call `PUT /v1/me/player/play`, `PUT /v1/me/player/pause`,
  `POST /v1/me/player/next`, `POST /v1/me/player/previous`.

### Screen layout

- **Left (64x64):** the animated icon (`animation_data.h`) cycles through
  its frames while a track is actively playing, and freezes on frame 0
  the moment playback pauses — driven directly by Spotify's `is_playing`
  flag, updated every poll.
- **Top right:** song name in bold, larger text. Scrolls automatically if
  it's too long to fit.
- **Middle right:** a play (triangle) / pause (bars) icon reflecting the
  current state.
- **Bottom right:** artist name, smaller text, also auto-scrolls if long.

This build doesn't download real album art — the animation is a fixed
local asset baked into the firmware, not fetched per-track. If you want
real cover art back later, that's a separate feature (JPEG download +
decode) layered on top of this.

### Adding your animation data (space-efficient version)

The animation is stored **RLE-compressed** in flash to keep firmware size
down — only one 512-byte frame is ever decompressed into RAM at a time,
right before it's drawn, so RAM usage stays flat no matter how many
frames the animation has.

To load your real animation:

1. Save your original frame array — the block starting with
   `const byte PROGMEM frames[][512] = {` and ending with the matching
   `};` — into a new file named `raw_frames.h` in this project folder.
2. Run: `python compress_frames.py`
   It prints how much space it saved (typically 70-95% smaller for
   icon-style bitmap animations, since they're mostly long runs of the
   same byte) and writes a new `animation_data.h`.
3. Replace the placeholder `animation_data.h` in your sketch folder with
   the generated one.

The compression is PackBits-style RLE: fully lossless, so the decoded
image is pixel-identical to your original — it just takes less flash to
store.

## WiFi signal strength icon

The status bar's WiFi bars now update in real time from `WiFi.RSSI()`,
checked once a second alongside the battery reading. Roughly: 4 bars above
-55dBm, 3 above -65dBm, 2 above -75dBm, 1 above -85dBm, 0 below that (or
whenever WiFi isn't connected at all). Adjust `wifiRssiToBars()` in the
`.ino` if those thresholds don't match your environment.

## WiFi setup / captive portal

On boot, the firmware tries the last-saved WiFi credentials (or the ones
in `secrets.h` on first boot ever) for `WIFI_CONNECT_TIMEOUT_MS` (1 minute
by default). If that fails:

1. The ESP32 starts its own access point (`AP_SSID` / `AP_PASSWORD` in
   `config.h`) and shows a QR code on the OLED.
2. Scanning it with an Android or iPhone camera joins that AP directly —
   no typing required, since it's a standard `WIFI:` QR payload both
   platforms recognize natively.
3. Once joined, the phone's OS should automatically pop up a "sign in to
   network" prompt (standard captive-portal detection) pointing at a
   simple form to enter your real WiFi name and password. If it doesn't
   pop up automatically, open a browser and go to the IP address shown
   next to the QR code.
4. Submitting the form saves the credentials (`Preferences`/NVS, so they
   survive reboots and power loss) and reboots the ESP32, which then
   connects using them.

This repeats on every boot if the saved network is unreachable — so it
also functions as your recovery path if you ever move the display to a
new WiFi network.

**Note on QR scannability:** at 128x64 resolution, the QR code is rendered
at a small module size (the AP name/password determine the QR version, so
shorter values in `config.h` produce a coarser but larger-per-module code
that's easier to scan). If scanning is unreliable, try shortening
`AP_SSID`/`AP_PASSWORD`, or just read the AP name off the screen and join
it manually from your phone's WiFi settings.

## Idle screen

If Spotify reports nothing playing continuously for `IDLE_TIMEOUT_MS` (5
minutes by default), the display switches to a dedicated idle screen — the
Spotify logo over a starfield. It reverts to the normal now-playing UI
immediately the moment playback resumes.

## Volume buttons

Volume+ / Volume- send `PUT /v1/me/player/volume` with an absolute
`volume_percent` (Spotify's API has no relative "step up/down" call). The
firmware tracks the current volume locally (`spotify_api.cpp`), starting
from the real device's current volume the first time either button is
pressed (fetched via `GET /v1/me/player`), then steps it by
`VOLUME_STEP_PERCENT` (5% by default, in `config.h`) on every press after
that. Like the playback buttons, this only works when Spotify has an
active device — the API can't set volume on a device that isn't playing.

## Liked Songs list

Double-pressing play/pause (two quick taps within `DOUBLE_PRESS_WINDOW_MS`,
350ms by default) opens a scrollable list of your Liked Songs. A single
press still works normally as play/pause — it just waits up to that same
window to confirm a second tap isn't coming before registering, which is
the necessary tradeoff for reliable double-press detection on one button.

Once open: vol-up/vol-down move the selection, play/pause selects (or
exits via the always-present "< Back" row). Selecting a song calls
`PUT /v1/me/player/play` with that track's URI, then returns to the home
screen. If there's no input for `LIKED_SONGS_LIST_TIMEOUT_MS` (45 seconds
by default), it closes back to the home screen on its own.

**On Spotify's rules:** reading Liked Songs (`GET /v1/me/tracks`) and
playing a specific track by URI (`PUT /v1/me/player/play`) are both
standard, actively-documented Web API endpoints — neither was affected by
Spotify's February 2026 Developer Mode changes (that migration only moved
the *save/remove* endpoints to a generic `/me/library`; reading is
unchanged). Two real requirements apply, though: playback control has
always required Spotify **Premium** on the account you're controlling,
and as of the Feb 2026 changes, **the developer account itself** (the one
that owns the Spotify app you registered) also needs an active Premium
subscription — if it lapses, the whole app stops working until
resubscribed.

This requires a fresh refresh token with the `user-library-read` scope —
if `spotifyGetLikedSongs()` returns nothing, re-run `get_refresh_token.py`
and update `secrets.h` with the new token.

## Battery behavior

The displayed battery percentage is intentionally decoupled from the raw
ADC reading in two ways:

1. Each reading is an average of 32 ADC samples, smoothing out
   sample-to-sample electrical noise before it's even converted to a
   percentage.
2. Even after that, the number shown on screen only ever moves by **one
   percentage point** per `BATTERY_SAMPLE_INTERVAL_MS` (2 seconds by
   default) — never jumping straight to a new reading. If the true
   reading is 15% away, it'll take 15 steps (30 seconds) to catch up,
   visibly counting up or down rather than snapping or flickering.

Both intervals and the 0.80V/1.35V calibration range are in `config.h` if
your actual battery/monitoring hardware needs different values.

## Known limitations / things to tune

- **TLS**: uses `client.setInsecure()` to skip certificate validation for
  simplicity. Fine for a hobby project on your home network; for anything
  more sensitive, pin Spotify's root CA with `setCACert()` instead.
- **Regular Spotify polling pauses while any menu is open** (device
  picker, Liked Songs list) — its blocking HTTPS call was the actual
  cause of laggy scrolling/selection, since it froze button handling for
  however long that request took. Track/device state resumes updating
  the moment you close the menu.
- **Playback control requires an active Spotify Connect device** (phone,
  desktop app, speaker, etc. with Spotify open) — the Web API can't start
  playback out of thin air, only control an existing session.
- Free Spotify accounts have some Web API playback restrictions imposed by
  Spotify itself (this is a Spotify-side limitation, not this code).
- Buffer sizes (`jpgBuf[14000]`, JSON doc sizes) assume small album art and
  typical track metadata; bump them if you hit parse/allocation failures
  in Serial output.
