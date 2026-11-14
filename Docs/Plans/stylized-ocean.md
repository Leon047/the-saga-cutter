# Stylized ocean: material, sound, buoyancy

Goal: the flat dark cube east of the shore becomes a stylized animated fjord — rolling waves, toon foam, shore
lapping, a look that changes with day / night / Ashen — with boats and flotsam that float on those same waves and
an ocean soundscape (surf loop, gulls, hull creaks, landing splash). Units that walk through the shallows wade
(legs under water, splashes), boats and the shore throw spray, and the shore in `Map_Saga` is cleaned up to fit.
Out of scope: UE Water plugin, Chaos rigid bodies, translucency / refraction / underwater view, swimming, Niagara,
the music. **Visual and audio only:** no change to rules, economy, unit speed, pathing (`Blocked` grid) or scene
scripts.

Step order by dependency (numbers are not chronological): wave 1 = S1, S2, S3, S4 in parallel; then S9 → S5 → S10
→ S6 → S7 → S11 → S8, one at a time.

## Context

- Jam deadline is 2026-11-15 17:59 PST. Prefer the simple working version of every choice below.
- The UE project is in `UE/` (UE 5.9). One C++ module `UE/Source/TheSagaCutter/`. The world is built at runtime in
  `ATSCGameMode::BuildWorld()` from `ATSCPrimActor`s (bags of engine primitives using `M_SagaFlat` MIDs with `Color`
  / `Emissive` params). Nothing uses collision or physics; everything is moved by code in `ATSCGameMode::Tick`.
- Grid: `CELL = 200`, `GW = 34`, `GH = 24`; `CellF(cx, cy) = ((cx-17)*200, (cy-12)*200, 0)`. +X is east (sea),
  +Y south. The shoreline is the straight line **X = 2200** (`CellF(28, *)`). The play area spans Y -3200..3200.
- Today the water is `WaterActor` (`TSCGameMode.cpp:371-372`): one cube, top face at **Z = 0**, colour `Pal::Water`.
  Shore strip top is at Z ≈ 5.5, dock floor at Z ≈ 34.
- Things on the water: `Longship` (root at `ShipMooring = CellF(30.6, 14.4) + Z -6`, hull along X; in the prologue
  it sails in from +2400 X and already has a hand-written sine bob at `TSCGameMode.cpp:856`), `RaiderBoat` (root Z
  -4, appears at `CellF(40, 12)`, rows to `CellF(33.6, 12)`, later leaves east; often hidden), the dock piles.
- Saga: each prim actor has `SagaAlpha` (0 Living, 1 Ashen). `DockActor->SagaAlpha` is the smooth "the dock region
  is Ashen" value. `ATSCGameMode::AshAmount` is the global 0..1.
- Time: `ATSCGameMode::WorldTime` advances every tick, also while the simulation is frozen. Visuals must use it
  (never the material `Time` node), so waves, floaters and foam stay in sync.
- Camera: `ATSCColonyCameraPawn`, the pawn location is the focus point on the ground, pitch -36°, spring arm
  6000..20000 (default 16500). Seen from that far, wave amplitudes under ~5 units are invisible.
- Lighting is three movable directional lights + height fog, deferred renderer, manual exposure, no sky light, no
  static lighting. A Default Lit opaque material therefore follows day / night by itself.
- Audio so far is music only: `ATSCGameMode::PlayMusic` (`SpawnSound2D`, `bIsUISound = true`). There are no SFX.
  Music is generated with ElevenLabs (`Scripts/generate_music.py`, key in `.env.local`, SDK 2.78 installed,
  outputs under `Docs/Audio/`, then imported into `/Game/Vikings/Audio/`). `ffmpeg` is at `C:\Tools\ffmpeg\bin`.
- Assets are loaded by string (`LoadObject`) and must tolerate a missing asset (see `FlatMaterial()` in
  `TSCPrimActor.cpp`). There is no cook rule for `/Game/Vikings`, so string-only assets can be missing from a
  packaged build; S5 adds the rule.
- Editor work goes through `unreal-mcp` (ue-editor skill; `execute_python` in toolset
  `editor_toolset.toolsets.python_scripting.PythonScriptingTools`). Only one agent drives the editor. Build C++ with
  `sh Scripts/build_editor.sh` **while the editor is closed**.
- Jam rule: every non-jam asset must be listed. Everything here is generated during the jam (script-made texture,
  ElevenLabs SFX) or is engine content, so nothing new needs listing.

### Design (why this shape)

1. **One wave function, owned by C++.** Four directional sine waves, vertical displacement only:
   `H(p, t) = Atten(p.x) * Σ A_i * sin(K_i * dot(D_i, p) - W_i * t)`,
   `Atten(x) = lerp(0.15, 1, smoothstep(0, 1600, x - ShoreX))` (calm at the shore so water never pokes through the
   shore strip or the dock). Vertical-only keeps the CPU height query exact, which is what makes the boats sit on
   the rendered surface. C++ pushes the table to the material as parameters, so there is one source of truth.

   | i | Dir (x, y), normalised in code | Wavelength L | Amplitude A | W = 0.6 * sqrt(980 * K), K = 2π/L |
   |---|---|---|---|---|
   | 0 | (-1.0, 0.25) | 1900 | 9.0 | ≈ 1.08 |
   | 1 | (-0.8, -0.6) | 1100 | 5.0 | ≈ 1.42 |
   | 2 | (-0.3, 1.0) | 620 | 2.5 | ≈ 1.89 |
   | 3 | (-1.0, -0.1) | 330 | 1.2 | ≈ 2.59 |

2. **Mesh = a `UProceduralMeshComponent` grid built in C++**, dense where the camera can look, stretched skirts
   beyond. No mesh asset. Vertex colour R = wave weight (1 dense zone, 0 at the outer edge) so coarse skirt
   triangles stay flat.
3. **Opaque Default Lit material** (`M_SagaOcean`), built by a committed Python script so it is reproducible.
   Opaque materials cannot read scene depth, so shore foam comes from the world X distance to `ShoreX` and boat
   foam from boat positions passed as parameters.
4. **"Physics" = spring-damper buoyancy per floater**, not Chaos. Each floater samples the wave height at bow,
   stern, port and starboard and is pulled toward the resulting height / pitch / roll by an underdamped spring, so
   it lags and overshoots like a hull. Free flotsam also slides down wave slopes and is tethered to its home.
