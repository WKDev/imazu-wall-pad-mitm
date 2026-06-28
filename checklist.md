# ESP32 MITM 프록시 — 체크리스트

## 1단계 — 해독 (완료)
- [x] Imazu 프레임/체크섬 구조 파악 (wp_imazu 0.0.36)
- [x] 비EV 기기 디코드 (조명/디밍/가스/보일러/콘센트/외출/환기)
- [x] EV(0x34) 캡처 + 양방향 프로토콜 해독 (ev_capture.log)
- [x] 설계 확정: 비EV=캐시/에뮬, EV=투명터널, core 분리

## 2단계 — 펌웨어 스캐폴드 (완료)
- [x] platformio.ini (esp32dev / arduino / WiFiManager)
- [x] config.h (핀·baud·포트·상수)
- [x] protocol.{h,cpp} (프레이밍 + XOR 체크섬 + FrameReader)
- [x] rs485_bus.{h,cpp} (UART1/2 + deviceTransact)
- [x] proxy.{h,cpp} (캐시 + 학습 폴 + EV 터널 + refresh)
- [x] ha_server.{h,cpp} (:8899 EW11 호환 + 코어간 큐)
- [x] debug.{h,cpp} (telnet 로그 + 웹 상태)
- [x] main.cpp (WiFiManager + core0 proxyTask)

## 3단계 — 빌드/검증
- [x] `pio run` 빌드 통과 (RAM 16.1%, Flash 64.8%)
- [ ] 벤치 테스트: UART1↔UART2 루프백으로 프레임 왕복 확인
- [ ] 실기기 버스만 연결 → 캐시 채워지는지(telnet 로그)
- [ ] 월패드 연결 → 조명/환기/보일러 표시 정상 + 버스 부하 측정
- [ ] **현관 EV 버튼 → 호출 동작 검증** (핵심)
- [ ] HA 연결(CONF_HOST=ESP32) → 제어/상태 검증
- [ ] 월패드 정정 명령 빈도 0 확인(에뮬 들킴 여부)

## 4단계 — 운영
- [ ] OTA 업데이트
- [ ] 워치독 + 자동 재접속
- [ ] 장시간 안정성 (24h)

## 미해결/관찰 대상
- 월패드 응답 타임아웃 실측 (현재 DEVICE_TIMEOUT_MS=60 보수적)
- 죽은 기기(THERMO 무응답) 부하 — RX버퍼 512 + 타임아웃 단축으로 완화
