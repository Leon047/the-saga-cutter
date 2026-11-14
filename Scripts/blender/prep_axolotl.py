"""Clean up and normalise a Tripo-rigged axolotl character for Unreal (plans axolotl-warrior-tripo S3, axolotl-all-classes S3).

Run headless:
    blender.exe -b -P Scripts/blender/prep_axolotl.py -- <key> <in.fbx> <out dir>

<key> is the character key (warrior, archer, druid, blacksmith, jarl); <Key> below is KEY.capitalize().
Input: the Tripo Studio export (Mixamo-preset rig, bones prefixed `mixamorig:`), textures in `<in>.fbm/`.
Output in <out dir>:
    SK_Axolotl<Key>.fbx          armature + mesh, no leaf bones, no animation, imports into UE at real size (cm)
    T_Axolotl<Key>_*.jpg         base colour, normal, roughness, metallic (copied, still jpg)
    <key>_bones.json             bones, hierarchy, rest pose, roles (hands, feet, head, hips, spine chain), weight summary
    check_front/side/back/34.png textured stills of the rest pose
    check_pose.png, check_pose2.png  posed test stills (arms down 60 deg, a thigh forward 40 deg) for skinning review
    check_tail.png               tail reweight selection (core red, blend band yellow)

Tail: a key with an entry in TAIL uses those explicit thresholds; any other key runs the auto tail detection
(see tail_selection). Blender axes: the figure faces -Y, Z up, feet at Z = 0. With the default FBX axes this
lands +Y-facing in UE. Exits with code 1 on any failure.
"""
import bpy
import bmesh
import fnmatch
import json
import math
import os
import shutil
import sys
import traceback
from mathutils import Matrix, Vector

PREFIX = "mixamorig:"
MAX_TRIS = 30000          # decimate only above this (decided after S2: keep Tripo's 29.7k mesh)
DECIMATE_TARGET = 8000
LOOSE_AREA_FRAC = 0.005   # delete loose parts below 0.5 % of the total surface
MAX_INFLUENCES = 4

KEY = None                # character key from the command line, e.g. "warrior"
PASCAL = None             # KEY.capitalize(), e.g. "Warrior"
MESH_NAME = None          # SK_Axolotl<Key>
MAT_NAME = None           # M_Axolotl<Key>
TEX_SUFFIXES = {}         # Tripo file suffix -> our name, T_Axolotl<Key>_<Kind>
USAGE = "usage: blender -b -P prep_axolotl.py -- <key> <in.fbx> <out dir>"


def set_character(key):
    global KEY, PASCAL, MESH_NAME, MAT_NAME, TEX_SUFFIXES
    KEY = key
    PASCAL = KEY.capitalize()
    MESH_NAME = f"SK_Axolotl{PASCAL}"
    MAT_NAME = f"M_Axolotl{PASCAL}"
    TEX_SUFFIXES = {
        "basecolor": f"T_Axolotl{PASCAL}_BaseColor",
        "normal": f"T_Axolotl{PASCAL}_Normal",
        "roughness": f"T_Axolotl{PASCAL}_Roughness",
        "metallic": f"T_Axolotl{PASCAL}_Metallic",
    }


def log(*a):
    print("[prep]", *a, flush=True)


def parse_args():
    argv = sys.argv
    if "--" not in argv:
        raise SystemExit(USAGE)
    rest = argv[argv.index("--") + 1:]
    if len(rest) < 3:
        raise SystemExit(USAGE)
    return rest[0].strip().lower(), os.path.abspath(rest[1]), os.path.abspath(rest[2])


# --------------------------------------------------------------------------------------------- import / transforms

def clear_scene():
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)


def import_fbx(path):
    bpy.ops.import_scene.fbx(filepath=path, use_anim=False, ignore_leaf_bones=False,
                             automatic_bone_orientation=False, use_custom_normals=True)
    arms = [o for o in bpy.context.scene.objects if o.type == 'ARMATURE']
    meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    if len(arms) != 1:
        raise RuntimeError(f"expected 1 armature, found {len(arms)}")
    if not meshes:
        raise RuntimeError("no mesh in FBX")
    # drop anything that is neither (empties, cameras, lights from the exporter)
    for o in list(bpy.context.scene.objects):
        if o.type not in ('ARMATURE', 'MESH'):
            bpy.data.objects.remove(o, do_unlink=True)
    return arms[0], meshes


def select_only(objs, active=None):
    bpy.ops.object.mode_set(mode='OBJECT') if bpy.context.object and bpy.context.object.mode != 'OBJECT' else None
    for o in bpy.context.scene.objects:
        o.select_set(False)
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = active or objs[0]


def apply_transforms(arm, meshes):
    for o in [arm] + meshes:
        log(f"before: {o.name} loc={tuple(round(v, 4) for v in o.location)} "
            f"rot={tuple(round(v, 4) for v in o.rotation_euler)} scale={tuple(round(v, 4) for v in o.scale)}")
    select_only([arm] + meshes, arm)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for o in [arm] + meshes:
        if o.matrix_world != Matrix.Identity(4):
            # a parented mesh can keep a parent-inverse; flatten it
            mw = o.matrix_world.copy()
            if o.type == 'MESH':
                o.data.transform(mw)
                o.matrix_parent_inverse = Matrix.Identity(4)
                o.matrix_basis = Matrix.Identity(4)
    for o in [arm] + meshes:
        assert all(abs(a - b) < 1e-5 for a, b in zip(o.matrix_world.col[0], (1, 0, 0, 0))), o.name


