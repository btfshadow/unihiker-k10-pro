#include "portal.h"
#include "portal_html.h"

#include <esp_system.h>
#include <esp_camera.h>
#include "log.h"

using namespace unihiker_pro;

static String trimQuote(const String &s) {
  if (s.length() >= 2 && ((s[0] == '"' && s[s.length()-1] == '"') || (s[0]=='\'' && s[s.length()-1]=='\''))) {
    return s.substring(1, s.length()-1);
  }
  return s;
}

PortalService::PortalService() {}
PortalService::~PortalService() {
  stopApInternal();
  prefs_.end();
}

Status PortalService::begin(UniHikerPro &board, DisplayService *display, bool autoStartIfNoNetwork) {
  board_ = &board;
  conn_ = &board.connectivity();
  if (conn_) conn_->setHttpPreferPortalRoot(true);
  display_ = display;
  prefs_.begin("portal", false);
  server_ = nullptr;
  dns_ = nullptr;
  apActive_ = false;
  // Load AP auto-start preference (default: enabled)
  apAutoStart_ = prefs_.getBool("ap_auto", true);
  if (autoStartIfNoNetwork) startIfNoNetwork();
  return Status::OkStatus();
}

bool PortalService::isAuthorized(WebServer *s) {
  // No password configured => allow access
  String stored = prefs_.getString("portal_pwd", "");
  if (stored.length() == 0) return true;
  if (!s) return false;
  // Check Cookie header for portal_auth
  if (s->hasHeader("Cookie")) {
    String c = s->header("Cookie");
    int idx = c.indexOf("portal_auth=");
    if (idx >= 0) {
      int start = idx + strlen("portal_auth=");
      int end = c.indexOf(';', start);
      String tok = end < 0 ? c.substring(start) : c.substring(start, end);
      if (tok.length() > 0 && authToken_.length() > 0 && tok == authToken_) {
        if (authExpiryMs_ == 0 || (unsigned long)millis() < authExpiryMs_) return true;
      }
    }
  }
  // Check Authorization: Bearer <token>
  if (s->hasHeader("Authorization")) {
    String a = s->header("Authorization");
    const String pref = "Bearer ";
    if (a.startsWith(pref)) {
      String tok = a.substring(pref.length());
      if (tok.length() > 0 && authToken_.length() > 0 && tok == authToken_) {
        if (authExpiryMs_ == 0 || (unsigned long)millis() < authExpiryMs_) return true;
      }
    }
  }
  return false;
}

String PortalService::generateToken(size_t bytes) const {
  static const char *hex = "0123456789abcdef";
  String out;
  out.reserve(bytes * 2 + 1);
  for (size_t i = 0; i < bytes; ++i) {
    uint32_t v = esp_random();
    out += hex[v & 0xF];
    out += hex[(v >> 4) & 0xF];
  }
  return out;
}

void PortalService::startIfNoNetwork() {
  if (!conn_) return;
  // If already connected, nothing to do.
  if (conn_->connected()) return;

  if (!apAutoStart_) {
    LOG_INFO("[PortalService] ap_auto disabled, skipping AP start");
    return;
  }

  startApInternal();
}

void PortalService::startApInternal() {
  if (apActive_) return;

  // build SSID from MAC tail
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  String tail = mac;
  if (tail.length() >= 4) tail = tail.substring(tail.length() - 4);
  apSsid_ = String("UniHiker-") + tail;

  // simple random password
  uint32_t r = esp_random();
  apPass_ = String("uh") + String(r % 1000000);

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(apSsid_.c_str(), apPass_.c_str());
  apIp_ = WiFi.softAPIP();

  // start DNS server to capture captive portal
  dns_ = new DNSServer();
  dns_->start(53, "*", apIp_);

  // start http server
  server_ = new WebServer(80);
  initServerRoutes();
  server_->begin();

  apActive_ = true;
  apStartedAtMs_ = millis();
}

void PortalService::stopApInternal() {
  if (dns_) { dns_->stop(); delete dns_; dns_ = nullptr; }
  if (server_) { server_->stop(); delete server_; server_ = nullptr; }
  if (apActive_) {
    WiFi.softAPdisconnect(true);
    apActive_ = false;
  }
}

