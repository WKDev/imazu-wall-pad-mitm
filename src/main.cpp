// ESP32 MITM 프록시 진입점: WiFi 프로비저닝, core 분리, 서버 기동
#include <Arduino.h>
#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <time.h>
#include "config.h"
#include "rs485_bus.h"
#include "proxy.h"
#include "ha_server.h"
#include "debug.h"

static constexpr time_t VALID_NTP_EPOCH = 1704067200;  // 2024-01-01 UTC
static constexpr int DAILY_REBOOT_WINDOW_SECONDS = 60;
static constexpr uint32_t REBOOT_LOG_FLUSH_MS = 1500;

static Preferences rebootPrefs;
static bool rebootPrefsReady = false;
static int lastRebootDay = -1;

static void proxyTask(void*);
static void scheduledRebootInit();
static void scheduledRebootLoop();

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
  scheduledRebootInit();

  // OTA — 이후 업데이트는 USB 없이 WiFi로 (pio run -e ota -t upload)
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.onStart([]() { logLine("OTA 시작 — 업데이트 중"); });
  ArduinoOTA.onEnd([]() { logLine("OTA 완료 — 재부팅"); });
  ArduinoOTA.onError([](ota_error_t e) { logLine(String("OTA 에러: ") + e); });
  ArduinoOTA.begin();
  logLine(String("OTA ready: ") + OTA_HOSTNAME + ".local");

  haServerInit();

  // 타이밍 크리티컬한 버스/프록시는 core0 전용 태스크로 격리(WiFi 간섭 차단)
  xTaskCreatePinnedToCore(proxyTask, "proxy", 8192, nullptr, 2, nullptr, 0);
}

void loop() {
  // core1: 네트워킹/디버그
  ArduinoOTA.handle();
  haServerLoop();
  debugLoop();
  scheduledRebootLoop();
  delay(1);
}

static void scheduledRebootInit() {
  configTime(KST_UTC_OFFSET_SECONDS, 0, "kr.pool.ntp.org", "pool.ntp.org", "time.nist.gov");
  rebootPrefsReady = rebootPrefs.begin("reboot", false);
  if (rebootPrefsReady) lastRebootDay = rebootPrefs.getInt("lastDay", -1);
  logLine("daily reboot armed: 03:00 KST");
}

static int rebootDayId(const tm& local) {
  return (local.tm_year + 1900) * 1000 + local.tm_yday;
}

static void restartForSchedule(const char* reason) {
  logLine(reason);
  delay(REBOOT_LOG_FLUSH_MS);
  ESP.restart();
}

static void scheduledRebootLoop() {
  time_t now = time(nullptr);
  if (now >= VALID_NTP_EPOCH) {
    tm local;
    localtime_r(&now, &local);

    const int dayId = rebootDayId(local);
    if (local.tm_hour == DAILY_REBOOT_HOUR &&
        local.tm_min == DAILY_REBOOT_MINUTE &&
        local.tm_sec < DAILY_REBOOT_WINDOW_SECONDS &&
        dayId != lastRebootDay) {
      // EV 호출 진행 중이면 이 창에서는 보류 — 리부트가 사이클을 끊어 모듈이 06에
      // 래치되는 걸 막는다. lastRebootDay를 안 찍으므로 06이 풀리면 창 내 재시도.
      if (evCallActive()) return;
      lastRebootDay = dayId;
      if (rebootPrefsReady) rebootPrefs.putInt("lastDay", dayId);
      restartForSchedule("daily reboot: scheduled 03:00 KST");
    }
    return;
  }

  if (millis() >= DAILY_REBOOT_FALLBACK_MS) {
    if (evCallActive()) return;   // 호출 중이면 보류(다음 loop에서 재평가)
    restartForSchedule("daily reboot: NTP unavailable, 24h fallback");
  }
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
