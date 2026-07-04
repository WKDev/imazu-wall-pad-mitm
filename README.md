# imazu-wall-pad-mitm

현대 HT(Imazu) 월패드와 실기기 사이에 끼어 **RS485 버스 충돌을 제거**하는 ESP32 MITM(Man-in-the-Middle) 프록시 펌웨어입니다.
Home Assistant 통합 [ha-component-imazu-wall-pad](https://github.com/WKDev/ha-component-imazu-wall-pad)와 함께 쓰도록 만들어졌습니다.

## 왜 필요한가

월패드는 RS485 버스를 쉼 없이 폴링합니다. 여기에 Home Assistant가 EW11 등으로 **제2의 마스터**처럼 끼면 폴링과 충돌해 명령이 간헐적으로 실패합니다(체감상 10번 중 2~3번만 성공). 그렇다고 월패드를 버스에서 빼면 **엘리베이터 호출이 불가능**해집니다 — 현관 EV 모듈이 월패드 폴링에 의존하기 때문입니다.

ESP32 MITM은 이 딜레마를 해결합니다.

- **실기기 버스에서는 ESP32가 유일한 마스터**로 폴링합니다.
- **월패드에는** 캐시해 둔 마지막 상태 프레임을 그대로 되쏘아, 월패드가 실기기와 대화하는 것처럼 속입니다.
- **HA에는** EW11과 동일한 `:8899` TCP 소켓을 흉내 냅니다. 통합의 `CONF_HOST`만 ESP32 IP로 바꾸면 됩니다.
- **엘리베이터(0x34)는 캐싱하지 않고 양방향 투명 터널**로 중계합니다. 호출·층표시가 실시간으로 통과합니다.

핵심 원칙은 **ESP32가 의미 해석을 하지 않는다**는 것입니다. `(DEV, SUB)`를 키로 마지막 STATUS 프레임의 raw 바이트를 캐싱해 그대로 되쏩니다. 상태 재인코딩은 틀릴 위험이 있지만 raw 복사는 진짜 기기와 구분되지 않습니다. brightness·fan speed 같은 의미 해석은 HA 쪽(`wp-imazu`)에만 둡니다.

## 아키텍처

세 개의 페이스(face)를 FreeRTOS 큐로 잇고, RS485 루프는 core 0, WiFi/소켓은 core 1에 핀합니다(WiFi 스택이 UART 타이밍을 블로킹하지 않도록).

```
   실기기 RS485            ESP32 MITM              월패드 RS485
  ┌──────────┐   UART1   ┌───────────┐   UART2   ┌──────────┐
  │ 조명/콘센트│◄────────►│  proxy    │◄────────►│  월패드   │
  │ 환기/EV… │ (마스터) │  + cache  │ (슬레이브) │ (마스터)  │
  └──────────┘          └─────┬─────┘          └──────────┘
                              │ WiFi/TCP :8899 (EW11 흉내)
                              ▼
                        Home Assistant
```

## 하드웨어

- 보드: **ESP-WROOM-32** (`board = esp32dev`)
- 트랜시버: **auto-direction RS485** 2개 (DE/RE 핀 제어 불필요, TX/RX만 연결)
- 버스: **9600 8N1**

### 핀아웃 (`src/config.h`)

| 용도 | RX | TX |
|---|---|---|
| UART1 — 실기기 버스 | GPIO16 | GPIO17 |
| UART2 — 월패드 버스 | GPIO26 | GPIO25 |

flash(6~11)·strapping(0,2,5,12,15)·input-only(34~39) 핀은 회피했습니다. UART0(GPIO1/3)은 USB 디버그로 유지합니다.

## 빌드 / 플래시

[PlatformIO](https://platformio.org/) 필요.

```bash
# USB로 최초 플래시
pio run -e esp32dev -t upload

# 이후 무선 업데이트 (OTA, mDNS 이름 imazu-mitm.local)
pio run -e ota -t upload
```

프로비저닝은 WiFiManager로 합니다. 최초 부팅 시 `imazu-mitm` AP에 접속해 WiFi를 설정하세요.

> **OTA 주의**: OTA 재부팅 시 `:8899` TCP가 끊깁니다. HA 통합이 half-open으로 물고 있을 수 있으니 OTA 후 통합을 리로드하세요.

## 런타임 인터페이스

| 포트 | 용도 |
|---|---|
| `:8899` | HA 연결 소켓 (EW11 흉내, 기기버스 스트림 미러). 클라이언트 3개까지 수용 |
| `:23` | Telnet 라이브 프레임 탭. 방향 태그 `WP->` `->WP` `HA->` `->HA` `DEV~` |
| `:80` | 웹 상태 — uptime·rssi·heap·maxAllocHeap·프레임/바이트 카운터 |

## 프레임 포맷

```
f7 LEN TYPE DEV CMD VTYPE SUB CHANGE [STATE...] CKSUM ee
```

- CKSUM = 헤더~STATE 전체 XOR (TAIL `ee` 제외)
- CMD: `01`=SCAN, `02`=CHANGE, `04`=STATUS. SUB 니블 = 방/서브(x0 = 전체)
- DEV: `19`조명 `1a`디밍 `1b`가스 `18`보일러 `1f`콘센트 `2a`외출 `2b`환기 `34`엘베

엘리베이터(0x34)는 양방향 대화입니다(월패드가 마스터로 먼저 보내고 현관기기가 STATUS 응답). 페이로드가 동적이라 캐싱하지 않고 그대로 중계합니다.

## 진단 도구 (`tools/`)

- `sniff_outlet.py <ESP32_IP>` — telnet 23 탭에서 콘센트(0x1f) 프레임만 추려 타임스탬프·방향·전원으로 출력. "누가 껐나" 진단용
- `capture_ev.py <IP> [PORT]` — `:8899`에서 60초 캡처(EV 분석용)

## 설계 상세

프로토콜 해독 과정, EV 캡처 분석, 타임아웃 튜닝, 힙 단편화 대응 등 결정 근거는 [`context-notes.md`](context-notes.md)에 누적되어 있습니다.

## 라이선스 / 크레딧

프로토콜 해독은 [`wp-imazu`](https://pypi.org/project/wp-imazu/) 라이브러리와 [ha-component-imazu-wall-pad](https://github.com/WKDev/ha-component-imazu-wall-pad) 통합을 기반으로 합니다.
