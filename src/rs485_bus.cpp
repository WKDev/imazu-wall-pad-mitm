// RS485 양 버스 입출력 구현. auto-direction 트랜시버라 DE/RE 제어 불필요.
#include "rs485_bus.h"
#include "config.h"

static FrameReader wpReader;
static FrameReader devReader;
static uint32_t lastWpMs = 0;

void busInit() {
  Serial1.setRxBufferSize(UART_RX_BUFFER);
  Serial2.setRxBufferSize(UART_RX_BUFFER);
  Serial1.begin(BUS_BAUD, SERIAL_8N1, DEVICE_RX, DEVICE_TX);   // 기기 버스
  Serial2.begin(BUS_BAUD, SERIAL_8N1, WALLPAD_RX, WALLPAD_TX); // 월패드 버스
}

bool readWallpadFrame(Frame& out) {
  while (Serial2.available()) {
    if (wpReader.push((uint8_t)Serial2.read(), out)) {
      lastWpMs = millis();
      return true;
    }
  }
  return false;
}

bool writeWallpad(const Frame& f) {
  if (!f.valid()) return false;
  Serial2.write(f.data, f.len);
  Serial2.flush();
  return true;
}

bool readDeviceFrame(Frame& out) {
  while (Serial1.available()) {
    if (devReader.push((uint8_t)Serial1.read(), out)) return true;
  }
  return false;
}

bool writeDevice(const Frame& f) {
  if (!f.valid()) return false;
  Serial1.write(f.data, f.len);
  Serial1.flush();
  return true;
}

bool deviceTransact(const Frame& req, Frame& reply, uint32_t timeoutMs) {
  Frame junk;
  while (readDeviceFrame(junk)) {}        // 잔여 입력 비우기
  devReader.reset();
  if (!writeDevice(req)) return false;
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    if (readDeviceFrame(reply)) return true;
    delay(1);                             // FreeRTOS yield
  }
  return false;
}

uint32_t wallpadIdleMs() { return millis() - lastWpMs; }