def join_meshes(arm, meshes):
    if len(meshes) > 1:
        select_only(meshes, meshes[0])
        bpy.ops.object.join()
    mesh = [o for o in bpy.context.scene.objects if o.type == 'MESH'][0]
    mesh.name = MESH_NAME
    mesh.data.name = MESH_NAME
    if mesh.parent != arm:
        select_only([mesh, arm], arm)
        bpy.ops.object.parent_set(type='OBJECT', keep_transform=True)
    mods = [m for m in mesh.modifiers if m.type == 'ARMATURE']
    if not mods:
        m = mesh.modifiers.new("Armature", 'ARMATURE')
        m.object = arm
    else:
        mods[0].object = arm
    return mesh


def single_material(mesh):
    mats = [s.material for s in mesh.material_slots if s.material]
    log(f"material slots: {[m.name for m in mats]}")
    if len(mats) > 1:
        # keep the first, point every face at it
        for p in mesh.data.polygons:
            p.material_index = 0
        while len(mesh.material_slots) > 1:
            mesh.active_material_index = len(mesh.material_slots) - 1
            select_only([mesh])
            bpy.ops.object.material_slot_remove()
    return mesh.material_slots[0].material if mesh.material_slots else None


def tri_count(mesh):
    return sum(len(p.vertices) - 2 for p in mesh.data.polygons)


def delete_small_loose_parts(mesh):
    bm = bmesh.new()
    bm.from_mesh(mesh.data)
    bm.faces.ensure_lookup_table()
    total = sum(f.calc_area() for f in bm.faces)
    seen = set()
    islands = []
    for f in bm.faces:
        if f.index in seen:
            continue
        stack = [f]
        seen.add(f.index)
        isl = []
        while stack:
            cur = stack.pop()
            isl.append(cur)
            for e in cur.edges:
                for nf in e.link_faces:
                    if nf.index not in seen:
                        seen.add(nf.index)
                        stack.append(nf)
        islands.append(isl)
    small = [isl for isl in islands if sum(f.calc_area() for f in isl) < LOOSE_AREA_FRAC * total]
    log(f"loose parts: {len(islands)} islands, {len(small)} below {LOOSE_AREA_FRAC * 100:.1f}% of surface")
    removed_faces = 0
    if small:
        faces = [f for isl in small for f in isl]
        removed_faces = len(faces)
        verts = {v for f in faces for v in f.verts}
        bmesh.ops.delete(bm, geom=faces, context='FACES_ONLY')
        lone = [v for v in verts if v.is_valid and not v.link_faces]
        bmesh.ops.delete(bm, geom=lone, context='VERTS')
    # stray vertices / edges without faces
    stray = [v for v in bm.verts if not v.link_faces]
    if stray:
        bmesh.ops.delete(bm, geom=stray, context='VERTS')
    bm.to_mesh(mesh.data)
    bm.free()
    mesh.data.update()
    return {"islands": len(islands), "removed_islands": len(small), "removed_faces": removed_faces,
            "removed_stray_verts": len(stray)}


def maybe_decimate(mesh):
    tris = tri_count(mesh)
    if tris <= MAX_TRIS:
        log(f"triangles {tris} <= {MAX_TRIS}: no decimation")
        return False
    log(f"triangles {tris} > {MAX_TRIS}: decimating to ~{DECIMATE_TARGET}")
    m = mesh.modifiers.new("Decimate", 'DECIMATE')
    m.ratio = DECIMATE_TARGET / tris
    select_only([mesh])
    # decimate must come before the armature modifier to be applied
    while mesh.modifiers[0].name != "Decimate":
        bpy.ops.object.modifier_move_up(modifier="Decimate")
    bpy.ops.object.modifier_apply(modifier="Decimate")
    log(f"after decimation: {tri_count(mesh)} triangles, {len(mesh.vertex_groups)} vertex groups")
    return True


def place_on_ground(arm, mesh):
    """Feet at Z = 0, centred on X/Y. Keeps the size (the game scales by imported bounds)."""
    pts = [mesh.matrix_world @ v.co for v in mesh.data.vertices]
    mn = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    mx = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    off = Vector((-(mn.x + mx.x) / 2, -(mn.y + mx.y) / 2, -mn.z))
    log(f"bounds before: min={tuple(round(v, 4) for v in mn)} max={tuple(round(v, 4) for v in mx)} offset={tuple(round(v, 4) for v in off)}")
    if off.length > 1e-4:
        arm.location += off
        bpy.context.view_layer.update()
        select_only([arm, mesh], arm)
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    pts = [mesh.matrix_world @ v.co for v in mesh.data.vertices]
    mn = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    mx = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    return mn, mx


def check_facing(arm):
    """Facing -Y: the toes are in front of the ankles (more negative Y)."""
    b = arm.data.bones
    foot, toe = b.get("LeftFoot"), b.get("LeftToeBase")
    if foot and toe:
        dy = toe.head_local.y - foot.head_local.y
        log(f"facing check: LeftToeBase.y - LeftFoot.y = {dy:.4f} ({'-Y ok' if dy < 0 else 'NOT -Y'})")
        if dy >= 0:
            raise RuntimeError("figure does not face -Y; the plan assumes it already does")


# --------------------------------------------------------------------------------------------- bones

def strip_prefix(arm, mesh):
    n = 0
    for b in arm.data.bones:
        if b.name.startswith(PREFIX):
            b.name = b.name[len(PREFIX):]   # Blender renames matching vertex groups too
            n += 1
    for vg in mesh.vertex_groups:            # in case any group was not linked
        if vg.name.startswith(PREFIX):
            vg.name = vg.name[len(PREFIX):]
    log(f"stripped '{PREFIX}' from {n} bones")
    left = [b.name for b in arm.data.bones if ":" in b.name] + [g.name for g in mesh.vertex_groups if ":" in g.name]
    if left:
        raise RuntimeError(f"names still containing ':' -> {left}")


