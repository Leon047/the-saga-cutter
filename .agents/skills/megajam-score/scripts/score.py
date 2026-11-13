#!/usr/bin/env python3
"""Epic AI MegaJam 2026 placement estimator.

Usage:
    python score.py assessment.json [--field field_model.json] [--json out.json]

assessment.json  scores for the 7 official criteria (see SKILL.md for the schema)
field_model.json optional override of the assumed competition (defaults below)

Model, in one paragraph: judges give each entry a mean score over the 7 criteria (equal weights, 1-5). Field entries'
mean scores follow a three-tier normal mixture (unfinished / ordinary / strong) calibrated so the top 1% of a ~400
entry pool sits around 4.2-4.6. Every entry, ours included, gets judge noise (panel disagreement + short playtime).
Our own true score is uncertain: each criterion gets a spread that grows as confidence falls, with a shared
component (if we misjudge one criterion we probably misjudge others). Given our observed score x, the number of
field entries beating us is Binomial(n, 1 - F(x)); averaging over samples of x gives P(1st), P(top 3), P(top 10).
Pure standard library; deterministic for a given --seed.
"""
import argparse
import json
import math
import random
import sys

CRITERIA = [
    ("ambition", "Ambition & innovation"),
    ("theme", "Theme"),
    ("fun", "Fun factor"),
    ("gameplay", "Gameplay"),
    ("visuals", "Visuals"),
    ("level_design", "Level design & progression"),
    ("audio", "Audio"),
]

GATES = [
    ("packaged_build", "Packaged development build that launches"),
    ("video", "30-60 s gameplay video"),
    ("itch_submitted", "Uploaded to itch and submitted to the jam"),
    ("assets_listed", "Sourced / pre-jam assets listed"),
    ("no_default_materials", "No DefaultMaterial / grey grid left in game"),
    ("no_foreign_ip", "No trademarks / other people's IP"),
    ("before_deadline", "Submitted before Nov 15 5:59 PM PST"),
]

DEFAULT_FIELD = {
    # Best Overall UE pool size (2025: ~490 entries total incl. UEFN and students). Sampled uniformly.
    "n_main": [300, 480],
    # Field mean-score mixture: [weight, mean, sd]
    "tiers": [[0.25, 1.9, 0.40], [0.60, 2.8, 0.45], [0.15, 3.55, 0.45]],
    # Judge noise on an entry's mean score (panel disagreement, 10-30 min of play)
    "judge_noise": 0.28,
    # Modifier pools (all UE + UEFN entries compete)
    "n_all": [420, 650],
    "solo_share": 0.40,
    "devlog_share": 0.10,
    "devlog_tier": [2.7, 0.7],
    # Visuals scores in the field: entry mean + this extra spread
    "visuals_extra_sd": 0.55,
    # Prize values in USD for expected value
    "prizes": {"main": [7500, 5000, 2500], "best_team": 5000, "solo_driver": 2500,
                "render_royalty": 2500, "glass_box": 2500},
}

SCENARIOS = {
    "equal weights (official)": {},
    "visual-bias judges (visuals x1.6)": {"visuals": 1.6},
}


def ncdf(x, mu=0.0, sd=1.0):
    return 0.5 * (1.0 + math.erf((x - mu) / (sd * math.sqrt(2.0))))


def mix_cdf(x, tiers, extra_sd):
    return sum(w * ncdf(x, m, math.sqrt(s * s + extra_sd * extra_sd)) for w, m, s in tiers)


def binom_cdf(k, n, p):
    """P(Binomial(n, p) <= k), stable for small k and large n."""
    if p <= 0.0:
        return 1.0
    if p >= 1.0:
        return 1.0 if k >= n else 0.0
    total = 0.0
    lq = math.log1p(-p)
    lp = math.log(p)
    for i in range(0, min(k, n) + 1):
        lc = math.lgamma(n + 1) - math.lgamma(i + 1) - math.lgamma(n - i + 1)
        total += math.exp(lc + i * lp + (n - i) * lq)
    return min(total, 1.0)


def crit_sd(conf):
    conf = max(0.0, min(1.0, conf))
    return 0.2 + 0.8 * (1.0 - conf)


