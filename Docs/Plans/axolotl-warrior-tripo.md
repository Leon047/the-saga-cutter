# Axolotl warrior: Tripo model replaces the warrior mesh

Goal: Every unnamed villager (`ETSCRole::Worker`, today a random `viking_male` / `viking_female`) is shown as the furry axolotl warrior from
`Docs/References/FurryAxolotlClasses/warrior.png`: a Tripo-generated, rigged, textured skeletal mesh that plays all 11
`ETSCAnim` clips, works unarmed by day and holds axe or sword plus shield in its paws when armed for the final night.
If the new assets are missing the game still falls back to the Viking meshes.

Out of scope: the other classes (archer, druid, blacksmith, jarl; Torvald keeps `viking_lord` until the jarl model is made), raiders, the named heroes, tail / gill secondary motion,
an Animation Blueprint (units keep `PlayAnimation` single-node mode), gameplay numbers, the primitive fallback figure.

## Context

**What "warrior" maps to.** There is no Warrior role. Decided with the user: the warrior is the **unnamed villager**, `ETSCRole::Worker`
(12 spawned at `TSCGameMode.cpp:784`, more at line 1721). By day they work unarmed (`Walk`, `Walk_Carry`, `Run_Carry`, `PickUp`, and
`SwordSlash` for chopping). In `StartNight` (`TSCGameMode.cpp:2193`) up to `6 + HousesBuilt` of them are armed with `SetArmed(true)`, which
shows the axe-or-sword and shield added in `BuildViking`; the rest shelter. So up to about 15 copies are on screen at once.
Torvald (`ETSCRole::Shield`) is the jarl and gets `jarl.png` in a later plan; raiders stay `viking_male` / `viking_female`.
`BuildViking` (`TSCUnit.cpp:292`) currently picks male or female at random for every unnamed unit, workers and raiders alike.

**How a character is wired (all in `UE/Source/TheSagaCutter/TSCUnit.cpp`).**
- `FVikingModel { Folder, Mesh, AnimPrefix }` (line 21). Mesh is loaded from `<Folder>/<Mesh>`, clips from
  `<Folder>/<AnimPrefix><Clip>` with clip names in `VikingAnimNames` (line 32): `Idle, Walk, Run, Walk_Carry, Run_Carry,
  SwordSlash, Shoot_OneHanded, Punch, PickUp, RecieveHit, Death`. No AnimBP, no montages, no notifies.
- The mesh is scaled so its imported bounds height = `VikingHeight` 120 cm (x1.12 for named units), feet on the ground, and yawed by
  `VikingYaw = -90` because the Viking FBX face +Y.
- Weapons: `AddWeapon` (line 593) attaches a static mesh to a socket, falling back to a bone. Sockets `weapon_r`, `shield_l`, `bow_l`,
  `staff_r` exist on all three Viking skeletons (confirmed in `VikingHelmet_Skeleton.uasset`). Socket convention:
  **+Z = where the shaft / blade points, +X = edge / facing / shooting side.** Fallback bones are `Fist_R` / `Fist_L`.
  Weapon size is `Def.Size * VikingHeight`, i.e. a fraction of the figure height. Workers get `DefShield` plus `DefAxe` or `DefSword` at random, hidden until `SetArmed(true)`.
- The Viking bones carry a x100 scale; `AddWeapon` already divides offsets by the socket's world scale, so a rig with unit bone scale works too.
- Materials: every slot gets a MID; `Refresh()` writes vector param `DiffuseColor` = lerp(colour, Ashify(colour), SagaAlpha). The slot whose
  material is named `Main` is tinted with the role colour, and `SetJob` recolours it per job (wood / food). A Tripo material has neither, so it needs a `DiffuseColor` param or it will
  never go Ashen (and `K2_GetVectorParameterValue` returns black for a missing param).
- `Sim()` has a TEMP `[WDIAG]` log (line 956) that prints, for named units, each weapon's socket, world scale and distance to the socket.
  Use it for verification; do not remove it in this work.