def chain(arm, start, end):
    """Bone names from start down to end following the parent links."""
    b = arm.data.bones.get(end)
    out = []
    while b is not None:
        out.append(b.name)
        if b.name == start:
            return list(reversed(out))
        b = b.parent
    return None


def write_bones_json(arm, path, bounds):
    bones = arm.data.bones
    req = ["Hips", "Spine", "Spine1", "Spine2", "Neck", "Head",
           "LeftShoulder", "LeftArm", "LeftForeArm", "LeftHand",
           "RightShoulder", "RightArm", "RightForeArm", "RightHand",
           "LeftUpLeg", "LeftLeg", "LeftFoot", "LeftToeBase",
           "RightUpLeg", "RightLeg", "RightFoot", "RightToeBase"]
    missing = [n for n in req if n not in bones]
    if missing:
        raise RuntimeError(f"biped chain incomplete, missing {missing}")

    roles = {
        "root": [b.name for b in bones if b.parent is None][0],
        "hips": "Hips",
        "spine_chain": chain(arm, "Spine", "Spine2"),
        "neck": "Neck",
        "head": "Head",
        "arm_l": chain(arm, "LeftShoulder", "LeftHand"),
        "arm_r": chain(arm, "RightShoulder", "RightHand"),
        "hand_l": "LeftHand",
        "hand_r": "RightHand",
        "leg_l": chain(arm, "LeftUpLeg", "LeftFoot"),
        "leg_r": chain(arm, "RightUpLeg", "RightFoot"),
        "foot_l": "LeftFoot",
        "foot_r": "RightFoot",
        "toe_l": "LeftToeBase",
        "toe_r": "RightToeBase",
        "fingers_l": sorted(b.name for b in bones if b.name.startswith("LeftHand") and b.name != "LeftHand"),
        "fingers_r": sorted(b.name for b in bones if b.name.startswith("RightHand") and b.name != "RightHand"),
    }
    for k, v in roles.items():
        if v is None:
            raise RuntimeError(f"chain {k} is broken")
    known = set(req) | set(roles["fingers_l"]) | set(roles["fingers_r"])
    roles["other"] = [b.name for b in bones if b.name not in known]

    # rest pose: angle of the upper arm below horizontal
    def arm_drop(name):
        b = bones[name]
        d = (b.tail_local - b.head_local).normalized()
        return math.degrees(math.asin(max(-1.0, min(1.0, -d.z))))
    drop = (arm_drop("LeftArm") + arm_drop("RightArm")) / 2
    rest = "T" if drop < 20 else "A"

    def r(v):
        return [round(x, 5) for x in v]

    data = {
        "source": "Tripo Studio auto-rig (Mixamo preset), prefix 'mixamorig:' stripped",
        "units": "metres, Blender axes (Z up, figure faces -Y, character's left = +X). The exported FBX is in cm and lands +Y-facing in UE.",
        "height_m": round(bounds[1].z - bounds[0].z, 4),
        "bounds_min": r(bounds[0]),
        "bounds_max": r(bounds[1]),
        "bone_count": len(bones),
        "rest_pose": rest,
        "upper_arm_drop_deg": round(drop, 2),
        "roles": roles,
        "bones": [
            {
                "name": b.name,
                "parent": b.parent.name if b.parent else None,
                "children": [c.name for c in b.children],
                "head": r(b.head_local),
                "tail": r(b.tail_local),
                "length": round(b.length, 5),
                "deform": b.use_deform,
                "matrix_local": [r(row) for row in b.matrix_local],
            }
            for b in bones
        ],
    }
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)
    log(f"wrote {path}: {len(bones)} bones, rest pose {rest} (upper arm drop {drop:.1f} deg)")

    def tree(b, d=0):
        log("  " * d + b.name)
        for c in b.children:
            tree(c, d + 1)
    log("hierarchy:")
    for b in bones:
        if b.parent is None:
            tree(b)
    return roles


# --------------------------------------------------------------------------------------------- weights

