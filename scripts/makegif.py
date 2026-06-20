#!/usr/bin/env python
"""Assemble a set of GridBot screenshot frames into an animated GIF.

  python scripts/makegif.py "<glob>" <out.gif> [frame_ms] [scale] [last_hold_ms]

Uses a single shared palette across frames (GridBot's palette is small) so there's
no inter-frame flicker. Run with the PlatformIO penv python (has Pillow).
"""
import sys, glob
from PIL import Image

def main():
    pattern = sys.argv[1]
    out = sys.argv[2]
    ms = int(sys.argv[3]) if len(sys.argv) > 3 else 360
    scale = int(sys.argv[4]) if len(sys.argv) > 4 else 1
    last_hold = int(sys.argv[5]) if len(sys.argv) > 5 else 1400

    files = sorted(glob.glob(pattern))
    if not files:
        sys.exit("no frames matched " + pattern)
    imgs = [Image.open(f).convert("RGB") for f in files]
    if scale != 1:
        imgs = [im.resize((im.width * scale, im.height * scale), Image.NEAREST) for im in imgs]

    base = imgs[0].convert("P", palette=Image.ADAPTIVE, colors=128)
    frames = [im.quantize(palette=base, dither=Image.NONE) for im in imgs]
    durations = [ms] * len(frames)
    durations[-1] = last_hold  # hold the final frame
    frames[0].save(out, save_all=True, append_images=frames[1:],
                   duration=durations, loop=0, disposal=2)
    print(f"wrote {out} ({len(frames)} frames, {imgs[0].width}x{imgs[0].height})")

if __name__ == "__main__":
    main()
