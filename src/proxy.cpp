// 프록시 코어 구현 — 단일 코어(core0)에서만 호출되어 캐시/폴 락 불필요
#include "proxy.h"
#include "rs485_bus.h"
#include "config.h"

// (dev,sub) -> 마지막 STATUS 프레임 원본을 그대로 보관(의미 해석 안 함)
struct CacheEntry {
  uint16_t key;
  Frame status;
  bool used = false;
};
static constexpr int CACHE_MAX = 48;
static CacheEntry cache_[CACHE_MAX];

// 월패드가 실제로 보낸 SCAN 프레임을 저장해 기기버스에 그대로 재생(VTYPE 자동 일치)
struct PollTarget {
  Frame scan;
  bool used = false;
};
static constexpr int POLL_MAX = 48;
static PollTarget polls_[POLL_MAX];
static int pollCursor_ = 0;

// 마지막으로 관측한 EV STATUS의 STATE 바이트와 관측 시각(core1의 리부트 가드가 읽음)
static volatile uint8_t evLastState_ = 0;
static volatile uint32_t evLastStateMs_ = 0;

void proxyInit() {
  for (auto& e : cache_) e.used = false;
  for (auto& p : polls_) p.used = false;
}

static CacheEntry* cacheFind(uint16_t key) {
  for (auto& e : cache_)
    if (e.used && e.key == key) return &e;
  return nullptr;
}

static void cachePut(const Frame& status) {
  CacheEntry* e = cacheFind(status.key());
  if (!e)
    for (auto& c : cache_)
      if (!c.used) { c.used = true; c.key = status.key(); e = &c; break; }
  if (e) e->status = status;
}

static void learnPoll(const Frame& scan) {
  uint16_t key = scan.key();
  for (auto& p : polls_)
    if (p.used && p.scan.key() == key) { p.scan = scan; return; }
  for (auto& p : polls_)
    if (!p.used) { p.used = true; p.scan = scan; return; }
}

void handleMasterFrame(const Frame& f, const Respond& respond, bool allowEv) {
  (void)allowEv;
  // --- EV: 양방향 투명 터널 ---
  // 월패드 poll, HA의 호출 CHANGE(01340241100600) 모두 그대로 EV 모듈로 전달.
  // 모듈이 CHANGE 06을 받으면 STATUS가 06이 되고, 다음 월패드 poll(역시 터널)이
  // 그 06을 받아 단지 LAN으로 호출 송출. 가짜 주입 없이 모듈이 진짜 호출함.
  if (f.dev() == DEV_EV) {
    Frame reply;
    if (deviceTransact(f, reply, DEVICE_TIMEOUT_MS)) {
      // STATUS 응답의 STATE 바이트를 기록 — 예약 리부트가 호출 사이클을 끊지 않도록.
      if (reply.cmd() == CMD_STATUS && reply.len > 8) {
        evLastState_ = reply.data[8];
        evLastStateMs_ = millis();
      }
      respond(reply);
    }
    return;
  }

  // --- 비EV: 캐시/에뮬 ---
  if (f.cmd() == CMD_SCAN) {
    learnPoll(f);
    if (CacheEntry* e = cacheFind(f.key())) {  // 캐시 적중 → 즉시 응답(기기버스 무부하)
      respond(e->status);
      return;
    }
    Frame reply;                               // 워밍업: 1회 터널 후 캐시 적재
    if (deviceTransact(f, reply, DEVICE_TIMEOUT_MS)) {
      cachePut(reply);
      respond(reply);
    }
    return;
  }

  if (f.cmd() == CMD_CHANGE) {                 // 제어 명령 → 실기기로 전달 + 결과 캐시
    Frame reply;
    if (deviceTransact(f, reply, DEVICE_TIMEOUT_MS)) {
      cachePut(reply);
      respond(reply);
    }
    return;
  }
}

void refreshStep(const Respond& onStatus) {
  for (int n = 0; n < POLL_MAX; n++) {
    int i = (pollCursor_ + n) % POLL_MAX;
    if (polls_[i].used) {
      pollCursor_ = (i + 1) % POLL_MAX;
      Frame reply;
      if (deviceTransact(polls_[i].scan, reply, DEVICE_TIMEOUT_MS)) {
        cachePut(reply);
        if (onStatus) onStatus(reply);
      }
      return;                                  // 한 번에 한 타겟만
    }
  }
}

int cacheCount() {
  int n = 0;
  for (auto& e : cache_) if (e.used) n++;
  return n;
}

int pollCount() {
  int n = 0;
  for (auto& p : polls_) if (p.used) n++;
  return n;
}

bool evCallActive() {
  // 최근(EV_CALL_FRESH_MS 이내) 관측이 호출활성(06)일 때만 true. 오래된 관측은
  // 버스 정지 등일 수 있어 무시 — 리부트를 영구히 막지 않도록.
  return evLastState_ == EV_STATE_CALLING &&
         (uint32_t)(millis() - evLastStateMs_) < EV_CALL_FRESH_MS;
}