**Viking skeleton (source of the animations).** 20-odd bones, no fingers: `Hips, Abdomen, Torso, Neck, Head, Shoulder_L/R, UpperArm_L/R,
LowerArm_L/R, Fist_L/R, UpperLeg_L/R, LowerLeg_L/R, Foot_L/R, PoleTarget_L/R` (+ `_end` leaves), root `CharacterArmature`. Each of the
three Vikings has its own skeleton and its own copy of the clips, so the axolotl gets its own skeleton and its own retargeted copies too. The source for retargeting is `viking_male`
(mesh `Viking_Male`, skeleton `Viking_Male_Skeleton`, prefix `Viking_MaleCharacterArmature_`).

**The reference image is not a usable Tripo input as it is.** It is a 3/4 view with a busy background, the axe and shield are in the paws,
and a cape hides the legs. Fed directly, Tripo fuses the weapons and cape into the body and the auto-rig fails or skins the axe to the arm.
The input must be a weapon-free A-pose on a plain background.

**Tooling on this machine.** UE 5.9 at `C:\Programs\UE_5.9` (IKRig plugin present in the engine), editor driven through `unreal-mcp`
(`Docs/UE_MCP.md`; use `execute_python`). Blender 5.2.1 at `C:\Tools\blender-5.2.1-windows-x64\blender.exe`. Python 3.9. Build:
`sh Scripts/build_editor.sh` (editor must be closed). **There is no Tripo API key**: `.env.local` only has `ELEVENLABS_API_KEY`.
Existing generation scripts (`Scripts/generate_sfx.py`) read `.env.local`; follow that pattern.

**Tripo API.** Base `https://api.tripo3d.ai/v2/openapi`, bearer key. Flow: `POST /upload` (image) -> `POST /task`
`{type: "image_to_model" | "multiview_to_model", ...}` -> poll `GET /task/<id>` -> `{type: "animate_prerigcheck"}` ->
`{type: "animate_rig", out_format: "FBX"}` -> download. Field names change between API versions: the implementer must read
https://platform.tripo3d.ai/docs before coding and not trust the names above.

**Conventions.** Asset names in `DefaultGame.ini`: `SK_`, `M_`, `T_` prefixes, but character folders under `/Game/Characters/<name>` use
the FBX names. New assets go to `/Game/Characters/axolotl_warrior`. Editor scripts live in `Scripts/ue/`, one file per job, runnable
through `execute_python`. Source art (GLB/FBX/PNG from Tripo) goes to `Art/Tripo/axolotl_warrior/` (new folder, not under `UE/`).

## Steps

### S1 — Make the Tripo input images ✅   [tricky]   deps: none   (needs the user or an image tool)
Files: `Docs/References/FurryAxolotlClasses/tripo_input/warrior_front.png` (new), `warrior_back.png` (new), optional `warrior_left.png` (new)
Do: Produce a clean character sheet of the same character as `warrior.png`, with the same generator that made the references. Requirements:
full body, **A-pose** (arms about 45 degrees down, paws open and empty, legs slightly apart), orthographic front view, plain light-grey
background, flat even lighting, no ground, no props. **No axe, no shield.** Keep helmet, braids, fur collar, tunic, belt, pouch, boots, tail,
gills. Shorten the cape to a back mantle that ends above the knees and does not wrap the arms (a full cape glued over the arms breaks
skinning). Then the back view of the same sheet. 1024 px or larger, square.
Suggested prompt: "character turnaround sheet, front view, same furry pink axolotl viking warrior, A-pose, empty open paws, no weapons,
no shield, short fur mantle on the back only, plain light grey background, flat lighting, full body, orthographic".
Accept: both images open, show the whole figure head to boots, paws empty and clear of the body, legs visible as two separate shapes.

### S2 — Tripo generation and rig script ✅ (done by hand in Tripo Studio)   [tricky]   deps: S1
Files: `Scripts/tripo_generate.py` (new), `Scripts/requirements.txt`, `.gitignore`, `Art/Tripo/axolotl_warrior/` (new, outputs)
Do: A CLI that reads `TRIPO_API_KEY` from `.env.local` (same loader as `Scripts/generate_sfx.py`) and runs:
1. upload front (+ back, left if present); `multiview_to_model` when there are 2+ views, else `image_to_model`. Newest model version,
   textures + PBR on, quad off, `face_limit` 10000, texture 2048.
