# Model of `guess`: a neuron predicts a person's next choice (press or wait) from the last six rounds.
# Two inputs for a round: the choice, and the choice times "the chip was right". The chip's learn rule.
import random
def clamp(v,a=-128,b=127): return max(a,min(b,v))
X,GOAL,RATE=16,2048,8
def play(person,rounds,seed):
    r=random.Random(seed); w=[0]*12; wb=0; x=[0]*12; won=0; hist=[]; late=0
    for n in range(rounds):
        s=wb*X+sum(a*b for a,b in zip(w,x)); guess=1 if s>0 else -1
        c=person(hist,r)
        right=guess==c; won+=right; late+=right and n>=rounds//2
        e=clamp(((GOAL if c>0 else -GOAL)-s)>>RATE)
        w=[clamp((wi*128+64+e*xi)>>7) for wi,xi in zip(w,x)]; wb=clamp((wb*128+64+e*X)>>7)
        hist.append((c,right))
        x=[c*X, c*X*(1 if right else -1)]+x[:10]          # two xsh
    return won/rounds, late/(rounds-rounds//2)
people={
 'switches 70% of the time': lambda h,r: (-h[-1][0] if r.random()<.7 else h[-1][0]) if h else 1,
 'repeats 70% of the time':  lambda h,r: (h[-1][0] if r.random()<.7 else -h[-1][0]) if h else 1,
 'pattern press press wait': lambda h,r: [1,1,-1][len(h)%3],
 'changes after being read': lambda h,r: (-h[-1][0] if h[-1][1] else h[-1][0]) if h else 1,
 'truly random':             lambda h,r: r.choice((1,-1)),
}
for name,p in people.items():
    for rounds in (30,80):
        res=[play(p,rounds,s) for s in range(40)]
        print('%-26s %3d rounds: chip right %2.0f%%, in the second half %2.0f%%'%(name,rounds,100*sum(a for a,_ in res)/40,100*sum(b for _,b in res)/40))
