// 프록시 코어: 비EV 상태 캐시·학습 폴·EV 투명터널·기기 갱신
#pragma once
#include "protocol.h"
#include <functional>

using Respond = std::function<void(const Frame&)>;

void proxyInit();

// 마스터(월패드/HA)가 보낸 프레임 처리. respond로 응답을 돌려줌.
// allowEv=true(월패드)면 EV 프레임을 기기버스로 투명 중계. HA는 false.
void handleMasterFrame(const Frame& f, const Respond& respond, bool allowEv);

// 기기버스 캐시 갱신 1스텝(라운드로빈). 갱신된 STATUS를 onStatus로 흘림(HA 스트림용).
void refreshStep(const Respond& onStatus);

// EV 호출이 방금(신선하게) 진행 중이면 true — 예약 리부트가 호출 사이클을 끊지 않도록 가드.
bool evCallActive();

// 디버그/웹 상태용
int cacheCount();
int pollCount();