2. poll to success, download the GLB to `Art/Tripo/axolotl_warrior/warrior_raw.glb` and the preview render.
3. `animate_prerigcheck`; if riggable, `animate_rig` (biped, FBX out) and download `warrior_rigged.fbx`.
4. write `warrior_tasks.json` with every task id, so a rerun with `--resume` skips paid steps that already succeeded.
Args: `--front --back --left --out --name --resume --no-rig`. Print credits used. Never print the key.
Add `Art/Tripo/**/*.glb` to `.gitignore` (keep the final FBX and textures tracked).
Accept: `python Scripts/tripo_generate.py --front ... --back ... --out Art/Tripo/axolotl_warrior --name warrior` ends with both files on
disk and the prerig check result printed. Open the preview PNG: it is the axolotl, no weapons, two legs, two arms.

### S3 — Clean up and normalise in Blender ✅   [tricky]   deps: S2
Files: `Scripts/blender/prep_axolotl_warrior.py` (new), `Art/Tripo/axolotl_warrior/SK_AxolotlWarrior.fbx` (new), `.../T_AxolotlWarrior_*.png` (new)
Do: Headless script (`blender.exe -b -P Scripts/blender/prep_axolotl_warrior.py -- <in.fbx> <out dir>`):
- import the rigged FBX; apply all transforms so the armature and mesh have scale 1 and rotation 0;
- one mesh object, one material; delete loose parts below 0.5% of the surface; if above 10k triangles, decimate to about 8k
  (about 15 copies are on screen) and confirm the weights survived;
- feet at Z = 0, centred on X/Y, height 1.2 m, **facing -Y in Blender** (this lands as +Y-facing in UE with the default FBX axes, the
  same as the Vikings, so `VikingYaw` keeps working);
- print the bone list, the hierarchy and the rest pose (T or A) to `warrior_bones.json`; record which bones are the hands, feet, head,
  hips, spine chain. Later steps read this file;
- check weights: every vertex has weights, max 4 influences, normalised; report vertices in the paws weighted to anything but the hand /
  forearm bones;
- export FBX (armature + mesh, no leaf bones, no baked animation, embed nothing) and textures as PNG (base colour, normal, ORM if present).
Also render four stills (front, side, back, 3/4) to `Art/Tripo/axolotl_warrior/check_*.png`.
Accept: the script exits 0, `warrior_bones.json` lists a full biped chain (hips, spine, head, 2 arms with hand bones, 2 legs with foot
bones), the stills show a correctly oriented, upright figure.

### S4 — Import into Unreal ✅   [routine]   deps: S3
Files: `Scripts/ue/import_axolotl_warrior.py` (new); assets under `/Game/Characters/axolotl_warrior/` (new)
Do: Through `execute_python`:
- import the FBX as skeletal mesh `Axolotl_Warrior` with a new skeleton `Axolotl_Warrior_Skeleton` (no animation import, no physics asset
  needed, import normals, do not import materials or textures from the FBX);
- import the textures as `T_AxolotlWarrior_BaseColor` / `_Normal` / `_ORM` (ORM: sRGB off);
- create material `M_AxolotlWarrior`: BaseColor = texture x vector param `DiffuseColor` (default white), Normal, Roughness, Metallic
  forced to 0, two-sided off. Name it so it does **not** collide with the `Main` tint slot. Assign to slot 0;
- log the imported bounds and the bone count; save all.
Accept: assets exist and are saved; imported bounds are about 120 cm tall and the mesh faces +Y in the asset preview; a viewport
screenshot of the mesh placed in an empty level shows correct textures.

