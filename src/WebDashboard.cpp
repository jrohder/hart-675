#include "WebDashboard.h"

#include "BatteryManager.h"
#include "HartBridge.h"
#include "HartMaster.h"
#include "HartMonitor.h"
#include "LedManager.h"
#include "ProfileManager.h"
#include "SettingsManager.h"
#include "SystemStatus.h"
#include "TcpBridge.h"
#include "TrendLogger.h"
#include "WebPages.h"
#include "DDParserJS.h"

WebDashboard::WebDashboard()
    : server(WEB_SERVER_PORT), startMs(0), settings(nullptr), battery(nullptr),
      hart(nullptr), tcp(nullptr), trend(nullptr), master(nullptr), led(nullptr),
      rebootReq(false), factoryReq(false) {}

void WebDashboard::begin(SettingsManager *s, BatteryManager *b, HartBridge *h,
                         TcpBridge *t, TrendLogger *tr, HartMaster *m,
                         LedManager *l) {
  settings = s;
  battery = b;
  hart = h;
  tcp = t;
  trend = tr;
  master = m;
  led = l;
  startMs = millis();

  setupRoutes();
  server.begin();
  systemStatus.log("[WEB] Dashboard on http://" +
                   WiFi.softAPIP().toString());
}

uint8_t WebDashboard::hexToBytes(const String &hex, uint8_t *out, size_t cap) {
  auto nib = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  };
  size_t n = 0;
  int hi = -1;
  for (size_t i = 0; i < hex.length() && n < cap; i++) {
    int v = nib(hex[i]);
    if (v < 0) {
      continue;  // skip spaces/separators
    }
    if (hi < 0) {
      hi = v;
    } else {
      out[n++] = (uint8_t)((hi << 4) | v);
      hi = -1;
    }
  }
  return (uint8_t)n;
}

String WebDashboard::buildStatusJson() {
  unsigned long uptime = (millis() - startMs) / 1000;
  ModemOwner owner = systemStatus.getOwner();

  String j = "{";
  j += "\"fwVersion\":\"" + String(FW_VERSION) + "\",";
  j += "\"batteryPercent\":" + String(battery->getPercentage()) + ",";
  j += "\"batteryVoltage\":" + String(battery->getVoltage(), 2) + ",";
  j += "\"batteryHealth\":\"" + String(battery->getHealth()) + "\",";
  j += "\"usbActive\":" + String(systemStatus.getUsbActive() ? "true" : "false") + ",";
  j += "\"tcpClient\":" + String(tcp->hasClient() ? "true" : "false") + ",";
  j += "\"wifiClients\":" + String(WiFi.softAPgetStationNum()) + ",";
  j += "\"carrier\":" + String(systemStatus.getCarrier() ? "true" : "false") + ",";
  j += "\"ocdRaw\":" + String(hart->getOcdRaw()) + ",";
  j += "\"transmitting\":" + String(hart->isTransmitting() ? "true" : "false") + ",";
  const char *hm = !master->isEnabled() ? "off"
                   : master->getDevice().valid ? "online"
                                               : "searching";
  j += "\"hartMaster\":\"" + String(hm) + "\",";
  j += "\"owner\":\"" + String(systemStatus.ownerName(owner)) + "\",";
  j += "\"uptime\":" + String(uptime) + ",";
  j += "\"txBytes\":" + String(systemStatus.getTxBytes()) + ",";
  j += "\"rxBytes\":" + String(systemStatus.getRxBytes()) + ",";
  j += "\"txPackets\":" + String(systemStatus.getTxPackets()) + ",";
  j += "\"rxPackets\":" + String(systemStatus.getRxPackets()) + ",";
  j += "\"uartErrors\":" + String(systemStatus.getUartErrors()) + ",";
  j += "\"bufferOverruns\":" + String(systemStatus.getBufferOverruns()) + ",";
  j += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
  j += "\"chipRev\":" + String(ESP.getChipRevision()) + ",";
  j += "\"cpuUsage\":" + String(systemStatus.getCpuUsage()) + ",";
  j += "\"bootCount\":" + String(systemStatus.getBootCount()) + ",";
  j += "\"wakeReason\":\"" + systemStatus.getWakeReason() + "\",";
  j += "\"ad5700Powered\":" + String(hart->isModemPowered() ? "true" : "false") + ",";
  j += "\"internalResistorEnabled\":" +
       String(hart->isInternalResistorEnabled() ? "true" : "false") + ",";
  j += "\"wifiConnected\":" + String(WiFi.getMode() != WIFI_OFF ? "true" : "false") + ",";
  j += "\"sleepTimerActive\":" +
       String(settings->getAutoSleepSec() > 0 ? "true" : "false") + ",";
  j += "\"lastError\":\"" + systemStatus.getLastError() + "\"";
  j += "}";
  return j;
}

