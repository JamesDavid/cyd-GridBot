#!/usr/bin/env python
"""GridBot serial UI driver for the PIO_DEBUG loop.

Opens COM3 once (no reset), then runs commands from argv (semicolon-separated) or
stdin: `tap X Y`, `shot PATH`, `wait SECONDS`, `key C` (raw line to device).

  python scripts/drive.py COM3 "shot .pio/a.png; tap 56 60; wait .4; shot .pio/b.png"
"""
import sys, struct, zlib, time

def get_serial():
    import serial  # type: ignore
    return serial

def open_port(serial, port):
    ser = serial.Serial()
    ser.port = port; ser.baudrate = 115200; ser.timeout = 2
    ser.dtr = False; ser.rts = False
    ser.open()
    # Force a clean reset to a known state (profile select) so navigation is
    # deterministic: pulse EN low via RTS.
    ser.setRTS(True); time.sleep(0.12); ser.setRTS(False)
    time.sleep(3.0)
    ser.reset_input_buffer()
    ser.write(b"H\n"); ser.flush()  # force the home screen for deterministic nav
    time.sleep(0.4)
    return ser

def read_exact(ser, n):
    buf = bytearray(); deadline = time.time() + 30
    while len(buf) < n and time.time() < deadline:
        c = ser.read(n - len(buf))
        if c: buf += c
    return bytes(buf)

def write_png(path, w, h, rgb):
    def chunk(t, d): c = t + d; return struct.pack(">I", len(d)) + c + struct.pack(">I", zlib.crc32(c) & 0xffffffff)
    raw = bytearray(); stride = w * 3
    for y in range(h):
        raw.append(0); raw += rgb[y*stride:(y+1)*stride]
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b"")
    open(path, "wb").write(png)

def shot(ser, path):
    ser.reset_input_buffer(); ser.write(b"S\n"); ser.flush()
    hdr = None; deadline = time.time() + 10
    while time.time() < deadline:
        line = ser.readline()
        if line.startswith(b"GBSHOT"): hdr = line.strip().split(); break
    if not hdr: print("  no GBSHOT header"); return
    w, h = int(hdr[1]), int(hdr[2])
    raw = read_exact(ser, w*h*2)
    rgb = bytearray(w*h*3)
    for i in range(w*h):
        v = (raw[i*2] << 8) | raw[i*2+1]
        b = (v >> 11) & 0x1f; r = (v >> 5) & 0x3f; g = v & 0x1f
        rgb[i*3] = (r<<2)|(r>>4); rgb[i*3+1] = (g<<3)|(g>>2); rgb[i*3+2] = (b<<3)|(b>>2)
    write_png(path, w, h, rgb); print(f"  wrote {path}")

def main():
    port = sys.argv[1] if len(sys.argv) > 1 else "COM3"
    cmds = sys.argv[2] if len(sys.argv) > 2 else sys.stdin.read()
    serial = get_serial(); ser = open_port(serial, port)
    for raw in cmds.split(";"):
        c = raw.strip()
        if not c: continue
        parts = c.split()
        op = parts[0]
        print(c)
        if op == "tap":
            ser.write(f"T {parts[1]} {parts[2]}\n".encode()); ser.flush()
        elif op == "shot":
            shot(ser, parts[1])
        elif op == "wait":
            time.sleep(float(parts[1]))
        elif op == "key":
            ser.write((" ".join(parts[1:]) + "\n").encode()); ser.flush()
        elif op == "dump":
            ser.reset_input_buffer(); ser.write(b"M\n"); ser.flush()
            deadline = time.time() + 5; got = []
            while time.time() < deadline:
                line = ser.readline().decode(errors="replace").rstrip("\r\n")
                if not line: continue
                got.append(line)
                if line.startswith("GOAL") or line == "NOMAZE": break
            print("\n".join(got))
    ser.close()

if __name__ == "__main__":
    main()
