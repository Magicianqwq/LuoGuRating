# -*- coding: utf-8 -*-
import json, os, statistics, math, collections
root = r"D:\STUDY\development\code\LuoGuRating\data"
ds = json.load(open(os.path.join(root,"difficulty_scores.json"), encoding="utf-8-sig"))
base = ds["tierBase"]; sc = ds["scores"]
info = {}
for f in ["problems.jsonl","problems_remote_CF.jsonl","problems_remote_AT.jsonl","problems_remote_UVA.jsonl","problems_remote_SP.jsonl"]:
    p=os.path.join(root,f)
    if os.path.exists(p):
        for line in open(p,encoding="utf-8"):
            if line.strip(): o=json.loads(line); o["_r"]=(f!="problems.jsonl"); info[o["pid"]]=o

def rank(xs):
    idx=sorted(range(len(xs)), key=lambda i: xs[i]); r=[0]*len(xs)
    i=0
    while i<len(idx):
        j=i
        while j+1<len(idx) and xs[idx[j+1]]==xs[idx[i]]: j+=1
        avg=(i+j)/2+1
        for k in range(i,j+1): r[idx[k]]=avg
        i=j+1
    return r
def spearman(a,b):
    ra,rb=rank(a),rank(b); n=len(a)
    ma,mb=statistics.mean(ra),statistics.mean(rb)
    num=sum((x-ma)*(y-mb) for x,y in zip(ra,rb))
    den=math.sqrt(sum((x-ma)**2 for x in ra)*sum((y-mb)**2 for y in rb))
    return num/den if den else 0

out=[]
for d in range(9):
    arr=[(sc[o["pid"]], (o.get("totalAccepted") or 0)/max(o.get("totalSubmit") or 1,1)) for o in info.values()
         if not o["_r"] and (o.get("difficulty") or 0)==d and o["pid"] in sc]
    if len(arr)<5: continue
    out.append("d%d n=%d spearman(rate,score)=%+.3f  min=%d max=%d" % (d,len(arr),spearman([a[1] for a in arr],[a[0] for a in arr]),min(a[0] for a in arr),max(a[0] for a in arr)))
out.append("")
out.append("== 每档最低/最高的通过率与分数 ==")
for d in [1,3,5,7,8]:
    arr=[(sc[o["pid"]], (o.get("totalAccepted") or 0)/max(o.get("totalSubmit") or 1,1), o["pid"]) for o in info.values()
         if not o["_r"] and (o.get("difficulty") or 0)==d and o["pid"] in sc]
    arr.sort()
    out.append("d%d 最高分: %s %d (rate %.1f%%) | 最低分: %s %d (rate %.1f%%)" % (
        d, arr[-1][2], arr[-1][0], arr[-1][1]*100, arr[0][2], arr[0][0], arr[0][1]*100))
out.append("")
out.append("== 全库分数直方图（每 400 分一档）==")
h=collections.Counter(min(10, sc[p]//400) for p in sc)
for k in sorted(h): out.append("%4d-%4d : %d" % (k*400, k*400+399, h[k]))
open(r"D:\STUDY\development\code\LuoGuRating\tmp\verify.txt","w",encoding="utf-8").write("\n".join(out))