# Tail: Tripo skins the big tail mostly to a thigh bone, so it would swing with that leg. It is moved to Hips.
# Selection, in Blender metres after place_on_ground.
#
# Explicit entry TAIL[key] (thresholds measured on that model):
#   core  = z in [zmin, zmax] and (y > back_y  or  (x * side > side_x * side and y > side_ymin)),
#           minus vertices dominated by an arm / hand / finger / left-leg / foot bone (from_bones None), or only
#           vertices whose dominant bone matches one of the from_bones patterns (from_bones a list),
#           then only the connected regions (over core-only edges) of at least min_region vertices (drops specks);
#   band  = vertices reached over mesh edges from the core within `blend` m of path length; their weights
#           blend from the original to `bone` with a smoothstep, so the tail root has no hard seam.
#   `side` is the sign of X the tail curls to (the figure's left is +X).
#
# Warrior: the tail sits behind the body and curls out to the figure's right (+Y and -X), skinned to RightUpLeg.
# Body ~0.97 m, trunk surface at about |x| <= 0.23, y <= 0.10, right leg/boot at x >= -0.21, y <= 0.03,
# right forearm/paw at z >= 0.50, y <= 0.01.
#
# Auto mode (key without an entry): candidates = y > back_y (back_y = max Y of the vertices dominated by Spine /
# Spine1, + AUTO_BACK_MARGIN) and z in [hips - AUTO_Z_BELOW, hips + AUTO_Z_ABOVE] (hips = Hips head z) and the
# dominant bone is a leg bone (AUTO_FROM_BONES); then the same region filter, band and blend. A core under
# AUTO_MIN_CORE vertices is left alone ("tail: nothing to reweight").
TAIL = {
    "warrior": {"zmin": 0.10, "zmax": 0.56, "back_y": 0.13, "side_x": -0.25, "side_ymin": -0.06, "side": -1,
                "min_region": 50, "blend": 0.035, "bone": "Hips", "from_bones": None},
    # Archer: tail behind (y > 0.11) and out to the figure's right (x < -0.24), z 0.17..0.49; Tripo gave it ~0.54 Hips +
    # ~0.46 UpLeg, so auto mode (leg-dominated only) finds nothing. Right arm z >= 0.47 (excluded as banned bones).
    "archer": {"zmin": 0.12, "zmax": 0.50, "back_y": 0.11, "side_x": -0.24, "side_ymin": -0.15, "side": -1,
               "min_region": 50, "blend": 0.035, "bone": "Hips", "from_bones": None},
    # Blacksmith: tail behind (y > 0.105; Spine trunk max y 0.102) and out to the figure's right, x -0.48..0.05, z 0.16..0.54.
    # Auto mode caught the RightUpLeg-dominated blade (y > 0.132) but left the ~10 cm root stub (y 0.105..0.184, Hips-dominant
    # with ~0.5-0.7 Left/RightUpLeg). The side test adds nothing here (all tail at x < -0.30 has y > 0.13).
    "blacksmith": {"zmin": 0.12, "zmax": 0.56, "back_y": 0.105, "side_x": -0.30, "side_ymin": 0.05, "side": -1,
                   "min_region": 50, "blend": 0.035, "bone": "Hips", "from_bones": None},
    # Jarl: tail wholly behind the body (y 0.078..0.337, x -0.476..0.004, z 0.135..0.484), weighted ~0.49 Hips + ~0.28
    # RightUpLeg + ~0.16 LeftUpLeg + ~0.07 Spine, so auto mode finds nothing. Trunk y <= 0.063, buttock/leg y <= 0.083,
    # right arm z >= 0.51; nothing but tail at y > 0.085, z < 0.50, so no side test.
    "jarl": {"zmin": 0.12, "zmax": 0.50, "back_y": 0.085, "side_x": None, "side_ymin": None, "side": None,
             "min_region": 50, "blend": 0.035, "bone": "Hips", "from_bones": None},
}
AUTO_TAIL = {"min_region": 50, "blend": 0.035, "bone": "Hips"}
AUTO_FROM_BONES = ["*UpLeg", "*Leg"]
AUTO_BACK_MARGIN = 0.03
AUTO_Z_BELOW, AUTO_Z_ABOVE = 0.25, 0.30
AUTO_MIN_CORE = 100
EXPLICIT_MIN_CORE = 300   # an explicit entry that finds fewer is a failure, not a skip

BANNED_PREFIX = ("LeftShoulder", "LeftArm", "LeftForeArm", "LeftHand", "RightShoulder", "RightArm",
                 "RightForeArm", "RightHand", "LeftUpLeg", "LeftLeg", "LeftFoot", "LeftToe",
                 "RightFoot", "RightToe", "Neck", "Head")


def _dominant(v, gidx):
    if not v.groups:
        return None
    return gidx[max(v.groups, key=lambda t: t.weight).group]


def tail_config(arm, mesh):
    """The tail selection settings for KEY: its TAIL entry, or auto settings measured on this mesh."""
    if KEY in TAIL:
        cfg = dict(TAIL[KEY])
        cfg["auto"] = False
        return cfg
    me = mesh.data
    gidx = {g.index: g.name for g in mesh.vertex_groups}
    trunk_y = [v.co.y for v in me.vertices if _dominant(v, gidx) in ("Spine", "Spine1")]
    if not trunk_y:
        raise RuntimeError("auto tail: no vertices dominated by Spine / Spine1")
    hips_z = (arm.matrix_world @ arm.data.bones["Hips"].head_local).z
    cfg = dict(AUTO_TAIL)
    cfg.update({"auto": True, "zmin": hips_z - AUTO_Z_BELOW, "zmax": hips_z + AUTO_Z_ABOVE,
                "back_y": max(trunk_y) + AUTO_BACK_MARGIN, "side_x": None, "side_ymin": None, "side": None,
                "from_bones": list(AUTO_FROM_BONES), "hips_z": hips_z})
    log(f"tail auto mode for '{KEY}': hips z {hips_z:.3f}, trunk max y {max(trunk_y):.3f} -> back_y {cfg['back_y']:.3f}, "
        f"z {cfg['zmin']:.3f}..{cfg['zmax']:.3f}, dominant bone in {cfg['from_bones']}")
    return cfg


def tail_selection(arm, mesh, cfg):
    me = mesh.data
    gidx = {g.index: g.name for g in mesh.vertex_groups}
    zmin, zmax, back_y = cfg["zmin"], cfg["zmax"], cfg["back_y"]
    side, side_x, side_ymin = cfg["side"], cfg["side_x"], cfg["side_ymin"]
    from_bones = cfg["from_bones"]

    cand = set()
    cand_dom = {}
    for v in me.vertices:
        x, y, z = v.co
        if not (zmin <= z <= zmax):
            continue
        behind = y > back_y
        beside = side_x is not None and x * side > side_x * side and y > side_ymin
        if not (behind or beside):
            continue
        d = _dominant(v, gidx)
        if from_bones is None:
            if d and d.startswith(BANNED_PREFIX):
                continue
        elif d is None or not any(fnmatch.fnmatchcase(d, p) for p in from_bones):
            continue
        cand.add(v.index)
        cand_dom[d] = cand_dom.get(d, 0) + 1
    log(f"tail candidates: {len(cand)} vertices, per dominant bone {dict(sorted(cand_dom.items(), key=lambda t: -t[1]))}")

    adj = [[] for _ in me.vertices]
    for e in me.edges:
        a, b = e.vertices
        ln = (me.vertices[a].co - me.vertices[b].co).length
        adj[a].append((b, ln))
        adj[b].append((a, ln))

    # connected regions within the candidate set
    core, regions, seen = set(), [], set()
    for s in cand:
        if s in seen:
            continue
        stack, reg = [s], []
        seen.add(s)
        while stack:
            c = stack.pop()
            reg.append(c)
            for n, _ in adj[c]:
                if n in cand and n not in seen:
                    seen.add(n)
                    stack.append(n)
        regions.append(len(reg))
        if len(reg) >= cfg["min_region"]:
            core.update(reg)
    regions.sort(reverse=True)

    # geodesic band from the core
    import heapq
    blend = cfg["blend"]
    dist = {i: 0.0 for i in core}
    heap = [(0.0, i) for i in core]
    heapq.heapify(heap)
    while heap:
        d, c = heapq.heappop(heap)
        if d > dist.get(c, 1e9):
            continue
        for n, ln in adj[c]:
            nd = d + ln
            if nd < blend and nd < dist.get(n, 1e9):
                dist[n] = nd
                heapq.heappush(heap, (nd, n))
    band = {i: d for i, d in dist.items() if i not in core}
    return core, band, regions


