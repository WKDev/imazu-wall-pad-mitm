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
- [x] OTA 무선 플래시 성공 (192.168.0.96)
- [x] 월패드 연결 → 폴링 수신 + 자동 학습(polls=9) + 캐시 응답
- [x] 조명/콘센트/외출/환기 device side 응답 + ->WP 캐시 응답 (타임아웃 250ms 수정 후)
- [x] EV 터널 ->WP 중계 동작 (idle 상태)
- [x] **현관 EV 버튼 실제 누름 → STATUS 00→06 중계 + 호출 동작 검증 ✅ 성공**
      (STATUS 06 중계 + 월패드 층표시 push 01→14 + 엘베 실제 도착 확인)
- [ ] 월패드 화면에서 조명/환기 표시·제어 정상 육안 확인
- [ ] HA 연결(CONF_HOST=ESP32) → 제어/상태 검증
- [ ] 월패드 정정 명령 빈도 / 버스 부하 측정 (에뮬 들킴 여부)

## 4단계 — 운영
- [ ] OTA 업데이트
- [ ] 워치독 + 자동 재접속
- [ ] 장시간 안정성 (24h)

## 미해결/관찰 대상
- 월패드 응답 타임아웃 실측 (현재 DEVICE_TIMEOUT_MS=60 보수적)
- 죽은 기기(THERMO 무응답) 부하 — RX버퍼 512 + 타임아웃 단축으로 완화
