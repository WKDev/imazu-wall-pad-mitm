// 하드웨어 핀·통신 상수 정의 (ESP-WROOM-32, auto-direction RS485 트랜시버)
#pragma once
#include <Arduino.h>

// RS485 #1 — 실기기 버스 (ESP32가 마스터). flash/strapping/input-only 핀 회피.
static constexpr int DEVICE_RX = 16;
static constexpr int DEVICE_TX = 17;
// RS485 #2 — 월패드 버스 (월패드가 마스터, ESP32는 슬레이브 집합처럼 응답)
static constexpr int WALLPAD_RX = 26;
static constexpr int WALLPAD_TX = 25;

static constexpr uint32_t BUS_BAUD = 9600;       // 캡처로 확인됨
static constexpr int UART_RX_BUFFER = 512;        // 죽은 기기 대기 중 FIFO 오버플로 방지

static constexpr uint16_t HA_PORT = 8899;         // EW11 호환 (HA 연결, CONF_HOST=ESP32)
static constexpr uint16_t TELNET_PORT = 23;       // 로그/프레임 탭 스트림
static constexpr const char* OTA_HOSTNAME = "imazu-mitm";  // OTA/mDNS 이름

// 운영 안정화: 매일 한국시간 03:00 정각에 1회 재부팅.
static constexpr int DAILY_REBOOT_HOUR = 3;
static constexpr int DAILY_REBOOT_MINUTE = 0;
static constexpr long KST_UTC_OFFSET_SECONDS = 9L * 60L * 60L;
static constexpr uint32_t DAILY_REBOOT_FALLBACK_MS = 24UL * 60UL * 60UL * 1000UL;

// 프로토콜 상수
static constexpr uint8_t DEV_EV = 0x34;           // 엘리베이터 — 투명 터널 대상
static constexpr uint8_t CMD_SCAN = 0x01;
static constexpr uint8_t CMD_CHANGE = 0x02;
static constexpr uint8_t CMD_STATUS = 0x04;

static constexpr uint32_t DEVICE_TIMEOUT_MS = 250;    // 기기 응답 대기 (조명/EV는 ~70-100ms로 느림)
static constexpr uint32_t WALLPAD_IDLE_MS = 30;       // 이만큼 조용하면 갱신 폴 1스텝
