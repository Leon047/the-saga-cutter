# Viking house kit (Blender -> Unreal)

Source: `C:\Linch\Art\Blender\VikingHouse\VikingHouse_Modular.blend` (units m, Z up, bay = 2.4 m, wall line 4.8 m deep).
75 modules: **26 Tripo**, **49 Blender** placeholders. 6 houses, 1112 instances.

## Naming rule

Every module has ONE stable, source-agnostic asset name: `SM_VH_<BlenderModuleName>` (e.g. `SM_VH_WALL_Low_Plank`).
The source (Tripo or Blender) is never in the asset name or path. It lives in the `Source` column of
`VikingHouseModules.tsv` and in the asset metadata tag `VH_Source`. A Tripo replacement overwrites the same asset, no reference breaks.

## Folder layout

| What | Where |
|---|---|
| Blender export script | `C:\Linch\Art\Blender\VikingHouse\export_to_unreal.py` |
| Exported FBX (one per module, at the origin, module pivot kept) | `C:\Linch\Art\Blender\VikingHouse\exportbx\SM_VH_<Module>.fbx` |
| Raw export data (modules, materials, layouts) | `...\VikingHouse\export\kit_export.json` |
| Meshes | `/Game/Vikings/Buildings/VikingHouse/Meshes/SM_VH_<Module>` |
| Materials | `/Game/Vikings/Buildings/VikingHouse/Materials/` (`M_Viking*_Set01` textured, `M_VH_<name>` flat placeholders, shared) |
| Textures | `/Game/Vikings/Buildings/VikingHouse/Textures/T_<Set>_<BaseColor\|Normal\|Roughness\|Metallic>` |
| Data | `UE/Content/Vikings/Data/VikingHouseModules.tsv`, `VikingHouses.tsv`, `VikingHouseLayouts.tsv` |

Units and axes: 1 m = 100 cm, Z up. The FBX is exported with forward -Y, up Z; Unreal's importer mirrors Y, so a Blender `(x, y, z)` m
is Unreal `(x*100, -y*100, z*100)` and no rotation is baked into the assets. Nanite is off, no collision, the FBX path is stored for Reimport.

## Data files (tab separated, UTF-8, first line starts with `#`)