5. **Sound is 2D and mixed by camera position** (RTS convention): the surf loop gets louder as the camera focus
   nears the shore and as it zooms in.
6. **Splashes are pooled droplets, not Niagara**: one `UInstancedStaticMeshComponent` of the engine sphere with a
   foam-coloured `M_SagaFlat` instance, simulated in `ATSCOcean` (ballistic, die on re-entering the water). Same
   idea as the existing embers (`SpawnEmbers` / `SimEmbers`), and it matches the primitive art style.
7. **Wading is "sink the unit"**: the water is opaque, so lowering a unit's root Z below the surface hides its legs.
   Units always stand at Z = 0 today (`ATSCUnit::Teleport`, `StepToward` keeps Z). Where they already walk in
   water: the shallows cells X 28–29 (world X 2200..2600, never blocked), the prologue landfall
   (`CellF(29.4, 11.3..12)` while the dock is hidden), Ashen-dock raiders (`CellF(29.6, 9..)`), and the dock
   footprint once it has burned. The seabed is modelled as a slope: `Depth = clamp((X - ShoreX) / 400, 0, 1) * 55`
   (knee to waist). No speed change (visual only).

## Steps

### S1 — Generate the ocean pattern texture ✅   [routine]   deps: none
Files: `Scripts/generate_ocean_textures.py` (new), `Docs/Art/Ocean/T_OceanPatterns.png` (new)
Do: a standalone Python script (numpy + Pillow, both installed; run with `py`). Output one 1024×1024 RGB PNG that
tiles seamlessly in both axes (compute distances with wrap-around):
- **R** = Worley cell edges: `F2 - F1` of 9×9 jittered feature points, remapped so thin bright lines sit on cell
  borders (`1 - smoothstep(0, 0.12, F2 - F1)`), i.e. a foam-net pattern.
- **G** = smooth tiling value noise, 3 octaves (base period 4), normalised to 0..1.
- **B** = same as R with 26×26 points (fine sparkle / small foam).
Fixed seed. Print the output path. Keep it under ~80 lines.
Accept: `py Scripts/generate_ocean_textures.py` writes the PNG; a script-side assert checks that the left/right and
top/bottom edge columns differ from their wrapped neighbours by < 0.08 mean (seamless); Read the PNG and confirm a
net of lines in R.

### S2 — Generate ocean sound effects with ElevenLabs ✅   [routine]   deps: none
Files: `Scripts/generate_sfx.py` (new), `Docs/Audio/SFX/*.wav` (new)
Do: model the script on `Scripts/generate_music.py` (same `.env.local` loading order, same error handling, `--only`
and `--takes` args). Use `client.text_to_sound_effects.convert(text=..., duration_seconds=..., prompt_influence=0.5,
model_id="eleven_text_to_sound_v2", loop=<bool>, output_format="mp3_44100_128")`, join the chunks, write
`Docs/Audio/SFX/<name>_take<N>.mp3`, then convert to 48 kHz 16-bit WAV with `ffmpeg -y -i in.mp3 -ar 48000 -ac 2
-c:a pcm_s16le out.wav` (mono `-ac 1` for the one-shots) and delete the mp3. Sounds:

| name | seconds | loop | prompt |
|---|---|---|---|
| `ocean_loop` | 30 | yes | Calm cold northern fjord, small waves washing steadily onto a pebble shore, soft continuous water movement, faint breeze. No birds, no voices, no music, no thunder. Even level, seamless loop. |
| `gull_1`, `gull_2`, `gull_3` | 4 | no | A single distant seagull calling two or three times over open water, far away, natural outdoor reverb, nothing else. (vary: "one long cry" / "short chattering cries") |
| `boat_creak_1`, `boat_creak_2` | 3 | no | Wooden ship hull creaking slowly as it rocks at its mooring, rope strain, close and dry, no water splash, no voices. |
| `boat_land` | 4 | no | A wooden boat keel sliding onto a gravel beach with a soft splash of shallow water, then settling. No voices. |
| `wade_1`, `wade_2` | 2 | no | One single footstep wading through knee-deep calm water, a small soft splash, close, nothing else. |

Run it once with 1 take each (2 takes for `ocean_loop`). This spends ElevenLabs credits: do not re-run sounds that
already exist on disk unless `--force` is passed (add that flag).
Accept: 10 WAV files exist, each plays (`ffprobe` shows the expected duration ±1 s, 48000 Hz). For `ocean_loop`,
check the seam: the RMS of the last 50 ms and the first 50 ms differ by < 6 dB. Report if `loop=True` was rejected
(see Risks).

### S3 — Add `ATSCOcean`: mesh, waves, floaters, audio ✅   [tricky]   deps: none
Files: `UE/Source/TheSagaCutter/TSCOcean.h` (new), `UE/Source/TheSagaCutter/TSCOcean.cpp` (new),
`UE/Source/TheSagaCutter/TheSagaCutter.Build.cs`, `UE/TheSagaCutter.uproject`
Do:
- `Build.cs`: add `"ProceduralMeshComponent"` to `PrivateDependencyModuleNames`. `.uproject`: add plugin
  `{"Name": "ProceduralMeshComponent", "Enabled": true}`.
- Header (keep the project's style: tabs, `// The Saga Cutter - game jam project.` first line, no `THESAGACUTTER_API`
  needed):