### S5 — IK Rigs, retargeter and the 11 clips ✅   [tricky]   deps: S4
Files: `Scripts/ue/retarget_axolotl_warrior.py` (new); assets `/Game/Characters/axolotl_warrior/IK_Axolotl_Warrior`, `/Game/Characters/viking_male/IK_Viking_Male`,
`/Game/Characters/axolotl_warrior/RTG_Viking_To_Axolotl`, and 11 `Axolotl_WarriorCharacterArmature_<Clip>` sequences (all new)
Do: Enable the `IKRig` plugin in `UE/TheSagaCutter.uproject` if it is not on. Then in Python (`IKRigController`, `IKRetargeterController`,
`IKRetargetBatchOperation`; check the 5.9 names with `help()` first):
- source IK Rig on `Viking_Male`: retarget root `Hips`; chains Spine (`Abdomen`..`Torso`), Head (`Neck`..`Head`), LeftArm
  (`Shoulder_L`..`Fist_L`), RightArm, LeftLeg (`UpperLeg_L`..`Foot_L`), RightLeg. No IK goals;
- target IK Rig on `Axolotl_Warrior`: same chain names, bones taken from `warrior_bones.json`. Tail, gill, braid or cape bones stay in no chain;
- retargeter: auto-map chains by name, auto-align the target retarget pose to the source, FK only, root translation scaled by hip-height ratio;
- batch-export the 11 clips of `VikingAnimNames` from `/Game/Characters/viking_male` into the axolotl folder, named exactly
  `Axolotl_WarriorCharacterArmature_<Clip>`; save.
Accept: 11 sequences exist on `Axolotl_Warrior_Skeleton`, each with the same length as its source (within one frame). A contact sheet
(Idle, Walk, Walk_Carry, PickUp, SwordSlash, Death at 4 frames each) shows the figure upright, feet near the ground, no limb twisted backwards.

### S6 — Hand sockets ✅   [tricky]   deps: S4
Files: `Scripts/ue/sockets_axolotl_warrior.py` (new); edits `Axolotl_Warrior_Skeleton`
Do: Add sockets `weapon_r` and `staff_r` on the right hand bone, `shield_l` and `bow_l` on the left hand bone (bone names from
`warrior_bones.json`). Read the same four sockets from `Viking_Male_Skeleton` and reproduce their **component-space orientation relative to
the forearm direction**, so the convention holds: +Z along the shaft (up and forward out of the fist), +X the edge / facing side. Position:
centre of the paw (halfway between the hand bone and the paw tip), not the wrist. Socket scale 1. The script must be rerunnable (replace, not duplicate).
Accept: the four sockets exist; in the retargeted Idle pose a debug draw of each socket's axes (screenshot) shows `weapon_r` +Z pointing up-forward
and `shield_l` +X pointing away from the body.
Run S5 and S6 one after the other (one agent drives the editor at a time).

### S7 — Route the villagers to the new model in C++ ✅   [routine]   deps: S5, S6
Files: `UE/Source/TheSagaCutter/TSCUnit.cpp`
Do:
- extend `FVikingModel` with `const TCHAR* HandR; const TCHAR* HandL; float WeaponScale;` and fill the three existing models with
  `Fist_R`, `Fist_L`, `1.f`;
- add `const FVikingModel AxolotlWarrior{ "/Game/Characters/axolotl_warrior", "Axolotl_Warrior", "Axolotl_WarriorCharacterArmature_", <hand R>, <hand L>, 1.f };`
- in `BuildViking`: an unnamed `ETSCRole::Worker` tries `AxolotlWarrior` first; if its mesh does not load, use the existing random
  male / female choice. Named units and raiders are unchanged;
- pass the model to `AddWeapon` (add a parameter or store `const FVikingModel*` in a member): the fallback bone becomes the model's
  `HandR` / `HandL` chosen by whether `Def.FallbackBone` is `Fist_R` or `Fist_L`, and `WorldScale` is multiplied by `WeaponScale`;
- in the material loop of `BuildViking` and in `AddTintedMaterials`: if the MID has no `DiffuseColor` parameter, store white instead of black;
- variety, since all copies share one texture: for the axolotl model multiply the figure scale by a random 0.94..1.06 and set its base
  `MeshCols` entry to a random light tint (each channel 0.85..1.0) instead of pure white. `SetJob` needs no change: `TunicSlot` stays -1,
  so the job recolour is skipped.
