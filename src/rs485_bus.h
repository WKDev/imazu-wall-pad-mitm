// 두 RS485 버스(UART1=기기 / UART2=월패드) 입출력과 기기 트랜잭션
#pragma once
#include "protocol.h"

void busInit();

bool readWallpadFrame(Frame& out);   // 비차단: 월패드 마스터 프레임 수신
bool writeWallpad(const Frame& f);

bool readDeviceFrame(Frame& out);    // 비차단: 기기 응답 수신
bool writeDevice(const Frame& f);

// 기기버스로 req 송신 후 응답 1개 수신(타임아웃). ESP32가 유일 마스터이므로 직렬.
bool deviceTransact(const Frame& req, Frame& reply, uint32_t timeoutMs);

uint32_t wallpadIdleMs();            // 마지막 월패드 수신 이후 경과(ms)