```cpp
struct FTSCOceanInput
{
	float Time = 0.f;                 // ATSCGameMode::WorldTime
	float Ash = 0.f;                  // 0..1, dock region saga alpha
	bool bDaylight = true;            // gulls only cry by day
	FVector Focus = FVector::ZeroVector;  // camera focus point on the ground
	float Zoom01 = 0.5f;              // 0 = zoomed in, 1 = zoomed out
	FLinearColor SunColor = FLinearColor::White;
	float SunIntensity = 1.f;
};

struct FTSCFloater
{
	TWeakObjectPtr<AActor> Actor;
	float HalfLen = 100.f;   // along the actor's local X
	float HalfWidth = 50.f;  // along local Y
	float RestZ = 0.f;       // root Z above the surface at rest (negative = sits lower)
	float FoamRadius = 0.f;  // > 0: reported to the material as a foam ring
	bool bFree = false;      // flotsam: the ocean also moves it in XY
	FVector2D Home = FVector2D::ZeroVector;
	// state
	float Z = 0.f, VelZ = 0.f, Pitch = 0.f, VelPitch = 0.f, Roll = 0.f, VelRoll = 0.f;  // degrees
	FVector2D Vel = FVector2D::ZeroVector;
	FVector2D LastXY = FVector2D::ZeroVector;
	bool bInit = false;
};

UCLASS()
class ATSCOcean : public AActor
{
	GENERATED_BODY()
public:
	ATSCOcean();
	/** Builds the grid mesh and the material instance. ShoreX = world X of the waterline, SurfaceZ = rest level. */
	void Build(float InShoreX, float InSurfaceZ);
	/** Pushes parameters, steps every floater, mixes the audio. Call once per tick after the game moved its boats. */
	void Sim(float Dt, const FTSCOceanInput& In);
	/** World Z of the water surface at P for the time of the last Sim call. */
	float HeightAt(const FVector2D& P) const;
	/** Registers an actor that rides the waves; the caller keeps moving it in XY (unless bFree), the ocean owns Z, pitch and roll. */
	int32 AddFloater(AActor* Actor, float HalfLen, float HalfWidth, float RestZ, float FoamRadius = 0.f, bool bFree = false);
	/** One-shot: keel on gravel (prologue landfall, raiders landing). S9 adds a spray burst at At. */
	void PlayLanding(const FVector2D& At, float Volume = 1.f);

	UPROPERTY(EditAnywhere, Category = "Saga|Ocean") float SurfVolume = 0.45f;
	UPROPERTY(EditAnywhere, Category = "Saga|Ocean") float SfxVolume = 0.5f;
private:
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Mesh;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> MID;
	UPROPERTY() TObjectPtr<UAudioComponent> Surf;
	UPROPERTY() TObjectPtr<USoundBase> SurfLoop;
	UPROPERTY() TArray<TObjectPtr<USoundBase>> Gulls;
	UPROPERTY() TArray<TObjectPtr<USoundBase>> Creaks;
	UPROPERTY() TObjectPtr<USoundBase> Landing;
	TArray<FTSCFloater> Floaters;
	float ShoreX = 0.f, SurfaceZ = 0.f, Time = 0.f;
	float GullTimer = 6.f, CreakCooldown = 0.f, Proximity = 0.f;
};
```
- Waves: a file-static table of the four waves from Design §1 (Dir normalised at static init, `K = 2π/L`,
  `W = 0.6 * sqrt(980 * K)`). `HeightAt` returns `SurfaceZ + Atten(P.X) * Σ A sin(K * dot(D, P) - W * Time)`. Add a
  private `FVector2D SlopeAt(P)` (analytic gradient of the same sum, ignoring the derivative of Atten).
- `Build`:
  - Grid lines in X: from `ShoreX - 300` to `ShoreX + 7200` every 100; then skirt lines at +9000, +12000, +18000,
    +30000, +50000 beyond `ShoreX`. Grid lines in Y: -6000..6000 every 100; skirts at ±7500, ±10000, ±15000,
    ±25000, ±45000. Vertices at `(X, Y, SurfaceZ)` in world space (actor at origin), normal +Z, tangent +X,
    `UV0 = (X, Y) / 1000`, vertex colour R = 1 inside the dense zone, on skirts `1 - skirtIndex / 5` (0 on the outer
    ring). Two triangles per quad, winding so the top faces +Z (verify: the surface must be visible from above).
    `CreateMeshSection_LinearColor(0, ..., bCreateCollision = false)`. Roughly 84 × 131 vertices.
  - Component: `SetCollisionEnabled(NoCollision)`, `SetCastShadow(false)`, `bAffectDistanceFieldLighting = false`,
    `SetBoundsScale(1.2f)`.
  - Material: `LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Vikings/Environment/Ocean/M_SagaOcean.M_SagaOcean"))`;
    if missing fall back to `/Game/Vikings/Environment/M_SagaFlat.M_SagaFlat` with `Color = (0.015, 0.05, 0.08)`
    (the old `Pal::Water`) and log one `LogSaga` warning. Create the MID and push the static parameters once:
    scalar `ShoreX`, `SurfaceZ`; vectors `Wave0..Wave3 = (Dir.X, Dir.Y, K, A)`; vector `WaveSpeed = (W0, W1, W2, W3)`.
  - Sounds: `LoadObject<USoundBase>` for `/Game/Vikings/Audio/Ambience/SW_OceanLoop`, `SW_Gull_1..3`,
    `SW_BoatCreak_1..2`, `SW_BoatLand` (object path `Name.Name`); skip nulls silently. If `SurfLoop` exists:
    `Surf = UGameplayStatics::SpawnSound2D(this, SurfLoop, 0.01f, 1.f, 0.f, nullptr, false, false)`, set
    `bIsUISound = true`, `FadeIn(2.f, 0.01f)`.
- `Sim(Dt, In)`:
  - `Time = In.Time`. MID per tick: scalar `OceanTime = Time`, `Ash = In.Ash`; vector `SunGlint = SunColor *
    SunIntensity`; vectors `Foam0`, `Foam1` = `(X, Y, FoamRadius, Strength)` of the first two visible floaters with
    `FoamRadius > 0` (Strength 1; 0 when the actor is hidden or there is none).
  - Floaters (skip when `Actor` is invalid or hidden, and clear `bInit` so it snaps when it reappears). Substep with
    `h = min(Dt, 1/60)` until Dt is used up. Read actor XY and yaw. If `!bInit` or the actor jumped more than 400
    units in XY since the last tick: snap state to target, zero velocities.
    Sample `HeightAt` at centre ± `HalfLen` along the yaw direction (bow/stern) and ± `HalfWidth` across
    (port/starboard). Targets: `Z* = mean of the four + RestZ`, `Pitch* = atan2(hBow - hStern, 2*HalfLen)` in
    degrees, `Roll* = atan2(hPort - hStarboard, 2*HalfWidth)` in degrees (pick the signs so the hull tilts with the
    surface; verify visually in S6). Spring, semi-implicit Euler: `Vel += (S * (Target - Cur) - D * Vel) * h;
    Cur += Vel * h` with `S = 22, D = 3.6` for Z and `S = 16, D = 2.6` for pitch and roll. Clamp pitch to ±12°,
    roll to ±16°.
    `bFree`: `Vel += (-SlopeAt(P) * 900 + (Home - P) * 0.35 - Vel * 0.8) * h; P += Vel * h`, and slowly yaw by
    `Vel.X * 0.02` degrees per step.
    Apply with `SetActorLocationAndRotation(FVector(P, Z), FRotator(Pitch, Yaw, Roll))`.
  - Audio: `Proximity = 1 - saturate((ShoreX - In.Focus.X - 400) / 5200)`;
    surf volume `= SurfVolume * lerp(0.15, 1, Proximity) * lerp(1, 0.6, In.Zoom01) * lerp(1, 0.75, In.Ash)`, apply
    with `Surf->SetVolumeMultiplier` only when it changed by > 0.01.
    Gulls: `GullTimer -= Dt`; at 0, if `In.bDaylight && In.Ash < 0.5 && Gulls.Num()`, `PlaySound2D` a random gull at
    volume `SfxVolume * FRandRange(0.35, 0.8) * lerp(0.4, 1, Proximity)`, pitch `FRandRange(0.92, 1.1)`; reset the
    timer to `FRandRange(9, 24)` either way.
    Creaks: `CreakCooldown -= Dt`; if any non-free visible floater has `|VelPitch| + |VelRoll| > 3` deg/s and the
    cooldown is over and `Proximity > 0.35`: play a random creak at `SfxVolume * 0.6 * Proximity`, pitch
    `FRandRange(0.9, 1.1)`, cooldown `FRandRange(4, 9)`.
  - `PlayLanding`: `PlaySound2D(Landing, SfxVolume * Volume * lerp(0.4, 1, Proximity))` if loaded.
