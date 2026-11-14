# Measure a hand socket over clips in component space (mesh units = cm of the 97 cm figure; figure faces +Y).
# Set CHAR, SOCK, LEN (item length along socket +Z in mesh cm, centred at GRIP_FRAC along it), CLIPS before exec.
import unreal, math
PASCAL = CHAR.capitalize()
F = "/Game/Characters/axolotl_%s/" % CHAR
P = unreal.AnimPoseExtensions
W = unreal.AnimPoseSpaces.WORLD
E = unreal.AnimPoseEvaluationOptions()
mesh = unreal.load_asset(F + "Axolotl_" + PASCAL)
s = mesh.find_socket(SOCK)
def _q(axis, deg):
    sn = math.sin(math.radians(deg) / 2)
    return unreal.Quat(axis[0] * sn, axis[1] * sn, axis[2] * sn, math.cos(math.radians(deg) / 2))


_r = s.relative_rotation.quaternion()
if "LEAN" in globals():  # extra trial lean on top of the saved socket (same composition as sockets_axolotl.py)
    _r = _r.multiply(_q((1, 0, 0), LEAN[0])).multiply(_q((0, 1, 0), LEAN[1]))
srel = unreal.Transform(s.relative_location, _r.rotator(), unreal.Vector(1, 1, 1))
ref = mesh.skeleton.get_reference_pose()
head_r = globals().get("HEAD_R", 17.0)


def seg_pt(a, b, p):
    ab = b - a
    t = max(0.0, min(1.0, (p - a).dot(ab) / max(ab.dot(ab), 1e-6)))
    return (a + ab * t - p).length()


rows = []
for clip in CLIPS:
    seq = unreal.load_asset(F + "Axolotl_%sCharacterArmature_%s" % (PASCAL, clip))
    L = seq.get_play_length()
    n = globals().get("NF", 8)
    worst = None
    out = []
    for i in range(n):
        t = min(L * i / (n - 1 if clip in ("Death", "SwordSlash", "Punch", "Shoot_OneHanded", "PickUp", "RecieveHit") else n), L - 0.001)
        pose = P.get_anim_pose_at_time(seq, t, E)
        hand = P.get_bone_pose(pose, s.bone_name, W)
        st = srel * hand  # socket in component space (child * parent)
        o = st.translation
        z = st.rotation.rotate_vector(unreal.Vector(0, 0, 1))
        x = st.rotation.rotate_vector(unreal.Vector(1, 0, 0))
        tilt = math.degrees(math.acos(max(-1, min(1, z.z))))
        head = P.get_bone_pose(pose, "Head", W).translation
        headc = head + unreal.Vector(0, 0, globals().get("HEAD_UP", 12.0))
        a = o - z * (LEN * GRIP_FRAC)
        b = o + z * (LEN * (1 - GRIP_FRAC))
        dh = seg_pt(a, b, headc) - head_r
        sp = P.get_bone_pose(pose, "Spine1", W).translation
        tob = (sp - o)
        tob.z = 0
        tob = tob.normal() if tob.length() > 1e-3 else tob
        xh = unreal.Vector(x.x, x.y, 0)
        xdot = xh.normal().dot(tob) if xh.length() > 1e-3 else 0
        out.append("t%.2f tilt%3.0f Xbody%+.2f head%+5.1f low%5.1f" % (t, tilt, xdot, dh, min(a.z, b.z)))
    rows.append(clip + ": " + " | ".join(out))
RESULT = "\n".join(rows)
