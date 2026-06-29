// Imazu 프레임 검증/직렬화 및 스트림 파서 구현
#include "protocol.h"

uint8_t frameChecksum(const uint8_t* d, uint8_t len) {
  uint8_t s = 0;
  for (uint8_t i = 0; i + 2 < len; i++) s ^= d[i];  // [0 .. len-3]
  return s;
}

bool Frame::valid() const {
  if (len < 5 || len > FRAME_MAX) return false;
  if (data[0] != FRAME_HEADER || data[len - 1] != FRAME_TAIL) return false;
  if (data[1] != len) return false;
  return data[len - 2] == frameChecksum(data, len);
}

void Frame::hexTo(char* out, uint8_t cap) const {
  static const char* H = "0123456789abcdef";
  uint8_t j = 0;
  for (uint8_t i = 0; i < len && j + 2 < cap; i++) {
    out[j++] = H[data[i] >> 4];
    out[j++] = H[data[i] & 0x0F];
  }
  out[j] = 0;
}

bool FrameReader::push(uint8_t b, Frame& out) {
  if (idx_ == 0) {
    if (b != FRAME_HEADER) return false;   // 헤더 동기화
    buf_[idx_++] = b;
    return false;
  }
  if (idx_ >= FRAME_MAX) { reset(); return false; }
  buf_[idx_++] = b;

  if (idx_ == 2) {                          // LEN 확보
    expected_ = buf_[1];
    if (expected_ < 5 || expected_ > FRAME_MAX) { reset(); return false; }
  }
  if (expected_ && idx_ >= expected_) {     // 프레임 완성
    Frame f;
    memcpy(f.data, buf_, expected_);
    f.len = expected_;
    reset();
    if (f.valid()) { out = f; return true; }
    return false;                           // 깨진 프레임 → 폐기 후 재동기화
  }
  return false;
}