Keep the `[WDIAG]` block.
Accept: `sh Scripts/build_editor.sh` reports `Result: Succeeded` with no new warnings.

### S8 — Review in PIE and fix animation and grip ✅ (Ashen point open)   [tricky]   deps: S7
Files: `Scripts/ue/retarget_axolotl_warrior.py`, `Scripts/ue/sockets_axolotl_warrior.py`, `UE/Source/TheSagaCutter/TSCUnit.cpp` (only `WeaponScale` and `DefAxe` / `DefShield` numbers if needed)
Do: Start PIE on the default map and capture close screenshots of the villagers. By day (unarmed): Idle, Walk, Walk_Carry, PickUp,
SwordSlash while chopping. At night (armed; `[WDIAG]` only logs named units and raiders, so temporarily widen its condition to armed
workers): Idle, Run, SwordSlash (3 frames through the swing) with the axe and with the sword, RecieveHit, Death (last frame). Also the
Ashen state. Check and fix, in this order:
0. day work: carry and pick-up poses read correctly with empty paws; arms do not sink into the belly or collar;
1. axe and sword: handle passes through the paw, head up, edge forward in the swing; does not cut through the big head or helmet during `SwordSlash`.
   Fix with the socket transform first, then a static rotation offset on the right arm chain in the retargeter (raise the arm away from the head);
2. shield: face outward, on the forearm side, not buried in the fur collar or the body during Idle / Walk / Run. Fix with the socket, then
   a static offset on the left arm chain;
3. sizes: axe about 45% and shield about 40% of figure height look small against the big head; tune `WeaponScale`;
4. arms through the belly, feet through the ground or floating, head pitching into the chest: fix with per-chain FK rotation mode / static
   offsets and the retarget pose, then re-export the clips;
5. Death: the body ends lying on the ground, not sunk or hovering;
6. Ashen state: the texture visibly darkens / greys with the rest of the village.
Rerun the scripts after each change so the assets always match the scripts. `[WDIAG]` must show `distToSocket` below about 40 for both items.
Accept: the screenshot set shows all six points correct; no `LogAnimation` / missing-asset warnings for the villagers in the log; the `[WDIAG]` condition is restored.

### S9 — Credits ✅   [routine]   deps: S8
Files: `Docs/ThirdPartyAssets.md`
Do: Under "Other credited content", extend the Tripo line: the axolotl warrior mesh, texture and rig are Tripo-generated from the team's
reference art in `Docs/References/FurryAxolotlClasses`; animations are retargeted from the Viking character clips.
Accept: the file reads correctly.

## Final check

`sh Scripts/build_editor.sh` succeeds. Start the editor, PIE on the default map. By day every unnamed villager is an axolotl, about as tall as
the Viking heroes, with slight size and tint variety, walking, carrying and gathering without sliding worse than the Vikings did. Play to the
final night (`StartNight`): the armed defenders show axe or sword in the right paw and shield on the left, swing, react to hits and fall with
the Death clip; the model turns Ashen with the village. Frame rate with all villagers on screen is not visibly lower than before. Rename
`/Game/Characters/axolotl_warrior` temporarily and confirm villagers fall back to the Viking meshes. Named heroes and raiders look unchanged.

## Risks / open questions

1. **Job colour is lost.** Workers showed their job (wood / food) by tunic colour; the single-texture axolotl has no tunic slot.
   Default: accept it. A follow-up can add a tunic mask texture and a second colour parameter.
2. **No Tripo API key.** Default: the user adds `TRIPO_API_KEY` to `.env.local`. Without it, S2 is done by hand in Tripo Studio (generate from
   the S1 images, auto-rig, export FBX to `Art/Tripo/axolotl_warrior/warrior_rigged.fbx`) and the plan continues at S3.
3. **S1 needs an image generator** this session does not have. Default: the user makes the two images. Worst case fallback: crop `warrior.png`
   to the figure, generate as-is and cut the axe and shield off in Blender in S3 (expect a damaged right paw and a poor rig).
