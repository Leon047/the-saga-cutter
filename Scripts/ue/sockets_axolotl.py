"""Hand sockets on one axolotl character's mesh (Docs/Plans/axolotl-all-classes.md, S4; first made for the warrior in
S6 of Docs/Plans/axolotl-warrior-tripo.md).

weapon_r / staff_r on RightHand, shield_l / bow_l on LeftHand, in the Viking socket convention
(+Z = where the shaft / blade points, +X = edge / facing / shooting side).

Orientation: the retargeter copies each Viking arm bone's global rotation change one to one onto the axolotl
bone (Fist_R -> RightHand), so a socket that should turn with the paw exactly as the Viking socket turns with
the fist is   R_axo_local = inv(Hand_comp(t)) * Fist_comp(t) * R_viking_local   at any frame t of a clip and
its retargeted copy. It is solved on Idle frame 0 and checked on other frames.
Position: centre of the paw, halfway between the hand bone and the middle-finger tip (<Hand>Middle4).
Units: the axolotl bones have scale 1 and cm, so the location is plain cm in hand space; socket scale 1.

Rerunnable: existing sockets with these names are removed first (mesh and skeleton). Run in the editor
(execute_python, which allows top-level await; CHAR defaults to "warrior"):
    CHAR = "archer"; exec(open(r"<repo>/Scripts/ue/sockets_axolotl.py").read()); await run()
A global SOCKET_LEAN / SOCKET_SHIFT set before exec overrides the per-character table value.
"""
import copy
import math
import unreal

if "CHAR" not in globals():
    CHAR = "warrior"
PASCAL = CHAR.capitalize()

AX_MESH = "/Game/Characters/axolotl_%s/Axolotl_%s" % (CHAR, PASCAL)
VK_MESH = "/Game/Characters/viking_male/Viking_Male"
AX_CLIP = "/Game/Characters/axolotl_%s/Axolotl_%sCharacterArmature_" % (CHAR, PASCAL)
VK_CLIP = "/Game/Characters/viking_male/Viking_MaleCharacterArmature_"
SOCKETS = [  # name, viking bone, axolotl bone, paw tip bone
    ("weapon_r", "Fist_R", "RightHand", "RightHandMiddle4"),
    ("staff_r", "Fist_R", "RightHand", "RightHandMiddle4"),
    ("shield_l", "Fist_L", "LeftHand", "LeftHandMiddle4"),
    ("bow_l", "Fist_L", "LeftHand", "LeftHandMiddle4"),
]
# S8 (warrior plan) per-socket corrections on top of the Viking-matched orientation / paw-centre position:
#  - weapon_r: lean the shaft by (deg about socket +X, deg about socket +Y), post-multiplied in socket space. +X (the edge)
#    is unchanged, so the edge still leads the swing. With the Viking orientation the axe / sword crossed the big head in
#    Idle, Run and RecieveHit (clearance to a 21 cm head sphere -3.5 / -18 / -4 cm); with (-40, -5): +13 / -1.6 / +13,
#    SwordSlash +0.8 (scratch scan over the retargeted clips, see the S8 report).
#  - shield_l: shift (cm toward the elbow, cm along socket +X = out of the shield face) so the shield sits on the
#    forearm like the Viking socket (~15 Viking units up the forearm) instead of on the paw.
# A key without an entry uses the warrior values.
SOCKET_TUNE = {
    "warrior": {
        "SOCKET_LEAN": {"weapon_r": (-40.0, -5.0)},
        "SOCKET_SHIFT": {"shield_l": (9.0, 1.5)},
    },
    # S9 (all-classes plan). Hakon: with the Viking orientation the bow showed its string side outward (socket +X, the
    # bow's back, pointed at the body) and leaned 21-25 deg in Idle. 160 deg about +Y turns the string toward the body
    # (the bow is symmetric, so it may end up upside down); 10 deg about +X stands it up: tilt Idle 4, Shoot_OneHanded
    # 5 deg (Walk 26, Run 55 with the arm swing), head clearance >= 8 cm (scratch scan over the retargeted clips).
    "archer": {
        "SOCKET_LEAN": {"weapon_r": (-40.0, -5.0), "bow_l": (10.0, 160.0)},
        "SOCKET_SHIFT": {"shield_l": (9.0, 1.5)},
    },
    # Sigrun: the staff stood upright (Idle tilt 5 deg) but its foot hung 26 cm (mesh) above the ground with the short
    # axolotl arm. The forearm runs within ~21 deg of the staff axis in every clip, so a shift of 14 cm away from the
    # elbow slides the staff down along itself (the 3.2 cm along +X cancels most of the sideways part): foot at
    # 14 cm in Idle, top at shoulder height. No lean helps Punch: the paw is at the face there, so the staff crosses
    # the crown in every orientation that keeps Idle upright.
    "druid": {
        "SOCKET_LEAN": {"weapon_r": (-40.0, -5.0)},
        "SOCKET_SHIFT": {"shield_l": (9.0, 1.5), "staff_r": (-14.0, 3.2)},
    },
}
_stune = copy.deepcopy(SOCKET_TUNE.get(CHAR, SOCKET_TUNE["warrior"]))
if "SOCKET_LEAN" not in globals():
    SOCKET_LEAN = _stune["SOCKET_LEAN"]
if "SOCKET_SHIFT" not in globals():
    SOCKET_SHIFT = _stune["SOCKET_SHIFT"]