void PortalService::loop() {
  if (apActive_) {
    if (dns_) dns_->processNextRequest();
    if (server_) server_->handleClient();
    // if someone connected to configured Wi-Fi (STA), stop AP
    if (conn_ && conn_->connected()) {
      stopApInternal();
    }
  }
  // If connectivity service has its own HTTP server, attach portal routes once
  if (!registeredToExternalServer_ && conn_) {
    WebServer *ext = conn_->httpServer();
    if (ext) {
      attachToConnectivityServer();
    }
  }
}

void PortalService::initServerRoutes() {
  if (!server_) return;
  registerRoutesTo(server_);
}

void PortalService::registerRoutesTo(WebServer *s) {
  if (!s) return;
  // Basic UI and helpers
  s->on("/", HTTP_GET, [this, s]() { handleRoot(s); });
  s->on("/scan", HTTP_GET, [this, s]() { handleScan(s); });
  s->on("/connect", HTTP_POST, [this, s]() { handleConnect(s); });
  s->on("/settings", HTTP_POST, [this, s]() { handleSettings(s); });
  s->on("/status", HTTP_GET, [this, s]() { handleStatus(s); });

  // API endpoints
  s->on("/api/login", HTTP_POST, [this, s]() { handleApiLogin(s); });
  s->on("/api/logout", HTTP_POST, [this, s]() { handleApiLogout(s); });
  s->on("/api/status", HTTP_GET, [this, s]() { handleStatus(s); });
  s->on("/api/wifi/scan", HTTP_GET, [this, s]() { handleScan(s); });
  s->on("/api/wifi/connect", HTTP_POST, [this, s]() { handleConnect(s); });
  s->on("/api/settings", HTTP_POST, [this, s]() { handleSettings(s); });
  s->on("/api/camera/start", HTTP_POST, [this, s]() { handleApiCameraStart(s); });
  s->on("/api/camera/stop", HTTP_POST, [this, s]() { handleApiCameraStop(s); });
  s->on("/api/camera/snapshot", HTTP_GET, [this, s]() { handleApiCameraSnapshot(s); });
  s->on("/api/camera/stream", HTTP_GET, [this, s]() { handleApiCameraStream(s); });
  s->on("/api/audio/record", HTTP_POST, [this, s]() { handleApiAudioRecord(s); });
  s->on("/api/sensors", HTTP_GET, [this, s]() { handleApiSensors(s); });
  s->onNotFound([s]() { s->send(404, "text/plain", "not found"); });
}

void PortalService::attachToConnectivityServer() {
  if (registeredToExternalServer_) return;
  if (!conn_) return;
  WebServer *ext = conn_->httpServer();
  if (!ext) return;
  registerRoutesTo(ext);
  registeredToExternalServer_ = true;
  LOG_INFO("[PortalService] attached routes to connectivity HTTP server");
}
// Handlers that accept a target WebServer pointer (used when attaching to external server)
void PortalService::handleRoot(WebServer *s) {
  if (!s) return;
  s->send_P(200, "text/html", kPortalHtml);
}

void PortalService::handleScan(WebServer *s) {
  if (!s) return;
  int n = WiFi.scanNetworks(false, true, false, 120);
  String body = "{";
  body += "\"networks\":";
  body += "[";
  for (int i = 0; i < n; ++i) {
    if (i) body += ",";
    body += "{";
    body += String("\"ssid\":\"") + jsonEscape(WiFi.SSID(i)) + String("\"");
    body += String(",\"rssi\":") + String((long)WiFi.RSSI(i));
    body += String(",\"channel\":") + String((unsigned long)WiFi.channel(i));
    body += "}";
  }
  body += "]}";
  WiFi.scanDelete();
  s->send(200, "application/json", body);
}

String PortalService::pickArgOrJson(WebServer *s, const String &key) {
  if (!s) return String();
  if (s->hasArg(key)) return s->arg(key);
  String body = s->arg("plain");
  if (body.length() == 0) return String();
  int idx = body.indexOf('"' + key + '"');
  if (idx < 0) idx = body.indexOf('\'' + key + '\'');
  if (idx < 0) return String();
  int colon = body.indexOf(':', idx);
  if (colon < 0) return String();
  int first = body.indexOf('"', colon);
  if (first < 0) return String();
  int second = body.indexOf('"', first + 1);
  if (second < 0) return String();
  return body.substring(first + 1, second);
}

