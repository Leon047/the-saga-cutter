# Anchored rubric (1–5, half points allowed)

Score as a **tired judge who plays 10 minutes of a jam game after watching its 60-second video**, not as the
developer. A 3 is "a decent jam game": that is the field median for finished entries, not a bad mark. Sourced
(Fab / marketplace / pre-jam) assets earn nothing on their own — score only what the team made or composed during the
jam, plus how well the sourced parts are put together.

Each anchor lists what you must **see** to give it. If you cannot see it (no build runs, no capture), score the
evidence you have and lower `confidence`.

## 1. Ambition & innovation
| | Anchor |
|---|---|
| 1 | Template or tutorial game with assets swapped in |
| 2 | Familiar genre, small scope, nothing new |
| 3 | Familiar genre with one twist, or a scope a bit above usual for 4 days |
| 4 | A mechanic most players have not seen, or clearly large scope (sim with many systems, many units, a crafted world) that works |
| 5 | Judges think "how was this done in 4 days?": novel core mechanic **and** scope/visual boldness, all working |
Evidence: count of distinct working systems, novelty of the core verb, map size/density, AI-assisted scale.

## 2. Theme ("Director's Cut")
| | Anchor |
|---|---|
| 1 | Theme absent or only in the title |
| 2 | Cosmetic: film props, a clapperboard, a menu called "cut" |
| 3 | Theme in the story or framing but the mechanics would be the same without it |
| 4 | A core mechanic is editing / choosing what makes the cut (re-cut events, swap takes, trim the timeline) |
| 5 | The core loop *is* directing/editing, the reading is fresh (not "you're on a movie set"), and the game says so in its first minute |
Evidence: is the theme mechanic used every minute or once? Is it explained in-game? Does the video show it?

## 3. Fun factor
| | Anchor |
|---|---|
| 1 | Can't play it, or nothing to do |
| 2 | Playable but aimless or tedious; quit in 2 minutes |
| 3 | A goal and a working loop; some tension; you'd finish a short run |
| 4 | Clear goals, meaningful choices, rising pressure, a satisfying end; you'd replay once |
| 5 | Hard to put down; strong moment-to-moment feedback and a memorable climax |
Evidence: a real playthrough (or autoplay log): time to first meaningful decision, failure/win states, pacing.

## 4. Gameplay
| | Anchor |
|---|---|
| 1 | Broken controls or core loop |
| 2 | Works but clunky; unclear feedback; bugs block progress sometimes |
| 3 | Solid controls and rules, readable feedback, few bugs |
| 4 | Mechanics interlock (decisions in one system matter in another), responsive, tuned |
| 5 | Original and juicy: tight feel, depth, no rough edges seen |
Evidence: number of interacting systems, input responsiveness, HUD clarity, crash/blocker list.

## 5. Visuals
| | Anchor |
|---|---|
| 1 | Default materials, grey boxes, broken lighting |
| 2 | Mixed asset styles, flat lighting, clutter |
| 3 | Consistent, readable, decent lighting; looks like a jam game |
| 4 | Distinct art direction, good lighting/fog/post, readable silhouettes, cohesive palette |
| 5 | Screenshot-worthy; a judge would put it in the highlight reel |
Evidence: screenshots from the gameplay camera (not the editor), style consistency between custom and sourced
assets, lighting, VFX, UI look.

## 6. Level design & progression
| | Anchor |
|---|---|
| 1 | No level structure; nothing guides the player |
| 2 | One space, little guidance, no progression |
| 3 | Space supports the loop; a basic difficulty curve or phases |
| 4 | Layout creates decisions (chokepoints, risk/reward areas); clear phases/escalation; good onboarding |
| 5 | Space and progression teach, escalate and pay off; every area has a purpose |
Evidence: tutorial/onboarding, days/waves/phases, how the map funnels threats and choices.

## 7. Audio
| | Anchor |
|---|---|
| 1 | Silent |
| 2 | Music only, or a few mismatched SFX |
| 3 | Music + core SFX (actions, UI, threats) that fit |
| 4 | Layered: ambience, reactive music or stingers, SFX for most actions, mixed levels |
| 5 | Audio carries mood and feedback; adaptive music, spatial ambience, maybe VO |
Evidence: list sound cues in source/content, play and listen to the capture.

## Confidence
Per criterion, `confidence` in 0–1:
- 0.9: scored from a fresh playthrough of the packaged build (or a gameplay capture made this session).
- 0.6: scored from code/content review plus older screenshots or videos.
- 0.3: scored mostly from the design doc / intent.
The calculator widens its uncertainty when confidence is low.

## Late-jam forecast (optional)
For each criterion you may also give `planned` (the score you expect if the planned work lands) and `p_lands`
(chance it lands before the deadline, given hours left). The calculator then reports both "as of now" and
"forecast at deadline".
