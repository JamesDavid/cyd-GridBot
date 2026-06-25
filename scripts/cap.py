#!/usr/bin/env python
"""Capture helper: boot COM3 to the real start screen (no H override), then run
tap/wait/shot/step commands. `step` sends N (debugStep: advance one match tick, paused)
so soccer frames can be grabbed exactly one tick apart for a smooth GIF.

  python scripts/cap.py COM3 "tap 266 56; wait .6; shot .pio/home.png"
"""
import sys, struct, zlib, time
import serial  # type: ignore


def shot(ser, path):
    ser.reset_input_buffer(); ser.write(b"S\n"); ser.flush()
    hdr = None; dl = time.time() + 10
    while time.time() < dl:
        l = ser.readline()
        if l.startswith(b"GBSHOT"): hdr = l.strip().split(); break
    if not hdr: print("  no GBSHOT"); return
    w, h = int(hdr[1]), int(hdr[2]); need = w * h * 2
    buf = bytearray(); dl = time.time() + 30
    while len(buf) < need and time.time() < dl:
        c = ser.read(need - len(buf))
        if c: buf += c
    rgb = bytearray(w * h * 3)
    for i in range(w * h):
        v = (buf[i * 2] << 8) | buf[i * 2 + 1]
        b = (v >> 11) & 0x1f; r = (v >> 5) & 0x3f; g = v & 0x1f
        rgb[i * 3] = (r << 2) | (r >> 4); rgb[i * 3 + 1] = (g << 3) | (g >> 2); rgb[i * 3 + 2] = (b << 3) | (b >> 2)
    def ch(t, d): c = t + d; return struct.pack(">I", len(d)) + c + struct.pack(">I", zlib.crc32(c) & 0xffffffff)
    raw = bytearray(); st = w * 3
    for y in range(h): raw.append(0); raw += rgb[y * st:(y + 1) * st]
    png = b"\x89PNG\r\n\x1a\n" + ch(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    png += ch(b"IDAT", zlib.compress(bytes(raw), 9)) + ch(b"IEND", b"")
    open(path, "wb").write(png); print("  wrote", path)


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else "COM3"
    cmds = sys.argv[2] if len(sys.argv) > 2 else sys.stdin.read()
    ser = serial.Serial(); ser.port = port; ser.baudrate = 115200; ser.timeout = 2
    ser.dtr = False; ser.rts = False; ser.open()
    ser.setRTS(True); time.sleep(0.12); ser.setRTS(False)
    time.sleep(5.5)            # full boot: splash + into "Choose a Player"
    ser.reset_input_buffer()
    ser.write(b"T 2 2\n"); ser.flush(); time.sleep(0.4)   # throwaway tap (first tap is often eaten)
    for raw in cmds.split(";"):
        c = raw.strip()
        if not c: continue
        p = c.split(); op = p[0]; print(c)
        if op == "tap":  ser.write(f"T {p[1]} {p[2]}\n".encode()); ser.flush()
        elif op == "shot": shot(ser, p[1])
        elif op == "wait": time.sleep(float(p[1]))
        elif op == "step": ser.write(b"N\n"); ser.flush()
        elif op == "key":  ser.write((" ".join(p[1:]) + "\n").encode()); ser.flush()
    ser.close()


if __name__ == "__main__":
    main()
