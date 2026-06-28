// HA TCP 서버 구현. core1에서 소켓을 돌리고 core0와는 FreeRTOS 큐로만 통신.
#include "ha_server.h"
#include "config.h"
#include <WiFi.h>

static constexpr int MAX_CLIENTS = 3;
static WiFiServer server(HA_PORT);
static WiFiClient clients_[MAX_CLIENTS];
static FrameReader readers_[MAX_CLIENTS];
static QueueHandle_t inQ_ = nullptr;
static QueueHandle_t outQ_ = nullptr;

void haServerInit() {
  inQ_ = xQueueCreate(16, sizeof(Frame));
  outQ_ = xQueueCreate(32, sizeof(Frame));
  server.begin();
  server.setNoDelay(true);
}

void haServerLoop() {
  // 신규 클라이언트 수용
  if (server.hasClient()) {
    bool placed = false;
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (!clients_[i] || !clients_[i].connected()) {
        if (clients_[i]) clients_[i].stop();
        clients_[i] = server.available();
        readers_[i].reset();
        placed = true;
        break;
      }
    }
    if (!placed) server.available().stop();   // 자리 없으면 거절
  }

  // 인바운드: 바이트 → 프레임 → inQ
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients_[i] && clients_[i].connected()) {
      Frame f;
      while (clients_[i].available()) {
        if (readers_[i].push((uint8_t)clients_[i].read(), f))
          xQueueSend(inQ_, &f, 0);
      }
    }
  }

  // 아웃바운드: outQ → 모든 클라이언트 (EW11 transparent 스트림 흉내)
  Frame f;
  while (xQueueReceive(outQ_, &f, 0) == pdTRUE) {
    for (int i = 0; i < MAX_CLIENTS; i++)
      if (clients_[i] && clients_[i].connected())
        clients_[i].write(f.data, f.len);
  }
}

bool haPopInbound(Frame& out) {
  return inQ_ && xQueueReceive(inQ_, &out, 0) == pdTRUE;
}

void haPushOutbound(const Frame& f) {
  if (outQ_) xQueueSend(outQ_, &f, 0);
}
