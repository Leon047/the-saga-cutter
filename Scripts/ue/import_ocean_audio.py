# The Saga Cutter - game jam project.
# Imports the ocean sound effects into /Game/Vikings/Audio/Ambience (idempotent: replaces existing assets).
# Run inside the editor:  exec(open(r"<repo>/Scripts/ue/import_ocean_audio.py").read())
# Names must match LoadAmbience() calls in TSCOcean.cpp.
import os
import unreal

FOLDER = "/Game/Vikings/Audio/Ambience"
REPO = os.path.abspath(os.path.join(unreal.Paths.project_dir(), ".."))
SRC = os.path.join(REPO, "Docs", "Audio", "SFX")

# asset name -> wav file (ocean_loop uses take 2: better seam)
SOUNDS = {
    "SW_OceanLoop": "ocean_loop_take2.wav",
    "SW_Gull_1": "gull_1_take1.wav",
    "SW_Gull_2": "gull_2_take1.wav",
    "SW_Gull_3": "gull_3_take1.wav",
    "SW_BoatCreak_1": "boat_creak_1_take1.wav",
    "SW_BoatCreak_2": "boat_creak_2_take1.wav",
    "SW_BoatLand": "boat_land_take1.wav",
    "SW_Wade_1": "wade_1_take1.wav",
    "SW_Wade_2": "wade_2_take1.wav",
}

tasks = []
for name, wav in SOUNDS.items():
    t = unreal.AssetImportTask()
    t.filename = os.path.join(SRC, wav)
    t.destination_path = FOLDER
    t.destination_name = name
    t.automated = True
    t.replace_existing = True
    t.save = False
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

for name in SOUNDS:
    path = FOLDER + "/" + name
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset:
        print("[ocean-audio] MISSING " + path)
        continue
    asset.set_editor_property("looping", name == "SW_OceanLoop")
    unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    print("[ocean-audio] %s looping=%s duration=%.2f" % (name, asset.get_editor_property("looping"), asset.get_editor_property("duration")))
