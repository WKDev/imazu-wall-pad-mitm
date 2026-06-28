// ESP32 MITM 프록시 진입점: WiFi 프로비저닝, core 분리, 서버 기동
#include <Arduino.h>
#include <WiFiManager.h>
#include "config.h"
#include "rs485_bus.h"
#include "proxy.h"
#include "ha_server.h"
#include "debug.h"

static void proxyTask(void*);

void setup() {
  Serial.begin(115200);
  debugInit();          // 로그 큐 먼저 (WiFi 이전 로그도 보존)
  busInit();
  proxyInit();

  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  if (!wm.autoConnect("imazu-mitm")) {
    logLine("WiFi 연결 실패 — 재시작");
    delay(3000);
    ESP.restart();
  }
  logLine(String("WiFi OK: ") + WiFi.localIP().toString());

  haServerInit();

  // 타이밍 크리티컬한 버스/프록시는 core0 전용 태스크로 격리(WiFi 간섭 차단)
  xTaskCreatePinnedToCore(proxyTask, "proxy", 8192, nullptr, 2, nullptr, 0);
}

void loop() {
  // core1: 네트워킹/디버그
  haServerLoop();
  debugLoop();
  delay(1);
}

// ---- core0: RS485 버스 + 프록시 ----
static void proxyTask(void*) {
  for (;;) {
    // 1) 월패드 인바운드 (최우선) — FIFO 빨리 비워 오버플로 방지
    Frame f;
    while (readWallpadFrame(f)) {
      tapFrame("WP->", f);
      handleMasterFrame(f, [](const Frame& r) {
        writeWallpad(r);     // 월패드로 응답 (core0 버스 직접)
        haStream(r);         // HA로도 스트림 (큐 적재, 안전)
        tapFrame("->WP", r);
      }, /*allowEv=*/true);
    }

    // 2) HA 인바운드 (큐)
    while (haPopInbound(f)) {
      tapFrame("HA->", f);
      handleMasterFrame(f, [](const Frame& r) {
        haPushOutbound(r);
        tapFrame("->HA", r);
      }, /*allowEv=*/false);
    }

    // 3) 캐시 갱신 (월패드 유휴 슬롯에서 1스텝)
    if (wallpadIdleMs() > WALLPAD_IDLE_MS) {
      refreshStep([](const Frame& s) {
        haStream(s);         // 상태 변화 HA로 푸시
        tapFrame("DEV~", s);
      });
    }

    vTaskDelay(1);
  }
}
