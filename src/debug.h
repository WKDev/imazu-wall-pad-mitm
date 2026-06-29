// 디버그: telnet 로그/프레임 탭 + 웹 상태. 모든 출력은 core1에서 수행.
#pragma once
#include "protocol.h"

void debugInit();                         // 로그 큐 생성 (WiFi 이전)
void debugLoop();                         // core1: telnet/web 처리 + 큐 플러시
void logLine(const char* s);              // 힙 무할당 경로(핫패스용) — 큐 적재
void logLine(const String& s);            // 어느 코어에서나 안전(큐 적재)
void tapFrame(const char* dir, const Frame& f);  // "WP-> f70b..." 형태로 로깅
