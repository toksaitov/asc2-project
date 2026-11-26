# Variants of the fence rule: halving saturated lines, restarting a stuck neuron, margins.
import sys
M=0xffffffff
def clamp(v): return max(-128,min(127,v))
class F:
    def __init__(s,seed): s.seed=seed&M or 1
    def rnd(s):
        x=s.seed; x^=(x<<13)&M; x^=x>>17; x^=(x<<5)&M; s.seed=x; return x
    def around(s,r): return (s.rnd()&(2*r-1))-r
def run(seed,ANIMALS=24,ROOM=400,L=16,limit=20000,halve=False,stuck=0,margin=0,wolfrule='min',info=False):
    f=F(seed)
    corners=[(-40,30),(40,34),(4,-44)]
    fx=-1 if f.rnd()&1 else 1; fy=-1 if f.rnd()&1 else 1
    mx=f.around(8); my=f.around(4)
    cx=[fx*c[0]+mx for c in corners]; cy=[fy*c[1]+my for c in corners]
    A=[]; sheep=wolves=0
    while sheep+wolves<ANIMALS:
        x=f.around(64); y=f.around(32)
        if x<-58 or x>58 or y<-23 or y>23: continue
        ins=0; near=0
        for i in range(3):
            n=(i+1)%3
            side=(cx[n]-cx[i])*(2*y-cy[i])-(cy[n]-cy[i])*(x-cx[i])
            if fx*fy>0: side=-side
            if side>0: ins+=1
            if -ROOM<side<ROOM: near=1
        if near or (sheep if ins==3 else wolves)==ANIMALS//2: continue
        A.append((x,y,ins==3))
        if ins==3: sheep+=1
        else: wolves+=1
    def w3(v): return [v&0xff,(v>>8)&0xff,(v>>16)&0xff,0]
    W=[w3(f.rnd()&0x1f1f1f) for _ in range(3)]
    wrongf=[1]*ANIMALS; wrong=ANIMALS; looks=0; since=0; restarts=0
    blame=[0,0,0]; strikes=[0]*ANIMALS
    def look(n,teach):
        x,y,sh=A[n]; X=[x,2*y,64,0]
        s=[sum(a*b for a,b in zip(W[k],X)) for k in range(3)]
        e=[0,0,0]
        if sh:
            for k in range(3):
                if s[k]<=margin: e[k]=L
        elif all(v>-margin for v in s):
            if wolfrule=='c':
                k=0
                if s[1]<s[0]: k=1
                if s[2]<s[k]: k=2
                r=f.rnd()&7
                if r<2:
                    k+=1+r
                    if k>2: k-=3
            elif wolfrule.startswith('rand'):
                p=int(wolfrule[4:])   # of 16: how often a line other than the smallest is taken
                order=sorted(range(3),key=lambda i:s[i])
                r=f.rnd()&15
                k=order[0] if r>=p else order[1+((r>>0)&1)] if wolfrule.endswith('x') else (order[0] if r>=p else order[1+(f.rnd()&1)])
            elif wolfrule.startswith('strike'):
                N=int(wolfrule[6:])
                order=sorted(range(3),key=lambda i:s[i])
                if teach:
                    strikes[n]+=1
                k=order[(strikes[n]//N)%3]
            elif wolfrule=='min':
                k=0
                if s[1]<s[0]: k=1
                if s[2]<s[k]: k=2
            elif wolfrule=='dist':
                d=[(s[i]>>5)*(s[i]>>5)/max(1,W[i][0]*W[i][0]+W[i][1]*W[i][1]) for i in range(3)]
                k=min(range(3),key=lambda i:d[i])
            elif wolfrule=='distint':
                # whole numbers only, as the C program would do it
                n=[max(1,W[i][0]*W[i][0]+W[i][1]*W[i][1]) for i in range(3)]
                q=[((s[i]>>4)*(s[i]>>4))//n[i] for i in range(3)]
                k=0
                if q[1]<q[0]: k=1
                if q[2]<q[k]: k=2
            e[k]=-L
        if teach:
            for k in range(3):
                if e[k]:
                    blame[k]+=1
                    W[k]=[clamp((wi*128+64+e[k]*xi)>>7) for wi,xi in zip(W[k],X)]
                    if halve and any(v>=120 or v<=-120 for v in W[k]):
                        W[k]=[v>>1 if v>=0 else -((-v)>>1) for v in W[k]]
        # wrong for the picture: the fence itself (no margin)
        fence_in=all(v>0 for v in s)
        return int(any(e)), int(fence_in!=sh)
    while looks<limit:
        for i in range(6):
            n=f.rnd()&31
            if n<ANIMALS:
                was=wrongf[n]; wrongf[n]=look(n,True)[0]; wrong+=wrongf[n]-was; looks+=1; since+=1
        if wrong==0:
            for i in range(ANIMALS):
                wrongf[i]=look(i,False)[0]; wrong+=wrongf[i]
            if wrong==0: return (looks,restarts) if info else looks
        if stuck and since>=stuck:
            # the neuron that was corrected the most starts over
            k=max(range(3),key=lambda k:blame[k])
            W[k]=w3(f.rnd()&0x1f1f1f); W[k]=[((b^0x10)-0x10) for b in W[k][:3]]+[0]
            blame=[0,0,0]; since=0; restarts+=1
    return None
def report(name,**kw):
    res=[run(0x2545f491^(k*2654435761&M),**kw) for k in range(300)]
    ok=sorted(r for r in res if r is not None)
    q=lambda p: ok[min(len(ok)-1,int(len(ok)*p))] if ok else None
    print(f"{name:40s} done {len(ok)}/300  median {q(.5)}  90% {q(.9)}  99% {q(.99)} max {ok[-1] if ok else None}")



report("c rule, room 400",wolfrule="c")
report("c rule, room 600",wolfrule="c",ROOM=600)
report("c rule, room 500",wolfrule="c",ROOM=500)
