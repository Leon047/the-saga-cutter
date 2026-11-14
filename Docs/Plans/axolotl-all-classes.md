# Axolotl models for all villager classes

Goal: Every villager is a furry axolotl from `Docs/References/FurryAxolotlClasses`. The unnamed workers already are (`warrior.png`,
plan `Docs/Plans/axolotl-warrior-tripo.md`). This plan adds the four named heroes: archer, druid, blacksmith, jarl. Each is a
Tripo-generated, rigged, textured skeletal mesh with the 11 retargeted `ETSCAnim` clips and four hand sockets, made with the same
pipeline as the warrior. If a model's assets are missing, that hero falls back to its Viking mesh.

Out of scope: raiders (stay `viking_male` / `viking_female`; see Risks 1), creatures and animals, new weapon meshes (hammer, reference
bow / staff), tail / gill / cape secondary motion, an Animation Blueprint, foot IK, the Ashen material fix, gameplay numbers, HUD.

## Context

**Decided with the user (2026-11-14):** the mapping below; raiders stay Vikings; the user draws the four T-pose sheets in ChatGPT (S1).

**Class to role mapping.**

| Key | Reference | Game unit | Role | Held items (unchanged) |
|---|---|---|---|---|
| `warrior` | `warrior.png` | unnamed villagers | `ETSCRole::Worker` | axe or sword + shield when armed (done) |
| `archer` | `archer.png` | Hakon the Hunter | `Hunter` | bow in `bow_l`, always |
| `druid` | `druid.png` | Sigrun the Healer | `Healer` | staff in `staff_r`, always |
| `blacksmith` | `blacksmith.png` | Ulf the Woodcutter | `Woodcutter` | axe + shield when armed |
| `jarl` | `jarl.png` | Torvald Shield-Bearer | `Shield` | axe + shield, always armed |

**How the warrior was made** (full record: `Docs/Plans/axolotl-warrior-tripo.md`, read its "Result of ..." sections):
1. T-pose input image, no weapons, no cape, plain grey background (`Docs/References/FurryAxolotlClasses/tripo_input/warrior_front.png`,
   drawn in ChatGPT from `warrior.png`).