- The actor never ticks by itself (`PrimaryActorTick.bCanEverTick = false`); the game mode calls `Sim`.
Accept: `sh Scripts/build_editor.sh` prints no `error` lines and a succeeded result (the class is not used yet).

### S4 — Add flotsam builders ✅   [routine]   deps: none
Files: `UE/Source/TheSagaCutter/TSCBuilders.h`, `UE/Source/TheSagaCutter/TSCBuilders.cpp`
Do: add two recipes next to `BuildRaiderBoat`, same style as the neighbours (flat `Pal` colours, `SetAshen`,
`SetVis`). Origin at the waterline, long axis along local X so the ocean's pitch works:
- `void BuildBarrel(ATSCPrimActor* A);` — a barrel lying on its side: cylinder (rotated so its axis is local X)
  about 70 long × 46 wide, `Pal::Plank`, two thin `Pal::Iron` hoop cylinders, centre ~8 above the origin so it sits
  half in the water. All parts `ETSCVis::LivingOnly`.
- `void BuildDriftwood(ATSCPrimActor* A, int32 Seed);` — 2–3 charred planks / a split log (thin cubes ~160 × 22 ×
  8, slight yaw offsets from `Seed`), `Pal::Char * 1.6f`, with a faint ember glow on one part via `SetGlow`
  (`FLinearColor(2.f, 0.6f, 0.1f)` in both sagas). All parts `ETSCVis::AshenOnly`: the wreckage only exists once
  the dock has burned.
Accept: `sh Scripts/build_editor.sh` has no `error` lines.

### S9 — Add splash droplets to `ATSCOcean` ✅   [tricky]   deps: S3
Files: `UE/Source/TheSagaCutter/TSCOcean.h`, `UE/Source/TheSagaCutter/TSCOcean.cpp`
Do:
- Members: `UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Drops;`, a plain array
  `struct FDrop { FVector Pos; FVector Vel; float Life; float Size; }; TArray<FDrop> DropSim;` (max 200),
  `UPROPERTY() TArray<TObjectPtr<USoundBase>> Wades;`, `float WadeSoundCooldown = 0.f, ShoreSprayTimer = 0.f;`.
- `Build`: create `Drops` with `/Engine/BasicShapes/Sphere.Sphere`, no collision, no shadow, a MID of
  `/Game/Vikings/Environment/M_SagaFlat.M_SagaFlat` with `Color = (0.80, 0.90, 0.92)` (skip the colour if the
  material is missing). Load `SW_Wade_1..2` from `/Game/Vikings/Audio/Ambience/` like the other sounds.
- Public API:
```cpp
	/** A burst of droplets at the surface. Strength ~0.3 (footstep) .. 2 (boat landing). Dir biases the throw (may be zero). */
	void Splash(const FVector2D& At, float Strength, const FVector2D& Dir = FVector2D::ZeroVector);
	/** Footstep in the shallows: small splash plus a rate-limited wade sound. */
	void WadeStep(const FVector2D& At, const FVector2D& MoveDir);
```
  `Splash`: spawn `RoundToInt(6 * Strength)` drops (respect the cap) at `(At, HeightAt(At) + 4)` with velocity =
  random horizontal direction × `FRandRange(50, 150) * sqrt(Strength)` + `Dir.GetSafeNormal() * 90 * sqrt(Strength)`
  + up `FRandRange(170, 340) * sqrt(Strength)`; `Life = FRandRange(0.5, 0.9)`; `Size = FRandRange(0.05, 0.11) *
  (0.8 + 0.4 * Strength)` (sphere scale).
  `WadeStep`: `Splash(At, 0.35f, MoveDir)`; if `WadeSoundCooldown <= 0 && Proximity > 0.3 && Wades.Num()`, play a
  random wade sound at `SfxVolume * 0.45 * Proximity`, pitch `FRandRange(0.9, 1.15)`, cooldown `FRandRange(0.3, 0.5)`.
- `PlayLanding(At, Volume)`: also `Splash(At, 2.f)` twice with a small offset.
- `Sim`: step drops: `Vel.Z -= 620 * Dt; Pos += Vel * Dt; Life -= Dt`; remove when `Life <= 0` or
  (`Vel.Z < 0 && Pos.Z < HeightAt(Pos.xy)`). Rebuild the instances each tick only while there are drops or the
  instance count is non-zero: simplest correct form is `ClearInstances()` then `AddInstances(Transforms, false,
  /*bWorldSpace*/ true)` with scale `Size * saturate(Life * 4)`.
  Emitters inside `Sim`:
  - bow spray: every non-free visible floater whose XY speed (from `LastXY`) exceeds 60 u/s emits, every 0.08 s,
    `Splash(bow point, 0.5f, sideways alternating left/right)`; bow = centre + travel direction × `HalfLen`.
  - shore break: `ShoreSprayTimer -= Dt`; at 0 reset to `FRandRange(0.1, 0.25)` and
    `Splash((ShoreX + FRandRange(40, 100), In.Focus.Y + FRandRange(-2600, 2600)), 0.3f, FVector2D(-1, 0))`.
  - free flotsam: nothing.
