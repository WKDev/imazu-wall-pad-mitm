# ESP32 MITM telnet(23) 탭에서 콘센트(0x1f) 프레임만 추려 "누가 껐나"를 진단하는 스니퍼
#
# 사용: python sniff_outlet.py [ESP32_IP=192.168.0.96] [--all] [--room N]
#   콘센트를 HA에서 켠 뒤 3-5초 지켜본다. 꺼지는 순간 앞뒤 프레임을 보고 원인을 가린다.
#   --all  : 콘센트뿐 아니라 모든 device 프레임 출력
#   --room N : 해당 방(SUB 상위 니블)만 출력 (예: 3-1 콘센트면 --room 3)
#
# 판정 가이드 (출력의 DIR 태그로 읽는다):
#   WP-> ...1f...CMD=CHANGE  → 월패드가 끈 것(대기전력 차단 등). 범인 확정.
#   DEV~ ...1f... 가 OFF 보고  → 실기기 모듈이 스스로 OFF로 되돌림(콘센트 자체 동작).
#   ->HA 만 OFF로 바뀜          → 캐시/표시 문제(폴이 낙관적 ON을 덮음).

import socket
import sys
import time

ESP32_IP = "192.168.0.96"
TELNET_PORT = 23

DEV = {
    0x16: "GUARD", 0x18: "THERMO", 0x19: "LIGHT", 0x1A: "DIM", 0x1B: "GAS",
    0x1C: "AC", 0x1F: "OUTLET", 0x2A: "AWAY", 0x2B: "FAN", 0x34: "EV",
    0x43: "USAGE", 0x48: "WPD_AC",
}
CMD = {0x01: "SCAN", 0x02: "CHANGE", 0x04: "STATUS"}


def parse_args(argv):
    host, show_all, room = ESP32_IP, False, None
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "--all":
            show_all = True
        elif a == "--room":
            i += 1
            room = int(argv[i])
        else:
            host = a
        i += 1
    return host, show_all, room


def decode(fr: bytes) -> str:
    """f7 LEN TYPE DEV CMD VTYPE SUB CHANGE [STATE...] CK ee"""
    if len(fr) < 8:
        return "(짧은 프레임)"
    dev = DEV.get(fr[3], f"?{fr[3]:02x}")
    cmd = CMD.get(fr[4], f"?{fr[4]:02x}")
    sub = fr[6]
    room, idx = sub >> 4, sub & 0x0F
    change = fr[7]
    state = fr[8:-2].hex()  # CK, ee 제외
    sub_str = f"{room}-{idx}" if idx else f"{room}-전체"
    return f"{dev:6} {cmd:6} sub={sub:02x}({sub_str}) chg={change:02x} state={state or '--'}"


def main():
    host, show_all, room_filter = parse_args(sys.argv[1:])
    print(f"# telnet {host}:{TELNET_PORT} 접속...")
    sock = socket.create_connection((host, TELNET_PORT), timeout=5)
    sock.settimeout(1.0)
    f = sock.makefile("r", encoding="utf-8", errors="replace")
    start = time.time()
    target = "모든 device" if show_all else "콘센트(0x1f)"
    rf = f" / 방 {room_filter}만" if room_filter is not None else ""
    print(f"# 필터: {target}{rf}")
    print(f"# 지금 HA에서 콘센트를 켜고 3-5초 지켜봐. Ctrl+C 종료.\n")
    print(f"{'wall':8} {'t(s)':>7}  {'DIR':4}  decoded{'':18}  raw")

    try:
        while True:
            try:
                line = f.readline()
            except socket.timeout:
                continue
            if not line:
                break
            line = line.strip()
            # "WP-> f70b011f..." 형태. 하트비트/기타 로그는 건너뜀.
            parts = line.split()
            if len(parts) != 2 or len(parts[0]) > 4:
                continue
            dir_tag, hx = parts
            if not hx.startswith("f7"):
                continue
            try:
                fr = bytes.fromhex(hx)
            except ValueError:
                continue
            if len(fr) < 8:
                continue
            dev = fr[3]
            if not show_all and dev != 0x1F:
                continue
            if room_filter is not None and (fr[6] >> 4) != room_filter:
                continue
            t = time.time() - start
            wall = time.strftime("%H:%M:%S")
            mark = ""
            if fr[4] == 0x02:           # CHANGE = 제어 명령 → 강조
                mark = "  <<< CHANGE"
            print(f"{wall:8} {t:7.2f}  {dir_tag:4}  {decode(fr):42}  {hx}{mark}")
    except KeyboardInterrupt:
        print("\n# 종료")
    finally:
        sock.close()


if __name__ == "__main__":
    main()