def tail_selection_info(cfg):
    if cfg["auto"]:
        return {"mode": "auto", "z": [round(cfg["zmin"], 4), round(cfg["zmax"], 4)],
                "back_y_gt": round(cfg["back_y"], 4), "dominant_bone_in": cfg["from_bones"]}
    sel = {"z": [cfg["zmin"], cfg["zmax"]], "back_y_gt": cfg["back_y"]}
    if cfg["side_x"] is not None:
        sel["side_x_lt" if cfg["side"] < 0 else "side_x_gt"] = cfg["side_x"]
        sel["side_y_gt"] = cfg["side_ymin"]
    if cfg["from_bones"] is not None:
        sel["dominant_bone_in"] = cfg["from_bones"]
    return sel


def fix_tail_weights(arm, mesh):
    me = mesh.data
    cfg = tail_config(arm, mesh)
    bone, blend = cfg["bone"], cfg["blend"]
    if bone not in mesh.vertex_groups:
        raise RuntimeError(f"no vertex group {bone}")
    hips = mesh.vertex_groups[bone]
    gidx = {g.index: g.name for g in mesh.vertex_groups}
    core, band, regions = tail_selection(arm, mesh, cfg)
    if cfg["auto"] and len(core) < AUTO_MIN_CORE:
        log(f"tail: nothing to reweight (auto core {len(core)} < {AUTO_MIN_CORE} vertices; candidate regions {regions[:6]})")
        return {"core_vertices": 0, "band_vertices": 0, "blend_m": blend, "bone": bone, "skipped": True,
                "auto_core_found": len(core), "selection": tail_selection_info(cfg)}, set(), {}
    before = {}
    for i in core:
        v = me.vertices[i]
        if v.groups:
            n = gidx[max(v.groups, key=lambda t: t.weight).group]
            before[n] = before.get(n, 0) + 1

    def set_blend(i, f):
        v = me.vertices[i]
        olds = [(g.group, g.weight) for g in v.groups]
        tot = sum(w for _, w in olds) or 1.0
        for gi, w in olds:
            mesh.vertex_groups[gi].add([i], (1.0 - f) * w / tot, 'REPLACE')
        cur_h = next((w / tot for gi, w in olds if gi == hips.index), 0.0)
        hips.add([i], (1.0 - f) * cur_h + f, 'REPLACE')

    for i in core:
        set_blend(i, 1.0)
    for i, d in band.items():
        t = min(1.0, d / blend)
        set_blend(i, 1.0 - t * t * (3 - 2 * t))   # smoothstep falloff: 1 at the core edge -> 0 at `blend`
    zs = [me.vertices[i].co for i in core]
    ext = None
    if zs:
        ext = {"min": [round(min(c[k] for c in zs), 3) for k in range(3)],
               "max": [round(max(c[k] for c in zs), 3) for k in range(3)]}
    log(f"tail: {len(core)} core vertices -> {bone} 100%, {len(band)} blend-band vertices "
        f"({blend * 100:.1f} cm); candidate regions {regions[:6]}; core was dominated by {before}; extent {ext}")
    if not cfg["auto"] and len(core) < EXPLICIT_MIN_CORE:
        raise RuntimeError("tail selection suspiciously small")
    return {"core_vertices": len(core), "band_vertices": len(band), "blend_m": blend, "bone": bone,
            "core_dominant_before": before, "core_extent_m": ext,
            "selection": tail_selection_info(cfg)}, core, band


