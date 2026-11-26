# Model: three neurons (one dotm, one learn) move three lines until they fence in the sheep.
import random
def clamp(v,a=-128,b=127): return max(a,min(b,v))
def run(seed,E=6,steps=4000):
    r=random.Random(seed)
    # sheep inside a triangle of the field, wolves outside; the field is 64 by 64 around 0
    tri=[(-40,30),(40,34),(4,-44)]
    def inside(p):
        s=[(tri[(i+1)%3][0]-tri[i][0])*(p[1]-tri[i][1])-(tri[(i+1)%3][1]-tri[i][1])*(p[0]-tri[i][0]) for i in range(3)]
        return all(v>0 for v in s) or all(v<0 for v in s)
    pts=[]
    while len(pts)<40:
        p=(r.randrange(-60,61),r.randrange(-60,61))
        # keep a little room between the two kinds
        q=[(p[0]+dx,p[1]+dy) for dx in(-5,5) for dy in(-5,5)]
        if len(set(inside(t) for t in q+[p]))==1: pts.append((p,inside(p)))
    w=[[r.randrange(-20,21),r.randrange(-20,21),r.randrange(-20,21),0] for _ in range(3)]
    hist=[]
    for n in range(steps):
        p,ins=pts[r.randrange(len(pts))]
        x=[p[0],p[1],64,0]
        s=[sum(a*b for a,b in zip(w[k],x)) for k in range(3)]       # dotm
        e=[0,0,0]
        if ins:
            for k in range(3):
                if s[k]<=0: e[k]=E                                    # every line must keep a sheep in
        elif all(v>0 for v in s):
            k=min(range(3),key=lambda k:s[k]); e[k]=-E                # the nearest line moves to shut a wolf out
        for k in range(3):
            if e[k]: w[k]=[clamp((wi*128+64+e[k]*xi)>>7) for wi,xi in zip(w[k],x)]   # learn
        if n%100==99:
            wrong=sum(1 for p,ins in pts if all(sum(a*b for a,b in zip(w[k],[p[0],p[1],64,0]))>0 for k in range(3))!=ins)
            hist.append(wrong)
    return hist
for E in (4,8,16):
    for seed in range(6):
        h=run(seed,E); print(E,seed,' '.join(map(str,h[:30])))
