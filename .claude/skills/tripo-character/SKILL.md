---
name: tripo-character
description: Turn one reference image of a character into a rigged, textured, animated skeletal mesh in TheSagaCutter and make a game unit use it. Pipeline: T-pose sheet in ChatGPT, model + auto-rig in Tripo Studio, clean-up in Blender, import / retarget / sockets in Unreal, C++ routing, review in play mode. Use when the user says "/tripo-character", "make a 3D model from this image", "generate a character from the reference", "replace the <unit> model with this picture", "add another axolotl class" or similar. Needs the user's signed-in ChatGPT and Tripo Studio in Chrome, Blender, and the ue-editor skill.
argument-hint: "<reference image path> <key> [which unit / role it replaces] [notes]"
---

# tripo-character

One reference picture in, one playable character out. This is the pipeline that made the axolotl warrior, archer, druid,
blacksmith and jarl. Full history with every measured value: `Docs/Plans/axolotl-warrior-tripo.md` and
`Docs/Plans/axolotl-all-classes.md`. Read their "Result" sections when something here is unclear.

Arguments:
- `<reference image>` — the picture of the character. Required; ask once if missing.
- `<key>` — short lowercase name, letters only (`archer`). `<Key>` below is `key.capitalize()`.
- which game unit it replaces (a role in `ETSCRole`, named or unnamed). Ask if not given.

**Ask before you start:** which unit the model replaces, and how many Tripo credits you may spend without asking again
(one clean pass is 75: 55 generate + 20 rig). Every credit spend beyond that allowance needs a yes.

## Names (all derived from the key)

| Thing | Name |
|---|---|
| T-pose image | `Docs/References/FurryAxolotlClasses/tripo_input/<key>_front.png` |
| Tripo export | `Art/Tripo/axolotl_<key>/rigged/<key>_rigged.fbx` + `<key>_rigged.fbm/` |
| Blender outputs | `Art/Tripo/axolotl_<key>/SK_Axolotl<Key>.fbx`, `T_Axolotl<Key>_*.jpg`, `<key>_bones.json`, `check_*.png` |
| UE folder | `/Game/Characters/axolotl_<key>` |
| Mesh, material | `Axolotl_<Key>`, `M_Axolotl<Key>` |
| IK rig, retargeter | `IK_Axolotl_<Key>`, `RTG_Viking_To_Axolotl_<Key>` |
| Clips | `Axolotl_<Key>CharacterArmature_<Clip>` (11 clips) |

The scripts put `Axolotl` in every name, also for a character that is not an axolotl. Keep it: the C++ and all scripts
agree on it. Renaming means editing the three UE scripts and the Blender script.

## Steps

### 1. T-pose sheet (ChatGPT in Chrome)
Open a **new chat** at chatgpt.com in Claude in Chrome, attach the reference image to the hidden file input
(`file_upload`, one file per call: 10 MB limit per call), and send this prompt with the costume list filled in:

> Generate an image: a 3D-modelling reference of exactly this character (same `<species / class>`, same face, `<list
> the costume parts to keep>`, same colours and materials). Full body, straight front view, orthographic, standing
> upright in a T-pose with both arms straight out to the sides and empty open paws. Remove the `<weapon / held item>`
> completely. Remove the long cape; keep only the short fur collar on the shoulders. Legs slightly apart and fully
> visible, feet flat. Plain light grey background, flat even studio lighting, no ground shadow, no props, no scenery,
> no text. Square image, the whole figure inside the frame with a small margin.

Rules that matter for the rig: nothing in the hands, no cape or cloth over the arms, two separate legs, the whole figure
in frame. Small items on the back or belt (quiver, pouch, horn) are fine. Open the result, download it with the
download icon, copy it from Downloads to the T-pose path. Look at it before going on.

### 2. Model and rig (Tripo Studio in Chrome)
https://studio.tripo3d.ai → 3D Workspace → Model → HD Model, single image.
- Settings (Geometry & Texture): **H3.1**, Ultra Mesh on, Texture on, **4K**, Remove Lighting on, PBR on, Triangle,
  polycount 30000. Cost shows on the button (55).
- Upload the T-pose image, Generate. Several generations can run at once: remove the image, upload the next, Generate.
- Open the result. Check: two arms, two legs, tail, nothing fused, about 30k faces.
- Rig tool → Humanoid, Skeleton Preset **Mixamo** → Auto Rig (20).
- Export → FBX, Blender, 4k, Export Skeleton on. The zip lands in Downloads as `tripo_convert_<id>.zip`.
- `sh .claude/skills/tripo-character/scripts/place_tripo_export.sh <key>` unpacks the newest zip to the export path.
  Export one character, place it, then export the next (the script takes the newest zip).

### 3. Blender clean-up
```
"C:\Tools\blender-5.2.1-windows-x64\blender.exe" -b -P Scripts/blender/prep_axolotl.py -- <key> Art/Tripo/axolotl_<key>/rigged/<key>_rigged.fbx Art/Tripo/axolotl_<key>
```
Must exit 0. Read the log and **look at every still**. Check: 65 bones, root `Hips`, no `mixamorig:` prefix, no
unweighted vertices, facing -Y, height about 0.97 m.

