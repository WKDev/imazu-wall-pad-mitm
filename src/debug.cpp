// 디버그 출력 구현. core0는 큐에 적재만, 실제 소켓/시리얼 출력은 core1(debugLoop).
#include "debug.h"
#include "config.h"
#include "proxy.h"
#include "rs485_bus.h"
#include <WiFi.h>
#include <WebServer.h>

struct LogMsg { char text[100]; };
static QueueHandle_t logQ_ = nullptr;
static volatile uint32_t frameCount_ = 0;

static constexpr int MAX_TELNET = 2;
static WiFiServer telnet_(TELNET_PORT);
static WiFiClient tclients_[MAX_TELNET];
static WebServer web_(80);
static bool serversUp_ = false;

static void handleRoot() {
  String p = "<html><head><meta charset=utf-8><title>imazu-mitm</title></head><body>";
  p += "<h2>Imazu MITM Proxy</h2><pre>";
  p += "uptime(s) : " + String(millis() / 1000) + "\n";
  p += "wifi rssi : " + String(WiFi.RSSI()) + " dBm\n";
  p += "free heap : " + String(ESP.getFreeHeap()) + "\n";
  p += "frames    : " + String(frameCount_) + "\n";
  p += "raw wp by : " + String(busRawWallpadBytes()) + "  (월패드 버스 수신 바이트)\n";
  p += "raw dev by: " + String(busRawDeviceBytes()) + "  (기기 버스 수신 바이트)\n";
  p += "cache ent : " + String(cacheCount()) + "\n";
  p += "poll tgts : " + String(pollCount()) + "\n";
  p += "</pre><p>telnet " + WiFi.localIP().toString() + " 23 for live frames</p>";
  p += "</body></html>";
  web_.send(200, "text/html", p);
}

void debugInit() {
  logQ_ = xQueueCreate(64, sizeof(LogMsg));
}

void logLine(const String& s) {
  if (!logQ_) return;
  LogMsg m;
  strncpy(m.text, s.c_str(), sizeof(m.text) - 1);
  m.text[sizeof(m.text) - 1] = 0;
  xQueueSend(logQ_, &m, 0);   // 가득 차면 버림(논블로킹)
}

void tapFrame(const char* dir, const Frame& f) {
  frameCount_++;
  logLine(String(dir) + " " + f.hex());
}

void debugLoop() {
  if (WiFi.status() == WL_CONNECTED && !serversUp_) {
    telnet_.begin();
    telnet_.setNoDelay(true);
    web_.on("/", handleRoot);
    web_.begin();
    serversUp_ = true;
  }
  if (!serversUp_) return;

  if (telnet_.hasClient()) {
    bool placed = false;
    for (int i = 0; i < MAX_TELNET; i++) {
      if (!tclients_[i] || !tclients_[i].connected()) {
        if (tclients_[i]) tclients_[i].stop();
        tclients_[i] = telnet_.available();
        placed = true;
        break;
      }
    }
    if (!placed) telnet_.available().stop();
  }

  LogMsg m;
  while (xQueueReceive(logQ_, &m, 0) == pdTRUE) {
    Serial.println(m.text);
    for (int i = 0; i < MAX_TELNET; i++)
      if (tclients_[i] && tclients_[i].connected()) tclients_[i].println(m.text);
  }

  // 3초마다 raw 바이트 하트비트 (프레임 0이어도 물리계층 진단 가능)
  static uint32_t lastBeat = 0;
  if (millis() - lastBeat > 3000) {
    lastBeat = millis();
    logLine(String("[stat] raw wp=") + busRawWallpadBytes() +
            " dev=" + busRawDeviceBytes() +
            " frames=" + frameCount_ +
            " polls=" + pollCount());
  }

  web_.handleClient();
}
