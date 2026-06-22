#!/usr/bin/env python
"""Read a GridBot maze screenshot -> the block sequence that solves it.

Vision only (the pixels a kid sees); moves still get tapped one button at a time.
Auto-detects the grid from the 1px void seams between tiles, then classifies each
cell (wall/floor/pit) by palette and BFS-es robot->goal.

  python scripts/maze.py .pio/L4.png S                 # img start-facing
  python scripts/maze.py .pio/L4.png S --robot 0 0 --goal 4 4   # override icons
"""
import sys
from collections import deque
from PIL import Image

BANDY0, BANDY1, W = 22, 204, 320

def void(p): return abs(p[0]-24)<20 and abs(p[1]-26)<20 and abs(p[2]-34)<22

def runs_centers(prof):
    mx=max(prof); thr=mx*0.55; out=[]; s=None
    for i,v in enumerate(prof):
        if v<thr and s is None: s=i
        elif v>=thr and s is not None:
            if i-s>=12: out.append((s,i-1))
            s=None
    if s is not None and len(prof)-s>=12: out.append((s,len(prof)-1))
    return [(a+b)//2 for a,b in out], out

def main():
    a=sys.argv; img=a[1]; facing=a[2] if len(a)>2 else 'N'
    rob_ov=goal_ov=None
    if '--robot' in a: i=a.index('--robot'); rob_ov=(int(a[i+1]),int(a[i+2]))
    if '--goal'  in a: i=a.index('--goal');  goal_ov=(int(a[i+1]),int(a[i+2]))
    im=Image.open(img).convert('RGB'); px=im.load()
    colprof=[sum(1 for y in range(BANDY0,BANDY1) if void(px[x,y])) for x in range(W)]
    cxs,cxr=runs_centers(colprof)
    x0,x1=cxr[0][0],cxr[-1][1]
    rowprof=[sum(1 for x in range(x0,x1+1) if void(px[x,y])) for y in range(BANDY0,BANDY1)]
    cyr0,_=runs_centers(rowprof); cys=[y+BANDY0 for y in cyr0]
    cols,rows=len(cxs),len(cys)
    def cellpx(cx,cy):
        out=[]
        rad=max(4,(x1-x0)//cols//3)
        for x in range(cx-rad,cx+rad+1):
            for y in range(cy-rad,cy+rad+1):
                if 0<=x<W and 0<=y<240: out.append(px[x,y])
        return out
    grid={}; sums={}; rob=goal=None; bluecells=[]
    for r,cy in enumerate(cys):
        for c,cx in enumerate(cxs):
            pp=cellpx(cx,cy)
            blue=sum(1 for p in pp if p[2]>110 and p[2]>p[0]+25 and p[2]>p[1]+10)
            yellow=sum(1 for p in pp if p[0]>185 and p[1]>150 and p[2]<110)
            avg=tuple(sum(p[i] for p in pp)//len(pp) for i in range(3))
            sums[(r,c)]=sum(avg); grid[(r,c)]='?'
            if blue>6: bluecells.append((blue,(r,c))); grid[(r,c)]='b'
            elif yellow>4: grid[(r,c)]='c'
    # robot = most-blue cell; the goal battery is the other blue-ish cell
    bluecells.sort(reverse=True)
    if bluecells: rob=bluecells[0][1]; grid[rob]='R'
    if len(bluecells)>1: goal=bluecells[1][1]; grid[goal]='G'
    for bl,rc in bluecells:
        if rc!=rob and rc!=goal: grid[rc]='c'   # stray blue -> treat as passable
    # wall/floor/pit among '?' cells
    qs=[(rc,sums[rc],) for rc,k in grid.items() if k=='?']
    nonpit=[s for rc,s in qs if not (s<100)]
    floor_lo=min(nonpit) if nonpit else 0
    for rc,s in qs:
        r,g,b=tuple(sum(px[cxs[rc[1]]+dx,cys[rc[0]]+dy][i] for dx in(-2,0,2) for dy in(-2,0,2))//9 for i in range(3))
        if s<100 and b>=r-6: grid[rc]='.'                       # pit/void
        elif s >= floor_lo+85: grid[rc]='#'                     # wall (clearly brighter)
        else: grid[rc]=' '
    if rob_ov: rob=rob_ov
    if goal_ov: goal=goal_ov
    print(f"{rows}x{cols}  robot={rob} goal={goal}")
    for r in range(rows):
        print(''.join('R' if (r,c)==rob else 'G' if (r,c)==goal else grid[(r,c)] for c in range(cols)))
    if not rob or not goal: print("** need --robot r c / --goal r c"); return
    DIRS={(-1,0):'N',(0,1):'E',(1,0):'S',(0,-1):'W'}
    prev={rob:None}; q=deque([rob])   # prev[cell]=(from, dir, kind)
    while q:
        cur=q.popleft()
        if cur==goal: break
        for dr,dc in DIRS:
            step=(cur[0]+dr,cur[1]+dc)
            jump=(cur[0]+2*dr,cur[1]+2*dc)
            # plain step onto floor/coin/goal
            if 0<=step[0]<rows and 0<=step[1]<cols and step not in prev and grid.get(step) not in ('#','.'):
                prev[step]=(cur,DIRS[(dr,dc)],'F'); q.append(step)
            # jump OVER a single pit to the tile beyond (Jump block)
            elif (grid.get(step)=='.' and 0<=jump[0]<rows and 0<=jump[1]<cols
                  and jump not in prev and grid.get(jump) not in ('#','.')):
                prev[jump]=(cur,DIRS[(dr,dc)],'J'); q.append(jump)
    if goal not in prev: print("NO PATH (try --robot/--goal or check pits)"); return
    moves=[]; cur=goal
    while prev[cur] is not None:
        frm,d,kind=prev[cur]; moves.append((d,kind)); cur=frm
    moves.reverse()
    order=['N','E','S','W']; f=order.index(facing); blk=[]
    for d,kind in moves:
        diff=(order.index(d)-f)%4
        if diff==1: blk.append('R')
        elif diff==3: blk.append('L')
        elif diff==2: blk+=['R','R']
        blk.append('J' if kind=='J' else 'F'); f=order.index(d)
    print("moves:",' '.join(f"{d}{k}" for d,k in moves))
    print("blocks:",' '.join(blk),f"({len(blk)})")
    T={'F':'tap 90 55','J':'tap 38 55','L':'tap 38 90','R':'tap 142 90'}
    print("DRIVE:",'; '.join(T[b]+'; wait .22' for b in blk))

main()
