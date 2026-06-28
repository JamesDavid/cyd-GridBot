#!/usr/bin/env python
"""Drive TWO CYDs at once to run an ESP-NOW Room match between their saved fighters.
Boots both, navigates both to Arena -> Radio -> Room, hosts on the first + joins on the
second, sets the discipline to Soccer, and starts. Screenshots at each milestone.

  python scripts/duel.py COM3 COM5
"""
import sys, struct, zlib, time
import serial  # type: ignore


def open_port(p):
    s = serial.Serial(); s.port = p; s.baudrate = 115200; s.timeout = 2
    s.dtr = False; s.rts = False; s.open()
    return s


def reset(s):
    s.setRTS(True); time.sleep(0.12); s.setRTS(False)


def tap(s, x, y):
    s.write(f"T {x} {y}\n".encode()); s.flush()


def shot(s, path):
    s.reset_input_buffer(); s.write(b"S\n"); s.flush()
    hdr = None; dl = time.time() + 12
    while time.time() < dl:
        l = s.readline()
        if l.startswith(b"GBSHOT"): hdr = l.strip().split(); break
    if not hdr: print("  no GBSHOT for", path); return
    w, h = int(hdr[1]), int(hdr[2]); need = w * h * 2
    buf = bytearray(); dl = time.time() + 30
    while len(buf) < need and time.time() < dl:
        c = s.read(need - len(buf))
        if c: buf += c
    rgb = bytearray(w * h * 3)
    for i in range(w * h):
        v = (buf[i * 2] << 8) | buf[i * 2 + 1]
        b = (v >> 11) & 0x1f; r = (v >> 5) & 0x3f; g = v & 0x1f
        rgb[i*3] = (r<<2)|(r>>4); rgb[i*3+1] = (g<<3)|(g>>2); rgb[i*3+2] = (b<<3)|(b>>2)
    def ch(t, d): c = t+d; return struct.pack(">I", len(d))+c+struct.pack(">I", zlib.crc32(c)&0xffffffff)
    raw = bytearray(); st = w*3
    for y in range(h): raw.append(0); raw += rgb[y*st:(y+1)*st]
    png = b"\x89PNG\r\n\x1a\n" + ch(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    png += ch(b"IDAT", zlib.compress(bytes(raw), 9)) + ch(b"IEND", b"")
    open(path, "wb").write(png); print("  wrote", path)


def main():
    pa = sys.argv[1] if len(sys.argv) > 1 else "COM3"   # host
    pb = sys.argv[2] if len(sys.argv) > 2 else "COM5"    # joiner
    a = open_port(pa); b = open_port(pb)
    reset(a); reset(b)
    time.sleep(5.5)
    a.reset_input_buffer(); b.reset_input_buffer()
    tap(a, 2, 2); tap(b, 2, 2); time.sleep(0.4)            # throwaway taps (first tap is eaten)

    # select players (COM3 unnamed = 160,140 ; COM5 V = 60,55)
    tap(a, 160, 140); tap(b, 60, 55); time.sleep(1.4)
    tap(a, 240, 72);  tap(b, 240, 72); time.sleep(1.1)     # Arena
    tap(a, 160, 143); tap(b, 160, 143); time.sleep(1.1)    # Radio
    tap(a, 160, 159); tap(b, 160, 159); time.sleep(1.4)    # Room -> lobby
    shot(a, ".pio/net/match_a_lobby.png"); shot(b, ".pio/net/match_b_lobby.png")

    tap(a, 160, 91); time.sleep(2.0)                       # A hosts
    tap(b, 160, 141); time.sleep(4.5)                      # B joins -> ESP-NOW pairing
    shot(a, ".pio/net/match_a_paired.png")                 # host should now show "2 in"

    tap(a, 263, 219); time.sleep(1.0)                      # host: discipline Sumo->Soccer (1 tap)
    tap(a, 160, 219); time.sleep(3.0)                      # host: START (needs >=2 players)
    shot(a, ".pio/net/match_a_play.png"); shot(b, ".pio/net/match_b_play.png")
    a.close(); b.close()


if __name__ == "__main__":
    main()