- `VikingHouseModules.tsv`: `Module Source AssetPath SizeX SizeY SizeZ` (bounding box, cm).
- `VikingHouses.tsv`: `House Bays LengthCm WidthCm HeightCm` (height = top of the assembled house above its origin).
- `VikingHouseLayouts.tsv`: `House Group Module X Y Z Pitch Yaw Roll ScaleX ScaleY ScaleZ`. `Group` is `Shell`, `Removable` or `Interior`
  (the house's `*_Shell`, `*_CutawayRemovable`, `*_Interior` collections). Transforms are relative to the house origin, in Unreal space
  (cm, left-handed, FRotator degrees).

## Modules

### DECO

| Module | Source | Materials |
|---|---|---|
| `DECO_BigChimney` | Blender | M_dark, M_stone, M_stone_b |
| `DECO_Chimney` | Blender | M_dark, M_stone, M_stone_b |
| `DECO_DragonHead` | Blender | M_black, M_carved, M_wood2 |
| `DECO_Knotwork` | Blender | M_carved, M_wood2, M_wood_b |
| `DECO_RavenBanner` | Blender | M_black, M_dark, M_red, M_wood2 |
| `DECO_RidgeBoss` | Blender | M_carved, M_iron, M_wood2, M_wood_b |
| `DECO_Shield` | Blender | M_iron, M_shield, M_wood2 |
| `DECO_Sign_Anvil` | Blender | M_iron, M_wood2, M_wood_b |
| `DECO_Sign_Barrel` | Blender | M_clay, M_iron, M_wood2, M_wood_b |
| `DECO_Sign_Saw` | Blender | M_dark, M_iron, M_wood2, M_wood_b |

### DOOR

| Module | Source | Materials |
|---|---|---|
| `DOOR_Arched` | Blender | M_iron, M_wood, M_wood_b |
| `DOOR_StairsHigh` | Blender | M_wood, M_wood2, M_wood_b |
| `DOOR_Steps` | Blender | M_wood, M_wood2, M_wood_b |
| `DOOR_Surround` | Blender | M_carved, M_wood2 |

### FLOOR

| Module | Source | Materials |
|---|---|---|
| `FLOOR_Plank` | Blender | M_wood, M_wood2, M_wood_b |

### FND

| Module | Source | Materials |
|---|---|---|
| `FND_Boulder_A` | Tripo | M_VikingFoundation_Set01 |
| `FND_Boulder_B` | Tripo | M_VikingFoundation_Set01 |
| `FND_Boulder_C` | Tripo | M_VikingFoundation_Set01 |
| `FND_Corner` | Tripo | M_VikingFoundation_Set01 |
| `FND_PlinthWindow` | Tripo | M_VikingFoundation_Set01 |
| `FND_Straight` | Tripo | M_VikingFoundation_Set01 |

### FRM

| Module | Source | Materials |
|---|---|---|
| `FRM_Beam` | Tripo | M_VikingFrame_Set01 |
| `FRM_Brace` | Tripo | M_VikingFrame_Set01 |
| `FRM_CornerPost` | Tripo | M_VikingFrame_Set01 |
| `FRM_KingPost` | Tripo | M_VikingFrame_Set01 |
| `FRM_LogEndCap` | Tripo | M_VikingFrame_Set01 |
| `FRM_MidPost` | Tripo | M_VikingFrame_Set01 |
| `FRM_Rafter` | Tripo | M_VikingFrame_Set01 |
| `FRM_TieBeam` | Tripo | M_VikingFrame_Set01 |

### INT

| Module | Source | Materials |
|---|---|---|
| `INT_Bench` | Blender | M_wood2, M_wood_b |
| `INT_BunkBed` | Blender | M_cloth, M_fur, M_wood2, M_wood_b |
| `INT_DeerSkull` | Blender | M_bone, M_wood2 |
| `INT_FeastTable` | Blender | M_bone, M_clay, M_fire, M_wood2, M_wood_b |
| `INT_FirePit` | Blender | M_dark, M_fire, M_stone, M_stone_b, M_wood_b |
| `INT_Lantern` | Blender | M_iron, M_window |
| `INT_Table` | Blender | M_clay, M_wood2, M_wood_b |

### PIER

| Module | Source | Materials |
|---|---|---|
| `PIER_BannerPole` | Blender | M_bone, M_red, M_wood2 |
| `PIER_Bollard` | Blender | M_clay, M_wood2 |
| `PIER_Deck` | Blender | M_wood, M_wood2, M_wood_b |
| `PIER_Lamp` | Blender | M_iron, M_window, M_wood2 |
| `PIER_Post` | Blender | M_clay, M_wood2, M_wood_b |

### PROP

| Module | Source | Materials |
|---|---|---|
| `PROP_Anvil` | Blender | M_iron, M_wood2 |
| `PROP_Barrel` | Blender | M_iron, M_wood_b |
| `PROP_Crate` | Blender | M_wood2, M_wood_b |
| `PROP_Flume` | Blender | M_wood2, M_wood_b |
| `PROP_Forge` | Blender | M_dark, M_fire, M_iron, M_stone, M_stone_b, M_wood2, M_wood_b |
| `PROP_Hoist` | Blender | M_bone, M_clay, M_iron, M_wood2 |
| `PROP_LogPile` | Blender | M_wood, M_wood_b |
| `PROP_Sack` | Blender | M_bone, M_dark |
| `PROP_SawBench` | Blender | M_dark, M_iron, M_wood, M_wood2, M_wood_b |
| `PROP_Shelf` | Blender | M_bone, M_clay, M_wood, M_wood2, M_wood_b |
| `PROP_Stilt` | Blender | M_stone, M_wood2 |
| `PROP_ToolRack` | Blender | M_iron, M_wood, M_wood2 |
| `PROP_WaterWheel` | Blender | M_wood, M_wood2, M_wood_b |
| `PROP_Workbench` | Blender | M_iron, M_wood, M_wood2, M_wood_b |

### ROOF

| Module | Source | Materials |
|---|---|---|
| `ROOF_Bargeboard` | Blender | M_wood2, M_wood_b |
| `ROOF_Eave_Flare` | Tripo | M_VikingRoofs_Set01 |
| `ROOF_Eave_Strip` | Tripo | M_VikingRoofs_Set01 |
| `ROOF_LeanTo` | Blender | M_dark, M_shingle, M_shingle_b, M_wood2 |
| `ROOF_RidgeCap` | Tripo | M_VikingFoundation_Set01 |
| `ROOF_RidgeCap_Strip` | Tripo | M_VikingFoundation_Set01 |
| `ROOF_Shingle_A` | Blender | M_shingle_b |
| `ROOF_Shingle_B` | Blender | M_shingle_b |
| `ROOF_Shingle_C` | Blender | M_shingle, M_shingle_b |
| `ROOF_Slope_Bay` | Tripo | M_VikingRoofs_Set01 |
| `ROOF_Slope_Strip` | Tripo | M_VikingRoofs_Set01 |

### WALL

| Module | Source | Materials |
|---|---|---|
| `WALL_Door` | Tripo | M_VikingWalls_Set01 |
| `WALL_Gable` | Tripo | M_VikingWalls_Set01 |
| `WALL_Low_Plank` | Tripo | M_VikingWalls_Set01 |
| `WALL_Low_Window` | Tripo | M_VikingWalls_Set01 |
| `WALL_Up_Plank` | Tripo | M_VikingWalls_Set01 |
| `WALL_Up_Window` | Tripo | M_VikingWalls_Set01 |

### WIN

| Module | Source | Materials |
|---|---|---|
| `WIN_Attic` | Blender | M_dark, M_iron, M_window, M_wood, M_wood2 |
| `WIN_Large` | Blender | M_dark, M_iron, M_window, M_wood, M_wood2 |
| `WIN_Small` | Blender | M_dark, M_iron, M_window, M_wood, M_wood2 |

## Replacing a `Blender` placeholder with a Tripo mesh

1. Model the Tripo mesh with the placeholder's pivot and size (module origin at the world origin of the file, Z up, 1 unit = 1 m in Blender;
   the bounding box in `VikingHouseModules.tsv` is the target). Either replace the object of the same name in `Modules_Library` in the blend
   (and put it on one of the `M_Viking*_Set01` materials), or skip Blender and export an FBX yourself.