Accept: `sh Scripts/build_editor.sh` has no `error` lines.

### S10 — Make units wade in the shallows ✅   [routine]   deps: S5
Files: `UE/Source/TheSagaCutter/TSCUnit.h`, `UE/Source/TheSagaCutter/TSCUnit.cpp`
Do: add private members `float WadeStepTimer = 0.f; FVector LastWadePos = FVector::ZeroVector;` and a private
`void UpdateWading(float Dt);`. Call it as the **first statement of `ATSCUnit::Sim`** (so early returns cannot skip
it) and also from `Idle(float Dt)` (move that inline body to the .cpp if needed). `Teleport` must also set Z to
`-GM->WadeDepthAt(Pos)` instead of 0 when `GM` is set.
`UpdateWading`: return if `!GM || bGone || bSheltered || IsHidden()`. `Depth = GM->WadeDepthAt(GetActorLocation())`
(0 on land). Move the actor's Z toward `-Depth` with `FMath::FInterpTo(Z, -Depth, Dt, 8.f)` (keep XY). If
`Depth > 6 && !bDead && !bDowned`: measure the 2D distance moved since `LastWadePos`; if the unit moved more than
`20 * Dt` this step, `WadeStepTimer -= Dt` and at 0 call `GM->WadeStep(Loc2D(), direction moved)` and reset the
timer to 0.3. Update `LastWadePos`. Do not touch `Speed`, pathing or combat.
Accept: `sh Scripts/build_editor.sh` has no `error` lines.

### S11 — Shore pass in `Map_Saga` ✅   [tricky]   deps: S7
Files: `UE/Content/Vikings/Maps/Map_Saga.umap`, `Scripts/ue/shore_pass.py` (new)
Do (editor running, one agent): the level holds hand-placed props (banners, crates, fences, the north/south log
walls, mushrooms); the game mode spawns everything else at runtime. Write the edits as a re-runnable Python script
that tags every actor it adds with the actor tag `OceanShorePass` and deletes actors with that tag first.
1. List every level actor with X > 1600 (class, mesh, location, bounds) and print the list.
2. Conflicts: anything that is a leftover water / sea / backdrop plane, or that sits below Z = 0 east of X = 2200
   and now pokes through or z-fights with the ocean — delete it (print what and why). Anything hand-placed east of
   X = 2200 that is meant to stand on the dock or shore stays.
3. The log walls end at the shore: check in a PIE screenshot that their east ends meet the water cleanly (no gap
   showing the dark ground base, no wall floating over water). Move or extend only if clearly broken.
4. Dressing, at most 10 new actors, using only meshes already in `/Game/Props/` (crates, fence set pieces): a few
   crates and a broken fence piece on the shore strip (X 1800..2200) north and south of the dock, away from cell
   rows Y 11–12 (the path to the dock; world Y -200..200). Keep them at grid X < 27.5 (world X < 2100) **and** inside the
   palisade so `CollectLevelProps` hides them during the prologue like the other props — or skip a prop that
   cannot satisfy that.
5. Save the level. Do not touch lights, the wind source, the game mode override or anything west of X = 1600.
Accept: the printed before/after actor lists; a PIE screenshot of the shore with no z-fighting and no gap between
land and water along the whole visible shoreline; prologue screenshot at T≈3 s shows none of the new props.

### S5 — Wire the ocean into the game mode ✅   [routine]   deps: S3, S4, S9
Files: `UE/Source/TheSagaCutter/TSCGameMode.h`, `UE/Source/TheSagaCutter/TSCGameMode.cpp`, `UE/Config/DefaultGame.ini`
Do:
- Header: forward-declare `class ATSCOcean;`. Replace `UPROPERTY() TObjectPtr<ATSCPrimActor> WaterActor;` with
  `UPROPERTY() TObjectPtr<ATSCOcean> Ocean;` and add `UPROPERTY() TArray<TObjectPtr<ATSCPrimActor>> Flotsam;` and
  `bool bProLandingPlayed = false;`.
- `BuildWorld()`: delete the two `WaterActor` lines (`TSCGameMode.cpp:371-372`) and instead
  `Ocean = GetWorld()->SpawnActor<ATSCOcean>(ATSCOcean::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
  Ocean->Build(CellF(28.f, 12.f).X, 0.f);`
- After the longship and raider boat are created (`:580-593`):
  `Ocean->AddFloater(Longship, 300.f, 55.f, -6.f, 420.f);`
  `Ocean->AddFloater(RaiderBoat, 190.f, 45.f, -4.f, 300.f);`
  then spawn flotsam with `SpawnPrim(pos, TSCRegion::Dock)` (region Dock so `TransitionRegion` flips them), call
  the builder, `Refresh()`, add to `Flotsam`, and `Ocean->AddFloater(A, 40.f, 22.f, 0.f, 0.f, true)` (driftwood:
  `80.f, 14.f`): three barrels at `CellF(29.3, 9.6)`, `CellF(30.4, 15.9)`, `CellF(32.2, 10.4)` and four driftwood
  pieces at `CellF(29.8, 10.3)`, `CellF(31.0, 13.4)`, `CellF(32.6, 12.6)`, `CellF(30.2, 8.4)`, each with a yaw of
  `Rand01(I, 7) * 360`. Add the barrels to `ProDressing` (they must not float there before the settlers land).
- Prologue (`:856`): drop the hand-written Z bob — set the longship to
  `ShipMooring + FVector(ProShipRun * Left, 0.f, 0.f)`; the ocean now owns Z. In `StartPrologue()` set
  `bProLandingPlayed = false`; in `SimPrologue` when `T >= ProTLand - 0.6f && !bProLandingPlayed` set it and call
  `Ocean->PlayLanding(FVector2D(Longship->GetActorLocation()) - FVector2D(300.f, 0.f));` (the west end of the hull).
- Night raid (`:2047`, inside the `Once(3, 9.f)` branch): add
  `Ocean->PlayLanding(FVector2D(CellF(bDockAshen ? 29.6f : 31.5f, 11.5f)), 0.8f);`.
