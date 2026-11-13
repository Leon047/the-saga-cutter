---
name: megajam-score
description: Estimate how the current project would score in the Epic AI MegaJam 2026 (theme "Director's Cut", 7 criteria judged 1-5, Best Overall UE top 3 = $7.5k/$5k/$2.5k) and calculate its chances of 1st place, top 3, top 10 and modifier prizes (Solo Driver, Best Team, Render Royalty, Glass Box). Gathers evidence from the repo, captures and builds, scores against an anchored rubric, checks the submission gates that disqualify, runs a placement model, and ranks what to improve before the deadline. Use when the user asks "how are we doing for the jam", "score the game against the jam criteria", "what are our chances", "what should we fix to win", "megajam score" or similar.
argument-hint: "[quick | full] [notes: what changed, hours left, team size...]"
---

# megajam-score

Judge the project the way an Epic MegaJam judge would, then turn the scores into placement chances and a to-do list
ranked by points per hour.

Files:
- `reference/jam-facts.md` — the 2026 rules, criteria, prizes, deadline, submission gates, past winners and what
  judges reward. **Read it first** each run (facts may have changed: re-check the itch page if the user says so).
- `reference/rubric.md` — 1-5 anchors for each criterion and how to set confidence.
- `scripts/collect_evidence.sh` — objective facts (time left, commits, code size, audio/VFX/UI counts, builds,
  videos, devlog, theme keywords).
- `scripts/score.py` — placement model (stdlib Python, no installs).

`quick` (default when time is short): repo evidence + existing captures only. `full`: also play the game (packaged
build if one exists, else PIE or the ue-demo-video `playthrough` mode) and capture fresh stills before scoring.

## Steps

1. **Facts.** Read `reference/jam-facts.md` and `reference/rubric.md`. Note hours left to the deadline.
2. **Evidence.** Run `bash .claude/skills/megajam-score/scripts/collect_evidence.sh` from the repo root. Then read:
   the design brief (e.g. `GAME_PROMPT.md`), README, the main game-mode / gameplay source, and look at the newest
   screenshots and video stills (extract a few frames with ffmpeg: `ffmpeg -ss 5 -i x.mp4 -frames:v 1 out.png`).
   Write findings into the scratchpad, not the repo.
   - In `full` mode: play or autoplay one full run; time the first minute (does the theme mechanic show up and get
     explained?), note bugs, win/lose states, audio heard.
   - Separate **team-made** work from **sourced** assets (Fab packs, marketplace, pre-jam). Sourced assets earn
     nothing on their own; how well they are composed and lit still counts toward Visuals.
3. **Score.** For each of the 7 criteria pick a score from the rubric anchors (half points OK), a confidence, and a
   one-line evidence note. Be a stern outside judge: a 3 is a decent finished jam game. Where real work is planned and
   hours remain, add `planned` + `p_lands`. Also score the dev log if one exists or is planned (Glass Box).
4. **Gates.** Fill the submission gates honestly: `true`, `false`, or `"todo"`. A missing packaged build or video is
   a zero no matter how good the game is.
5. **Calculate.** Write the assessment JSON to the scratchpad and run
   `python .claude/skills/megajam-score/scripts/score.py <assessment.json>`. It prints criteria, gates, chances "now"
   and "deadline forecast" under two judge scenarios (official equal weights, and the visual-bias pattern seen in past
   jams), expected cash, and which criterion gains most from +0.5.
6. **Advise.** Turn the gain table into a ranked to-do list: for each candidate fix estimate hours and which
   criteria it lifts (one fix can lift several: e.g. an in-game theme intro lifts Theme and Level design/onboarding),
   and rank by expected gain per hour against the hours left. Always put unmet gates first, then "first 60 seconds"
   fixes (judges play briefly), then audio if below 3 (cheap points), then the rest.
7. **Report.** Give the user: the score table, gates, chances (1st / top 3 / top 10 / best modifier), the top 3-5
   actions with hours, and the model caveat. Save the report to `Docs/MegaJam/score-<date>.md` only if the user asks.

## Assessment JSON

```json
{
  "project": "The Saga Cutter",
  "as_of": "2026-11-13 18:00 UTC",
  "team_size": 1,
  "gates": {"packaged_build": "todo", "video": true, "itch_submitted": "todo", "assets_listed": "todo",
            "no_default_materials": true, "no_foreign_ip": true, "before_deadline": "todo"},
  "devlog": {"score": 3.5, "confidence": 0.4},
  "criteria": {
    "ambition":     {"score": 3.5, "confidence": 0.6, "note": "...", "planned": 4.0, "p_lands": 0.6},
    "theme":        {"score": 3.5, "confidence": 0.6, "note": "..."},
    "fun":          {"score": 3.0, "confidence": 0.4, "note": "..."},
    "gameplay":     {"score": 3.0, "confidence": 0.5, "note": "..."},
    "visuals":      {"score": 3.5, "confidence": 0.7, "note": "..."},
    "level_design": {"score": 3.0, "confidence": 0.5, "note": "..."},
    "audio":        {"score": 2.5, "confidence": 0.7, "note": "..."}
  }
}
```

`devlog` is optional (omit if there is none and none planned). `--field field.json` overrides the competition
model (keys in `DEFAULT_FIELD` in score.py: pool sizes, tier mix, judge noise, prize values); use it if the real
entry count on the itch page differs a lot (2026 page showed 32 entries on day 2; final counts land late).

## How to read the numbers

- Mean ≈ 3.0 → around the field median; ≈ 3.7 → top 10% ; ≈ 4.2+ → top-3 contention (top 1% of ~400).
- The model is calibrated on past jams, not on real 2026 scores (Epic never publishes them). Use it to compare
  "before vs after a fix" and to rank work, not as an exact probability.
- Equal-weight criteria mean the cheapest half point anywhere is worth as much as the hardest; the advice step is
  where effort matters.
