#include "wifi_setup.h"
#include "config.h"
#include "secrets.h"
#include "display_ui.h"

#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <Preferences.h>

static DNSServer dnsServer;
static WebServer server(80);
static Preferences prefs;
static const byte DNS_PORT = 53;

static String htmlForm(const String &errorMsg) {
  String html = "<!DOCTYPE html><html><head><meta name='viewport' "
                "content='width=device-width, initial-scale=1'>"
                "<title>WiFi Setup</title></head>"
                "<body style='font-family:sans-serif;max-width:400px;"
                "margin:40px auto;padding:0 16px;'>"
                "<h2>Spotify Display - WiFi Setup</h2>";
  if (errorMsg.length()) {
    html += "<p style='color:red;'>" + errorMsg + "</p>";
  }
  html += "<form method='POST' action='/save'>"
          "<label>WiFi Network Name (SSID)</label><br>"
          "<input name='ssid' style='width:100%;padding:8px;margin:6px 0;box-sizing:border-box;' required><br>"
          "<label>WiFi Password</label><br>"
          "<input name='pass' type='password' style='width:100%;padding:8px;margin:6px 0;box-sizing:border-box;'><br><br>"
          "<button type='submit' style='padding:10px 20px;'>Save &amp; Connect</button>"
          "</form></body></html>";
  return html;
}

static void handleRoot() {
  server.send(200, "text/html", htmlForm(""));
}

static void handleSave() {
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");

  if (ssid.length() == 0) {
    server.send(200, "text/html", htmlForm("Network name can't be empty."));
    return;
  }

  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();

  server.send(200, "text/html",
    "<html><body style='font-family:sans-serif;text-align:center;margin-top:60px;'>"
    "<h2>Saved</h2><p>Restarting and connecting to your network now.</p>"
    "</body></html>");

  delay(1500);
  ESP.restart();
}

// Android/iOS probe specific URLs (generate_204, hotspot-detect.html, etc.)
// to decide whether a network needs a sign-in page. Redirecting everything
// unrecognized back to our own root is what makes that sign-in prompt pop
// up automatically after the phone joins the AP.
static void handleNotFound() {
  server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
  server.send(302, "text/plain", "");
}

static void startCaptivePortal() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  IPAddress apIP = WiFi.softAPIP();

  dnsServer.start(DNS_PORT, "*", apIP);

  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.onNotFound(handleNotFound);
  server.begin();

  // Standard "WIFI:" QR payload — both Android's and iOS's built-in camera
  // apps recognize this and offer to join the network directly, no manual
  // password typing needed.
  String qrPayload = String("WIFI:T:WPA;S:") + AP_SSID + ";P:" + AP_PASSWORD + ";;";
  displayShowWifiSetup(qrPayload, AP_SSID, apIP.toString());

  Serial.println("=== WiFi setup portal active ===");
  Serial.print("AP SSID: ");
  Serial.println(AP_SSID);
  Serial.print("Portal:  http://");
  Serial.println(apIP);

  for (;;) {
    dnsServer.processNextRequest();
    server.handleClient();
    delay(2);
  }
}

void wifiSetupBegin() {
  // Preferences persist across reboots, so once someone submits the setup
  // form, every future boot uses the new network automatically. Until
  // then, secrets.h's values act as the initial default.
  prefs.begin("wifi", true);
  String ssid = prefs.getString("ssid", WIFI_SSID);
  String pass = prefs.getString("pass", WIFI_PASSWORD);
  prefs.end();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());

  // Configured here directly (not just in buttons.cpp) because this runs
  // before buttonsBegin() does in setup() — needed early so a long-press
  // can interrupt the connect wait below.
  pinMode(BTN_PLAY_PIN, INPUT_PULLUP);

  Serial.print("Connecting to WiFi");
  unsigned long start = millis();
  unsigned long lastDotMs = start;
  unsigned long pressStart = 0;

  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) {
      Serial.println();
      Serial.println("WiFi connection timed out — starting setup portal.");
      startCaptivePortal();  // never returns; ESP32 reboots once credentials are saved
    }

    // Holding play/pause during the connect attempt jumps straight to the
    // setup portal, regardless of how much of WIFI_CONNECT_TIMEOUT_MS has
    // elapsed — useful when you already know the saved network is wrong.
    if (digitalRead(BTN_PLAY_PIN) == LOW) {
      if (pressStart == 0) {
        pressStart = millis();
      } else if (millis() - pressStart >= PLAY_LONG_PRESS_MS) {
        Serial.println();
        Serial.println("Long-press detected — skipping straight to setup portal.");
        startCaptivePortal();  // never returns
      }
    } else {
      pressStart = 0;
    }

    if (millis() - lastDotMs >= 300) {
      Serial.print(".");
      lastDotMs = millis();
    }
    delay(20);  // short poll interval so the long-press feels responsive
  }

  Serial.println(" connected!");
  Serial.println(WiFi.localIP());
}