- Wading services for units (public, next to "services for units" in the header):
  `float WadeDepthAt(const FVector& P) const;` and `void WadeStep(const FVector& P, const FVector& Dir);`.
  `WadeDepthAt`: `ShoreX = CellF(28, 0).X`; return 0 if `P.X <= ShoreX`. Return 0 if the unit is on standing dock
  planks: `DockActor && !DockActor->IsHidden() && DockActor->SagaAlpha < 0.5f` and P inside the dock footprint
  (X from `CellCorner(26, 11).X` to `+6 * CELL`, Y from `CellCorner(26, 11).Y` to `+2 * CELL`). Otherwise
  `FMath::Clamp((P.X - ShoreX) / 400.f, 0.f, 1.f) * 55.f`.
  `WadeStep`: forward to `Ocean->WadeStep(FVector2D(P), FVector2D(Dir))` if `Ocean`.
- `Tick()`: just before `UpdateAtmosphere(RealDt);` fill an `FTSCOceanInput` and call `Ocean->Sim(RealDt, In)`:
  `Time = WorldTime`; `Ash = DockActor ? DockActor->SagaAlpha : 0`; `bDaylight = Phase == Day || Phase == Title`;
  `Focus` = the player pawn location (same lookup as the `bCameraSet` block; fall back to `CellF(17, 12)`);
  `Zoom01 = saturate((dist(camera location, Focus) - 6000) / 14000)` using
  `PC->PlayerCameraManager->GetCameraLocation()`; `SunColor = Sun->GetLightColor()`, `SunIntensity = Sun->Intensity`.
  Null-check `Ocean`.
- `#include "TSCOcean.h"`.
- `UE/Config/DefaultGame.ini`: under `[/Script/UnrealEd.ProjectPackagingSettings]` (create the section if absent)
  add `+DirectoriesToAlwaysCook=(Path="/Game/Vikings")` so string-loaded materials and sounds reach the package.
Accept: `sh Scripts/build_editor.sh` has no `error` lines; `grep -n WaterActor UE/Source` returns nothing.

### S6 — Build `M_SagaOcean` in the editor ✅   [tricky]   deps: S1, S10
Files: `Scripts/ue/build_ocean_assets.py` (new), `UE/Content/Vikings/Environment/Ocean/T_OceanPatterns.uasset`
(new), `UE/Content/Vikings/Environment/Ocean/M_SagaOcean.uasset` (new)
Do: start the editor (ue-editor skill, map `/Game/Vikings/Maps/Map_Saga`). Write an idempotent editor Python script
(delete-and-recreate the material each run) and run it through `execute_python`
(`exec(open(r"<abs path>").read())`). The script:
- Imports `Docs/Art/Ocean/T_OceanPatterns.png` to `/Game/Vikings/Environment/Ocean/T_OceanPatterns` with
  `AssetImportTask` (automated, replace existing); then `srgb = False`,
  `compression_settings = TextureCompressionSettings.TC_MASKS`, save.
- Creates material `/Game/Vikings/Environment/Ocean/M_SagaOcean` with `MaterialFactoryNew` +
  `MaterialEditingLibrary`: opaque, Default Lit, `tangent_space_normal = False`, two-sided off. Use small Python
  helpers (`add(cls, x, y)`, `link(a, out, b, inp)`, `scalar(name, default)`, `vector(name, default)`) to keep the
  graph code readable. Parameters and defaults (C++ overrides the ones marked *):
  scalars `OceanTime`* 0, `ShoreX`* 2200, `SurfaceZ`* 0, `Ash`* 0, `NormalBoost` 3.0, `FoamScale` 1400;
  vectors `Wave0..3`* and `WaveSpeed`* (defaults from Design §1), `SunGlint`* (1,1,1), `Foam0`*, `Foam1`* (0,0,0,0),
  `ColDeep` (0.010, 0.045, 0.085), `ColShallow` (0.04, 0.26, 0.30), `ColFoam` (0.80, 0.90, 0.92),
  `ColAshDeep` (0.030, 0.030, 0.034), `ColAshFoam` (0.30, 0.28, 0.26).
- Graph (P = `WorldPosition` set to "absolute world position excluding material offsets", `.xy`):
  1. Per wave i: `ph = dot(P, Wave_i.rg) * Wave_i.b - OceanTime * WaveSpeed[i]`; `h_i = Wave_i.a * sin(ph)`;
     `g_i = Wave_i.a * Wave_i.b * cos(ph) * Wave_i.rg`. `H = Σ h_i`, `G = Σ g_i` (2D).
     `Atten = lerp(0.15, 1, smoothstep(0, 1600, P.x - ShoreX)) * VertexColor.R`.
     **World Position Offset** = `(0, 0, H * Atten)`. **Normal** = `normalize(float3(-G * Atten * NormalBoost, 1))`.
     (Use `sin(x)` on radians: the Sine node's period must be set to 2π, or use a one-line Custom node — either is
     fine, but the result must equal `sin` in radians so it matches C++.)
  2. Depth colour: `d = P.x - ShoreX`; `Depth = pow(saturate(d / 2600), 0.6)`;
     `Water = lerp(ColShallow, ColDeep, Depth) + ColShallow * 0.25 * saturate(H * Atten / 14)` (lighter crests).
  3. Foam net: sample `T_OceanPatterns` twice at `P / FoamScale + G_noise_offset`, panned by `OceanTime` in two
     different directions ((0.012, 0.004) and (-0.007, 0.010), second at 1.37× scale); distort each UV by the G
     channel (±0.04). `Net = smoothstep(0.55, 0.75, R1 * R2 * 2)`. Hard-edged on purpose (toon look).
     `FoamOpen = Net * (0.25 + 0.75 * saturate(H * Atten / 10))` — more foam on crests.
  4. Shore: `dn = d + (G1 - 0.5) * 260`; lapping bands `Lap = smoothstep(0.80, 0.90, frac(dn / 460 + OceanTime *
     0.11)) * saturate(1 - dn / 1300)` (bands travel toward the shore and fade out to sea); edge
     `Edge = 1 - smoothstep(40, 110 + 40 * sin(OceanTime * 0.9 + P.y / 300), dn)`.
  5. Boat rings, for `Foam0` and `Foam1`: `r = length(P - Foam.rg) / max(Foam.b, 1)`;
     `Ring = saturate(1 - abs(r - 1) * 3.5) * step(0.35, B1) * Foam.a`.
  6. `Foam = saturate(FoamOpen + Lap + Edge + Ring0 + Ring1)`.
     `Base = lerp(lerp(Water, ColAshDeep, Ash * 0.85), lerp(ColFoam, ColAshFoam, Ash), Foam)`.
     **Base Color** = Base. **Roughness** = `lerp(0.22, 0.9, Foam)`. **Specular** = 0.5. **Metallic** = 0.
     **Emissive** = `SunGlint * 0.05 * step(0.93, B1 * B2 * 4) * (1 - Foam) * (1 - Ash)` (tiny sparkles).
  Recompile and save the material. Print a summary line (`[ocean] material built, N expressions`).