4. **Tripo auto-rig may reject or mis-skin a chibi** with a huge head, short legs, tail and mantle. Default fallback: export `Viking_Male` as FBX
   from UE, fit that armature to the axolotl in Blender (scale bones to the new joints), bind with automatic weights and fix by hand. Then the
   bone names equal the Viking ones, S5 becomes trivial (same chain names) and S6 can copy sockets directly.
5. **Style clash.** The existing axe and shield are flat-coloured low-poly; the axolotl is textured fur. Not in this plan. Default: keep the
   existing weapons; a follow-up can generate the reference axe and shield in Tripo as static meshes and add them as first candidates in
   `WeaponAxe` / `WeaponShield` for this model only.
6. **Tail and gills are rigid.** No source animation drives them. Default: leave them static.
7. **Clones.** About 15 identical characters; only scale and tint vary. Default: accept it for the jam. More variety means a second
   Tripo generation (S1-S6 again with another sheet) and a random pick in `BuildViking`.
8. **Villagers hold nothing by day**, while the reference warrior is defined by axe and shield. Default: keep the game's rule
   (weapons only when armed).

## Update: Tripo runs through Tripo Studio in Chrome (decided with the user)

S1 and S2 are done by the orchestrator in the user's signed-in Tripo Studio (https://studio.tripo3d.ai), not by a script; no
`Scripts/tripo_generate.py` and no API key. **Every credit spend needs the user's approval first.** S3 onwards is unchanged and starts from the
exported file in `Art/Tripo/axolotl_warrior/`.

Account facts (free plan, 145 credits at the start):
- S1: Image tool, template "T Pose", model Nano Banana, `warrior.png` as reference, 1 image, 1:1. **10 credits per image.**
- S2: Model tool, HD Model. Listed prices: HD generate 15, texture +10, PBR +5, HD (4K) texture +10, Ultra mesh +15. Use 2K texture, PBR on,
  Ultra off, triangles: about **30 credits**. Auto Rig: **20 credits**.
- Free plan limits: multi-view to 3D is locked (single front image only); **exports are limited to 15 and only for models made with
  AI model H2.5**, so pick H2.5, not H3.1; models are public and licensed for **non-commercial use**; 1-day edit history, so export the
  same day.
- Expected total for one clean pass: about 60 credits, which leaves room for one retry of the image or the model.

## Result of S1 and S2 (done)

- Input image: `Docs/References/FurryAxolotlClasses/tripo_input/warrior_front.png` (made in ChatGPT from `warrior.png`; T-pose, no weapons, no cape).
- The user upgraded Tripo to a paid plan, so the licence and export limits listed above no longer apply. Model: H3.1, Ultra mesh, 2K PBR, 30k triangles. Rig: Humanoid, Mixamo preset.
- **S3 input: `Art/Tripo/axolotl_warrior/rigged/warrior_rigged.fbx`** with textures in `rigged/warrior_rigged.fbm/` (`*_basecolor.jpg`, `*_normal.jpg`, `*_roughness.jpg`, `*_metallic.jpg`, `*_rm.jpg`, all 2048). Unrigged copies: `warrior_raw.glb` (H3.1) and `warrior_raw_h25.glb` (first try, worse).
- Measured in Blender 5.2: armature `Armature` and mesh `tripo_node_f9d2ab39`, both scale 1 and rotation 0; 29,736 triangles, 14,812 vertices, one material; feet at Z = 0, height 0.971 m, width 0.983 m (T-pose), **already facing -Y**; T-pose rest; no animation; max 4 influences; **1 unweighted vertex** (fix in S3).
- Skeleton: 65 bones, standard Mixamo names with the `mixamorig:` prefix, root `mixamorig:Hips`. Chains: spine `Spine, Spine1, Spine2`; head `Neck, Head`; arms `LeftShoulder, LeftArm, LeftForeArm, LeftHand` (and Right); legs `LeftUpLeg, LeftLeg, LeftFoot, LeftToeBase` (and Right); four finger joints per finger. No tail, gill or braid bones: those parts follow the hips, head and spine.
- S3 change: **strip the `mixamorig:` prefix** from all bone names before export (a colon in bone names is awkward in Unreal). The hand bones are then `RightHand` / `LeftHand`; use these in S5, S6 and as `HandR` / `HandL` in S7. Scale to 1.2 m is not needed in S3: the game scales by imported bounds; keep the 0.971 m size and only make sure the FBX imports at 97 cm, not 0.97 cm.
- S5 note: with fingers present, add no finger chains; the Viking source has none, so fingers stay in their rest pose (open paw). S8 may need a slight static finger curl so the paw closes on the axe handle; do that by editing the target retarget pose.

