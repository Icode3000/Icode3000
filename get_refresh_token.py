#!/usr/bin/env python3
"""
One-time helper to obtain a Spotify OAuth refresh token for the
ESP32 Spotify Display project. Run this on your COMPUTER, not the ESP32.

Setup:
    1. pip install requests
    2. In the Spotify Developer Dashboard (https://developer.spotify.com/dashboard),
       open your app -> Settings -> Redirect URIs, and add EXACTLY:
           http://127.0.0.1:8888/callback
    3. Fill in CLIENT_ID / CLIENT_SECRET below (same values you'll put in secrets.h)
    4. Run:  python get_refresh_token.py
    5. A browser tab opens. Log in and click "Agree".
    6. The refresh token prints in your terminal — paste it into secrets.h.

You only need to do this once. The refresh token doesn't expire unless you
revoke the app's access from your Spotify account settings.
"""

import base64
import http.server
import threading
import urllib.parse
import webbrowser
import requests

CLIENT_ID = "f7f86a4563d343ac9ed4703bead7fc1f"
CLIENT_SECRET = "88af551988e9444988c603f3b70003ba"
REDIRECT_URI = "http://127.0.0.1:8888/callback"
SCOPES = "user-read-currently-playing user-read-playback-state user-modify-playback-state user-library-read"

auth_code = None


class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        global auth_code
        qs = urllib.parse.urlparse(self.path).query
        params = urllib.parse.parse_qs(qs)
        if "code" in params:
            auth_code = params["code"][0]
            self.send_response(200)
            self.end_headers()
            self.wfile.write(b"Success! You can close this tab and return to the terminal.")
        else:
            self.send_response(400)
            self.end_headers()

    def log_message(self, *args):
        pass  # silence default request logging


def start_server():
    server = http.server.HTTPServer(("127.0.0.1", 8888), Handler)
    while auth_code is None:
        server.handle_request()


def main():
    if "YOUR_SPOTIFY" in CLIENT_ID or "YOUR_SPOTIFY" in CLIENT_SECRET:
        print("Edit this script and fill in CLIENT_ID / CLIENT_SECRET first.")
        return

    auth_url = "https://accounts.spotify.com/authorize?" + urllib.parse.urlencode({
        "client_id": CLIENT_ID,
        "response_type": "code",
        "redirect_uri": REDIRECT_URI,
        "scope": SCOPES,
    })
    print("Opening browser for Spotify login...")
    print(auth_url)
    webbrowser.open(auth_url)

    t = threading.Thread(target=start_server)
    t.start()
    t.join()

    print("Got authorization code, exchanging for tokens...")
    b64_creds = base64.b64encode(f"{CLIENT_ID}:{CLIENT_SECRET}".encode()).decode()
    resp = requests.post(
        "https://accounts.spotify.com/api/token",
        headers={
            "Authorization": f"Basic {b64_creds}",
            "Content-Type": "application/x-www-form-urlencoded",
        },
        data={
            "grant_type": "authorization_code",
            "code": auth_code,
            "redirect_uri": REDIRECT_URI,
        },
    )
    tokens = resp.json()
    if "refresh_token" not in tokens:
        print("Error getting tokens:", tokens)
        return

    print("\n=== SUCCESS ===")
    print("Refresh token (paste this into secrets.h as SPOTIFY_REFRESH_TOKEN):\n")
    print(tokens["refresh_token"])
    print()


if __name__ == "__main__":
    main()