void PortalService::handleConnect(WebServer *s) {
  if (!s || !conn_) { if (s) s->send(500, "text/plain", "server not ready"); return; }
  String ssid = pickArgOrJson(s, "ssid");
  String pass = pickArgOrJson(s, "password");
  if (ssid.length() == 0) { s->send(400, "text/plain", "ssid empty"); return; }

  WifiConnectOptions opts;
  opts.persistOnSuccess = true;
  Status st = conn_->connect(ssid, pass, opts);
  if (!st.ok()) {
    s->send(500, "text/plain", String("connect failed: ") + st.message);
    return;
  }

  s->send(200, "text/plain", "ok");
}

void PortalService::handleSettings(WebServer *s) {
  if (!s) return;
  String portalPwd = pickArgOrJson(s, "portalPwd");
  String aiProvider = pickArgOrJson(s, "aiProvider");
  String aiModel = pickArgOrJson(s, "aiModel");
  String apiKey = pickArgOrJson(s, "apiKey");
  String apEnabled = pickArgOrJson(s, "apEnabled");
  String screenSaver = pickArgOrJson(s, "screenSaver");
  String sleepTm = pickArgOrJson(s, "sleep");
  String deepSleep = pickArgOrJson(s, "deepSleep");

  if (portalPwd.length()) prefs_.putString("portal_pwd", portalPwd);
  if (aiProvider.length()) prefs_.putString("ai_provider", aiProvider);
  if (aiModel.length()) prefs_.putString("ai_model", aiModel);
  if (apiKey.length()) prefs_.putString("api_key", apiKey);
  if (apEnabled.length()) {
    bool enabled = (apEnabled == "1" || apEnabled.equalsIgnoreCase("true"));
    prefs_.putBool("ap_auto", enabled);
    apAutoStart_ = enabled;
    if (!enabled && apActive_) {
      stopApInternal();
    }
  }

  // Persist power timeouts into the LUCI prefs namespace so the app can read them.
  if (screenSaver.length() || sleepTm.length() || deepSleep.length()) {
    Preferences p;
    if (p.begin("luci", false)) {
      int curScreen = p.getInt("scr_tm", 30);
      int curSleep = p.getInt("sleep_tm", 300);
      int curDeep = p.getInt("dsleep_tm", 3600);

      int newScreen = screenSaver.length() ? screenSaver.toInt() : curScreen;
      int newSleep = sleepTm.length() ? sleepTm.toInt() : curSleep;
      int newDeep = deepSleep.length() ? deepSleep.toInt() : curDeep;

      if (newScreen < 5) newScreen = 5;
      if (newSleep < newScreen) newSleep = newScreen;
      if (newDeep < newSleep) newDeep = newSleep;

      p.putInt("scr_tm", newScreen);
      p.putInt("sleep_tm", newSleep);
      p.putInt("dsleep_tm", newDeep);
      p.end();
    }
  }

  s->send(200, "text/plain", "saved");
}

void PortalService::handleStatus(WebServer *s) {
  if (!s) return;
  String body = "{";
  bool connected = (conn_ && conn_->connected());
  body += String("\"connected\":") + (connected ? "true" : "false");
  if (connected) {
    body += String(",\"ssid\":\"") + jsonEscape(conn_->ssid()) + String("\"");
    body += String(",\"ip\":\"") + jsonEscape(conn_->localIp()) + String("\"");
  } else if (apActive_) {
    body += String(",\"apSsid\":\"") + jsonEscape(apSsid_) + String("\"");
    body += String(",\"apPass\":\"") + jsonEscape(apPass_) + String("\"");
  }
  bool authRequired = prefs_.getString("portal_pwd", "").length() > 0;
  body += String(",\"authRequired\":") + (authRequired ? "true" : "false");
  body += String(",\"authenticated\":") + (isAuthorized(s) ? "true" : "false");
  body += String(",\"aiProvider\":\"") + jsonEscape(prefs_.getString("ai_provider", "")) + String("\"");
  body += String(",\"aiModel\":\"") + jsonEscape(prefs_.getString("ai_model", "")) + String("\"");
  bool keyPresent = prefs_.getString("api_key", "").length() > 0;
  body += String(",\"apiKeyPresent\":") + (keyPresent ? "true" : "false");
    // Include power/pref timeouts if present in the LUCI prefs namespace
    int screenSec = 30; int sleepSec = 300; int deepSec = 3600;
    Preferences p;
    if (p.begin("luci", true)) {
      screenSec = p.getInt("scr_tm", screenSec);
      sleepSec = p.getInt("sleep_tm", sleepSec);
      deepSec = p.getInt("dsleep_tm", deepSec);
      p.end();
    }
    body += String(",\"screenSaver\":") + String(screenSec);
    body += String(",\"sleep\":") + String(sleepSec);
    body += String(",\"deepSleep\":") + String(deepSec);
  body += "}";
  s->send(200, "application/json", body);
}

