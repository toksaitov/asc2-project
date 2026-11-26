# Model of a neuron that learns to fly the game from its crashes, with the chip's learn rule.
import random, sys
def clamp(v,a=-128,b=127): return max(a,min(b,v))
def learn(w,x,e): return [clamp((wi*128+64+e*xi)>>7) for wi,xi in zip(w,x)]
GR,FLAP,MAXF,GAP,PW,STEP,SPD,EX,EW,EH,GY=2,-20,27,24,10,48,21,20,12,8,62
def flight(w,r,K,E,maxscore=200):
    px=[(128+i*STEP)<<4 for i in range(3)]; gap=[4+r.randrange(16)+r.randrange(16) for _ in range(3)]; passed=[0]*3
    h=(28<<4); v=0; score=0; hist=[]
    while True:
        # the next pillar: the first one whose right edge is not behind the burkut
        nxt=min((i for i in range(3) if (px[i]>>4)+PW>=EX), key=lambda i:px[i])
        y=h>>4
        dy=clamp((y+EH//2)-(gap[nxt]+GAP//2)); 
        x=[dy*2 if abs(dy)<64 else (127 if dy>0 else -127), clamp(v*4), 64, clamp(((px[nxt]>>4)-EX))]
        s=sum(a*b for a,b in zip(w,x)); flap=s>0
        hist.append((x,flap)); hist=hist[-K:]
        if flap: v=FLAP
        v=min(v+GR,MAXF); h+=v
        if h<0: h=0; v=0
        y=h>>4; down=None
        if y+EH>GY: down='low'
        for i in range(3):
            px[i]-=SPD; xx=px[i]>>4
            if xx<-PW: px[i]+=(3*STEP)<<4; xx+=3*STEP; gap[i]=4+r.randrange(16)+r.randrange(16); passed[i]=0
            if not passed[i] and xx+PW<EX: passed[i]=1; score+=1
            if xx<EX+EW-1 and xx+PW>EX+1:
                if y+1<gap[i]: down='high'
                elif y+EH-1>gap[i]+GAP: down='low'
        if down:
            e=E if down=='low' else -E
            for x,f in hist:
                if (down=='low' and not f) or (down=='high' and f) or True:
                    w=learn(w,x,e)
            return score,w,down
        if score>=maxscore: return score,w,None
for K,E in ((6,8),(10,6),(16,4),(8,16)):
    for seed in range(4):
        r=random.Random(seed); w=[0,0,0,0]; out=[]
        for n in range(60):
            s,w,d=flight(w,r,K,E); out.append(s)
        print(K,E,seed,' '.join(str(s) for s in out[:40]),'| w',w)