void WebDashboard::setupRoutes() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send_P(200, "text/html", INDEX_HTML);
  });

  // Browser DD profiler engine (served from flash, fully offline).
  server.on("/ddparser.js", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send_P(200, "application/javascript", DDPARSER_JS);
  });

  server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *req) {
    req->send(200, "application/json", buildStatusJson());
  });

  server.on("/api/hart/hardware", HTTP_POST, [this](AsyncWebServerRequest *req) {
    if (req->hasParam("internalResistor", true)) {
      String v = req->getParam("internalResistor", true)->value();
      bool enabled = (v == "1" || v.equalsIgnoreCase("true") ||
                      v.equalsIgnoreCase("on"));
      hart->setInternalResistor(enabled);
      systemStatus.log(String("[HART HW] Internal 250 ohm resistor ") +
                       (enabled ? "enabled" : "disabled"));
    }

    String resp = "{\"ok\":true,\"ad5700Powered\":";
    resp += hart->isModemPowered() ? "true" : "false";
    resp += ",\"internalResistorEnabled\":";
    resp += hart->isInternalResistorEnabled() ? "true" : "false";
    resp += "}";
    req->send(200, "application/json", resp);
  });

  server.on("/api/settings", HTTP_GET, [this](AsyncWebServerRequest *req) {
    req->send(200, "application/json", settings->toJson());
  });

  server.on("/api/settings", HTTP_POST, [this](AsyncWebServerRequest *req) {
    auto getP = [&](const char *n, String &dst) {
      if (req->hasParam(n, true)) {
        dst = req->getParam(n, true)->value();
      }
    };
    String v;
    String name = settings->getDeviceName();
    String ssid = settings->getSsid();
    String pass = settings->getPassword();
    getP("deviceName", name);
    getP("ssid", ssid);
    getP("password", pass);
    settings->setDeviceName(name);
    settings->setSsid(ssid);
    settings->setPassword(pass);

    if (req->hasParam("tcpPort", true)) {
      settings->setTcpPort(req->getParam("tcpPort", true)->value().toInt());
    }
    if (req->hasParam("autoSleepSec", true)) {
      settings->setAutoSleepSec(
          req->getParam("autoSleepSec", true)->value().toInt());
    }
    if (req->hasParam("ledBrightness", true)) {
      uint8_t b = req->getParam("ledBrightness", true)->value().toInt();
      settings->setLedBrightness(b);
      if (led) {
        led->setBrightnessPercent(b);
      }
    }
    if (req->hasParam("dashRefreshMs", true)) {
      settings->setDashRefreshMs(
          req->getParam("dashRefreshMs", true)->value().toInt());
    }
    if (req->hasParam("hartPollAddress", true)) {
      uint8_t a = req->getParam("hartPollAddress", true)->value().toInt();
      settings->setHartPollAddress(a);
      master->setPollAddress(settings->getHartPollAddress());
    }
    settings->save();
    systemStatus.log("[WEB] Settings saved");

    String resp = "{\"ok\":true,\"message\":\"Saved. Network changes need reboot.\",\"dashRefreshMs\":" +
                  String(settings->getDashRefreshMs()) + "}";
    req->send(200, "application/json", resp);
  });

  server.on("/api/hart", HTTP_GET, [this](AsyncWebServerRequest *req) {
    req->send(200, "application/json", master->toJson());
  });

  server.on("/api/hart/refresh", HTTP_POST, [this](AsyncWebServerRequest *req) {
    master->requestRefresh();
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/hart/config/read", HTTP_POST, [this](AsyncWebServerRequest *req) {
    bool ok = master->readConfigurationNow();
    req->send(ok ? 200 : 503, "application/json",
              String("{\"ok\":") + (ok ? "true" : "false") + ",\"hart\":" +
                  master->toJson() + "}");
  });

  server.on("/api/hart/range", HTTP_POST, [this](AsyncWebServerRequest *req) {
    if (!req->hasParam("urv", true)) {
      req->send(400, "application/json", "{\"error\":\"urv required\"}");
      return;
    }
    float urv = req->getParam("urv", true)->value().toFloat();
    float lrv = NAN;
    if (req->hasParam("lrv", true)) {
      lrv = req->getParam("lrv", true)->value().toFloat();
    }
    bool ok = master->writeRangeValues(urv, lrv);
    if (ok) {
      req->send(200, "application/json",
                String("{\"ok\":true,\"message\":\"Range updated\",\"hart\":") +
                    master->toJson() + "}");
    } else {
      req->send(503, "application/json",
                "{\"ok\":false,\"message\":\"Write failed (check write "
                "protect / units)\"}");
    }
  });

  server.on("/api/hart/damping", HTTP_POST, [this](AsyncWebServerRequest *req) {
    if (!req->hasParam("value", true)) {
      req->send(400, "application/json", "{\"error\":\"value required\"}");
      return;
    }
    float v = req->getParam("value", true)->value().toFloat();
    bool ok = master->writeDampingValue(v);
    if (ok) {
      req->send(200, "application/json",
                String("{\"ok\":true,\"hart\":") + master->toJson() + "}");
    } else {
      req->send(503, "application/json", "{\"ok\":false}");
    }
  });

  server.on("/api/hart/polladdr", HTTP_POST, [this](AsyncWebServerRequest *req) {
    if (!req->hasParam("address", true)) {
      req->send(400, "application/json", "{\"error\":\"address required\"}");
      return;
    }
    uint8_t addr = (uint8_t)req->getParam("address", true)->value().toInt();
    bool ok = master->writePollAddressValue(addr);
    req->send(ok ? 200 : 503, "application/json",
              String("{\"ok\":") + (ok ? "true" : "false") + "}");
  });

  server.on("/api/master", HTTP_POST, [this](AsyncWebServerRequest *req) {
    if (req->hasParam("enabled", true)) {
      String v = req->getParam("enabled", true)->value();
      bool en = (v == "1" || v == "true");
      master->enable(en);
      settings->setMasterEnabled(en);
      settings->save();
      systemStatus.log(String("[WEB] HART master ") + (en ? "enabled" : "disabled"));
    }
    String resp = "{\"ok\":true,\"enabled\":" +
                  String(master->isEnabled() ? "true" : "false") + "}";
    req->send(200, "application/json", resp);
  });

  server.on("/api/trends", HTTP_GET, [this](AsyncWebServerRequest *req) {
    req->send(200, "application/json", trend->toJson());
  });

  server.on("/api/hartmon", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "application/json", hartMonitor.toJson());
  });

  server.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "application/json", systemStatus.getLogJson());
  });

  // ---- Generic HART command engine ----
  // POST /api/hart/cmd  params: command=<int>, data=<hex bytes optional>
  server.on("/api/hart/cmd", HTTP_POST, [this](AsyncWebServerRequest *req) {
    if (!req->hasParam("command", true)) {
      req->send(400, "application/json", "{\"error\":\"command required\"}");
      return;
    }
    uint8_t command = (uint8_t)req->getParam("command", true)->value().toInt();
    uint8_t data[64];
    uint8_t len = 0;
    if (req->hasParam("data", true)) {
      String hex = req->getParam("data", true)->value();
      len = hexToBytes(hex, data, sizeof(data));
    }
    uint32_t id = master->queueCommand(command, len ? data : nullptr, len);
    if (id == 0) {
      req->send(409, "application/json",
                "{\"error\":\"busy or no device\"}");
      return;
    }
    req->send(200, "application/json", "{\"id\":" + String(id) + "}");
  });

  // GET /api/hart/cmd?id=<n>  -> result of a queued command
  server.on("/api/hart/cmd", HTTP_GET, [this](AsyncWebServerRequest *req) {
    uint32_t id = 0;
    if (req->hasParam("id")) {
      id = (uint32_t)req->getParam("id")->value().toInt();
    }
    req->send(200, "application/json", master->resultJson(id));
  });

  // ---- Profiles ----
  server.on("/api/profiles", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "application/json", profiles.listJson());
  });

  server.on("/api/profile/status", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "application/json", profiles.statusJson());
  });

  server.on("/api/profile/active", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "application/json", profiles.activeProfileJson());
  });

  server.on("/api/profile", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!req->hasParam("file")) {
      req->send(400, "application/json", "{\"error\":\"file required\"}");
      return;
    }
    req->send(200, "application/json",
              profiles.readProfile(req->getParam("file")->value()));
  });

  server.on("/api/profile/delete", HTTP_POST, [](AsyncWebServerRequest *req) {
    bool ok = false;
    if (req->hasParam("file", true)) {
      ok = profiles.deleteProfile(req->getParam("file", true)->value());
    }
    req->send(ok ? 200 : 400, "application/json",
              String("{\"ok\":") + (ok ? "true" : "false") + "}");
  });

  // Profile upload (multipart). The browser posts a JSON file.
  server.on(
      "/api/profile/upload", HTTP_POST,
      [](AsyncWebServerRequest *req) {
        req->send(200, "application/json", "{\"ok\":true}");
      },
      [](AsyncWebServerRequest *req, const String &filename, size_t index,
         uint8_t *data, size_t len, bool final) {
        if (index == 0) {
          profiles.uploadBegin(filename);
        }
        profiles.uploadWrite(data, len);
        if (final) {
          profiles.uploadEnd();
          systemStatus.log("[PROFILE] Uploaded " + filename);
        }
      });

  server.on("/api/reboot", HTTP_POST, [this](AsyncWebServerRequest *req) {
    req->send(200, "application/json", "{\"ok\":true}");
    rebootReq = true;
  });

  server.on("/api/factoryreset", HTTP_POST, [this](AsyncWebServerRequest *req) {
    req->send(200, "application/json", "{\"ok\":true}");
    factoryReq = true;
  });

  server.onNotFound([](AsyncWebServerRequest *req) {
    req->send(404, "text/plain", "Not Found");
  });
}
