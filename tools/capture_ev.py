# 월패드 RS485 버스를 EW11 TCP 브리지로 60초간 캡처해 EV(device 0x34) 프레임을 분석하는 스크립트
import os
import socket
import sys
import time
from collections import Counter

HOST = sys.argv[1] if len(sys.argv) > 1 else None
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 8899
DURATION = 60

LOG = os.path.join(os.path.dirname(os.path.abspath(__file__)), "ev_capture.log")

DEV = {
    0x16: "GUARD", 0x18: "THERMO", 0x19: "LIGHT", 0x1A: "DIM", 0x1B: "GAS",
    0x1C: "AC", 0x1F: "OUTLET", 0x2A: "AWAY", 0x2B: "FAN", 0x34: "EV",
    0x43: "USAGE", 0x48: "WPD_AC",
}
CMD = {0x01: "SCAN", 0x02: "CHANGE", 0x04: "STATUS"}


def xor_ok(frame: bytes) -> bool:
    """체크섬 검증. 헤더~STATE XOR == 체크섬 바이트."""
    if len(frame) < 4:
        return False
    s = 0
    for b in frame[:-2]:
        s ^= b
    return frame[-2] == s


def decode(frame: bytes) -> str:
    """프레임을 사람이 읽을 수 있게 디코드."""
    dev = DEV.get(frame[3], f"?{frame[3]:02x}")
    cmd = CMD.get(frame[4], f"?{frame[4]:02x}")
    sub = f"{frame[6]:02x}" if len(frame) > 6 else "--"
    ok = "" if xor_ok(frame) else "  <CKSUM_FAIL>"
    return f"{dev:7} {cmd:7} sub={sub}{ok}"


def extract_frames(buf: bytes):
    """f7 LEN ... ee 프레임을 LEN 기준으로 견고하게 분리."""
    out = []
    while buf:
        i = buf.find(0xF7)
        if i < 0:
            buf = b""
            break
        buf = buf[i:]
        if len(buf) < 2:
            break
        n = buf[1]
        if n < 5 or n > 40:           # 길이 sanity → 깨진 헤더면 1바이트 전진
            buf = buf[1:]
            continue
        if len(buf) < n:              # 프레임 아직 안 들어옴 → 다음 recv 대기
            break
        frame = buf[:n]
        if frame[-1] == 0xEE:
            out.append(frame)
            buf = buf[n:]
        else:
            buf = buf[1:]             # 리싱크
    return out, buf


def main():
    if not HOST:
        print("사용법: python capture_ev.py <EW11_IP> [PORT=8899]")
        print("예:    python capture_ev.py 192.168.0.50")
        sys.exit(1)

    sock = socket.create_connection((HOST, PORT), timeout=5)
    sock.settimeout(1.0)
    start = time.time()
    buf = b""
    seen = Counter()
    ev_frames = []
    logf = open(LOG, "w", encoding="utf-8")

    def emit(line: str):
        print(line)
        logf.write(line + "\n")

    emit(f"# capture start  host={HOST}:{PORT}  duration={DURATION}s")
    emit("# t(s)   raw_hex                          decoded")
    print("\n>>> 지금부터 60초. 10초/30초/50초쯤에 현관 EV 버튼을 한 번씩 눌러줘. <<<\n")

    try:
        while time.time() - start < DURATION:
            try:
                data = sock.recv(1024)
            except socket.timeout:
                continue
            if not data:
                break
            buf += data
            frames, buf = extract_frames(buf)
            for fr in frames:
                t = time.time() - start
                hx = fr.hex()
                line = f"{t:6.2f}  {hx:32}  {decode(fr)}"
                emit(line)
                key = (fr[3], fr[4], fr[6] if len(fr) > 6 else 0)
                seen[key] += 1
                if fr[3] == 0x34:
                    ev_frames.append((round(t, 2), hx))
    finally:
        sock.close()

    emit("\n# ===== summary: (device, cmd, sub) -> count =====")
    for (d, c, s), n in sorted(seen.items()):
        emit(f"#   {DEV.get(d, f'?{d:02x}'):7} {CMD.get(c, f'?{c:02x}'):7} sub={s:02x}  x{n}")

    emit("\n# ===== EV (0x34) frames =====")
    if ev_frames:
        for t, hx in ev_frames:
            emit(f"#   {t:6.2f}  {hx}")
    else:
        emit("#   (없음) EV 프레임이 안 잡혔어. 월패드가 연결돼 있는지, "
             "버튼을 캡처 중에 눌렀는지 확인해줘.")

    logf.close()
    print(f"\n로그 저장: {LOG}")


if __name__ == "__main__":
    main()