def sample_scores(crits, rng, forecast):
    """One sample of our true per-criterion scores."""
    shared = rng.gauss(0, 1)
    out = {}
    for key, _ in CRITERIA:
        c = crits[key]
        base = c["score"]
        if forecast and "planned" in c and rng.random() < c.get("p_lands", 0.5):
            base = c["planned"]
        sd = crit_sd(c.get("confidence", 0.5))
        v = base + sd * (0.7 * shared + 0.714 * rng.gauss(0, 1))
        out[key] = min(5.0, max(1.0, v))
    return out


def wmean(scores, weights):
    tot = sum(weights.get(k, 1.0) for k, _ in CRITERIA)
    return sum(scores[k] * weights.get(k, 1.0) for k, _ in CRITERIA) / tot


def simulate(crits, field, weights, forecast, sims, seed, devlog=None, team_size=1):
    rng = random.Random(seed)
    tiers = field["tiers"]
    jn = field["judge_noise"]
    acc = dict(p1=0.0, p2=0.0, p3=0.0, top3=0.0, top10=0.0, pct=0.0,
               solo=0.0, team=0.0, render=0.0, glass=0.0, mean=0.0)
    for _ in range(sims):
        s = sample_scores(crits, rng, forecast)
        x = wmean(s, weights) + rng.gauss(0, jn)
        acc["mean"] += x
        n = rng.randint(*field["n_main"])
        q = 1.0 - mix_cdf(x, tiers, jn)  # chance a random field entry is scored above us
        c0, c1, c2 = binom_cdf(0, n, q), binom_cdf(1, n, q), binom_cdf(2, n, q)
        acc["p1"] += c0
        acc["p2"] += c1 - c0
        acc["p3"] += c2 - c1
        acc["top3"] += c2
        acc["top10"] += binom_cdf(9, n, q)
        acc["pct"] += 1.0 - q
        n_all = rng.randint(*field["n_all"])
        if team_size <= 1:
            acc["solo"] += binom_cdf(0, int(n_all * field["solo_share"]), q)
        else:
            acc["team"] += binom_cdf(0, int(n_all * (1 - field["solo_share"])), q)
        # Render Royalty: our visuals vs field visuals (entry mean + extra spread)
        v = s["visuals"] + rng.gauss(0, jn * 1.3)
        qv = 1.0 - mix_cdf(v, tiers, math.sqrt((jn * 1.3) ** 2 + field["visuals_extra_sd"] ** 2))
        acc["render"] += binom_cdf(0, n_all, qv)
        if devlog:
            d = devlog["score"] + crit_sd(devlog.get("confidence", 0.5)) * rng.gauss(0, 1) + rng.gauss(0, jn)
            m, sd = field["devlog_tier"]
            qd = 1.0 - ncdf(d, m, math.sqrt(sd * sd + jn * jn))
            acc["glass"] += binom_cdf(0, max(1, int(n_all * field["devlog_share"])), qd)
    return {k: v / sims for k, v in acc.items()}


def expected_cash(r, field):
    p = field["prizes"]
    return (r["p1"] * p["main"][0] + r["p2"] * p["main"][1] + r["p3"] * p["main"][2]
            + r["solo"] * p["solo_driver"] + r["team"] * p["best_team"]
            + r["render"] * p["render_royalty"] + r["glass"] * p["glass_box"])