def fix_weights(arm, mesh, roles):
    me = mesh.data
    deform = {b.name for b in arm.data.bones if b.use_deform}
    gidx = {g.index: g.name for g in mesh.vertex_groups}
    valid = {i for i, n in gidx.items() if n in deform}

    def weights_of(v):
        return [(g.group, g.weight) for g in v.groups if g.group in valid and g.weight > 0.0]

    unweighted = [v.index for v in me.vertices if not weights_of(v)]
    log(f"unweighted vertices: {len(unweighted)} {unweighted[:20]}")
    fixed = []
    if unweighted:
        uw = set(unweighted)
        weighted = [v for v in me.vertices if v.index not in uw]
        from mathutils.kdtree import KDTree
        kd = KDTree(len(weighted))
        for v in weighted:
            kd.insert(v.co, v.index)
        kd.balance()
        for vi in unweighted:
            co, src, dist = kd.find(me.vertices[vi].co)
            for gi, w in weights_of(me.vertices[src]):
                mesh.vertex_groups[gi].add([vi], w, 'REPLACE')
            fixed.append({"vertex": vi, "copied_from": src, "distance_m": round(dist, 5),
                          "weights": {gidx[gi]: round(w, 4) for gi, w in weights_of(me.vertices[vi])}})
            log(f"  vertex {vi}: copied weights from {src} ({dist * 1000:.2f} mm away)")

    tail, tail_core, tail_band = fix_tail_weights(arm, mesh)

    # remove zero weights, limit to 4, normalise
    select_only([mesh])
    bpy.ops.object.mode_set(mode='WEIGHT_PAINT')
    bpy.ops.object.vertex_group_clean(group_select_mode='ALL', limit=0.0, keep_single=True)
    bpy.ops.object.vertex_group_limit_total(group_select_mode='ALL', limit=MAX_INFLUENCES)
    bpy.ops.object.vertex_group_normalize_all(group_select_mode='ALL', lock_active=False)
    bpy.ops.object.mode_set(mode='OBJECT')

    # verify
    max_inf = 0
    bad_norm = 0
    none = 0
    for v in me.vertices:
        ws = weights_of(v)
        max_inf = max(max_inf, len(ws))
        if not ws:
            none += 1
        elif abs(sum(w for _, w in ws) - 1.0) > 1e-3:
            bad_norm += 1
    log(f"weights after fix: unweighted={none} max_influences={max_inf} not_normalised={bad_norm}")
    if none or max_inf > MAX_INFLUENCES or bad_norm:
        raise RuntimeError("weight check failed")
    tail_bone = tail["bone"]
    hips_i = mesh.vertex_groups[tail_bone].index
    not_hips = sum(1 for i in tail_core
                   if any(gi != hips_i and w > 1e-4 for gi, w in weights_of(me.vertices[i])))
    log(f"tail core vertices with any non-{tail_bone} weight after normalise: {not_hips}")
    if not_hips:
        raise RuntimeError("tail core not fully on Hips")

    # paws: vertices dominated by a hand / finger bone that also carry other bones
    paw_ok_l = {"LeftHand", "LeftForeArm"} | set(roles["fingers_l"])
    paw_ok_r = {"RightHand", "RightForeArm"} | set(roles["fingers_r"])
    paw_l = paw_r = 0
    stray = {}
    for v in me.vertices:
        ws = weights_of(v)
        if not ws:
            continue
        dom = gidx[max(ws, key=lambda t: t[1])[0]]
        for side, ok in (("L", paw_ok_l), ("R", paw_ok_r)):
            hand = "LeftHand" if side == "L" else "RightHand"
            if dom == hand or dom in (roles["fingers_l"] if side == "L" else roles["fingers_r"]):
                if side == "L":
                    paw_l += 1
                else:
                    paw_r += 1
                for gi, w in ws:
                    n = gidx[gi]
                    if n not in ok and w > 0.01:
                        stray.setdefault(n, [0, 0.0])
                        stray[n][0] += 1
                        stray[n][1] = max(stray[n][1], w)
    log(f"paw vertices: left={paw_l} right={paw_r}; weighted to other bones: "
        + (", ".join(f"{n} ({c} verts, max {mw:.2f})" for n, (c, mw) in sorted(stray.items())) or "none"))
    return {"fixed_unweighted": fixed, "max_influences": max_inf, "normalised": True,
            "paw_vertices": {"L": paw_l, "R": paw_r},
            "paw_stray_bones": {n: {"verts": c, "max_weight": round(mw, 3)} for n, (c, mw) in stray.items()},
            "tail_reweight": tail}, tail_core, tail_band


# --------------------------------------------------------------------------------------------- textures / export

def copy_textures(fbm_dir, out_dir, mat):
    copied = {}
    files = os.listdir(fbm_dir) if os.path.isdir(fbm_dir) else []
    for suffix, name in TEX_SUFFIXES.items():
        src = [f for f in files if os.path.splitext(f)[0].lower().endswith("_" + suffix)]
        if not src:
            raise RuntimeError(f"texture *_{suffix} not found in {fbm_dir}")
        ext = os.path.splitext(src[0])[1].lower()
        dst = os.path.join(out_dir, name + ext)
        shutil.copyfile(os.path.join(fbm_dir, src[0]), dst)
        copied[suffix] = dst
        log(f"texture {src[0]} -> {os.path.basename(dst)}")
    # point the material's images at the copies so the FBX references sit next to it
    if mat and mat.node_tree:
        for node in mat.node_tree.nodes:
            if node.type == 'TEX_IMAGE' and node.image:
                fn = os.path.basename(bpy.path.abspath(node.image.filepath)).lower()
                for suffix, dst in copied.items():
                    if os.path.splitext(fn)[0].endswith("_" + suffix):
                        node.image.filepath = dst
                        node.image.name = os.path.basename(dst)
                        node.image.reload()
        log("material image nodes: " + ", ".join(
            f"{n.name}={n.image.name if n.image else None}" for n in mat.node_tree.nodes if n.type == 'TEX_IMAGE'))
    return copied


def to_centimetres(arm, mesh):
    """Turn the scene into real centimetre data: 1 Blender unit = 1 cm, bones and vertices x100, object scale 1.

    Everything before this (tail selection thresholds, json, stills) works in metres; this runs last, right
    before the export, so the FBX carries cm numbers with UnitScaleFactor 1 and no compensating node scale.
    """
    scene = bpy.context.scene
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.scale_length = 0.01
    select_only([arm, mesh], arm)
    arm.scale = (100.0, 100.0, 100.0)
    bpy.context.view_layer.update()
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for o in (arm, mesh):
        if any(abs(s - 1.0) > 1e-6 for s in o.matrix_world.to_scale()):
            raise RuntimeError(f"{o.name} still scaled after cm conversion: {tuple(o.matrix_world.to_scale())}")
    hips = arm.data.bones["Hips"]
    zs = [v.co.z for v in mesh.data.vertices]
    log(f"cm conversion: Hips head {tuple(round(v, 3) for v in hips.head_local)}, mesh z {min(zs):.2f}..{max(zs):.2f}")