- Then verify in PIE (StartPIE, console `tsc.Prologue 0` is not needed — just let the title prologue run): take
  screenshots with the viewport/screenshot tools and check them yourself.
  You may tune the *default* numbers in the script (colours, thresholds, `NormalBoost`, `FoamScale`) and the wave
  amplitudes in `TSCOcean.cpp` (then rebuild with the editor closed) until Accept holds; report every value changed.
Accept: the editor log has no material compile errors and no `M_SagaOcean` warning from `LogSaga`; a PIE
screenshot of the dock area shows (a) blue-green water lighter at the shore and darker out to sea, (b) visible
moving wave shading, (c) white foam along the shoreline and a ring around the longship, (d) the longship and
barrels sitting on the surface — not sunk, not hovering — and tilting; no water visible on top of the shore strip
or the dock planks. Two screenshots 2 s apart differ (it animates).

### S7 — Import the ocean sounds ✅   [routine]   deps: S2, S6
Files: `Scripts/ue/import_ocean_audio.py` (new), `UE/Content/Vikings/Audio/Ambience/SW_OceanLoop.uasset`,
`SW_Gull_1..3.uasset`, `SW_BoatCreak_1..2.uasset`, `SW_BoatLand.uasset`, `SW_Wade_1..2.uasset` (new)
Do: with the editor from S6 still running (or start it), run an editor Python script that imports the WAVs from
`Docs/Audio/SFX/` into `/Game/Vikings/Audio/Ambience/` with `AssetImportTask` under the names above (for
`ocean_loop` use take 2: S2 measured the better seam on it), sets `looping = True` on
`SW_OceanLoop` only, and saves. Idempotent (replace existing).
Accept: the nine assets exist (`EditorAssetLibrary.does_asset_exist`); `SW_OceanLoop.looping` is True, duration
28–31 s; in PIE the editor log shows no load warnings for them and `au.Debug.Sounds 1` (or the audio debug
output) lists `SW_OceanLoop` as playing.

### S8 — Look and mix pass ✅   [tricky]   deps: S6, S7, S11
Files: `Scripts/ue/build_ocean_assets.py`, `UE/Source/TheSagaCutter/TSCOcean.cpp` (constants only),
`UE/Content/Vikings/Environment/Ocean/M_SagaOcean.uasset`
Do: one tuning pass in PIE over the four states the game shows, with a screenshot of each:
1. title prologue while the ship sails in (camera arm 10500 at the dock),
2. day at the default camera (arm 16500, focus `CellF(17, 12)`) — the water is only a band on the east side here,
   it must still read as water and must not out-shine the village,
3. final night (`TSCSkip` exec advances phases; `TSCAuto` plays the game by itself),
4. Ashen dock (`TSCCut 1 1` during the dock scene, or `TSCAuto LLLA none 4`): water grey and oily, charred
   driftwood floating, barrels gone, no sparkles.
Also check wading and spray: in the prologue the settlers come ashore with their legs under water and small
splashes, and are back at ground level on the shore strip; the sailing ship throws bow spray; droplets are
readable at arm 10500 but not a blizzard at arm 16500 (tune drop `Size`, counts and the shore-break interval).
Fix only what is wrong: foam too busy or invisible, waves invisible at arm 16500 (raise `NormalBoost` before
amplitudes), night water glowing, boats jittering (raise damping `D`) or lifeless (lower it), surf drowning the
music (music is 0.6; surf should sit clearly under it). Keep C++ and material wave numbers identical — they come
from the same table, so only edit the table in `TSCOcean.cpp`.
Accept: the four screenshots saved to `Docs/Screenshots/ocean_{prologue,day,night,ashen}.png`; a short list of
values changed; build clean if C++ changed.

## Follow-up (user feedback after the first pass)

"The sound of water can be a bit quieter and aligned with waves coming to shore. Shoreline is flat - I want it
more realistically shaped, maybe fractal."

### S12 — Fractal shoreline ✅   [tricky]   deps: S8
Files: `UE/Source/TheSagaCutter/TSCOcean.h`, `UE/Source/TheSagaCutter/TSCOcean.cpp`,
`UE/Source/TheSagaCutter/TSCGameMode.cpp`, `Scripts/ue/build_ocean_assets.py`,
`UE/Content/Vikings/Environment/Ocean/M_SagaOcean.uasset`
Design: the waterline stops being the straight line X = ShoreX and becomes `X = ShoreX + S(y)`, a 1D fractal
(sum of octaves), owned by C++ and pushed to the material, exactly like the wave table.
- `S(y) = Bias + Σ_{i=0..4} A_i * sin(F_i * y + P_i)` with `A_i = 78 * 0.58^i`, `F_i = (2π / 2900) * 2.17^i`,
  fixed phases `P_i` (any constants, e.g. 0.0, 1.9, 4.1, 0.7, 2.6), `Bias = 60`. Then clamp to **[-60, +180]**
  (soft clamp is fine). Seaward bias on purpose: level props and runtime rocks sit at X ≤ ~2130 and must stay on
  land; the moored longship's west end is at X ≈ 2420 and must stay afloat. Tune amplitudes / octaves by eye, but
  keep that range.
- C++: file-static octave table; `float ATSCOcean::ShoreXAt(float Y) const` (public) returns `ShoreX + S(Y)`.
  Replace every use of the straight shore inside `TSCOcean.cpp` by it: `Atten(X - ShoreXAt(Y))` in `HeightAt` /
  `SlopeAt`, the shore-break spray position (`ShoreXAt(y) + 40..100`). `Proximity` keeps using plain `ShoreX`.
  Push to the MID once in `Build`: vectors `Shore0..Shore4 = (A, F, P, 0)`, scalars `ShoreBias`, `ShoreMin = -60`,
  `ShoreMax = 180`.