def pct(v):
    if v < 0.001:
        return "<0.1%"
    return f"{100 * v:.1f}%"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("assessment")
    ap.add_argument("--field")
    ap.add_argument("--json")
    ap.add_argument("--sims", type=int, default=6000)
    ap.add_argument("--seed", type=int, default=2026)
    a = ap.parse_args()
    try:
        sys.stdout.reconfigure(encoding="utf-8")  # Windows consoles default to cp1252
    except AttributeError:
        pass

    with open(a.assessment, encoding="utf-8") as f:
        A = json.load(f)
    field = dict(DEFAULT_FIELD)
    if a.field:
        with open(a.field, encoding="utf-8") as f:
            field.update(json.load(f))

    crits = A["criteria"]
    missing = [k for k, _ in CRITERIA if k not in crits]
    if missing:
        sys.exit(f"assessment is missing criteria: {', '.join(missing)}")
    team = int(A.get("team_size", 1))
    devlog = A.get("devlog")
    has_plan = any("planned" in crits[k] for k, _ in CRITERIA)

    gates = A.get("gates", {})
    failed = [lbl for k, lbl in GATES if gates.get(k) is False]
    pending = [lbl for k, lbl in GATES if gates.get(k) in (None, "planned", "todo")]

    L = []
    L.append(f"# MegaJam 2026 estimate — {A.get('project', 'project')} ({A.get('as_of', '')})\n")
    L.append("## Criteria (1-5)\n")
    L.append("| Criterion | Now | Conf | Planned | p(lands) | Note |")
    L.append("|---|---|---|---|---|---|")
    for k, lbl in CRITERIA:
        c = crits[k]
        L.append(f"| {lbl} | {c['score']:.1f} | {c.get('confidence', 0.5):.1f} | "
                 f"{c.get('planned', '')} | {c.get('p_lands', '')} | {c.get('note', '')} |")
    now_mean = sum(crits[k]["score"] for k, _ in CRITERIA) / 7
    L.append(f"\nMean now: **{now_mean:.2f} / 5** (total {now_mean * 7:.1f} / 35)\n")

    L.append("## Submission gates\n")
    for k, lbl in GATES:
        g = gates.get(k)
        mark = "OK" if g is True else ("FAIL" if g is False else "TODO")
        L.append(f"- [{mark}] {lbl}")
    if failed:
        L.append(f"\n**Disqualified as things stand** ({'; '.join(failed)}). Chances below assume these get fixed.")
    elif pending:
        L.append("\nNot yet done: these are worth zero points but an unmet gate means zero chance.")
    L.append("")

    results = {}
    modes = [("now", False)] + ([("deadline forecast", True)] if has_plan else [])
    for mode, fc in modes:
        L.append(f"## Chances — {mode}\n")
        L.append("| Scenario | Est. percentile | 1st | Top 3 | Top 10 | Modifier | Exp. cash |")
        L.append("|---|---|---|---|---|---|---|")
        for sname, w in SCENARIOS.items():
            r = simulate(crits, field, w, fc, a.sims, a.seed, devlog, team)
            results[f"{mode} / {sname}"] = r
            mod = (f"Solo {pct(r['solo'])}" if team <= 1 else f"Team {pct(r['team'])}")
            mod += f", Render {pct(r['render'])}"
            if devlog:
                mod += f", Glass Box {pct(r['glass'])}"
            L.append(f"| {sname} | {100 * r['pct']:.1f} | {pct(r['p1'])} | {pct(r['top3'])} | "
                     f"{pct(r['top10'])} | {mod} | ${expected_cash(r, field):,.0f} |")
        L.append("")

    # Marginal value: +0.5 on each criterion, same random stream
    fc = has_plan
    base = simulate(crits, field, {}, fc, a.sims, a.seed, devlog, team)
    gains = []
    for k, lbl in CRITERIA:
        c2 = json.loads(json.dumps(crits))
        c2[k]["score"] = min(5.0, c2[k]["score"] + 0.5)
        if "planned" in c2[k]:
            c2[k]["planned"] = min(5.0, c2[k]["planned"] + 0.5)
        r = simulate(c2, field, {}, fc, a.sims, a.seed, devlog, team)
        gains.append((r["top3"] - base["top3"], r["top10"] - base["top10"], r["p1"] - base["p1"], lbl,
                      crits[k]["score"]))
    gains.sort(reverse=True)
    L.append("## Where +0.5 helps most (equal weights" + (", forecast" if fc else "") + ")\n")
    L.append("| Criterion | Now | +1st | +Top 3 | +Top 10 |")
    L.append("|---|---|---|---|---|")
    for g3, g10, g1, lbl, sc in gains:
        L.append(f"| {lbl} | {sc:.1f} | +{100 * g1:.1f} pp | +{100 * g3:.1f} pp | +{100 * g10:.1f} pp |")
    L.append("\nGains are similar per criterion because weights are equal; the cheapest +0.5 to earn wins. "
             "Criteria at 5.0 cannot gain.")
    L.append("\n_Model assumptions (field size, tier mix, judge noise) are estimates from past jams; "
             "treat chances as relative, not exact. Override with --field._")

    print("\n".join(L))
    if a.json:
        with open(a.json, "w", encoding="utf-8") as f:
            json.dump({"results": results, "gates_failed": failed, "gates_pending": pending,
                       "mean_now": now_mean}, f, indent=2)


if __name__ == "__main__":
    main()