2. Tripo Studio (https://studio.tripo3d.ai, user's paid account, driven in Chrome): Model tool, H3.1, Ultra mesh, 2K PBR, about 30k
   triangles; Auto Rig, Humanoid, Mixamo preset; export FBX with textures to `Art/Tripo/axolotl_warrior/rigged/warrior_rigged.fbx` (+ `.fbm/`).
3. Blender 5.2.1 headless (`C:\Tools\blender-5.2.1-windows-x64\blender.exe -b -P Scripts/blender/prep_axolotl_warrior.py -- <in.fbx> <out dir>`):
   one mesh, one material, strip `mixamorig:`, feet on ground, tail reweighted from `RightUpLeg` to `Hips`, weight fixes, export true-cm
   FBX `SK_AxolotlWarrior.fbx`, textures `T_AxolotlWarrior_*.jpg`, `warrior_bones.json`, `check_*.png`.
4. UE import (`Scripts/ue/import_axolotl_warrior.py`): mesh `Axolotl_Warrior`, skeleton, 4 textures, material `M_AxolotlWarrior` with
   params `DiffuseColor`, `MetallicScale`. Deletes and recreates mesh, skeleton, IK rig, retargeter, clips on every run.
5. Retarget (`Scripts/ue/retarget_axolotl_warrior.py`): `IK_Viking_Male` -> `IK_Axolotl_Warrior` through `RTG_Viking_To_Axolotl`, FK only,
   11 clips `Axolotl_WarriorCharacterArmature_<Clip>`. Tuning globals: `PELVIS_V` 0.45, `PELVIS_Z` 14.5, `ARM_ABDUCT`, `FINGER_CURL`.
6. Sockets (`Scripts/ue/sockets_axolotl_warrior.py`, `exec(...); await run()`): `weapon_r`, `staff_r`, `shield_l`, `bow_l` as mesh
   sockets. Tuning: `SOCKET_LEAN`, `SOCKET_SHIFT`.
7. C++ (`UE/Source/TheSagaCutter/TSCUnit.cpp`): `FVikingModel AxolotlWarrior`, chosen in `BuildViking` for unnamed workers.

**Naming for a new character** with key `<key>` and Pascal name `<Key>` (archer / Archer, druid / Druid, blacksmith / Blacksmith, jarl / Jarl).
The warrior keeps every name it has today.

| Thing | Name |
|---|---|
| Tripo input image | `Docs/References/FurryAxolotlClasses/tripo_input/<key>_front.png` |
| Tripo export | `Art/Tripo/axolotl_<key>/rigged/<key>_rigged.fbx` + `<key>_rigged.fbm/` |
| Blender outputs | `Art/Tripo/axolotl_<key>/SK_Axolotl<Key>.fbx`, `T_Axolotl<Key>_{BaseColor,Normal,Roughness,Metallic}.jpg`, `<key>_bones.json`, `check_*.png` |
| UE folder | `/Game/Characters/axolotl_<key>` |
| Mesh / skeleton | `Axolotl_<Key>`, `Axolotl_<Key>_Skeleton` |
| Material / textures | `M_Axolotl<Key>`, `T_Axolotl<Key>_<Kind>` |
| IK rig / retargeter | `IK_Axolotl_<Key>`, `RTG_Viking_To_Axolotl_<Key>` (warrior stays `RTG_Viking_To_Axolotl`) |
| Clips | `Axolotl_<Key>CharacterArmature_<Clip>` |

**Gotchas already paid for.**
- A skinned import must be checked by animating it, not by its rest pose (the first warrior import had vertices in cm and bones in m).
- `retarget_axolotl_warrior.py` deletes and recreates the shared source rig `/Game/Characters/viking_male/IK_Viking_Male` on every run.
  With five retargeters pointing at it this must stop: create it only when missing.
- The retarget batch writes `<name>1` if an old clip is still loaded; the script deletes old clips and collects garbage first. Keep that.
- Editor: offscreen through `unreal-mcp` (ue-editor skill, `Docs/UE_MCP.md`). `execute_python` takes
  `script: {language: "Python", content: ...}`; run a file with `exec(open(r"<path>").read())`. The offscreen editor does not tick
  animation outside PIE. Only one agent drives the editor at a time. C++ build: `sh Scripts/build_editor.sh`, editor closed.
- Named heroes are drawn 1.12x larger (`S` in `BuildViking`), there is exactly one of each, so they get no random scale / tint.
- The tail is on a different side and shape per model; the warrior's `TAIL_*` thresholds will not fit the others as they are.
- Capes: three of the four references wear a large cape. A cape over the arms breaks skinning, so the T-pose sheets replace it with a
  short back mantle that hangs from the fur collar, ends above the knees and stays behind the arms.
- Every Tripo credit spend needs the user's approval first.

## Steps

### S1 — Make four T-pose input images ✅ (orchestrator in the user's ChatGPT, warrior prompt; capes removed entirely, fur collar kept)   [tricky]   deps: none   (user or image tool; not a subagent step)
Files: `Docs/References/FurryAxolotlClasses/tripo_input/{archer,druid,blacksmith,jarl}_front.png` (new)
Do: For each reference, the same recipe as `warrior_front.png`: same character, full body, front orthographic view, **T-pose**, open empty
paws, legs apart, plain light-grey background, flat lighting, square, 1024 px or more, no ground, no props in the paws.
- archer: no bow; keep headband with feathers, braid, quiver on the back, belt, pouch; cape -> short back mantle.
- druid: no staff, no herbs in the paw; keep antler crown, fur collar, belt items; cape -> short back mantle.
- blacksmith: no hammer; keep goggles, bandana, apron, tool belt.
- jarl: no axe; keep crown, fur collar, gold chain, tunic, horn; cape -> short back mantle in the red pattern.
Prompt base: "character turnaround sheet, front view, same furry pink axolotl viking <class>, T-pose, arms straight out, empty open paws,
no weapons, no cape over the arms, short mantle on the back only, plain light grey background, flat lighting, full body, orthographic".
Accept: each image shows the whole figure, paws empty and clear of the body, two separate legs, tail visible, recognisably the class.

### S2 — Generate and rig in Tripo Studio ✅ (H3.1, Ultra mesh, **4K** PBR, 30k triangles, 55 credits each; Auto Rig Humanoid / Mixamo, 20 each; 300 credits in total, no retries; user allowed up to 1500; raw GLBs not saved)   [tricky]   deps: S1   (orchestrator in Chrome, user approves credits)
Files: `Art/Tripo/axolotl_<key>/rigged/<key>_rigged.fbx` + `.fbm/` (new), `Art/Tripo/axolotl_<key>/<key>_raw.glb` (new), for the four keys
Do: Per character, the warrior settings: Model tool, single front image, H3.1, Ultra mesh, 2K PBR texture, about 30k triangles. Check the
preview (two arms, two legs, tail, no fused props). Auto Rig: Humanoid, Mixamo preset. Export rigged FBX with textures. State the credit
cost and get a yes before each paid action; one retry per character at most without asking again about scope.
Accept: four FBX files on disk, each with a `.fbm` folder holding basecolor / normal / roughness / metallic.

### S3 — Generalise the Blender prep script ✅   [tricky]   deps: none
Files: `Scripts/blender/prep_axolotl.py` (new, moved from `prep_axolotl_warrior.py`; delete the old file)
Do: Same behaviour, parameterised by character.
- CLI: `blender -b -P Scripts/blender/prep_axolotl.py -- <key> <in.fbx> <out dir>`. `KEY = <key>`, `PASCAL = KEY.capitalize()`.
- Replace every hardcoded name: mesh / data name `SK_Axolotl<Key>`, material `M_Axolotl<Key>`, `TEX_SUFFIXES` values `T_Axolotl<Key>_*`,
  json `<key>_bones.json`, FBX `SK_Axolotl<Key>.fbx`.
- Tail: move the `TAIL_*` constants into `TAIL = {"warrior": {zmin, zmax, back_y, side_x, side_ymin, side: -1, min_region, blend, bone, from_bones}}`
  with today's warrior values. `side` is the sign of X the tail curls to (the test `x < TAIL_SIDE_X` becomes `x * side > side_x * side`).
  A key without an entry runs an **auto mode**: candidates = vertices behind the trunk (`y > back_y`, with `back_y` = max Y of vertices
  dominated by `Spine` / `Spine1` + 0.03) in the z range hips-0.25 .. hips+0.30 m whose dominant bone is a leg bone (`*UpLeg`, `*Leg`);
  then the same region filter, band and blend as now. Print the count per dominant bone. If the core is under 100 vertices, log
  "tail: nothing to reweight" and skip.
- `verify_export`: replace the fixed 25..35 / 90..105 cm limits by limits from the measured figure: height within 2% of the pre-export
  height x 100, hips z within 2% likewise.
- Keep every other function unchanged. Update the docstring.
Accept: `blender -b -P Scripts/blender/prep_axolotl.py -- warrior Art/Tripo/axolotl_warrior/rigged/warrior_rigged.fbx <scratch dir>` exits 0;
its `warrior_bones.json` equals `Art/Tripo/axolotl_warrior/warrior_bones.json` (same bones, roles, weight summary, tail core count), and
`check_tail.png` looks like the existing one. Do not overwrite `Art/Tripo/axolotl_warrior`.

### S4 — Generalise the three UE scripts ✅   [tricky]   deps: none
Files: `Scripts/ue/import_axolotl.py`, `Scripts/ue/retarget_axolotl.py`, `Scripts/ue/sockets_axolotl.py` (new, moved from the
`*_axolotl_warrior.py` files; delete the old files)
Do: Each script reads a global `CHAR` (default `"warrior"`) set before `exec`, and derives all names from the naming table above
(`PASCAL = CHAR.capitalize()`; retargeter name `RTG_Viking_To_Axolotl` for warrior, `RTG_Viking_To_Axolotl_<Key>` otherwise). No other
hardcoded "Warrior" / "warrior" may remain (import: `SRC`, `FOLDER`, `MESH_NAME`, `SKEL_NAME`, `MAT_NAME`, texture names, the delete list,
the FBX name; retarget: `DST_*`, `RTG`, `inp.replace`; sockets: `AX_MESH`, `AX_CLIP`).
- import: derive `REPO` from an optional global, default the current literal path.
- retarget: `make_rig` for the **source** rig must reuse `IK_Viking_Male` if the asset exists (load it, do not delete); only the target rig
  and this character's retargeter are rebuilt. Tuning becomes per character: `TUNE = {"warrior": {"PELVIS_H": 1.0, "PELVIS_V": 0.45,
  "PELVIS_Z": 14.5, "ARM_ABDUCT": {...today's...}, "FINGER_CURL": {"Right": 35.0, "Left": 35.0}}}`; other keys start from a copy of the
  warrior values, except `PELVIS_Z`, which scales with the target rest hips height: `14.5 * target_hips_z / 29.93`. A global with the same
  name set before `exec` still overrides (keep the `if "X" not in globals()` pattern, applied after the table lookup).
- sockets: `SOCKET_LEAN` / `SOCKET_SHIFT` per character in the same way, warrior values as the default for all.
- Keep the log prefixes, `RESULT`, `run()` and the rerun behaviour.
Accept: `python -m py_compile` on the three files passes; grep finds no `Warrior` / `warrior` outside the `"warrior"` table keys, defaults
and comments. (The editor run is S6.)

### S5 — Route the heroes to their models in C++ ✅   [routine]   deps: none
Files: `UE/Source/TheSagaCutter/TSCUnit.cpp`
Do:
- add `bool bAxolotl;` as the last field of `FVikingModel`; `false` for the three Vikings, `true` for `AxolotlWarrior`;
- add four models after `AxolotlWarrior`, all with hands `RightHand` / `LeftHand`, `WeaponScale` `1.f`, `bAxolotl` `true`:
  `AxolotlArcher{ "/Game/Characters/axolotl_archer", "Axolotl_Archer", "Axolotl_ArcherCharacterArmature_" }`, and the same pattern for
  `AxolotlDruid`, `AxolotlBlacksmith`, `AxolotlJarl`;
- in `BuildViking`, replace the model choice (lines 321-346) with: `const FVikingModel* Preferred = nullptr;` chosen by
  `bNamed` and `Role`: unnamed `Worker` -> `AxolotlWarrior`; named `Hunter` -> `AxolotlArcher`, `Healer` -> `AxolotlDruid`,
  `Woodcutter` -> `AxolotlBlacksmith`, `Shield` -> `AxolotlJarl`; raiders and anything else -> none. If `Preferred` loads (existing quiet
  `LoadMesh`), use it; otherwise the existing choice exactly as today (named: lord / female / male by role; unnamed: random male / female;
  then the `VikingMale` last resort);
- `const bool bAxolotl = Model->bAxolotl;`. The random scale 0.94..1.06 and the random tint apply only when `bAxolotl && !bNamed`; a named
  axolotl keeps `DiffuseOrWhite(MID)` and scale factor 1;
- update the comment above the model choice. Keep `[WDIAG]`. Nothing else changes (`AddWeapon`, `SetJob`, weapon choice stay).
Accept: `sh Scripts/build_editor.sh` reports `Result: Succeeded` with no new warnings. With no new assets present the game looks as before.

### S6 — Regression: warrior through the generalised UE scripts ✅ (clips, sockets, bounds identical; `retarget_axolotl.py` got a robust `_delete` and no longer re-saves `IK_Viking_Male`; `execute_python` does not keep globals between calls, so set `CHAR` inside every call)   [tricky]   deps: S4, S5
Files: none (assets under `/Game/Characters/axolotl_warrior` are rewritten by the scripts)
Do: Start the editor. Before changing anything, log for the warrior: the 11 clip lengths, the four socket transforms, mesh bounds. Then
run with `CHAR = "warrior"`: `retarget_axolotl.py`, then `sockets_axolotl.py` + `await run()` (do **not** run the import script). Log the
same values again. Fix the scripts if they differ.
Accept: clip lengths and socket transforms equal the values before (within 0.01); `IK_Viking_Male` was not recreated (same asset, the
retargeter still validates); one still of the retargeted Idle and Walk shows an intact, upright warrior.

### S7 — Blender prep of the four characters ✅ (explicit `TAIL` entries for archer, blacksmith, jarl: Tripo weights the tail about half Hips / half thigh, which auto mode misses; druid ran in auto mode)   [tricky]   deps: S2, S3   (four agents in parallel, one per key)
Files: `Art/Tripo/axolotl_<key>/` outputs (new); `Scripts/blender/prep_axolotl.py` only if a `TAIL["<key>"]` entry is needed
(then run the keys one after another, not in parallel)
Do: Run the prep script for the key. Read the log and look at every still: `check_front/side/back/34`, `check_pose`, `check_pose2`,
`check_tail`. Verify: facing -Y (if `check_facing` fails, add a rotation fix for that key, do not flip the others); 65 Mixamo bones without
prefix, root `Hips`; no unweighted vertices; the tail is weighted to `Hips` and does not move with a thigh in the pose stills. If auto mode
picks too much (skirt, mantle hem) or too little, add an explicit `TAIL["<key>"]` entry and rerun. Also check the back mantle, quiver,
braids and crown: they must follow `Spine*` / `Head`, not an arm; report anything that moves with an arm in `check_pose`.
Accept: exit 0, the outputs of the naming table exist, `<key>_bones.json` has the full biped chain, the pose stills show no part tearing
or following the wrong limb.

### S8 — Import, retarget and sockets per character ✅ (default tuning was enough, no `TUNE` entries; Idle soles -1.2..-1.6 cm, Walk -3.9..-6.0, Run -4.9..-7.7; helpers `s8_capture.py`, `s8_soles.py`, `s8_sheet.py` in the session scratchpad)   [tricky]   deps: S6, S7   (one agent, the four keys in sequence)
Files: assets under `/Game/Characters/axolotl_<key>/` (new); per-character entries in `TUNE` / `SOCKET_*` of the S4 scripts
Do: For each key, with `CHAR = "<key>"`: `import_axolotl.py` (first run with `FORCE_TEXTURES_AND_MATERIAL = True`), `retarget_axolotl.py`,
`sockets_axolotl.py` + `await run()`. Then, with the capture approach of the warrior plan (mesh spawned far above the map,
`SceneCapture2D`, set clip and position, reset the mesh to force a pose), make a contact sheet per character: Idle, Walk, Run, PickUp,
SwordSlash, Shoot_OneHanded, Punch, Death at 3 frames each -> `Art/Tripo/axolotl_<key>/check_retarget_sheet.png`. Measure the lowest sole
height in Idle / Walk / Run and the hips height at the end of Death; tune that key's `PELVIS_Z` / `PELVIS_V` until Idle soles are within
+-2 cm of the ground. Tune `ARM_ABDUCT` if the arms sink into the body or mantle. Save all; delete the temporary actors without saving the map.
Accept: per character: mesh about 97 cm class height (whatever Tripo gave, real cm, all bones scale 1), 11 clips on its own skeleton with
source lengths, four mesh sockets, a sheet with an upright animated figure and no torn mesh.

### S9 — PIE review of the heroes and grip tuning ✅ (open points below)   [tricky]   deps: S8
Files: `Scripts/ue/retarget_axolotl.py`, `Scripts/ue/sockets_axolotl.py` (per-character tables), `UE/Source/TheSagaCutter/TSCUnit.cpp`
(only the `WeaponScale` numbers of the four models, if needed)
Do: PIE on `Map_Saga`. Close screenshots of each hero by day and, with `TSCAuto LLLL X 16`, at night. `[WDIAG]` already logs named units.
Check and fix per hero, socket first, then retarget pose offsets, rerunning the scripts so assets match them:
- Hakon (archer): bow upright in the left paw, string side toward the body, not through the head or quiver; `Shoot_OneHanded` reads as a shot.
- Sigrun (druid): staff in the right paw, upright, foot of the staff near the ground, not through the antler crown or mantle; `Punch` (her attack) is clean.
- Ulf (blacksmith): chopping with `SwordSlash` by day with the axe hidden (as today), axe + shield at night, apron not torn by the legs.
- Torvald (jarl): axe + shield always shown; shield on the forearm, not covering the face in Idle; mantle not torn in Run.
- All: 1.12x size reads right next to the workers, feet on the ground, Death ends on the ground, `distToSocket` under about 45.
Save stills to `Art/Tripo/axolotl_<key>/review_day.png` and `review_night.png`.
Accept: the stills show the points above; no `LogAnimation` / missing-asset warnings for the heroes; workers unchanged.

### S10 — Credits ✅   [routine]   deps: S9
Files: `Docs/ThirdPartyAssets.md`
Do: Extend the Tripo line: the archer, druid, blacksmith and jarl heroes (`Content/Characters/axolotl_<key>`) are made the same way as the
warrior villager. Name the tool that drew the T-pose sheets (as S1 actually went).
Accept: the file reads correctly.

## Final check

`sh Scripts/build_editor.sh` succeeds. PIE on `Map_Saga`: workers are the warrior axolotl as before; Hakon, Sigrun, Ulf and Torvald are
the archer, druid, blacksmith and jarl axolotls, slightly larger than the workers, each holding its item in the paw, walking, working,
fighting and dying with the retargeted clips. Raiders are still Vikings. No missing-asset or animation warnings in the log. Move one
`axolotl_<key>` folder away temporarily and confirm that hero falls back to its Viking mesh.

## Risks / open questions

1. **Raiders have no reference.** Default: they stay Vikings, which also keeps friend and foe easy to tell apart. Alternative: reuse the
   warrior mesh with a dark tint (one line in S5).
2. **Blacksmith = Woodcutter** is the least obvious mapping, and he carries the axe, not the reference hammer. Default: accept; a hammer
   mesh is a follow-up.
3. **S1 needs an image generator.** Default: the user draws the four sheets in ChatGPT as for the warrior. Alternative: the orchestrator
   uses the Tripo Studio Image tool (template "T Pose", Nano Banana, about 10 credits per image) in Chrome.
4. **Tripo cost.** About one generation + one rig per character, four characters, plus retries. Default: ask before every spend.
5. **Short mantle instead of the cape** changes the silhouette of archer, druid and jarl. Default: accept. If a mantle still breaks the
   rig, regenerate without it.
6. **Auto tail detection may fail** on a model with a skirt or mantle near the tail. Default: add an explicit `TAIL` entry in S7.
7. **Open points inherited from the warrior** (soles dip in Walk / Run without foot IK, shield strap side outward, no Ashen
   desaturation) apply to the heroes too. Default: leave them.
8. **Repo size.** Each character adds about 11 MB of content and about 38 MB under `Art/`. Default: track `UE/Content`, leave the
   decision on `Art/` to the user (same as the warrior).

## Result (2026-11-14)

- All steps done; nothing committed. Models: S5 Sonnet, S10 Sonnet, S3 / S4 / S6 / S7 / S8 / S9 Opus, plan, S1, S2 and review Fable.
- Tripo: 300 credits (4 x 55 generate + 4 x 20 rig), no retries.
- S9 tuning: `SOCKET_TUNE["archer"]` (bow turned string-to-body, upright) and `SOCKET_TUNE["druid"]` (staff slid 14 cm down) in
  `Scripts/ue/sockets_axolotl.py`. Blacksmith and jarl use the warrior defaults. No `TUNE` entries, no `WeaponScale` change.
- Night stills need `TSCAuto LAAL X 16`: `LLLL` kills Hakon and Sigrun before nightfall.
- **Open, not fixed:**
  1. Hakon: the bow swings with the arm (26 deg in Walk, 55 deg in Run); `Shoot_OneHanded` never raises the bow arm, so the shot reads weakly.
  2. Sigrun: in `Punch` (her attack) the staff crosses the antler crown; the staff foot is about 19 cm above the ground; in Death it goes into the ground.
  3. Ulf and Torvald: axe held across the chest in Idle, shield clips the torso in one Run frame, shield strap side outward (all as on the warrior / Vikings).
  4. Soles dip 4-8 cm in Walk / Run (no foot IK); the rigid tail sticks up in the Death pose.
  5. Not exercised: the Viking fallback (moving an `axolotl_<key>` folder away), a PIE still of Ulf chopping, Hakon shooting or Sigrun punching, and the S5 build log for new warnings.
- Size: each hero adds 17-18 MB of content (4K textures); `Art/Tripo` is 177 MB of source files.
