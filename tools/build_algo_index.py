# -*- coding: utf-8 -*-
"""生成 data/algorithm_index.jsonl

输入：
  data_problems.jsonl          洛谷主题库/入门题（pid, name, difficulty, tags=[标签id]）
  data_tags.json               洛谷标签树（id, name, parent）
  curriculum/curriculum.json   45 个算法专题 + 每专题 7 档难度各 10 道推荐题
  data/difficulty_scores.json  每道题的参考分

输出（一行一条，方便 C++ 逐行解析）：
  {"k":"h","v":1,...}
  {"k":"topic","id":"moni","n":"模拟","ch":"基础算法","tg":[1],"recs":[[pid,score,diff,name],...]}
  {"k":"p","pid":"P1001","tg":[1,3]}
"""
import json, os, sys, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "data", "algorithm_index.jsonl")

def load(p):
    with open(os.path.join(ROOT, p), encoding="utf-8-sig") as f:
        return json.load(f)

def main():
    cur = load("curriculum/curriculum.json")
    tags = load("data_tags.json")["tags"]
    scores = load("data/difficulty_scores.json")["scores"]

    children = {}
    for t in tags:
        children.setdefault(t.get("parent"), []).append(t["id"])

    def with_children(ids):
        out, stack = set(), list(ids)
        while stack:
            i = stack.pop()
            if i in out:
                continue
            out.add(i)
            stack.extend(children.get(i, []))
        return sorted(out)

    # 题目 -> 标签
    prob_tags = {}
    with open(os.path.join(ROOT, "data_problems.jsonl"), encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            o = json.loads(line)
            pid = o.get("pid")
            if pid:
                prob_tags[pid] = [int(x) for x in (o.get("tags") or [])]

    topics = []
    for t in cur["topics"]:
        recs, seen = [], set()
        for lv in t.get("levels", []):
            d = lv.get("difficulty", 0)
            for it in lv.get("items", []):
                pid = it.get("pid")
                if not pid or pid in seen:
                    continue
                seen.add(pid)
                sc = it.get("score")
                if sc is None:
                    sc = scores.get(pid, 0)
                recs.append([pid, int(sc), int(d), it.get("name", "")])
        topics.append({
            "k": "topic",
            "id": t["id"],
            "n": t["name"],
            "ch": t.get("chapterName", ""),
            "tg": with_children(t.get("tags") or []),
            "recs": recs,
        })

    lines = [json.dumps({"k": "h", "v": 1, "generated": time.strftime("%Y-%m-%d"),
                         "topics": len(topics), "problems": len(prob_tags),
                         "note": "P/B 题库标签 + 45 专题推荐题"}, ensure_ascii=False)]
    for t in topics:
        lines.append(json.dumps(t, ensure_ascii=False, separators=(",", ":")))
    for pid, tg in prob_tags.items():
        lines.append(json.dumps({"k": "p", "pid": pid, "tg": tg},
                                ensure_ascii=False, separators=(",", ":")))

    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")

    size = os.path.getsize(OUT)
    rec_total = sum(len(t["recs"]) for t in topics)
    print("topics=%d problems=%d recs=%d size=%.1fKB" %
          (len(topics), len(prob_tags), rec_total, size / 1024.0))

if __name__ == "__main__":
    main()