def export_fbx(arm, mesh, path):
    if arm.animation_data:
        arm.animation_data_clear()
    for a in list(bpy.data.actions):
        bpy.data.actions.remove(a)
    for pb in arm.pose.bones:
        pb.matrix_basis = Matrix.Identity(4)
    to_centimetres(arm, mesh)
    select_only([arm, mesh], arm)
    bpy.ops.export_scene.fbx(
        filepath=path,
        use_selection=True,
        object_types={'ARMATURE', 'MESH'},
        global_scale=1.0,
        apply_unit_scale=True,
        apply_scale_options='FBX_SCALE_NONE',  # data already in cm (scale_length 0.01 -> UnitScaleFactor 1): write as is
        axis_forward='-Z', axis_up='Y',
        use_mesh_modifiers=False,
        mesh_smooth_type='FACE',
        use_tspace=False,
        add_leaf_bones=False,
        primary_bone_axis='Y', secondary_bone_axis='X',
        use_armature_deform_only=False,
        armature_nodetype='NULL',
        bake_anim=False,
        path_mode='STRIP',
        embed_textures=False,
        use_custom_props=False,
    )
    log(f"exported {path} ({os.path.getsize(path)} bytes)")


# --------------------------------------------------------------------------------------------- renders

def setup_render(mesh, bounds):
    scene = bpy.context.scene
    try:
        scene.render.engine = 'BLENDER_WORKBENCH'
        sh = scene.display.shading
        sh.light = 'STUDIO'
        sh.color_type = 'TEXTURE'
        sh.show_specular_highlight = False
        engine = 'BLENDER_WORKBENCH'
    except Exception as e:  # noqa: BLE001
        log(f"workbench unavailable ({e}), using EEVEE")
        scene.render.engine = 'BLENDER_EEVEE'
        engine = 'BLENDER_EEVEE'
        for name, rot, energy in (("Key", (math.radians(50), 0, math.radians(-35)), 3.0),
                                  ("Fill", (math.radians(60), 0, math.radians(150)), 1.5)):
            ld = bpy.data.lights.new(name, 'SUN')
            ld.energy = energy
            lo = bpy.data.objects.new(name, ld)
            lo.rotation_euler = rot
            scene.collection.objects.link(lo)
    if scene.world is None:
        scene.world = bpy.data.worlds.new("World")
    scene.world.color = (0.55, 0.57, 0.6)
    scene.render.resolution_x = 768
    scene.render.resolution_y = 1024
    scene.render.film_transparent = False
    scene.render.image_settings.file_format = 'PNG'

    cam_data = bpy.data.cameras.new("CheckCam")
    cam_data.type = 'ORTHO'
    h = bounds[1].z - bounds[0].z
    w = max(bounds[1].x - bounds[0].x, bounds[1].y - bounds[0].y)
    aspect = scene.render.resolution_x / scene.render.resolution_y
    cam_data.ortho_scale = max(h, w / aspect) * 1.12   # ortho_scale spans the longer (vertical) side
    try:
        scene.view_settings.view_transform = 'Standard'
    except Exception:  # noqa: BLE001
        pass
    cam = bpy.data.objects.new("CheckCam", cam_data)
    scene.collection.objects.link(cam)
    scene.camera = cam
    log(f"render engine {engine}")
    return cam


def render_view(cam, bounds, direction, path):
    """direction: unit vector from the figure towards the camera (Blender axes)."""
    centre = (bounds[0] + bounds[1]) / 2
    d = Vector(direction).normalized()
    cam.location = centre + d * 5.0
    cam.rotation_euler = (-d).to_track_quat('-Z', 'Y').to_euler()
    bpy.context.scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    log(f"rendered {path}")


def rotate_bone_world(arm, name, axis, deg):
    """Rotate a pose bone about its head by `deg` around a world/armature axis."""
    pb = arm.pose.bones[name]
    m = pb.matrix.copy()
    head = m.translation.copy()
    rot = Matrix.Rotation(math.radians(deg), 4, axis)
    pb.matrix = Matrix.Translation(head) @ rot @ Matrix.Translation(-head) @ m
    bpy.context.view_layer.update()


def render_pose(arm, cam, bounds, path, arms, leg, view):
    """Swing the upper arms in `arms` down 60 deg and `leg` forward 40 deg, render from `view`, reset."""
    bones = arm.data.bones
    # character's left is +X when facing -Y; pick the sign from the bone positions to be safe
    for name in arms:
        x = bones[name].head_local.x
        # Ry(+a) swings +X downwards, Ry(-a) swings -X downwards
        rotate_bone_world(arm, name, 'Y', 60 if x > 0 else -60)
    # forward is -Y: Rx(-a) swings a downward bone towards -Y
    rotate_bone_world(arm, leg, 'X', -40)
    render_view(cam, bounds, view, path)
    for pb in arm.pose.bones:
        pb.matrix_basis = Matrix.Identity(4)
    bpy.context.view_layer.update()


def render_tail_selection(mesh, cam, bounds, core, band, path):
    """Rest pose, tail core red, blend band yellow, rest grey; seen from behind on the figure's right."""
    me = mesh.data
    attr = me.color_attributes.new("TailCheck", 'FLOAT_COLOR', 'POINT')
    for v in me.vertices:
        if v.index in core:
            c = (0.9, 0.1, 0.1, 1.0)
        elif v.index in band:
            c = (0.95, 0.85, 0.1, 1.0)
        else:
            c = (0.6, 0.6, 0.6, 1.0)
        attr.data[v.index].color = c
    me.color_attributes.active_color = attr
    sh = bpy.context.scene.display.shading
    prev = sh.color_type
    sh.color_type = 'VERTEX'
    render_view(cam, bounds, (-0.8, 0.9, 0.2), path)
    sh.color_type = prev
    me.color_attributes.remove(attr)


