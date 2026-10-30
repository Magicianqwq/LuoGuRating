# -*- coding: utf-8 -*-
import json, os, math, statistics, shutil, time, collections

HERE = os.path.dirname(os.path.abspath(__file__))
PROJ = os.path.dirname(HERE)
root = os.path.join(PROJ, "data")
score_path = os.path.join(root, "difficulty_scores.json")
old = json.load(open(score_path, encoding="utf-8-sig"))
base = old["tierBase"]
old_scores = old["scores"]

info = {}
for f in ["problems.jsonl","problems_remote_CF.jsonl","problems_remote_AT.jsonl",
          "problems_remote_UVA.jsonl","problems_remote_SP.jsonl"]:
    p = os.path.join(root, f)
    if not os.path.exists(p): continue
    for line in open(p, encoding="utf-8"):
        if line.strip():
            o = json.loads(line)
            o["_remote"] = (f != "problems.jsonl")
            info[o["pid"]] = o

def rate_of(o):
    s = o.get("totalSubmit") or 0
    a = o.get("totalAccepted") or 0
    if s <= 0: return None
    a = min(a, s)
    return (a + 0.5) / (s + 1.0)      # 平滑，避免 logit(0) / logit(1)

def logit(r):
    return math.log(r / (1 - r))

# ---------- 主库：按档位公式 ----------
prim = [o for o in info.values() if not o["_remote"]]
groups = collections.defaultdict(list)
for o in prim:
    d = o.get("difficulty") or 0
    if d < 0 or d > 8: d = 0
    r = rate_of(o)
    if r is None: continue
    groups[d].append((logit(r), o))

zmap = {}       # pid -> z
pctmap = {}     # pid -> 档内分位（0=最难）
for d, arr in groups.items():
    arr.sort(key=lambda t: t[0])
    n = len(arr)
    vals = [t[0] for t in arr]
    mu = statistics.mean(vals)
    sd = statistics.pstdev(vals) or 1.0
    for i, (lv, o) in enumerate(arr):
        zmap[o["pid"]] = max(-1.0, min(1.0, (lv - mu) / sd))
        pctmap[o["pid"]] = i / (n - 1) if n > 1 else 0.5   # 0=最难, 1=最易（arr 按通过率升序，i=0 最难）

def shrink(score, anchor, sub):
    c = min(1.0, math.sqrt(max(sub, 0)) / 40.0)
    return anchor + (score - anchor) * c

new_scores = {}
for o in prim:
    pid = o["pid"]; d = o.get("difficulty") or 0
    if d < 0 or d > 8: d = 0
    sub = o.get("totalSubmit") or 0
    if pid not in zmap:                      # 没提交量
        new_scores[pid] = base[d] if d else 800
        continue
    p = pctmap[pid]; z = zmap[pid]
    if d == 8:
        sc = 3200 + 800 * (1 - p); anchor = 3200
    elif d == 7:
        if p < 0.25:
            sc = 3200 + 800 * (0.25 - p) / 0.25; anchor = 3200
        else:
            sc = min(3199, base[7] + 180 * (-z)); anchor = base[7]
    elif d == 0:
        sc = 800 + 180 * (-z); anchor = 800
    else:
        sc = base[d] + 180 * (-z); anchor = base[d]
    sc = shrink(sc, anchor, sub)
    sc = int(round(sc))
    if d == 8 or (d == 7 and p < 0.25):
        sc = max(3200, min(4000, sc))
    new_scores[pid] = max(200, min(4000, sc))

# ---------- 远程库：用主库校准的 rate->score 曲线 ----------
cal = sorted(((rate_of(o), new_scores[o["pid"]]) for o in prim if o["pid"] in new_scores),
             key=lambda t: t[0])
# 分箱取中位，再强制单调不增（通过率越高分越低）
nb = 60
bins = []
for k in range(nb):
    lo = int(len(cal) * k / nb); hi = int(len(cal) * (k + 1) / nb)
    chunk = cal[lo:hi]
    if not chunk: continue
    bins.append((statistics.median([c[0] for c in chunk]), statistics.median([c[1] for c in chunk])))
xs = [b[0] for b in bins]; ys = [b[1] for b in bins]
for i in range(1, len(ys)):                 # 单调化
    if ys[i] > ys[i-1]: ys[i] = ys[i-1]

def curve(r):
    if r <= xs[0]: return ys[0]
    if r >= xs[-1]: return ys[-1]
    for i in range(1, len(xs)):
        if r <= xs[i]:
            t = (r - xs[i-1]) / (xs[i] - xs[i-1]) if xs[i] > xs[i-1] else 0
            return ys[i-1] + (ys[i] - ys[i-1]) * t
    return ys[-1]

