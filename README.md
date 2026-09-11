# Wireless Spotify Display (ESP32)

A 1.3" I2C OLED "now playing" display for Spotify, built on an ESP32. Shows
the current track, artist, and an animated disk icon that spins while
playing and freezes when paused, with a real-time WiFi/battery status bar.
Five physical buttons drive playback, volume, a Spotify Connect device
picker, and a Liked Songs browser — no phone required once it's set up.

No password ever touches the device: setup uses Spotify OAuth (see
[Why not just "log in with username and password"?](#why-not-just-log-in-with-username-and-password)),
and WiFi provisioning happens through an on-device QR code + captive
portal, so it's recoverable even if you move it to a new network later.

## Features

- **Now playing**: track name (bold, auto-scrolling), artist, play/pause
  state, animated disk icon
- **Status bar**: real-time WiFi signal bars (from RSSI) and battery
  percentage (calibrated, filtered, never flickers)
- **5 buttons**: previous, play/pause, next, volume up, volume down —
  see the [gesture cheat sheet](#button-gestures) below for the full set
  of single-press / double-press / long-press actions
- **Spotify Connect device picker**: long-press play/pause to see and
  switch between your active Spotify devices
- **Liked Songs browser**: double-press play/pause to scroll your Liked
  Songs and start playing one directly from the device
- **Idle screen**: switches to a Spotify logo screen after 5 minutes of
  no playback, or after 2 minutes if it's never successfully connected
  to Spotify at all — reverts to the home screen the instant playback
  starts
- **WiFi setup via QR code**: if it can't connect on boot, it opens its
  own access point, shows a scannable QR code, and serves a captive
  portal form to enter new WiFi credentials — no reflashing needed to
  switch networks
- **OAuth-only**: no password ever stored on the device

## Hardware

- ESP32 dev board (any variant with WiFi + enough GPIO)
- 1.3" 128x64 I2C OLED display (SH1106 controller — see the note under
  [Wiring](#wiring) if yours is SSD1306 instead)
- 5x momentary push buttons
- A battery voltage monitoring circuit that outputs 0.80V (empty) to
  1.35V (full) — see `config.h` to recalibrate for different hardware

## Wiring

| Component             | ESP32 Pin                   |
|------------------------|------------------------------|
| OLED SDA               | GPIO 21                      |
| OLED SCL                | GPIO 22                      |
| OLED VCC                | 3.3V                         |
| OLED GND                | GND                          |
| Previous button         | GPIO 25 → other leg to GND   |
| Play/Pause button       | GPIO 26 → other leg to GND   |
| Next button              | GPIO 27 → other leg to GND   |
| Volume+ button           | GPIO 32 → other leg to GND   |
| Volume- button           | GPIO 33 → other leg to GND   |
| Battery analog input     | GPIO 34 (ADC1, input-only)   |

Buttons use the ESP32's internal pull-ups (set in `buttons.cpp`), so no
external resistors are needed — just a plain momentary switch to GND. All
pin numbers live in `config.h` if you need to move them.

**Battery input**: GPIO 34 expects an analog voltage between 0.80V (empty)
and 1.35V (full) from your battery monitoring circuit. GPIO 34 is an
ADC1, input-only pin — deliberately chosen since ADC2 pins can conflict
with WiFi on the ESP32. See [Battery reading](#battery-reading) below
for how the raw reading gets turned into a stable percentage.

**Display controller**: most 1.3" 128x64 I2C OLEDs use the **SH1106**
driver (0.96" ones are usually SSD1306). This code defaults to SH1106. If
your text/graphics look shifted or garbled, open `display_ui.cpp` and
swap the constructor line to the SSD1306 variant (commented right above
it).

## Setup

### 1. Install the Arduino IDE and libraries

Board package: **esp32** by Espressif Systems, installed via Boards
Manager, board **ESP32 Dev Module** (or your specific variant).

Install via Library Manager:
- **U8g2** by olikraus
- **ArduinoJson** (v6.x) by Benoit Blanchon
- **QRCode** by Richard Moore (for the WiFi setup screen)

`WiFi`, `WebServer`, `DNSServer`, `Preferences`, and `HTTPClient` come
bundled with the ESP32 core — no separate install needed.

### 2. Create a Spotify app

1. Go to https://developer.spotify.com/dashboard and log in. **A Premium
   subscription on this account is required** — as of Spotify's February
   2026 Developer Mode changes, apps stop working entirely if the
   developer account's Premium lapses.
2. Click **Create app**. Any name/description works.
3. In **Settings**, add this Redirect URI exactly:
   `http://127.0.0.1:8888/callback`
4. Copy the **Client ID** and **Client Secret** — you'll need both twice
   (once in `get_refresh_token.py`, once in `secrets.h`).

### 3. Get a refresh token (one time, on your computer)

Spotify doesn't support logging in with a plain username/password for
third-party apps anymore — the supported way in is OAuth. You approve
access once in a browser, and Spotify hands back a **refresh token**,
which the ESP32 uses forever after to silently mint new short-lived
access tokens, without ever storing your password. `get_refresh_token.py`
does this one-time approval for you.

Edit `CLIENT_ID` and `CLIENT_SECRET` at the top of the script first, then
run it:

**macOS / Linux (bash):**
```bash
pip install requests
python3 get_refresh_token.py
```

**Windows (PowerShell or Command Prompt):**
```powershell
pip install requests
python get_refresh_token.py
```

It opens a browser tab — log in, click **Agree**, and the script prints
your refresh token in the terminal.

> **If you ever add a new feature that needs a different permission
> scope** (this project has needed a few over its lifetime — playback
> control, then volume, then reading your library), you'll need to
> re-run this script and update `secrets.h` with the new token. An old
> token only carries the scopes it was originally issued with; Spotify
> will return `403` on anything outside them.

### 4. Configure the firmware

1. Rename `secrets_template.h` to `secrets.h`.
2. Fill in: WiFi SSID/password, Spotify Client ID, Client Secret, and the
   refresh token from step 3.

`secrets.h` is the only file with real credentials — **add it to
`.gitignore` before pushing this repo anywhere public.**

### 5. Flash it

Open `spotify_oled_display.ino` in the Arduino IDE (keep every file in
the same folder — they'll show up as tabs), select your board and port,
and upload.

On first boot it'll try to connect with the credentials from `secrets.h`.
If that fails, see [WiFi setup / captive portal](#wifi-setup--captive-portal)
below.

## Button gestures

| Gesture | Action |
|---|---|
| Previous — short press | Skip to previous track |
| Next — short press | Skip to next track |
| Play/Pause — single press | Toggle play/pause |
| Play/Pause — long press (1s) | Open the Spotify Connect **device picker** |
| Play/Pause — double press | Open the **Liked Songs** list |
| Volume+ / Volume- | Adjust Spotify volume ±5% |

**Inside the device picker or Liked Songs list**, Previous/Next and
Volume+/Volume- are both repurposed as list navigation (up/down), and
Play/Pause becomes the select button — see
[Screens](#screens) below for the full behavior of each.

A single play/pause press always waits up to `DOUBLE_PRESS_WINDOW_MS`
(350ms) to confirm a second tap isn't coming, before registering as a
plain press — the necessary tradeoff for reliable double-press detection
on one button. See `buttons.cpp` for the full single/double/long-press
state machine.

## Screens

### Home screen

Left side: the animated disk icon, cycling through frames while playing
and frozen on frame 0 while paused. Right side: song name (bold, large,
auto-scrolling), a play/pause status icon, and artist name
(auto-scrolling). Top-right: WiFi signal bars and battery percentage.

### Idle screen

Shown after `IDLE_TIMEOUT_MS` (5 minutes) of continuous "not playing", or
after `SPOTIFY_DATA_TIMEOUT_MS` (2 minutes) if the device has never
successfully heard back from Spotify at all — a Spotify logo over a
starfield, instead of leaving the home screen sitting empty. The first
button press while this is showing only wakes the screen back to home;
it is not also treated as a playback command. The press after that
behaves normally. It also reverts automatically, with no button needed,
the instant playback actually starts.

### WiFi setup / captive portal

If the firmware can't connect within `WIFI_CONNECT_TIMEOUT_MS` (1
minute) — or you long-press play/pause during that wait to skip
straight to it — it:

1. Starts its own access point (`AP_SSID` / `AP_PASSWORD` in `config.h`)
   and shows a QR code on the OLED, sized to use as much of the display
   as the code's data will allow.
2. Scanning it with an Android or iPhone camera joins that AP directly —
   it's a standard `WIFI:` QR payload both platforms recognize natively.
3. The phone's OS should then automatically prompt to "sign in to
   network" (captive-portal detection) with a form to enter your real
   WiFi credentials. If it doesn't pop up, open a browser to the IP
   address shown next to the QR code.
4. Submitting the form saves the credentials to flash (`Preferences`/NVS
   — survives reboots and power loss) and reboots into the new network.

This repeats on every boot the saved network is unreachable, so it also
doubles as your recovery path if you move the display to a new network.

If the QR code is hard to scan, try shortening `AP_SSID`/`AP_PASSWORD` in
`config.h` (a shorter payload needs a lower QR version, which renders
with larger, easier-to-scan modules) — or just read the AP name off the
screen and join it manually from your phone's WiFi settings.

### Device picker

Long-press play/pause on the home screen. Lists your active Spotify
Connect devices (current one marked) plus a "< Back" row. Previous/Next
and Volume+/Volume- all navigate the list; play/pause selects — choosing
a device transfers playback to it via `PUT /v1/me/player`. Either way, it
returns to the home screen afterward with buttons back to normal.

### Liked Songs list

Double-press play/pause. Lists your Liked Songs plus a "< Back" row.
Volume+/Volume- navigate; play/pause selects, starting playback of that
track (`PUT /v1/me/player/play` with its URI) and returning to the home
screen. Auto-closes back to home after `LIKED_SONGS_LIST_TIMEOUT_MS` (45
seconds) with no input.

Requires the `user-library-read` OAuth scope (see step 3 of Setup) — if
the list only ever shows "< Back", check the Serial Monitor: a `403` on
`get liked songs` means your current refresh token predates this scope
and needs regenerating.

## Technical notes

### OAuth / authentication

Spotify's Web API doesn't support raw username/password login for
third-party apps. `get_refresh_token.py` performs the one-time OAuth
Authorization Code flow on your computer and hands back a refresh token,
which `spotify_api.cpp` uses to silently mint new access tokens roughly
every hour (`TOKEN_REFRESH_MARGIN_S` before expiry) — no re-authorization
ever needed unless a new permission scope is added later.

### Animation storage

The disk animation is stored **PackBits-RLE compressed** in flash
(`animation_data.h`) — only one frame is ever decompressed into RAM at a
time, right before it's drawn, so RAM usage stays flat regardless of
frame count. To regenerate it from a raw frame array, save that array
into `raw_frames.h` and run `compress_frames.py`; it typically shrinks
icon-style bitmap animations by 70-95%, losslessly.

### Battery reading

The usable input range here is only 0.55V (0.80V-1.35V), which means
ordinary ESP32 ADC noise translates directly into several percentage
points of error if read naively — so the reading pipeline is deliberately
heavier than a typical battery gauge:

1. 200 samples per reading via `analogReadMilliVolts()` (uses the
   ESP32's factory calibration, correcting for the ADC's known
   non-linearity and per-chip variance — important for consistent
   readings across reboots and across different boards).
2. Sorted, with the top/bottom 15% trimmed before averaging, to reject
   glitches.
3. A slow exponential trend filter smooths that further.
4. The number actually shown only ever steps by **one percentage point**
   at a time, and only in the currently "locked" direction (discharging
   can only count down, charging only up) — a direction flip requires
   the trend to move `BATTERY_HYSTERESIS_PERCENT` (4%) past the current
   value, filtering out noise while still catching a real charger
   plug/unplug.

All the relevant constants are in `config.h` if your hardware needs
different calibration.

### Performance

Regular Spotify polling (`pollSpotify()`, a blocking HTTPS call) is
skipped entirely while the device picker or Liked Songs list is open —
running it on a timer in the background was the actual cause of
periodic input lag while browsing those menus, since it froze button
handling for however long the request took. Track/device state resumes
updating the moment you close the menu.

### Spotify API compliance

Every endpoint used here — playback control, volume, device transfer,
reading and playing Liked Songs — is a standard, actively-documented
Spotify Web API call, verified against Spotify's current developer
documentation including their February 2026 Web API migration (which
only moved the *save/remove* library endpoints to a generic `/me/library`;
reading and playback were unaffected). Two real requirements apply:
playback control has always required Spotify **Premium** on the account
being controlled, and as of Feb 2026, the **developer account** also
needs active Premium, or Development Mode apps stop functioning.

## Known limitations

- **TLS**: uses `client.setInsecure()` to skip certificate validation for
  simplicity. Fine for a hobby project on a home network; for anything
  more sensitive, pin Spotify's root CA with `setCACert()` instead.
- **Playback control requires an active Spotify Connect device** (phone,
  desktop app, speaker, etc. with Spotify open) — the Web API can't start
  playback from nothing, only control or transfer an existing session.
- Free Spotify accounts have Web API playback restrictions imposed by
  Spotify itself — this is a Spotify-side limitation, not something this
  firmware can work around.
- `displayDebugAnimation()` in `setup()` is a diagnostic leftover from
  bringing the animation up initially — harmless to leave in (it just
  prints to Serial), but safe to remove once you've confirmed the disk
  icon renders correctly on your hardware.

## Project structure

```
spotify_oled_display.ino   Main sketch: setup/loop, button-to-action wiring
config.h                   All pins, timeouts, and tunable constants
secrets_template.h         Copy to secrets.h and fill in your credentials
secrets.h                  Your real credentials (gitignore this)
spotify_api.h / .cpp       All Spotify Web API calls
display_ui.h / .cpp        OLED rendering: home screen, idle, WiFi setup,
                            device picker, Liked Songs list
buttons.h / .cpp           Button debouncing and gesture state machines
battery.h / .cpp           Battery ADC reading, filtering, and smoothing
wifi_setup.h / .cpp        WiFi connect + QR/captive-portal provisioning
animation_data.h           Compressed disk animation frame data
compress_frames.py         Regenerates animation_data.h from a raw frame array
get_refresh_token.py       One-time OAuth helper — run on your computer
```

## License

No license has been chosen for this project yet — add a `LICENSE` file
before publishing if you want to specify how others can use, modify, or
distribute this code.

## Why not just "log in with username and password"?

Spotify retired that option (the "password grant") for third-party apps
years ago — it's a security risk and no longer works. See
[OAuth / authentication](#oauth--authentication) above for how this
project handles it instead.
