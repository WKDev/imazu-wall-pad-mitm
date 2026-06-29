// Imazu 월패드 RS485 프레임 구조·체크섬·스트림 파서(FrameReader)
#pragma once
#include <Arduino.h>

static constexpr uint8_t FRAME_HEADER = 0xF7;
static constexpr uint8_t FRAME_TAIL = 0xEE;
static constexpr uint8_t FRAME_MAX = 64;

// f7 LEN TYPE DEV CMD VTYPE SUB CHANGE [STATE...] CK ee
struct Frame {
  uint8_t data[FRAME_MAX];
  uint8_t len = 0;

  uint8_t dev() const { return len > 3 ? data[3] : 0; }
  uint8_t cmd() const { return len > 4 ? data[4] : 0; }
  uint8_t sub() const { return len > 6 ? data[6] : 0; }
  // 캐시 키 = device << 8 | sub
  uint16_t key() const { return (uint16_t(dev()) << 8) | sub(); }

  bool valid() const;
  void hexTo(char* out, uint8_t cap) const;  // 힙 무할당: 고정 버퍼에 직접 hex 기록
};

// XOR 체크섬: 인덱스 [0 .. len-3] (CK·TAIL 제외)
uint8_t frameChecksum(const uint8_t* d, uint8_t len);

// 바이트 스트림에서 프레임을 한 개씩 복원. LEN 필드 기준이라 페이로드 내 f7/ee 무관.
class FrameReader {
 public:
  bool push(uint8_t b, Frame& out);   // 완성 시 true + out
  void reset() { idx_ = 0; expected_ = 0; }

 private:
  uint8_t buf_[FRAME_MAX];
  uint8_t idx_ = 0;
  uint8_t expected_ = 0;
};