## Result of S3 and S7 (done)

- S3 outputs in `Art/Tripo/axolotl_warrior/`: `SK_AxolotlWarrior.fbx` (root bone `Hips`, no `mixamorig:` prefix, 0.971 m, faces -Y in Blender, FBX unit scale 100), `T_AxolotlWarrior_BaseColor.jpg` / `_Normal.jpg` / `_Roughness.jpg` / `_Metallic.jpg` (separate roughness and metallic, **no ORM texture**), `warrior_bones.json`, `check_*.png`. FBX material name: `M_AxolotlWarrior`.
- The tail (796 vertices) was reweighted from the right thigh to `Hips`. Watch in S5/S8: the right thigh or skirt may poke into the tail underside when the leg swings back.
- S4 change: the material uses separate Roughness and Metallic textures (both sRGB off) instead of ORM; keep metallic from the texture multiplied by a scalar param `MetallicScale` default 0.5 (gold fittings) rather than forcing 0.
- S7 is built (`Result: Succeeded`): model `AxolotlWarrior` = folder `/Game/Characters/axolotl_warrior`, mesh `Axolotl_Warrior`, clips `Axolotl_WarriorCharacterArmature_<Clip>`, fallback hand bones `RightHand` / `LeftHand`. The asset names in S4-S6 must match these exactly.

## Result of S4 (done)

- Assets in `/Game/Characters/axolotl_warrior/`: `Axolotl_Warrior`, `Axolotl_Warrior_Skeleton`, `M_AxolotlWarrior` (params `DiffuseColor`, `MetallicScale`), four `T_AxolotlWarrior_*` textures. 65 bones, root `Hips`, 97.1 cm tall, feet at z = 0, faces +Y like `Viking_Male`, one material slot.
- Import recipe (in `Scripts/ue/import_axolotl_warrior.py`): `unreal.AssetImportTask` + `FbxImportUI` with `convert_scene` True, `convert_scene_unit` True, `import_uniform_scale` 0.01. A default import comes out 9712 cm.
- Editor runs offscreen on port 8010. `execute_python` takes `script: {language: "Python", content: ...}`; run a script file with `exec(open(r"<path>").read())`.
- Python notes: ref-pose bones via `skeleton.get_reference_pose().get_bone_names()`; component-space transforms via `unreal.AnimPoseExtensions.get_bone_pose(pose, name, unreal.AnimPoseSpaces.WORLD)`; `unreal.Rotator` is (roll, pitch, yaw). Stills: spawn the mesh far above the map with a `SceneCapture2D`, then delete the temporary actors without saving the map.

## Result of S5 and S6 (done), and a corrected S4

