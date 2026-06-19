#!/usr/bin/env python
"""GridBot USB-serial screenshot for the PIO_DEBUG loop.

Sends 'S' to the device, reads the framed raw RGB565 panel readback, and writes a
PNG (pure stdlib: zlib only). No WiFi needed.

Usage:  python scripts/shot.py [COM3] [out.png]
Run with PlatformIO's bundled Python so pyserial is available:
  C:/Users/James/.platformio/penv/Scripts/python.exe scripts/shot.py COM3 .pio/shot.png
"""
import sys, struct, zlib, time

def find_serial():
    try:
        import serial  # type: ignore
        return serial
    except ImportError:
        sys.exit("pyserial not found; run with the PlatformIO penv python")

def read_exact(ser, n):
    buf = bytearray()
    deadline = time.time() + 30
    while len(buf) < n and time.time() < deadline:
        chunk = ser.read(n - len(buf))
        if chunk:
            buf += chunk
    if len(buf) < n:
        sys.exit(f"timeout: got {len(buf)}/{n} bytes")
    return bytes(buf)

def write_png(path, w, h, rgb):
    def chunk(typ, data):
        c = typ + data
        return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c) & 0xffffffff)
    raw = bytearray()
    stride = w * 3
    for y in range(h):
        raw.append(0)  # filter: none
        raw += rgb[y * stride:(y + 1) * stride]
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)

def main():
    port = sys.argv[1] if len(sys.argv) > 1 else "COM3"
    out = sys.argv[2] if len(sys.argv) > 2 else ".pio/shot.png"
    serial = find_serial()
    # Configure DTR/RTS LOW before opening so opening doesn't pulse-reset the ESP32
    # (CH340 DTR->EN). If a reset still happens, the wait below lets it boot.
    ser = serial.Serial()
    ser.port = port
    ser.baudrate = 115200
    ser.timeout = 2
    ser.dtr = False
    ser.rts = False
    ser.open()
    time.sleep(2.5)            # let the app boot if the open reset it
    ser.reset_input_buffer()
    ser.write(b"S\n")
    ser.flush()

    # find the GBSHOT header
    deadline = time.time() + 12
    header = None
    while time.time() < deadline:
        line = ser.readline()
        if line.startswith(b"GBSHOT"):
            header = line.strip().split()
            break
    if not header:
        sys.exit("no GBSHOT header (is the normal firmware flashed?)")
    w, h = int(header[1]), int(header[2])

    raw565 = read_exact(ser, w * h * 2)

    # Panel-specific readback mapping (CYD-ESP32-2432S028R.md "JPEG screenshot"):
    # byte-swap, then hi5=Blue, mid6=Red, lo5=Green.
    rgb = bytearray(w * h * 3)
    for i in range(w * h):
        lo = raw565[i * 2]
        hi = raw565[i * 2 + 1]
        v = (lo << 8) | hi            # byte-swap (device sent little-endian)
        b = (v >> 11) & 0x1f
        r = (v >> 5) & 0x3f
        g = v & 0x1f
        rgb[i * 3] = (r << 2) | (r >> 4)
        rgb[i * 3 + 1] = (g << 3) | (g >> 2)
        rgb[i * 3 + 2] = (b << 3) | (b >> 2)
    write_png(out, w, h, rgb)
    ser.close()
    print(f"wrote {out} ({w}x{h})")

if __name__ == "__main__":
    main()