// wrappers keep original signature for backward compatibility
void PortalService::handleRoot() { handleRoot(server_); }
void PortalService::handleScan() { handleScan(server_); }
void PortalService::handleConnect() { handleConnect(server_); }
void PortalService::handleSettings() { handleSettings(server_); }
void PortalService::handleStatus() { handleStatus(server_); }

String PortalService::qrPayload() {
  // WIFI:T:WPA;S:...;P:...;;
  String ssid = prefs_.getString("last_ssid", "");
  String pass = prefs_.getString("last_pass", "");
  if (ssid.length() == 0) return String();
  String p = String("WIFI:T:WPA;S:") + ssid + String(";P:") + pass + String(";;");
  return p;
}

String PortalService::jsonEscape(const String &s) const {
  String out;
  out.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default: out += c; break;
    }
  }
  return out;
}

String PortalService::apSsid() const { return apSsid_; }

String PortalService::apPass() const { return apPass_; }

String PortalService::apQrPayload() {
  if (apSsid_.length() == 0) return String();
  return String("WIFI:T:WPA;S:") + apSsid_ + String(";P:") + apPass_ + String(";;");
}

bool PortalService::apActive() const { return apActive_; }

void PortalService::setApAutoStart(bool enabled) {
  apAutoStart_ = enabled;
  prefs_.putBool("ap_auto", enabled);
  if (!enabled && apActive_) stopApInternal();
}

bool PortalService::apAutoStart() const { return apAutoStart_; }

void PortalService::stopAp() { stopApInternal(); }

void PortalService::handleApiLogin(WebServer *s) {
  if (!s) return;
  String pwd = pickArgOrJson(s, "password");
  String stored = prefs_.getString("portal_pwd", "");
  if (stored.length() > 0 && pwd != stored) {
    s->send(403, "application/json", "{\"ok\":false,\"error\":\"invalid_password\"}");
    return;
  }
  authToken_ = generateToken(12);
  authExpiryMs_ = millis() + kAuthTtlMs;
  String cookie = String("portal_auth=") + authToken_ + "; Path=/; HttpOnly";
  s->sendHeader("Set-Cookie", cookie, false);
  s->send(200, "application/json", "{\"ok\":true}");
}

void PortalService::handleApiLogout(WebServer *s) {
  if (!s) return;
  authToken_.clear();
  authExpiryMs_ = 0;
  // expire cookie
  s->sendHeader("Set-Cookie", "portal_auth=; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT", false);
  s->send(200, "application/json", "{\"ok\":true}");
}