**Tail.** Tripo skins the tail to the thighs. Sometimes it is thigh-dominant (auto mode catches it), often it is about
half `Hips`, half thigh, and then auto mode logs "tail: nothing to reweight" although the tail still swings with the
leg. So always re-import the exported FBX and measure the weights of the vertices behind the trunk. If tail vertices
outside the thin blend band at the root carry thigh weight, add a `TAIL["<key>"]` entry in `prep_axolotl.py` with
thresholds measured on this model (`back_y` just behind the trunk's back, the z range, the side the tail curls to) and
rerun. In `check_tail.png` the whole tail must be red; in `check_pose2.png` it must not follow the thigh.

### 4. Unreal: import, retarget, sockets
Editor through the ue-editor skill; one agent drives it at a time. `execute_python` does **not** keep globals between
calls, so set `CHAR` inside every call. Three calls, in this order:
```python
CHAR = "<key>"; FORCE_TEXTURES_AND_MATERIAL = True
exec(open(r"<repo>/Scripts/ue/import_axolotl.py").read())
```
```python
CHAR = "<key>"; exec(open(r"<repo>/Scripts/ue/retarget_axolotl.py").read()); RESULT
```
```python
CHAR = "<key>"; exec(open(r"<repo>/Scripts/ue/sockets_axolotl.py").read()); await run()
```
The import script deletes and recreates the mesh, skeleton, IK rig, retargeter and clips, so after any rerun of it,
rerun the other two. Do not save `Map_Saga`.

Then verify **by animating**, never by the rest pose (a bad unit import looks fine at rest and tears when it moves):
- `scripts/capture_clips.py`: `CHAR = "<key>"; OUT_DIR = r"<scratch>"; exec(...); await run([clips], 3, tag="<key>_")`
- `python scripts/contact_sheet.py <key> Art/Tripo/axolotl_<key>/check_retarget_sheet.png <scratch>/cap`
- `scripts/measure_soles.py`: `CHAR = "<key>"; exec(...); RESULT` → lowest sole in Idle / Walk / Run, hips at the end of Death.

Accept: all bones scale 1, 11 clips with the source lengths, four mesh sockets (`weapon_r`, `staff_r`, `shield_l`,
`bow_l`), Idle soles within ±2 cm of the ground, figure upright and untorn on the sheet. If Idle soles are off, add a
`TUNE["<key>"]` entry in `retarget_axolotl.py` (`PELVIS_Z`, `PELVIS_V`). The default already scales `PELVIS_Z` by the
rest hips height and was enough for all five characters so far.

### 5. C++ routing
`UE/Source/TheSagaCutter/TSCUnit.cpp`: add
`const FVikingModel Axolotl<Key>{ "/Game/Characters/axolotl_<key>", "Axolotl_<Key>", "Axolotl_<Key>CharacterArmature_", "RightHand", "LeftHand", 1.f, true };`
next to the others and one line in the `Preferred` choice of `BuildViking` for the role. The existing code falls back
to the Viking model when the package is missing. Build with `sh Scripts/build_editor.sh` (editor closed), then restart
the editor.

### 6. Review in play mode and grip tuning
PIE on `Map_Saga`. `scripts/pie_review.py` has the helpers (`find_hero`, `describe`, `best`, `pair`: stills of a unit
in the PIE world with blocking props hidden). `TSCAuto LLLL X 1` plays the day; `TSCAuto LAAL X 16` reaches the night
with all four heroes alive (`LLLL` kills Hakon and Sigrun first). `[WDIAG]` log lines give `distToSocket` for named
units; it should be under about 45.

Fix the held item with the socket first, then the retarget pose:
- `SOCKET_TUNE["<key>"]` in `sockets_axolotl.py`: `SOCKET_LEAN` (degrees about socket +X, +Y) turns an item,
  `SOCKET_SHIFT` (cm toward the elbow, cm out) moves it. A new key replaces the warrior defaults, so repeat the ones
  you want to keep (`weapon_r` lean, `shield_l` shift). `scripts/grip_measure.py` measures a socket's tilt over clips.
- `TUNE["<key>"]` in `retarget_axolotl.py`: `ARM_ABDUCT`, `FINGER_CURL`.
Rerun the script after each change so the assets match the scripts. Save stills to
`Art/Tripo/axolotl_<key>/review_day.png` and `review_night.png`, and look at them.

### 7. Credits
Add the character to the Tripo line in `Docs/ThirdPartyAssets.md`.

## Report
Tell the user: what was made, credits spent, what the stills show, what is still wrong, what was not tested. Do not
commit unless asked. Each character adds about 17 MB under `UE/Content` (4K textures) and about 35 MB under `Art/`.

## Known limits (do not promise these)
- No foot IK: soles dip 4-8 cm in Walk and Run.
- Tail, gills, braids and cloth are rigid; the tail sticks up in the Death pose.
- The clips come from the Viking set. A pose the source does not have cannot be tuned in: `Shoot_OneHanded` never raises
  the bow arm; in `Punch` the paws are at the face, so a staff crosses a tall crown.
- Axe carriers hold the axe across the chest in Idle, and the shield shows its strap side outward (same on the Vikings).
- A big cape, a skirt to the ground or an item in the hand in the T-pose image breaks the auto-rig: fix the image, not the mesh.