# --------------------------------------------------------------------------------------------- main

def main():
    key, in_fbx, out_dir = parse_args()
    set_character(key)
    os.makedirs(out_dir, exist_ok=True)
    fbm_dir = os.path.splitext(in_fbx)[0] + ".fbm"
    log(f"character {KEY} ({PASCAL})\ninput {in_fbx}\noutput {out_dir}")

    clear_scene()
    arm, meshes = import_fbx(in_fbx)
    log(f"armature {arm.name}, meshes {[m.name for m in meshes]}")
    apply_transforms(arm, meshes)
    mesh = join_meshes(arm, meshes)
    mat = single_material(mesh)
    if mat:
        mat.name = MAT_NAME
    log(f"triangles {tri_count(mesh)}, vertices {len(mesh.data.vertices)}")
    loose = delete_small_loose_parts(mesh)
    log(f"after loose-part cleanup: triangles {tri_count(mesh)}, vertices {len(mesh.data.vertices)}")
    decimated = maybe_decimate(mesh)

    strip_prefix(arm, mesh)
    mn, mx = place_on_ground(arm, mesh)
    log(f"bounds: min={tuple(round(v, 4) for v in mn)} max={tuple(round(v, 4) for v in mx)} height={mx.z - mn.z:.4f} m")
    check_facing(arm)

    jp = os.path.join(out_dir, f"{KEY}_bones.json")
    roles = write_bones_json(arm, jp, (mn, mx))
    weights, tail_core, tail_band = fix_weights(arm, mesh, roles)

    # weight summary + cleanup info into the json
    with open(jp, encoding="utf-8") as f:
        data = json.load(f)
    data["mesh"] = {"triangles": tri_count(mesh), "vertices": len(mesh.data.vertices),
                    "material": mat.name if mat else None, "decimated": decimated, "loose_parts": loose}
    data["weights"] = weights
    with open(jp, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)

    copy_textures(fbm_dir, out_dir, mat)

    cam = setup_render(mesh, (mn, mx))
    views = {"front": (0, -1, 0), "side": (1, 0, 0), "back": (0, 1, 0), "34": (-0.7, -1.0, 0.25)}
    for name, d in views.items():
        render_view(cam, (mn, mx), d, os.path.join(out_dir, f"check_{name}.png"))
    # both arms down, left thigh forward; front 3/4 from the figure's left
    render_pose(arm, cam, (mn, mx), os.path.join(out_dir, "check_pose.png"),
                ("LeftArm", "RightArm"), "LeftUpLeg", (0.55, -1.0, 0.15))
    # right arm down, right thigh forward; front 3/4 from the figure's right (tail side)
    render_pose(arm, cam, (mn, mx), os.path.join(out_dir, "check_pose2.png"),
                ("RightArm",), "RightUpLeg", (-0.8, -1.0, 0.15))
    if bpy.context.scene.render.engine == 'BLENDER_WORKBENCH':
        render_tail_selection(mesh, cam, (mn, mx), tail_core, tail_band, os.path.join(out_dir, "check_tail.png"))

    # export last: it converts the scene to centimetres (the stills and json above are in metres)
    height_m = mx.z - mn.z
    hips_z_m = (arm.matrix_world @ arm.data.bones["Hips"].head_local).z
    fbx = os.path.join(out_dir, f"{MESH_NAME}.fbx")
    export_fbx(arm, mesh, fbx)
    verify_export(fbx, height_m, hips_z_m)
    log("done")


def verify_export(path, height_m, hips_z_m):
    """Re-import the FBX into an empty cm scene: object scale 1, height and Hips z within 2% of the measured
    pre-export figure x 100 (metres -> cm)."""
    clear_scene()
    for blk in (bpy.data.meshes, bpy.data.armatures):
        for d in list(blk):
            if d.users == 0:
                blk.remove(d)
    bpy.ops.import_scene.fbx(filepath=path, use_anim=False, ignore_leaf_bones=False, automatic_bone_orientation=False)
    arm = [o for o in bpy.context.scene.objects if o.type == 'ARMATURE'][0]
    mesh = [o for o in bpy.context.scene.objects if o.type == 'MESH'][0]
    hips = arm.matrix_world @ arm.data.bones["Hips"].head_local
    zs = [(mesh.matrix_world @ v.co).z for v in mesh.data.vertices]
    log(f"re-import check: armature scale {tuple(round(s, 4) for s in arm.scale)} mesh scale "
        f"{tuple(round(s, 4) for s in mesh.scale)} Hips z {hips.z:.2f} height {max(zs) - min(zs):.2f} (scene units = cm)")
    if any(abs(s - 1.0) > 1e-4 for s in tuple(arm.scale) + tuple(mesh.scale)):
        raise RuntimeError("re-imported FBX has a non-unit object scale")
    exp_h, exp_hips = height_m * 100.0, hips_z_m * 100.0
    got_h = max(zs) - min(zs)
    log(f"re-import expected: height {exp_h:.2f} Hips z {exp_hips:.2f} (+-2%)")
    if not (abs(got_h - exp_h) <= 0.02 * exp_h and abs(hips.z - exp_hips) <= 0.02 * abs(exp_hips)):
        raise RuntimeError("re-imported FBX is not in centimetres")


if __name__ == "__main__":
    try:
        main()
    except SystemExit:
        raise
    except Exception:  # noqa: BLE001
        traceback.print_exc()
        sys.stdout.flush()
        sys.exit(1)
    sys.exit(0)
