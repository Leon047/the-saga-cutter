# S8 sole / hips measurement for one axolotl character. Set CHAR before exec. Result in RESULT.
# Sole height at a frame = min over Foot / ToeBase / Toe_End (both sides) of (pose z - rest z): the rest pose has the
# soles at z = 0, so each bone's rest height is its clearance above the sole.
import unreal
PASCAL = CHAR.capitalize()
F = "/Game/Characters/axolotl_%s/" % CHAR
P = unreal.AnimPoseExtensions
W = unreal.AnimPoseSpaces.WORLD
E = unreal.AnimPoseEvaluationOptions()
mesh = unreal.load_asset(F + "Axolotl_" + PASCAL)
ref = mesh.skeleton.get_reference_pose()
BONES = [s + b for s in ("Left", "Right") for b in ("Foot", "ToeBase", "Toe_End")]
rest = {b: P.get_bone_pose(ref, b, W).translation.z for b in BONES}
out = {}
for clip in ("Idle", "Walk", "Run", "Death"):
    seq = unreal.load_asset(F + "Axolotl_%sCharacterArmature_%s" % (PASCAL, clip))
    L = seq.get_play_length()
    n = 60
    lo, hi = 1e9, -1e9
    for i in range(n + 1):
        t = min(L * i / n, L - 0.001)
        pose = P.get_anim_pose_at_time(seq, t, E)
        s = min(P.get_bone_pose(pose, b, W).translation.z - rest[b] for b in BONES)
        lo, hi = min(lo, s), max(hi, s)
    end = P.get_anim_pose_at_time(seq, L - 0.001, E)
    out[clip] = (round(lo, 2), round(hi, 2), round(P.get_bone_pose(end, "Hips", W).translation.z, 2))
RESULT = "%s rest hips %.2f | " % (CHAR, P.get_bone_pose(ref, "Hips", W).translation.z) + " ; ".join(
    "%s sole min %.2f max %.2f endHips %.2f" % (k, v[0], v[1], v[2]) for k, v in out.items())
