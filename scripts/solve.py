#!/usr/bin/env python
"""Solve a GridBot maze from the firmware's exact serial dump (the 'M' command).

  python scripts/drive.py COM3 "...; dump" > m.txt
  python scripts/solve.py m.txt
Reads the MAZE/ROBOT/GOAL block (exact tiles, no pixel guessing) and prints the
forward/turn/jump block sequence + the drive.py tap string. Works in every biome.
"""
import sys
from collections import deque

def main():
    txt = open(sys.argv[1]).read().splitlines() if len(sys.argv)>1 else sys.stdin.read().splitlines()
    i = next(k for k,l in enumerate(txt) if l.startswith("MAZE"))
    rows, cols = map(int, txt[i].split()[1:3])
    grid = txt[i+1:i+1+rows]
    rob = next(l for l in txt if l.startswith("ROBOT")).split()
    gl  = next(l for l in txt if l.startswith("GOAL")).split()
    R = (int(rob[1]), int(rob[2])); facing = int(rob[3]); G = (int(gl[1]), int(gl[2]))
    def t(rc):
        r,c = rc
        if 0<=r<rows and 0<=c<cols: return grid[r][c]
        return '#'
    blocked = lambda rc: t(rc) in ('#','O')      # walls + pits block (pits need a jump)
    DIRS = {(-1,0):'N',(0,1):'E',(1,0):'S',(0,-1):'W'}
    prev = {R: None}; q = deque([R])
    while q:
        cur = q.popleft()
        if cur == G: break
        for (dr,dc),d in DIRS.items():
            step = (cur[0]+dr, cur[1]+dc); jump = (cur[0]+2*dr, cur[1]+2*dc)
            if not blocked(step) and step not in prev:
                prev[step] = (cur,d,'F'); q.append(step)
            elif t(step)=='O' and not blocked(jump) and jump not in prev:
                prev[jump] = (cur,d,'J'); q.append(jump)
    if G not in prev: print("NO PATH"); return
    moves = []; cur = G
    while prev[cur] is not None:
        frm,d,k = prev[cur]; moves.append((d,k)); cur = frm
    moves.reverse()
    order = ['N','E','S','W']; f = facing; blk = []
    for d,k in moves:
        diff = (order.index(d)-f) % 4
        if diff==1: blk.append('R')
        elif diff==3: blk.append('L')
        elif diff==2: blk += ['R','R']
        blk.append('J' if k=='J' else 'F'); f = order.index(d)
    for r in range(rows):
        print(''.join('R' if (r,c)==R else 'G' if (r,c)==G else grid[r][c] for c in range(cols)))
    print("blocks:", ' '.join(blk), f"({len(blk)})")
    T = {'F':'tap 90 55','J':'tap 38 55','L':'tap 38 90','R':'tap 142 90'}
    print("DRIVE:", '; '.join(T[b]+'; wait .2' for b in blk))

main()