- Beach mesh: a second `UProceduralMeshComponent` on `ATSCOcean` (`Beach`), built in `Build`, replacing the shore
  strip cube. Columns along Y: every 25 units for |Y| ≤ 7000, every 500 beyond, out to ±45000. Per column three
  rows: `(1800, Z 4)`, `(ShoreXAt(y) - 20, Z 4)`, `(ShoreXAt(y) + 100, Z -20)` — the slope crosses Z = 0 exactly
  at the waterline. Section 0 = flat top, `M_SagaFlat` MID with `Color = Pal::Shore` (0.17, 0.165, 0.155), normal
  up. Section 1 = the slope, own vertices, `Color = Pal::Shore * 0.6` (wet band), normal up is acceptable. No
  collision, cast shadow off, receives shadows. Triangle winding must face +Z (same order as the ocean grid).
- `TSCGameMode.cpp`: delete the "rocky shore" cube part in `BuildWorld` (the `Pal::Shore` strip); the ground base
  keeps ending at X = 2200 (it is at Z -7, under the beach slope: check nothing dark shows where S > 100, and if it
  does extend the slope's lower row to `+140, Z -28` or end the base at 2140). `WadeDepthAt` uses
  `Ocean->ShoreXAt(P.Y)` instead of the straight `ShoreX` (fall back to the straight line if `Ocean` is null).
- Material script: add the `Shore*` parameters and compute `Sy` from `P.y` with the same formula and clamp; use
  `d = P.x - ShoreX - Sy` everywhere the script used `P.x - ShoreX` (Atten, depth colour, lap bands, edge foam).
  Re-run the script.
Accept: build clean. PIE screenshots (prologue off): the default camera view and a close view of the shore north
of the dock show an irregular coastline with bays and points, lap lines and edge foam following it; no gap or
dark seam between beach and water; no water on top of the beach; level props and rocks on land; longship and
barrels still afloat; units at the prologue landing still wade and come up on the beach. Save
`Docs/Screenshots/ocean_shoreline.png` (and overwrite `ocean_day.png`).

### S13 — Surf quieter and in time with the waves ✅   [tricky]   deps: S12
Files: `UE/Source/TheSagaCutter/TSCOcean.cpp`, `Scripts/ue/build_ocean_assets.py`,
`UE/Content/Vikings/Environment/Ocean/M_SagaOcean.uasset`
- Quieter: `SurfGain` 1.8 → 1.1.
- In time: the lapping bands in the material reach the waterline once per `1 / LapRate` seconds (the script uses
  `frac(dn / 460 + OceanTime * 0.11)` with the band at 0.86..0.92). Make `LapRate` (0.11) and `LapLength` (460) a
  single C++ table pushed as scalar parameters (script defaults equal), and in `Sim` compute the arrival phase
  `Ph = frac(Time * LapRate)`; a band touches the shore when `Ph ≈ 0.89`. Envelope
  `Env = pow(0.5 + 0.5 * cos(2π * (Ph - 0.89 - 0.03)), 2)` (peak just after the band lands: the wash). Surf volume
  is multiplied by `lerp(0.45, 1.0, Env)`, smoothed with `FInterpTo(..., 3.0)` so it swells and recedes instead of
  stepping. Shore-break spray is tied to the same event: spray interval `lerp(0.5, 0.12, Env)` instead of random
  0.18..0.4.
- Verify the phase convention against the actual graph (which way the bands travel, where the band is at
  `Ph = 0.89`) on two screenshots taken at known `OceanTime`, and fix the offset if the swell is out of step.
Accept: build clean; in a standalone `-game` run (the MCP editor has no sound) or by logging, show `Ph`, `Env` and
the applied volume over ~20 s at a fixed camera: volume peaks coincide with lap bands reaching the shore (state
the measured offset in seconds). Nobody can listen here — say so.

## Final check

1. Editor closed: `sh Scripts/build_editor.sh` — no errors.
2. Start the editor, PIE on `Map_Saga`: prologue shows the ship riding the swell in and a landing sound at
   landfall; surf loop audible and louder when the camera is panned to the shore; gulls by day only.
3. `TSCAuto LLLA none 4`: the game plays through with no ensure / crash; after the dock scene goes Ashen the water
   greys, driftwood appears; the night raid plays the landing sound; the epilogue is reached.
4. Editor log: no `Error`, no missing-asset warnings for `/Game/Vikings/Environment/Ocean` or
   `/Game/Vikings/Audio/Ambience`.
5. Not required for this plan but worth doing before submission: `sh Scripts/package_jam.sh <Team>` and check the
   ocean is not the dark fallback colour in the packaged build (that would mean the cook rule from S5 is missing).

## Risks / open questions

- **ElevenLabs `loop=True` or the v2 model is rejected.** Default: drop `loop`/`model_id`, generate 30 s, and make
  the loop in the script by cross-fading the last 1.5 s into the first 1.5 s with ffmpeg (`acrossfade`) or numpy.
- **ElevenLabs credits / key missing.** Default: S2 reports blocked; everything else still works because the C++
  skips missing sounds. Do not substitute downloaded sounds (asset-listing rule).
- **Sine node period / Custom node in the Python material API.** Default: Sine expression with `period = 6.283185`;
  if that property is not exposed, multiply the phase by `1 / (2π)` and keep period 1.
- **`ProceduralMeshComponent` + World Position Offset culling.** Default: `SetBoundsScale(1.2f)`; if the mesh
  flickers at screen edges raise it.
- **Pitch / roll sign.** Cannot be decided on paper; S6 checks it on a screenshot and flips the sign in
  `TSCOcean.cpp` if the bow dips into a rising wave.
- **Flotsam spring in the prologue.** Barrels are in `ProDressing`, hidden during the prologue; the ocean skips
  hidden actors and re-snaps them when shown.
- **Wading vs. unit speed.** Kept visual only: no slowdown (Ashen raiders are already slowed by the scene script).
  If wading looks like gliding, the fix is more splashes, not a speed change.
- **Dead or downed units in water** stay sunk to the seabed depth (at most 55 units), so a body may be mostly
  hidden. Default: accept.
- **`Map_Saga` is a binary file shared with teammates** (see the `Leon047/units` merge). Default: S11 keeps its
  edits small and scripted (`Scripts/ue/shore_pass.py`) so they can be re-applied after a conflicting merge.
- **Performance.** ~11k vertices, one opaque draw, two texture samples ×2; negligible. If the frame rate drops,
  halve the dense grid (spacing 200).
- **Unknown: whether `Map_Saga` already contains a water or backdrop mesh east of the shore.** Default: leave the
  level untouched; if S6's screenshot shows z-fighting with a level mesh, report it instead of editing the map.