for o in info.values():
    if not o["_remote"]: continue
    r = rate_of(o); sub = o.get("totalSubmit") or 0
    if r is None:
        new_scores[o["pid"]] = 800
        continue
    sc = curve(r)
    sc = shrink(sc, 1200, sub)
    new_scores[o["pid"]] = max(200, min(4000, int(round(sc))))

# ---------- 手改项 ----------
manual = old.get("manualOverrides", [])
applied = []
for m in manual:
    pid = m.get("pid")
    if pid in new_scores:
        new_scores[pid] = int(m.get("score", new_scores[pid]))
        applied.append(pid)

# ---------- 备份 + 写回 ----------
stamp = time.strftime("%Y%m%d-%H%M%S")
bak = os.path.join(root, "difficulty_scores.backup-%s.json" % stamp)
shutil.copy2(score_path, bak)

out = collections.OrderedDict()
out["generated"] = time.strftime("%Y-%m-%d %H:%M")
out["note"] = ("按通过率生成：同档内用 logit(通过率) 的 z 分数，分数 = 档位基准 + 180*(-z)，同档最多拉开 360 分；"
               "省选/NOI- 中通过率最低的 25% 抬到 3200~4000 连续区间；NOI/NOI+/CTSC 固定 3200~4000 且不低于 3200；"
               "未评定按普及- 基准估计；最后按提交量 sqrt(submit)/40 的置信度向基准收缩。远程题库（CF/AT/SP/UVA）"
               "的 difficulty 语义与主库不同，改用主库校准的 通过率->分数 曲线插值。手改项最后覆盖。")
out["tierBase"] = base
out["problemCount"] = len(new_scores)
out["manualOverrides"] = manual
out["scores"] = collections.OrderedDict(sorted(new_scores.items()))
json.dump(out, open(score_path, "w", encoding="utf-8"), ensure_ascii=False, indent=0)

# ---------- 统计，写文件供读取 ----------
def tier_of(pid, sc):
    d = (info[pid].get("difficulty") or 0)
    if d < 0 or d > 8: d = 0
    if d < 8 and sc >= base[8]: d = 8
    return d

lines = []
lines.append("backup: %s" % os.path.basename(bak))
lines.append("scores: %d  (manual applied: %s)" % (len(new_scores), ",".join(applied) or "-"))
lines.append("")
lines.append("== 主库按洛谷档位（新分）==")
for d in range(9):
    arr = [new_scores[o["pid"]] for o in prim if (o.get("difficulty") or 0) == d and o["pid"] in new_scores]
    if not arr: continue
    lines.append("d%d base=%d n=%d min=%d med=%d max=%d mean=%.0f" % (
        d, base[d], len(arr), min(arr), statistics.median(arr), max(arr), statistics.mean(arr)))
lines.append("")
lines.append("== 主库分档差异幅度（|score-base| 分布）==")
for d in range(1, 7):
    arr = [abs(new_scores[o["pid"]] - base[d]) for o in prim if (o.get("difficulty") or 0) == d and o["pid"] in new_scores]
    if not arr: continue
    arr.sort()
    near = sum(1 for x in arr if x <= 50) / len(arr) * 100
    lines.append("d%d  n=%d  |dev|<=50: %.1f%%  p90=%d  max=%d" % (d, len(arr), near, arr[int(len(arr)*0.9)], arr[-1]))
lines.append("")
lines.append("== 变化最大的题（旧 -> 新）==")
delta = []
for pid, sc in new_scores.items():
    if pid in old_scores:
        delta.append((sc - old_scores[pid], pid, old_scores[pid], sc))
delta.sort()
for dd, pid, a, b in delta[:12]:
    o = info.get(pid, {})
    print_ = "%-8s %5d -> %5d (%+5d)  sub=%s  %s" % (pid, a, b, dd, o.get("totalSubmit"), (o.get("title") or "")[:28])
    lines.append(print_)
lines.append("...")
for dd, pid, a, b in delta[-8:]:
    o = info.get(pid, {})
    lines.append("%-8s %5d -> %5d (%+5d)  sub=%s  %s" % (pid, a, b, dd, o.get("totalSubmit"), (o.get("title") or "")[:28]))
lines.append("")
lines.append("== 抽查 ==")
for pid in ["P1001","P1002","P1003","P1080","P2293","B3611","P5162","P5326","P8990","P4725","P4550","P1012","P1090"]:
    if pid in new_scores:
        o = info.get(pid, {})
        r = rate_of(o)
        lines.append("%-8s old=%4s new=%4s d=%s rate=%s sub=%s %s" % (
            pid, old_scores.get(pid), new_scores[pid], o.get("difficulty"),
            ("%.1f%%" % (r*100)) if r else "-", o.get("totalSubmit"), (o.get("title") or "")[:26]))
os.makedirs(os.path.join(PROJ, "tmp"), exist_ok=True)
open(os.path.join(PROJ, "tmp", "gen_summary.txt"), "w", encoding="utf-8").write("\n".join(lines))