- **S4 was wrong and is fixed.** The first import left vertices in cm and bones in metres (rest pose looked fine, any animation tore the mesh). Now: Blender exports true cm data (`to_centimetres()` + `FBX_SCALE_NONE`), UE imports with `convert_scene` True, `convert_scene_unit` False, `import_uniform_scale` 1.0, and the import script deletes and recreates mesh, skeleton, IK rig, retargeter and clips on every run. All 65 bones have scale 1, units cm, `Hips` at (0.19, 7.11, 29.93). **After rerunning the import script, rerun the retarget and socket scripts.** Lesson for checks: verify a skinned import by animating it, not by its rest pose.
- S5: `Scripts/ue/retarget_axolotl_warrior.py`; assets `IK_Viking_Male`, `IK_Axolotl_Warrior`, `RTG_Viking_To_Axolotl`, 11 clips `Axolotl_WarriorCharacterArmature_<Clip>` with source lengths. Retargeter: chains Spine, Head, arms, legs (thigh + shin), single-bone feet with FK off (feet follow the shins), no fingers, no IK; Pelvis Motion with `scale_horizontal` 1/90.03 and `scale_vertical` 0.9/90.03 (`PELVIS_V` in the script); Root Motion op off. Viking `Foot_L/R` are free IK-style bones, hence the split leg chains.
- S6: `Scripts/ue/sockets_axolotl_warrior.py` (run with `exec(open(...).read()); await run()`). Sockets `weapon_r`, `staff_r` on `RightHand` and `shield_l`, `bow_l` on `LeftHand`, scale 1, at the paw centre, orientation equal to the Viking sockets in component space (0 degrees error across clips). **They are mesh sockets on `Axolotl_Warrior`, not skeleton sockets**; `DoesSocketExist` finds them.
- Stills: `Art/Tripo/axolotl_warrior/check_retarget_sheet.png`, `check_sockets.png`. Capture helpers in the session scratchpad: `capture_anims.py`, `sheet.py` (the offscreen editor does not tick animation outside PIE; the helper sets clip and position, then resets the mesh to force a pose update).
- Open for S8: feet float 2-3 cm in Idle and dip to -6 cm in Walk and -12 cm in Run (tune `PELVIS_V` or accept); arms close to the fat body; axe held diagonally in front of the face in Idle; axe edge direction unconfirmed; shield sits on the paw, not the forearm (the Viking socket is offset about 15 cm toward the forearm); weapon size against the big head; open fingers on the handle; tail against the right thigh.

## Result of S8 and S9 (done), review notes

- Real PIE on `Map_Saga`: all unnamed workers are `Axolotl_Warrior`, 115-127 cm, individually tinted, feet on the ground, facing their movement, animating; no warnings. `TSCAuto LLLL X 16` reaches the night; the game armed 6 (3 axe, 3 sword, all with shields). Weapon-to-socket distance 21.6 / 22.9 / 1.7, all under 40.
- Tuned in the scripts (both rerun): `PELVIS_V` 0.45 and `PELVIS_Z` 14.5 cm lift, `ARM_ABDUCT` (right 20, left 15 degrees), `FINGER_CURL` 35 degrees; `SOCKET_LEAN` on `weapon_r` (-40, -5 degrees), `SOCKET_SHIFT` on `shield_l` (9 cm toward the elbow, 1.5 cm out). `WeaponScale` stays 1.0; no C++ change in S8.
- Stills: `Art/Tripo/axolotl_warrior/review_day.png`, `review_armed.png`, `review_ashen.png`.
- **Open, not fixed:**
  1. Ashen: unnamed villagers never turn Ashen in the current game (only `Prims` actors with a region do, `TSCGameMode.cpp:358`); the plan's assumption was wrong. If they did, `M_AxolotlWarrior` would only darken about 40%, not desaturate; a real fix is a desaturation scalar in the material plus one line in `ATSCUnit::Refresh`.
  2. Soles dip about 6 cm (Walk) and 9 cm (Run) at the lowest frame; needs foot IK, which this plan excludes.
  3. The weapon brushes the helmet edge at one Run frame; the shield covers the face in the SwordSlash wind-up; mid-swing the axe shaft sweeps sideways.
  4. The shield shows its strap side outward. It does the same on the Vikings (`DefShield`), so it is an existing issue, not part of this change.
  5. Tail against the right thigh was not judged.
- Review of the diff (orchestrator): `TSCUnit.cpp` / `TSCUnit.h` match S7; fallback to the Vikings when the axolotl package is missing is by `DoesPackageExist`; `[WDIAG]` untouched. The fallback itself (renaming the folder) was not exercised in PIE.
- Uncommitted. New content: `UE/Content/Characters/axolotl_warrior/` (11 MB) and `UE/Content/Characters/viking_male/IK_Viking_Male.uasset`. `Art/` is 38 MB of source files (GLB, FBX, textures, stills); decide whether to track it.
