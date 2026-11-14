# The Saga Cutter - game jam project.
# S11 of Docs/Plans/stylized-ocean.md: shore pass in Map_Saga. Re-runnable editor script (run with PIE stopped):
#   exec(open(r"<repo>/Scripts/ue/shore_pass.py").read())
# 1. lists every level actor east of X = 1600, 2. deletes leftover water / backdrop planes and anything sunk below the
# water east of the shoreline, 3. dresses the shore strip (X 1800..2200, top Z = +4) north and south of the dock with
# a few /Game/Props meshes, all tagged OceanShorePass (deleted first on every run), 4. saves the level.
import unreal

TAG = "OceanShorePass"
SHORE_X = 2200.0
STRIP_TOP = 4.0
LIST_X = 1600.0

# (label, mesh, x, y, yaw). Rules: x < 2100 and inside the palisade so CollectLevelProps hides them in the prologue;
# |y| >= 1000 keeps them off the dock path (rows 11-12) and out of the Dock region (y -600..600), which turns Ashen.
# Spots avoid the runtime shore rocks (BuildWorld) at about (2060,-1478), (1862,-1913), (2129,926), (1876,1518),
# (2041,1867), (1705,2064). The WoodenCrate pack's names do not match the meshes: only _07 and _08 are crates
# (_06 barrel, _05 chair, _01 stool, _02 / _03 / _04 mossy rocks), so the crates use _07 / _08 plus one barrel.
PROPS = [
    ("Shore_N_Crate_01", "/Game/Props/WoodenCrate/SM_WoodenCrate_08", 1930.0, -1150.0, 12.0),
    ("Shore_N_Crate_02", "/Game/Props/WoodenCrate/SM_WoodenCrate_07", 1995.0, -1080.0, -20.0),
    ("Shore_N_Crate_03", "/Game/Props/WoodenCrate/SM_WoodenCrate_07", 1890.0, -1620.0, 30.0),
    ("Shore_N_Fence",    "/Game/Props/WoodenFenceSet/SM_WoodenFenceSet_04", 2040.0, -1650.0, 75.0),
    ("Shore_S_Crate_01", "/Game/Props/WoodenCrate/SM_WoodenCrate_08", 1900.0, 1150.0, -8.0),
    ("Shore_S_Crate_02", "/Game/Props/WoodenCrate/SM_WoodenCrate_07", 1975.0, 1200.0, 25.0),
    ("Shore_S_Barrel",   "/Game/Props/WoodenCrate/SM_WoodenCrate_06", 1850.0, 1240.0, -35.0),
    ("Shore_S_Fence",    "/Game/Props/WoodenFenceSet/SM_WoodenFenceSet_03", 1870.0, 1900.0, 90.0),
]

actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor_sub = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

if editor_sub.get_game_world() is not None:
    raise RuntimeError("[shore] PIE is running: stop it before editing the level")
_world = editor_sub.get_editor_world()
if not _world.get_path_name().startswith("/Game/Vikings/Maps/Map_Saga"):
    raise RuntimeError("[shore] wrong level loaded: " + _world.get_path_name())


def _describe(a):
    loc = a.get_actor_location()
    origin, ext = a.get_actor_bounds(False)
    mesh = ""
    if isinstance(a, unreal.StaticMeshActor):
        m = a.static_mesh_component.static_mesh
        mesh = m.get_path_name().split(".")[0] if m else "None"
    return "%-20s %-28s %-50s loc=(%.0f, %.0f, %.0f) bounds=(%.0f..%.0f, %.0f..%.0f, %.0f..%.0f)%s" % (
        a.get_actor_label(), a.get_class().get_name(), mesh, loc.x, loc.y, loc.z,
        origin.x - ext.x, origin.x + ext.x, origin.y - ext.y, origin.y + ext.y, origin.z - ext.z, origin.z + ext.z,
        "  [%s]" % TAG if a.actor_has_tag(TAG) else "")


def _list(title):
    rows = [_describe(a) for a in actor_sub.get_all_level_actors() if a.get_actor_location().x > LIST_X]
    rows.sort()
    lines = ["[shore] %s: %d actors with X > %.0f" % (title, len(rows), LIST_X)] + ["[shore]   " + r for r in rows]
    for line in lines:
        print(line)
    return lines


report = _list("before")

# previous run of this script
for a in actor_sub.get_all_level_actors():
    if a.actor_has_tag(TAG):
        report.append("[shore] removing previous shore-pass actor " + a.get_actor_label())
        actor_sub.destroy_actor(a)

# conflicts with the ocean
WATER_WORDS = ("water", "sea", "ocean", "backdrop", "plane")
for a in actor_sub.get_all_level_actors():
    loc = a.get_actor_location()
    if loc.x <= LIST_X:
        continue
    origin, ext = a.get_actor_bounds(False)
    name = (a.get_actor_label() + " " + _describe(a)).lower()
    why = ""
    if any(w in name for w in WATER_WORDS):
        why = "leftover water / backdrop mesh"
    elif origin.x - ext.x > SHORE_X and origin.z + ext.z <= 0.0:
        why = "sunk below the water east of the shoreline"
    if why:
        report.append("[shore] DELETE %s: %s" % (a.get_actor_label(), why))
        actor_sub.destroy_actor(a)
if not any(r.startswith("[shore] DELETE") for r in report):
    report.append("[shore] no conflicting actors found, nothing deleted")

# dressing
for label, mesh_path, x, y, yaw in PROPS:
    assert x < 2100.0 and abs(y) >= 1000.0, label
    mesh = unreal.EditorAssetLibrary.load_asset(mesh_path)
    if not mesh:
        report.append("[shore] SKIP %s: mesh %s missing" % (label, mesh_path))
        continue
    z = STRIP_TOP - mesh.get_bounding_box().min.z - 1.0  # bottom 1 unit into the strip: never hovers
    a = actor_sub.spawn_actor_from_object(mesh, unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, yaw))
    a.set_actor_label(label)
    a.tags = [unreal.Name(TAG)]
    a.set_folder_path("OceanShorePass")
    origin, ext = a.get_actor_bounds(False)
    if origin.x + ext.x >= 2200.0:
        report.append("[shore] WARNING %s reaches X %.0f (over the water)" % (label, origin.x + ext.x))
    report.append("[shore] added %s (%s) at (%.0f, %.0f, %.1f) yaw %.0f" % (label, mesh_path.split("/")[-1], x, y, z, yaw))

report += _list("after")
saved = level_sub.save_current_level()
report.append("[shore] level saved: %s" % saved)
for line in report[-1:]:
    print(line)
shore_report = "\n".join(report)