void PortalService::handleApiCameraStart(WebServer *s) {
  if (!s) return;
  if (!isAuthorized(s)) { s->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
  if (!board_) { s->send(500, "application/json", "{\"error\":\"board_missing\"}"); return; }
  Status st = board_->camera().start();
  String body = String("{\"ok\":") + (st.ok() ? "true" : "false") + String(",\"msg\":\"") + jsonEscape(st.message) + String("\"}");
  s->send(200, "application/json", body);
}

void PortalService::handleApiCameraStop(WebServer *s) {
  if (!s) return;
  if (!isAuthorized(s)) { s->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
  if (!board_) { s->send(500, "application/json", "{\"error\":\"board_missing\"}"); return; }
  Status st = board_->camera().stop();
  String body = String("{\"ok\":") + (st.ok() ? "true" : "false") + String(",\"msg\":\"") + jsonEscape(st.message) + String("\"}");
  s->send(200, "application/json", body);
}

void PortalService::handleApiCameraSnapshot(WebServer *s) {
  if (!s) return;
  if (!isAuthorized(s)) { s->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
  if (!board_) { s->send(500, "application/json", "{\"error\":\"board_missing\"}"); return; }

  bool startedByUs = false;
  if (!board_->camera().isPreviewActive()) {
    Status st = board_->camera().start();
    if (!st.ok()) {
      String body = String("{\"error\":\"start_failed\",\"msg\":\"") + jsonEscape(st.message) + String("\"}");
      s->send(500, "application/json", body);
      return;
    }
    startedByUs = true;
    delay(180);
  }

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    if (startedByUs) board_->camera().stop();
    s->send(500, "text/plain", "camera capture failed");
    return;
  }

  s->sendHeader("Content-Type", "image/jpeg", false);
  s->sendHeader("Content-Length", String(fb->len), false);
  s->send(200, "image/jpeg", "");
  s->client().write(fb->buf, fb->len);
  esp_camera_fb_return(fb);

  if (startedByUs) board_->camera().stop();
}

void PortalService::handleApiCameraStream(WebServer *s) {
  if (!s) return;
  if (!isAuthorized(s)) { s->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
  if (!board_) { s->send(500, "application/json", "{\"error\":\"board_missing\"}"); return; }

  bool startedByUs = false;
  if (!board_->camera().isPreviewActive()) {
    Status st = board_->camera().start();
    if (!st.ok()) {
      String body = String("{\"error\":\"start_failed\",\"msg\":\"") + jsonEscape(st.message) + String("\"}");
      s->send(500, "application/json", body);
      return;
    }
    startedByUs = true;
    delay(180);
  }

  const char *boundary = "frame";
  s->sendHeader("Access-Control-Allow-Origin", "*", false);
  s->sendHeader("Content-Type", String("multipart/x-mixed-replace; boundary=") + boundary, false);
  s->send(200, "multipart/x-mixed-replace; boundary=frame", "");

  WiFiClient client = s->client();
  while (client.connected()) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) break;
    String head = String("--") + boundary + "\r\n";
    head += "Content-Type: image/jpeg\r\n";
    head += "Content-Length: " + String(fb->len) + "\r\n\r\n";
    client.write((const uint8_t *)head.c_str(), head.length());
    client.write(fb->buf, fb->len);
    client.write((const uint8_t *)"\r\n", 2);
    esp_camera_fb_return(fb);
    client.flush();
    // Simple rate limit
    delay(100);
  }

  if (startedByUs) board_->camera().stop();
}

void PortalService::handleApiAudioRecord(WebServer *s) {
  if (!s) return;
  if (!isAuthorized(s)) { s->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
  if (!board_) { s->send(500, "application/json", "{\"error\":\"board_missing\"}"); return; }
  String ssec = pickArgOrJson(s, "seconds");
  int seconds = 5;
  if (ssec.length()) seconds = ssec.toInt();
  String fileName = String("web_rec_") + String(millis()) + String(".wav");
  String apiPath = board_->storage().audioPath(fileName);
  Status st = board_->audio().recordFile(apiPath, (uint8_t)seconds);
  String body = String("{\"ok\":") + (st.ok() ? "true" : "false") + String(",\"msg\":\"") + jsonEscape(st.message) + String("\",\"path\":\"") + jsonEscape(apiPath) + String("\"}");
  s->send(200, "application/json", body);
}

void PortalService::handleApiSensors(WebServer *s) {
  if (!s) return;
  if (!isAuthorized(s)) { s->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
  if (!board_) { s->send(500, "application/json", "{\"error\":\"board_missing\"}"); return; }
  board_->sensors().refreshAll();
  float t = board_->sensors().temperatureC();
  float h = board_->sensors().humidityRh();
  uint16_t lux = board_->sensors().ambientLux();
  int ax = board_->sensors().accelX();
  int ay = board_->sensors().accelY();
  int az = board_->sensors().accelZ();
  uint64_t mic = board_->sensors().micLevel();
  String body = "{";
  body += String("\"temperatureC\":") + String(t, 2);
  body += String(",\"humidityRh\":") + String(h, 2);
  body += String(",\"ambientLux\":") + String((unsigned long)lux);
  body += String(",\"accel\":[") + String(ax) + String(",") + String(ay) + String(",") + String(az) + String("]");
  body += String(",\"micLevel\":") + String((unsigned long)mic);
  body += "}";
  s->send(200, "application/json", body);
}

// wrappers for backward compatibility
void PortalService::handleApiLogin() { handleApiLogin(server_); }
void PortalService::handleApiLogout() { handleApiLogout(server_); }
void PortalService::handleApiCameraStart() { handleApiCameraStart(server_); }
void PortalService::handleApiCameraStop() { handleApiCameraStop(server_); }
void PortalService::handleApiCameraSnapshot() { handleApiCameraSnapshot(server_); }
void PortalService::handleApiCameraStream() { handleApiCameraStream(server_); }
void PortalService::handleApiAudioRecord() { handleApiAudioRecord(server_); }
void PortalService::handleApiSensors() { handleApiSensors(server_); }
