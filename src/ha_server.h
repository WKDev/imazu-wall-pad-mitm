// HA 연결용 EW11 호환 TCP 서버(:8899) + 코어간 프레임 큐
#pragma once
#include "protocol.h"

void haServerInit();                  // 큐 생성 + 서버 시작 (WiFi 연결 후 호출)
void haServerLoop();                  // core1: 소켓 입출력

bool haPopInbound(Frame& out);        // core0가 HA→ESP 프레임 소비
void haPushOutbound(const Frame& f);  // core0가 ESP→HA 프레임 생산(응답/스트림)
inline void haStream(const Frame& f) { haPushOutbound(f); }