FOREARM = {"RightHand": "RightForeArm", "LeftHand": "LeftForeArm"}
P = unreal.AnimPoseExtensions
W = unreal.AnimPoseSpaces.WORLD
E = unreal.AnimPoseEvaluationOptions()
log = ["%s: lean %s shift %s" % (CHAR, SOCKET_LEAN, SOCKET_SHIFT)]


def comp_rot(seq, t, bone):
    return P.get_bone_pose(P.get_anim_pose_at_time(seq, t, E), bone, W).rotation


def ang(q):
    return math.degrees(q.get_angle())


ax_mesh = unreal.load_asset(AX_MESH)
vk_mesh = unreal.load_asset(VK_MESH)
ax_skel = ax_mesh.skeleton
ref = ax_skel.get_reference_pose()
vk_idle, ax_idle = unreal.load_asset(VK_CLIP + "Idle"), unreal.load_asset(AX_CLIP + "Idle")

# remove old copies (on the mesh and on the skeleton)
sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
for name in [s[0] for s in SOCKETS] + ["Socket", "None"]:  # (+ unnamed leftovers of failed attempts)
    for _ in range(4):
        if ax_mesh.find_socket(name) is None:
            break
        sub.remove_socket(ax_mesh, name)

PLAN = []
for name, vbone, abone, tip in SOCKETS:
    vs = vk_mesh.find_socket(name)
    r_vk = vs.relative_rotation.quaternion()
    # solve on Idle frame 0
    sock_comp = comp_rot(vk_idle, 0.0, vbone).multiply(r_vk)
    r_ax = comp_rot(ax_idle, 0.0, abone).inversed().multiply(sock_comp)
    # check on other clips / frames: the axolotl socket's component rotation vs the Viking socket's
    errs = []
    for clip, t in (("Idle", 2.0), ("Walk", 0.4), ("SwordSlash", 0.5), ("Run", 0.3)):
        sv, sa = unreal.load_asset(VK_CLIP + clip), unreal.load_asset(AX_CLIP + clip)
        a = comp_rot(sv, t, vbone).multiply(r_vk)
        b = comp_rot(sa, t, abone).multiply(r_ax)
        errs.append("%s %.1f" % (clip, ang(a.inversed().multiply(b))))
    # position: halfway between hand and paw tip, in hand space (ref pose)
    hand = P.get_bone_pose(ref, abone, W)
    mid = (hand.translation + P.get_bone_pose(ref, tip, W).translation) * 0.5
    loc = hand.inverse_transform_location(mid)

    if name in SOCKET_LEAN:
        ax_deg, ay_deg = SOCKET_LEAN[name]

        def _q(axis, deg):
            s = math.sin(math.radians(deg) / 2)
            return unreal.Quat(axis[0] * s, axis[1] * s, axis[2] * s, math.cos(math.radians(deg) / 2))
        r_ax = r_ax.multiply(_q((1, 0, 0), ax_deg)).multiply(_q((0, 1, 0), ay_deg))
    if name in SOCKET_SHIFT:
        up_cm, out_cm = SOCKET_SHIFT[name]
        elbow = hand.inverse_transform_location(P.get_bone_pose(ref, FOREARM[abone], W).translation)
        loc = loc + elbow * (up_cm / elbow.length()) + r_ax.rotate_vector(unreal.Vector(out_cm, 0, 0))

    PLAN.append((name, abone, loc, r_ax.rotator()))
    log.append("%-8s on %-9s loc (%.2f, %.2f, %.2f) rot %s | Viking %s rot %s | check err deg: %s" % (
        name, abone, loc.x, loc.y, loc.z, r_ax.rotator(), vbone, vs.relative_rotation, ", ".join(errs)))



async def run():
    """Create the sockets. SkeletalMeshSocket.socket_name / bone_name are read-only in Python, so the
    EditorToolset AddSocket / SetSocketTransform tools do it (call_tool is provided by execute_python)."""
    mesh_ref = {"refPath": ax_mesh.get_path_name()}
    for name, abone, loc, rot in PLAN:
        await call_tool("EditorToolset.SkeletalMeshToolset.AddSocket", mesh=mesh_ref, socketName=name, boneName=abone)
        await call_tool("EditorToolset.SkeletalMeshToolset.SetSocketTransform", mesh=mesh_ref, socketName=name,
                        translation={"x": loc.x, "y": loc.y, "z": loc.z},
                        rotation={"pitch": rot.pitch, "yaw": rot.yaw, "roll": rot.roll},
                        scale={"x": 1.0, "y": 1.0, "z": 1.0})
    unreal.EditorAssetLibrary.save_loaded_asset(ax_skel, False)
    unreal.EditorAssetLibrary.save_loaded_asset(ax_mesh, False)
    got = []
    for i in range(ax_mesh.num_sockets()):
        s = ax_mesh.get_socket_by_index(i)
        got.append("%s on %s (%s) loc %s rot %s scale %s" % (s.socket_name, s.bone_name, s.get_outer().get_name(),
                                                             s.relative_location, s.relative_rotation, s.relative_scale))
    log.append("sockets now:\n  " + "\n  ".join(got))
    return "\n".join(log)

# Usage: CHAR = "<key>"; exec(open(r"<repo>/Scripts/ue/sockets_axolotl.py").read()); await run()
