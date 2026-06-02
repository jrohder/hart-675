#include "WiFiDashboard.h"

WiFiDashboard::WiFiDashboard()
    : clientConnected(false), server(WEB_SERVER_PORT), lastClientCheckTime(0),
      trendIndex(0), lastTrendSampleTime(0) {
  memset(trendBuffer, 0, sizeof(trendBuffer));
}

void WiFiDashboard::begin() {
  // Configure WiFi AP mode
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(WIFI_AP_IP, WIFI_AP_GATEWAY, WIFI_AP_SUBNET);
  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD, 1, false, WIFI_AP_MAX_CONNECTIONS);

  Serial.printf("[WiFi] AP started: %s\n", WIFI_AP_SSID);
  Serial.printf("[WiFi] IP: %s\n", WiFi.softAPIP().toString().c_str());

  // Setup web server routes
  setupRoutes();

  // Start web server
  server.begin();
  Serial.println("[WiFi] Web server started");
}

void WiFiDashboard::end() {
  server.end();
  WiFi.softAPdisconnect(true);
  clientConnected = false;
  Serial.println("[WiFi] Disabled");
}

void WiFiDashboard::update() {
  checkClientConnection();
}

void WiFiDashboard::setupRoutes() {
  // Root dashboard
  server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
    request->send(200, "text/html; charset=utf-8", generateDashboardHtml());
  });

  // JSON API for status updates
  server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
    String json = "{";
    json += "\"mode\":\"wifi\",";
    json += "\"bt_connected\":false,";
    json += "\"wifi_connected\":true,";
    json += "\"uptime\":" + String(millis() / 1000) + ",";
    json += "\"hart_tx\":" + String(0) + ",";
    json += "\"hart_rx\":" + String(0) + ",";
    json += "\"hart_carrier\":" + String(0) + ",";
    json += "\"heap\":" + String(ESP.getFreeHeap());
    json += "}";
    request->send(200, "application/json", json);
  });

  // Trend data API
  server.on("/api/trend", HTTP_GET, [this](AsyncWebServerRequest *request) {
    request->send(200, "application/json", generateTrendJson());
  });

  // 404 handler
  server.onNotFound([this](AsyncWebServerRequest *request) {
    handleNotFound(request);
  });
}

void WiFiDashboard::checkClientConnection() {
  unsigned long now = millis();
  if (now - lastClientCheckTime < CLIENT_CHECK_INTERVAL_MS) {
    return;
  }
  lastClientCheckTime = now;

  uint8_t clientCount = WiFi.softAPgetStationNum();
  bool hasClientNow = (clientCount > 0);

  if (hasClientNow != clientConnected) {
    clientConnected = hasClientNow;
    if (clientConnected) {
      Serial.println("[WiFi] Client connected");
    } else {
      Serial.println("[WiFi] Client disconnected");
    }
  }
}

int WiFiDashboard::available() {
  return 0;
}

uint8_t WiFiDashboard::read() {
  return 0;
}

void WiFiDashboard::write(uint8_t byte) {
  // WiFi dashboard is read-only for HART data
}

void WiFiDashboard::flush() {}

void WiFiDashboard::setHartActivity(bool txRx) {
  // Update trend tracking
}

void WiFiDashboard::recordTrendSample(bool carrier, uint32_t hartActivityCount) {
  unsigned long now = millis();
  if (now - lastTrendSampleTime >= TREND_SAMPLE_INTERVAL_MS) {
    lastTrendSampleTime = now;
    trendBuffer[trendIndex] = hartActivityCount & 0xFFFF;
    trendIndex = (trendIndex + 1) % TREND_BUFFER_SIZE;
  }
}

