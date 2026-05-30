#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <WiFi.h>
#include <Preferences.h>
#include <unihiker_pro.h>

namespace unihiker_pro {

class PortalService {
 public:
  PortalService();
  ~PortalService();

  // Inicializa o serviço. `display` pode ser nullptr.
  Status begin(UniHikerPro &board, DisplayService *display = nullptr, bool autoStartIfNoNetwork = true);

  // Deve ser chamada periodicamente pelo loop da aplicação.
  void loop();

  // Força start do AP caso não haja conexão conhecida.
  void startIfNoNetwork();

  // Attach portal routes to connectivity server (public wrapper)
  void attachToConnectivityServer();

  // Gera payload QR padrão para Wi-Fi: WIFI:T:WPA;S:...;P:...;;
  String qrPayload();

  // AP getters: quando o captive AP estiver ativo retornam as credenciais do AP.
  String apSsid() const;
  String apPass() const;
  String apQrPayload();
  bool apActive() const;

  // Control AP auto-start behavior (persisted in prefs namespace "portal").
  void setApAutoStart(bool enabled);
  bool apAutoStart() const;
  // Stop AP if active
  void stopAp();

 private:
  ConnectivityService *conn_ = nullptr;
  UniHikerPro *board_ = nullptr;
  DisplayService *display_ = nullptr;
  WebServer *server_ = nullptr;
  DNSServer *dns_ = nullptr;
  Preferences prefs_;

  bool apActive_ = false;
  String apSsid_;
  String apPass_;
  IPAddress apIp_;
  unsigned long apStartedAtMs_ = 0;
  // Whether the portal should automatically start an AP when offline.
  bool apAutoStart_ = true;

  // Simple session auth token (stored in RAM) and expiry.
  String authToken_;
  unsigned long authExpiryMs_ = 0;
  static constexpr unsigned long kAuthTtlMs = 60UL * 60UL * 1000UL;  // 1 hour

  bool isAuthorized(WebServer *s);
  String generateToken(size_t bytes = 12) const;

  // Additional HTTP handlers (API) - overloads that accept target server
  void handleApiLogin(WebServer *s);
  void handleApiLogout(WebServer *s);
  void handleApiCameraStart(WebServer *s);
  void handleApiCameraStop(WebServer *s);
  void handleApiCameraSnapshot(WebServer *s);
  void handleApiCameraStream(WebServer *s);
  void handleApiAudioRecord(WebServer *s);
  void handleApiSensors(WebServer *s);
  // Backwards-compatible no-arg wrappers (use server_)
  void handleApiLogin();
  void handleApiLogout();
  void handleApiCameraStart();
  void handleApiCameraStop();
  void handleApiCameraSnapshot();
  void handleApiCameraStream();
  void handleApiAudioRecord();
  void handleApiSensors();

  // AI provider management endpoints
  void handleAiProvidersList(WebServer *s);
  void handleAiProviderSave(WebServer *s);
  void handleAiProviderDelete(WebServer *s);
  void handleAiProviderActivate(WebServer *s);
  void handleAiPromptsList(WebServer *s);
  void handleAiProviderTest(WebServer *s);
  // wrappers
  void handleAiProvidersList();
  void handleAiProviderSave();
  void handleAiProviderDelete();
  void handleAiProviderActivate();
  void handleAiPromptsList();
  void handleAiProviderTest();

  // Attach portal routes to the connectivity HTTP server (STA)
  bool registeredToExternalServer_ = false;
  void registerRoutesTo(WebServer *s);

  void initServerRoutes();
  void startApInternal();
  void stopApInternal();

  // HTTP handlers
  void handleRoot();
  void handleScan();
  void handleConnect();
  void handleSettings();
  void handleStatus();

  // Handlers that accept server pointer (used when attaching to external server)
  void handleRoot(WebServer *s);
  void handleScan(WebServer *s);
  void handleConnect(WebServer *s);
  void handleSettings(WebServer *s);
  void handleStatus(WebServer *s);

  String jsonEscape(const String &s) const;
  String pickArgOrJson(WebServer *s, const String &key);
  String pickArgOrJson(const String &key) { return pickArgOrJson(server_, key); }
};

}  // namespace unihiker_pro