2. Put the FBX at `exportbx\SM_VH_<Module>.fbx` (the same name). If the module is replaced in the blend, re-run
   `blender --background VikingHouse_Modular.blend --python export_to_unreal.py` (it does not save the blend).
3. In Unreal, right click `SM_VH_<Module>` -> Reimport (source path is stored), or import the FBX over the same asset name in
   `/Game/Vikings/Buildings/VikingHouse/Meshes`. Disable Import Materials, Auto Generate Collision and Nanite.
4. Re-assign the material slots (slot name = material name in the FBX). New textured sets go to `.../Textures` and get a material in
   `.../Materials` named as in Blender (base colour, normal, roughness, metallic).
5. Flip the `Source` column for that module to `Tripo` in `VikingHouseModules.tsv`, update `SizeX/Y/Z`, and set the asset metadata tag
   `VH_Source` = `Tripo` (Python: `unreal.EditorAssetLibrary.set_metadata_tag(asset, "VH_Source", "Tripo")`). Save the asset.
6. If the pivot or size changed, the layouts must be regenerated: re-run the export script and rewrite `VikingHouseLayouts.tsv` from `kit_export.json`.

Notes: `export_to_unreal.py` classifies a module as `Tripo` when any of its materials matches `M_Viking*_Set<N>`. The tab-separated files
are generated from `kit_export.json` plus the bounding boxes measured in Unreal after import.