String WiFiDashboard::generateDashboardHtml() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Wireless HART 67 Dashboard</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        body {
            background-color: #121212;
            color: #e0e0e0;
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
            padding: 16px;
        }
        .container {
            max-width: 600px;
            margin: 0 auto;
        }
        header {
            text-align: center;
            margin-bottom: 24px;
            padding: 16px 0;
            border-bottom: 2px solid #333;
        }
        h1 {
            font-size: 24px;
            margin-bottom: 8px;
            color: #4fc3f7;
        }
        .subtitle {
            font-size: 12px;
            color: #888;
        }
        .grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 12px;
            margin-bottom: 24px;
        }
        .card {
            background-color: #1e1e1e;
            border-radius: 8px;
            padding: 16px;
            border-left: 4px solid #4fc3f7;
        }
        .card.red {
            border-left-color: #f44336;
        }
        .card.green {
            border-left-color: #4caf50;
        }
        .card.blue {
            border-left-color: #2196f3;
        }
        .card-label {
            font-size: 12px;
            color: #888;
            margin-bottom: 4px;
            text-transform: uppercase;
        }
        .card-value {
            font-size: 20px;
            font-weight: bold;
            color: #e0e0e0;
        }
        .card-unit {
            font-size: 12px;
            color: #666;
            margin-left: 4px;
        }
        .chart-container {
            background-color: #1e1e1e;
            border-radius: 8px;
            padding: 16px;
            margin-bottom: 24px;
        }
        canvas {
            width: 100%;
            height: auto;
        }
        .status-indicator {
            display: inline-block;
            width: 12px;
            height: 12px;
            border-radius: 50%;
            margin-right: 6px;
            vertical-align: middle;
        }
        .status-indicator.on {
            background-color: #4caf50;
            box-shadow: 0 0 8px rgba(76, 175, 80, 0.5);
        }
        .status-indicator.off {
            background-color: #666;
        }
        .refresh-time {
            text-align: center;
            font-size: 11px;
            color: #666;
            margin-top: 16px;
        }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>Wireless HART 67</h1>
            <p class="subtitle">Portable Field Communicator</p>
        </header>

        <div class="grid">
            <div class="card blue">
                <div class="card-label">Mode</div>
                <div class="card-value" id="mode">WiFi</div>
            </div>
            <div class="card">
                <div class="card-label">Uptime</div>
                <div class="card-value" id="uptime">0:00:00</div>
            </div>
            <div class="card green">
                <div class="card-label">Battery</div>
                <div class="card-value"><span id="battery">--</span><span class="card-unit">%</span></div>
            </div>
            <div class="card">
                <div class="card-label">Heap Free</div>
                <div class="card-value"><span id="heap">--</span><span class="card-unit">KB</span></div>
            </div>
            <div class="card">
                <div class="card-label">Bluetooth</div>
                <div class="card-value"><span class="status-indicator off" id="bt-status"></span><span id="bt-text">Off</span></div>
            </div>
            <div class="card">
                <div class="card-label">WiFi Clients</div>
                <div class="card-value" id="wifi-clients">1</div>
            </div>
        </div>

        <div class="card">
            <div class="card-label">HART Status</div>
            <div style="margin-top: 12px;">
                <div style="margin-bottom: 8px;">Carrier: <span class="status-indicator off" id="carrier-status"></span><span id="carrier-text">None</span></div>
                <div style="margin-bottom: 8px;">TX Bytes: <span id="hart-tx">0</span></div>
                <div>RX Bytes: <span id="hart-rx">0</span></div>
            </div>
        </div>

        <div class="chart-container">
            <div class="card-label">HART Activity (Last 5 minutes)</div>
            <canvas id="trend-chart" width="300" height="150"></canvas>
        </div>

        <div class="refresh-time">
            Last updated: <span id="last-update">now</span>
        </div>
    </div>

    <script>
        const API_INTERVAL = 1000;  // 1 second
        let lastUpdate = Date.now();
        let trendData = [];

        async function updateStatus() {
            try {
                const response = await fetch('/api/status');
                const data = await response.json();

                document.getElementById('mode').textContent = data.mode.toUpperCase();
                document.getElementById('uptime').textContent = formatUptime(data.uptime);
                document.getElementById('battery').textContent = data.battery || '--';
                document.getElementById('heap').textContent = (data.heap / 1024).toFixed(1);
                document.getElementById('hart-tx').textContent = data.hart_tx;
                document.getElementById('hart-rx').textContent = data.hart_rx;

                // Update status indicators
                const btStatus = document.getElementById('bt-status');
                if (data.bt_connected) {
                    btStatus.className = 'status-indicator on';
                    document.getElementById('bt-text').textContent = 'Connected';
                } else {
                    btStatus.className = 'status-indicator off';
                    document.getElementById('bt-text').textContent = 'Off';
                }

                const carrierStatus = document.getElementById('carrier-status');
                if (data.hart_carrier) {
                    carrierStatus.className = 'status-indicator on';
                    document.getElementById('carrier-text').textContent = 'Detected';
                } else {
                    carrierStatus.className = 'status-indicator off';
                    document.getElementById('carrier-text').textContent = 'None';
                }

                lastUpdate = Date.now();
                updateLastUpdateTime();
            } catch (error) {
                console.error('Status update failed:', error);
            }
        }

        function formatUptime(seconds) {
            const h = Math.floor(seconds / 3600);
            const m = Math.floor((seconds % 3600) / 60);
            const s = seconds % 60;
            return `${h}:${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`;
        }

        function updateLastUpdateTime() {
            const now = Date.now();
            const diff = Math.floor((now - lastUpdate) / 1000);
            if (diff === 0) {
                document.getElementById('last-update').textContent = 'now';
            } else if (diff < 60) {
                document.getElementById('last-update').textContent = diff + 's ago';
            } else {
                document.getElementById('last-update').textContent = Math.floor(diff / 60) + 'm ago';
            }
        }

        function drawChart() {
            const canvas = document.getElementById('trend-chart');
            const ctx = canvas.getContext('2d');
            const width = canvas.width;
            const height = canvas.height;

            // Clear canvas
            ctx.fillStyle = '#1e1e1e';
            ctx.fillRect(0, 0, width, height);

            // Draw grid
            ctx.strokeStyle = '#333';
            ctx.lineWidth = 1;
            for (let i = 0; i <= 5; i++) {
                const y = (height / 5) * i;
                ctx.beginPath();
                ctx.moveTo(0, y);
                ctx.lineTo(width, y);
                ctx.stroke();
            }

            // Draw trend line
            if (trendData.length > 1) {
                ctx.strokeStyle = '#4fc3f7';
                ctx.lineWidth = 2;
                ctx.beginPath();
                for (let i = 0; i < trendData.length; i++) {
                    const x = (width / (trendData.length - 1)) * i;
                    const normalizedValue = trendData[i] / 1000;  // Normalize to 0-1
                    const y = height - (normalizedValue * height);
                    if (i === 0) {
                        ctx.moveTo(x, y);
                    } else {
                        ctx.lineTo(x, y);
                    }
                }
                ctx.stroke();
            }
        }

        // Update immediately and then every interval
        updateStatus();
        setInterval(updateStatus, API_INTERVAL);
        setInterval(updateLastUpdateTime, 1000);
        setInterval(drawChart, API_INTERVAL);
    </script>
</body>
</html>
)rawliteral";
  return html;
}

String WiFiDashboard::generateTrendJson() {
  String json = "[";
  for (int i = 0; i < TREND_BUFFER_SIZE; i++) {
    json += String(trendBuffer[i]);
    if (i < TREND_BUFFER_SIZE - 1) json += ",";
  }
  json += "]";
  return json;
}

void WiFiDashboard::handleNotFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not Found");
}
