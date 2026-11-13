---
name: ue-demo-video
description: Record a demo / showcase / flythrough video of an Unreal level (TheSagaCutter or another UE 5.9 project driven over unreal-mcp). Default mode builds a camera Level Sequence through chosen spots, renders it with Movie Render Queue, encodes an mp4 with ffmpeg and reviews stills for black frames and camera clipping. With the argument `playthrough` it instead records the game playing itself with the normal in-game camera and HUD and cuts a gameplay-highlights reel. Use when the user asks to "record a demo video", "make a flythrough", "render a video of the map", "show the props in a video", "gameplay video", "gameplay highlights", "trailer of the game being played" or similar. Flythrough mode needs the ue-editor skill for starting the editor.
---

# ue-demo-video

**Mode from the arguments:** if the first word is `playthrough` (or the user asks for gameplay / highlights / "as in
the game"), follow **Playthrough mode** below and skip the flythrough steps. Otherwise follow the flythrough **Steps**.
The rest of the argument text is notes (scene takes, build order, length, music) for that mode.

Flythrough: camera flythrough → MRQ PNG frames → ffmpeg mp4 → contact-sheet review. Helpers:

- `scripts/ue_demo.py` (runs **inside** the editor): `look_at`, `build_flythrough`, `set_shot`, `render`, `output_dir`.
- `scripts/wait_and_encode.sh` (runs in Bash): waits for all frames, encodes, warns on tiny (black) output, writes a
  contact sheet.

Playthrough: standalone game plays itself → one PNG per frame → pick highlight segments from the event log →
ffmpeg reel. Helpers (Bash, no editor needed):

- `scripts/playthrough.sh`: records a whole self-playing run, writes `events.txt` (frame → game event), `full.mp4`,
  `contact.png`.
- `scripts/cut_highlights.sh`: joins chosen `start:len` frame segments with short dips to black, adds music.

Skill dir: `.agents/skills/ue-demo-video` in the repo root (one level above `UE/`).

## Playthrough mode (gameplay highlights)

The camera is the game's own: `ATSCColonyCameraPawn`, fixed isometric view (pitch -36, yaw -45, FOV 28, arm 16500),
placed over the clearing by `ATSCGameMode::Tick`, which frames the whole settlement, so it is never moved. Do **not**
build a Level Sequence or use MRQ here: MRQ renders through a sequence camera and does not draw the AHUD canvas, and
the HUD (resource bar, job panel, CUT! timeline, banners, toasts, epilogue) is half of what gameplay looks like.

1. **Pick the run.** `ATSCGameMode::TSCAuto <takes> <build> <speed>` (Exec) plays the game: it starts from the title,
   presses CUT! and picks the Ashen take for every `A` in `takes` (one L/A per scene, in order), builds the `build`
   letters (H house, M sawmill, E healer, S hammer shrine, T stag shrine) as soon as affordable, and sets game speed.
   Default `LALA HM 4`: both sagas get shown, one construction plus a sawmill, brisk pace. Use the user's notes if
   they give takes or a build order. Read `AutoPlay` in `TSCGameMode.cpp` if the rules may have changed.
2. **Record** (background, ~10-15 min of wall time; the game renders ~10 fps while dumping):
   ```bash
   bash .agents/skills/ue-demo-video/scripts/playthrough.sh <JobName> LALA HM 4
   ```
   It runs `UnrealEditor.exe <uproject> <map> -game -windowed -ResX=1280 -ResY=720 -benchmark -fps=30 -dumpmovie
   -ExecCmds="TSCAuto ..."`: `-benchmark -fps=30` fixes the game clock at 30 fps whatever the render speed, so the
   frames play back in real time; `-dumpmovie` writes `Saved/Screenshots/WindowsEditor/MovieFrame#####.png` each
   frame, HUD included. Use a fresh job name and keep the screenshot folder clear of other capture runs.
   The script stops its own game process `tail_frames` (240) after the first `[Epilogue]` line and moves everything to
   `UE/Saved/MovieRenders/<JobName>/` (`frames/`, `events.txt`, `full.mp4`, `contact.png`, `game.log`).
   Additional Unreal launch flags can follow the optional map and tail frame count. If Unreal cannot write to its
   default cache or shader directories, create writable directories under `UE/Saved` and pass
   `-DDC=(ProjectPak,EnginePak=InstalledEnginePak,Local)`, `-LocalDataCachePath=<absolute cache path>`, and
   `-ShaderWorkingDir=<absolute shader path>`. Use `-unattended -RenderOffscreen` for a background capture.
3. **Choose highlights** from `events.txt` (`<frame> [Saga] ...`; the frame is the engine frame and matches the
   MovieFrame index to within a few frames) and `contact.png`. 30 s = 900 frames, 6-8 segments of 90-180 frames,
   in game order so the story reads: title → start, a crisis, a CUT! edit, building, the night, the epilogue.
   TheSagaCutter recipe (frame = the event line's number):
   - opening: `5:75` (day 1 banner and HUD; frame 0 is black and autoplay leaves the title screen on frame 1, so the
     title screen is not in the capture)
   - a construction: `"The house stands"` - 90 : 120 (tiles rising, then settlers arrive)
   - each scene you want: `"Scene N starts"` + 20 for ~120 frames (the alert banner and the threat); for an Ashen take
     `"EDIT:"` - 30 : 150 (the alert with a red CUT! button, then the world flips to ash with embers and the
     "THE ASHEN SAGA - ..." title - the signature shot). Autoplay presses CUT!, picks and confirms in one tick, so the
     CUT! timeline panel never appears in a capture
   - the night: the `Music: SC_Action` line (night start) + 60 : 180, then a later slice of the gate fight (tracers,
     HP bars) chosen from the contact sheet
   - ending: the first `[Epilogue]` line - 10 : 120 (the skald's text)
   Scene events that resolve LIVING without drama (the timer just runs out) are the first to drop.
4. **Cut** (the music is the game's own; main theme for day-heavy reels, action for night-heavy):
   ```bash
   bash .agents/skills/ue-demo-video/scripts/cut_highlights.sh "<ProjectDir>/UE/Saved/MovieRenders/<JobName>/frames" "<ProjectDir>/<video>.mp4" -m Docs/Audio/Music/main_theme_take1.mp3 0:75 250:120 ...
   ```
   `-f 0` gives hard cuts instead of 5-frame dips to black.
5. **Review** `<video>.contact.png` (one row per segment, 4 frames each) with the image viewer. Each row should show one readable
   moment: banner text not cut mid-sentence, the ash flip and its title in edit segments, no black first frame, no row
   of nearly identical idle frames (shift or drop that segment; the epilogue row is static text by design). Inspect a single frame with
   `ffmpeg -start_number <n> -i frames/MovieFrame%05d.png -frames:v 1 x.png`. Re-cut (seconds,
   no re-record); re-record only for a different run (takes, build order, speed).
6. Share the rendered MP4 in the final response and report the run (takes, build, outcome of the night) and the
   segments used. Do not commit unless asked; frames stay in gitignored `UE/Saved/`.

Playthrough gotchas (hit in practice):
- Git Bash rewrites a `/Game/...` argument into `C:/Program Files/Git/Game/...` → "Failed to enter" map. The script
  sets `MSYS_NO_PATHCONV=1`; do the same for any manual launch.
- The log's `[frame]` column is `GFrameCounter % 1000`; the script unwraps it while recording. Do not read raw log
  frame numbers as MovieFrame indices.
- No `[Saga] Autoplay` line in `game.log` = `TSCAuto` never ran (the game sits on the title screen): check the Exec
  name and the `-ExecCmds` quoting.
- `-dumpmovie` has no audio; the music comes from `Docs/Audio/Music` in the cut step.

## Steps (flythrough mode)

1. **Editor.** Follow the `ue-editor` skill: `ue_status`, else `ue_start` with `offscreen: true` (MRQ renders fine
   offscreen) and the exact `/Game/...` map. `PythonScriptingTools.read_me` once, then use `execute_python`.
2. **Know what is in the level at runtime.** If a game mode spawns the world in `BeginPlay` (TheSagaCutter:
   `ATSCGameMode::BuildWorld`, grid `CELL=200`, world = `((cx-17)*200, (cy-12)*200)`), the editor level looks empty.
   Read the spawn code or run PIE and read actor bounds (`get_actor_bounds(False)` - with `True` procedural/prim
   actors report zero) to get building positions and heights. Plan camera positions from those, not from the editor view.
3. **Shot list.** 30 s at 30 fps = 900 frames, ~110 frames per stop, last key = overview. Each shot is
   `(frame, cam_xyz, target_xyz)`. Rules learned the hard way:
   - Keep neighbouring shots facing roughly the same way. A shot looking the opposite way from its neighbours makes
     two 180° whip-turns. Look along a row of props instead of flipping round.
   - The camera moves in straight-ish cubic curves between keys: check the line between consecutive cameras does not
     cross a building (use runtime bounds, which can be wider than the grid footprint, e.g. roof eaves). Add a
     waypoint key above/beside it, or pass `linear_after=[frame]` to stop the curve bulging into a wall.
   - Wide lens (20 mm) makes close passes look like clipping; keep >300 uu from walls when passing.
4. **Build:**
   ```python
   import sys, unreal
   sys.path.insert(0, unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()) + '../.agents/skills/ue-demo-video/scripts')
   import importlib, ue_demo
   importlib.reload(ue_demo)
   shots = [(0, (-900, 0, 420), (-1800, 0, 330)), (110, ...), ..., (900, (0, 2600, 2300), (0, -200, 0))]
   ue_demo.build_flythrough('/Game/Vikings/Cinematics/LS_<Name>', shots,
                            sub_sequences=['/Game/Props/medieval_banner/LS_MedievalBanner_WindGusts'])
   ```
   `sub_sequences` plays level animation sequences (wind, doors...) under the camera; MRQ only evaluates the rendered
   sequence, so autoplay actors in the level do NOT animate in the render otherwise.
5. **Render** (returns at once; ~15 s for 900 frames at 720p after shaders are compiled):
   ```python
   ue_demo.render('/Game/Vikings/Cinematics/LS_<Name>', '/Game/Vikings/Maps/<Map>', '<JobName>',
                  game_mode='/Script/TheSagaCutter.TSCGameMode')
   ```
   Then in Bash (foreground, timeout 600000; the script times out itself after 25 min):
   ```bash
   bash .agents/skills/ue-demo-video/scripts/wait_and_encode.sh "<ProjectDir>/UE/Saved/MovieRenders/<JobName>" 900 "<ProjectDir>/<video>.mp4" "<scratchpad>/contact.png"
   ```
6. **Review** the contact sheet with the image viewer. Look for: black frames, frames filled by one wall/roof colour (camera
   inside geometry), props cut off by the frame edge, whip-turns. Fix with
   `ue_demo.set_shot(seq, frame, cam, target)` (or `add=True` for a waypoint, `linear=True` for a straight segment),
   re-run step 5, re-check only the fixed frame range:
   `ffmpeg -i v.mp4 -vf "select='between(n\,330\,440)*not(mod(n\,10))',scale=480:-1,tile=4x3" -frames:v 1 seg.png`.
7. Share the rendered MP4 in the final response, `ue_stop`, report what the video shows and any known flaw.
   Do not commit unless asked. Frames stay in `UE/Saved/MovieRenders/` (gitignored Saved/).

## Gotchas (all hit in practice)

- **Black video** (~50 KB for 30 s): MRQ used `MoviePipelineGameMode`, so the runtime world and its lights never
  spawned. Set the override via `soft_game_mode_override` with a loaded UClass (`unreal.load_class`); the old
  `game_mode_override` property is deprecated and silently ignored. Check the log for `Game class is '<YourGameMode>'`.
- Waiting on the log line `MoviePipelineLinearExecutorBase finished` alone races: the render may finish before you
  start counting, or PNGs still be flushing. Count frames (the wait script does).
- Do not edit the sequence while a render runs; wait for it, then edit and re-render.
- `get_spawnables()[0].get_tracks()[0]` is not always the transform track - use
  `find_tracks_by_exact_type(unreal.MovieScene3DTransformTrack)`.
- Spawnable camera component: `get_object_template().get_editor_property('camera_component')`
  (`get_cine_camera_component()` / `get_component_by_class` return None on the template).
- execute_python returns the value of the **last expression**; a script ending in `if/else` returns nothing - assign
  to a variable and end with it. Imports only at top level; no `time.sleep` (use `await asyncio.sleep`).
- The CineCamera default autofocus blurs everything; `build_flythrough` disables it.
- Fast camera moves get heavy motion blur in MRQ (cinematic quality): keep ~100+ frames between stops that are far
  apart, or the in-between frames smear.
