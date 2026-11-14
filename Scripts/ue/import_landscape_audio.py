# The Saga Cutter - game jam project.
# Imports the landscape sound effects into /Game/Vikings/Audio/Ambience (idempotent: replaces existing assets).
# Run inside the editor:  exec(open(r"<repo>/Scripts/ue/import_landscape_audio.py").read())
# Names must match LoadAmbience() calls in TSCTerrain.cpp.
import os
import unreal

FOLDER = "/Game/Vikings/Audio/Ambience"
REPO = os.path.abspath(os.path.join(unreal.Paths.project_dir(), ".."))
SRC = os.path.join(REPO, "Docs", "Audio", "SFX")

# asset name -> wav file (river_loop take 1: better seam; birds_loop take 2: better seam and level)
SOUNDS = {
    "SW_RiverLoop": "river_loop_take1.wav",
    "SW_WindLoop": "wind_loop_take1.wav",
    "SW_BirdsLoop": "birds_loop_take2.wav",
    "SW_Raven_1": "raven_1_take1.wav",
    "SW_Raven_2": "raven_2_take1.wav",
    "SW_Owl_1": "owl_1_take1.wav",
    "SW_Owl_2": "owl_2_take1.wav",
}
LOOPS = ("SW_RiverLoop", "SW_WindLoop", "SW_BirdsLoop")

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
        print("[landscape-audio] MISSING " + path)
        continue
    asset.set_editor_property("looping", name in LOOPS)
    unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    print("[landscape-audio] %s looping=%s duration=%.2f" % (name, asset.get_editor_property("looping"), asset.get_editor_property("duration")))
