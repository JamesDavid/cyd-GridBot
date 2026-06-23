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

def void(p): return abs(p[0]-24)<13 and abs(p[1]-25)<13 and abs(p[2]-33)<14

def boundaries(grad):
    # cell boundaries = strong color-step peaks (biome-independent: the floor checker,
    # walls, pits all contrast with neighbours). Returns merged boundary x/y positions.
    mx=max(grad); thr=mx*0.33; bs=[]
    for i,v in enumerate(grad):
        if v>thr and (not bs or i-bs[-1]>10): bs.append(i)
        elif v>thr and bs and grad[bs[-1]]<v: bs[-1]=i   # keep the taller peak in a run
    return bs

def centers_from_grad(grad):
    bs=boundaries(grad)
    if len(bs)<2: return []
    return [(bs[i]+bs[i+1])//2 for i in range(len(bs)-1)]

def main():
    a=sys.argv; img=a[1]; facing=a[2] if len(a)>2 else 'N'
    rob_ov=goal_ov=None
    if '--robot' in a: i=a.index('--robot'); rob_ov=(int(a[i+1]),int(a[i+2]))
    if '--goal'  in a: i=a.index('--goal');  goal_ov=(int(a[i+1]),int(a[i+2]))
    im=Image.open(img).convert('RGB'); px=im.load()
    def bright(x,y): p=px[x,y]; return p[0]+p[1]+p[2]
    # 1) maze bbox from the void margins (C_BG is C_BG in every biome).
    # pick the WIDEST contiguous content run so left/right UI slivers don't extend it.
    def widest_run(prof, thr, gap=8):
        # widest cluster of content, tolerating small (seam-sized) internal gaps but
        # not the large void gap between the maze and any side UI sliver.
        on=[i for i,v in enumerate(prof) if v>thr]
        if not on: return 0,len(prof)-1
        runs=[[on[0],on[0]]]
        for i in on[1:]:
            if i-runs[-1][1]<=gap: runs[-1][1]=i
            else: runs.append([i,i])
        b=max(runs,key=lambda r:r[1]-r[0]); return b[0],b[1]
    nvx=[sum(1 for y in range(BANDY0,BANDY1) if not void(px[x,y])) for x in range(W)]
    x0,x1=widest_run(nvx, max(nvx)*0.25)
    nvy=[sum(1 for x in range(x0,x1+1) if not void(px[x,y])) if BANDY0<=y<BANDY1 else 0 for y in range(240)]
    y0,y1=widest_run(nvy, max(nvy)*0.25)
    # 2) cells are square: find the one tile size that divides BOTH bbox dims to integers
    bw,bh=x1-x0+1, y1-y0+1
    best=None
    for t in range(24,65):
        cc,rr=round(bw/t),round(bh/t)
        if cc<1 or rr<1: continue
        err=abs(bw-t*cc)+abs(bh-t*rr)
        if best is None or err<best[0]: best=(err,cc,rr)
    cols,rows=best[1],best[2]
    tilex=bw/cols; tiley=bh/rows
    cxs=[int(x0+tilex*(i+0.5)) for i in range(cols)]
    cys=[int(y0+tiley*(i+0.5)) for i in range(rows)]
    def cellpx(cx,cy):
        out=[]
        rad=max(3,min(tilex,tiley)//5)   # tight center window: avoid bleeding into neighbours
        for x in range(cx-int(rad),cx+int(rad)+1):
            for y in range(cy-int(rad),cy+int(rad)+1):
                if 0<=x<W and 0<=y<240: out.append(px[x,y])
        return out
    grid={}; sums={}; rob=goal=None; bluecells=[]
    for r,cy in enumerate(cys):
        for c,cx in enumerate(cxs):
            pp=cellpx(cx,cy)
            voidfrac=sum(1 for p in pp if void(p))/len(pp)
            avg=tuple(sum(p[i] for p in pp)//len(pp) for i in range(3))
            # --open: Nebula floor ~= void background, so don't trust pit detection there;
            # treat void-ish cells as floor and rely on the bright-wall test for blocking.
            ispit = voidfrac>0.4 and '--open' not in a
            sums[(r,c)]=sum(avg); grid[(r,c)]='.' if ispit else '?'
            # saturated-blue pixel count, only used to GUESS the robot in warm biomes
            blue=sum(1 for p in pp if p[2]>120 and p[2]>p[0]+60 and p[2]>p[1]+50)
            if grid[(r,c)]=='?' and blue>6: bluecells.append((blue,(r,c)))
    # wall vs floor by the FRACTION of bright pixels: a wall is uniformly bright, while a
    # coin/goal is a bright dot on dark floor. (Avg alone mis-reads coins as walls.)
    nonpit=sorted(s for rc,s in sums.items() if grid[rc]=='?')
    floor_med=nonpit[int(len(nonpit)*0.4)] if nonpit else 0
    pthr=floor_med*1.3/3*1.0   # per-pixel bright threshold (sum) ~ 1.3x floor level
    for r,cy in enumerate(cys):
        for c,cx in enumerate(cxs):
            if grid[(r,c)]!='?': continue
            pp=cellpx(cx,cy)
            bf=sum(1 for p in pp if sum(p) > floor_med*1.3)/len(pp)
            grid[(r,c)]='#' if bf>0.5 else ' '
    # robot/goal: explicit override wins; else best-effort auto (warm biomes)
    bluecells.sort(reverse=True)
    if not rob_ov and bluecells: rob=bluecells[0][1]
    if not goal_ov and len(bluecells)>1: goal=bluecells[1][1]
    if rob_ov: rob=rob_ov
    if goal_ov: goal=goal_ov
    if rob: grid[rob]='R'
    if goal: grid[goal]='G'
    # manual wall overrides for low-contrast cells the brightness test can't resolve:
    #   --wall r c r c ...   (any number of pairs)
    if '--wall' in a:
        i=a.index('--wall')+1
        while i+1<len(a) and a[i].lstrip('-').isdigit():
            grid[(int(a[i]),int(a[i+1]))]='#'; i+=2
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
