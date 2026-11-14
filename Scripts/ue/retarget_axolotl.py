"""IK Rigs, retargeter and the 11 retargeted clips for one axolotl character (Docs/Plans/axolotl-all-classes.md, S4;
first made for the warrior in S5 / S8 of Docs/Plans/axolotl-warrior-tripo.md).

Rerunnable: the target IK Rig and this character's retargeter are deleted and rebuilt, the clips are overwritten.
The shared source rig /Game/Characters/viking_male/IK_Viking_Male is created only when missing, never rebuilt.
Run in the editor through execute_python (CHAR defaults to "warrior"):

    CHAR = "archer"; exec(open(r"<repo>/Scripts/ue/retarget_axolotl.py").read()); RESULT

Set RUN_EXPORT = False before exec to only rebuild the rigs and retargeter.
Tuning is per character in TUNE; a global PELVIS_H / PELVIS_V / PELVIS_Z / ARM_ABDUCT / FINGER_CURL set before exec
overrides the table value.
"""
import copy
import unreal

if "CHAR" not in globals():
    CHAR = "warrior"
PASCAL = CHAR.capitalize()

SRC_FOLDER = "/Game/Characters/viking_male"
DST_FOLDER = "/Game/Characters/axolotl_" + CHAR
SRC_MESH = SRC_FOLDER + "/Viking_Male"
DST_MESH = DST_FOLDER + "/Axolotl_" + PASCAL
SRC_RIG = SRC_FOLDER + "/IK_Viking_Male"
DST_RIG = DST_FOLDER + "/IK_Axolotl_" + PASCAL
RTG = DST_FOLDER + ("/RTG_Viking_To_Axolotl" if CHAR == "warrior" else "/RTG_Viking_To_Axolotl_" + PASCAL)
SRC_PREFIX = "Viking_MaleCharacterArmature_"
DST_PREFIX = "Axolotl_" + PASCAL + "CharacterArmature_"
CLIPS = ["Idle", "Walk", "Run", "Walk_Carry", "Run_Carry", "SwordSlash",
         "Shoot_OneHanded", "Punch", "PickUp", "RecieveHit", "Death"]

# Chain name -> (start, end). The Viking foot is an IK-style bone parented to `Bone`, not to the
# lower leg, so it cannot end the leg chain: each foot gets its own single-bone chain.
SRC_CHAINS = [
    ("Spine", "Abdomen", "Torso"),
    ("Head", "Neck", "Head"),
    ("LeftArm", "Shoulder_L", "Fist_L"),
    ("RightArm", "Shoulder_R", "Fist_R"),
    ("LeftLeg", "UpperLeg_L", "LowerLeg_L"),
    ("RightLeg", "UpperLeg_R", "LowerLeg_R"),
    ("LeftFoot", "Foot_L", "Foot_L"),
    ("RightFoot", "Foot_R", "Foot_R"),
]
DST_CHAINS = [
    ("Spine", "Spine", "Spine2"),
    ("Head", "Neck", "Head"),
    ("LeftArm", "LeftShoulder", "LeftHand"),
    ("RightArm", "RightShoulder", "RightHand"),
    ("LeftLeg", "LeftUpLeg", "LeftLeg"),
    ("RightLeg", "RightUpLeg", "RightLeg"),
    # single bone like the source foot; the toe follows the foot locally
    ("LeftFoot", "LeftFoot", "LeftFoot"),
    ("RightFoot", "RightFoot", "RightFoot"),
]

if "RUN_EXPORT" not in globals():
    RUN_EXPORT = True

EAL = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
log = []


def _split(path):
    return path.rsplit("/", 1)


def _delete(path):
    """Delete an asset and make sure it is gone. In a fresh editor, deleting a package that was not loaded yet could
    log a force delete yet leave the .uasset on disk, and the following create_asset then returned None (S6 of
    axolotl-all-classes). Loading it first and checking afterwards avoids that."""
    for _ in range(2):
        if not EAL.does_asset_exist(path):
            return True
        unreal.load_asset(path)
        EAL.delete_asset(path)
        unreal.SystemLibrary.collect_garbage()
    if EAL.does_asset_exist(path):
        raise RuntimeError("could not delete " + path)
    return True


def make_rig(path, mesh_path, chains, reuse=False):
    """Build an IK Rig. With reuse=True an existing asset is loaded and returned as it is (the shared Viking source
    rig, referenced by every axolotl retargeter, must not be deleted)."""
    folder, name = _split(path)
    if EAL.does_asset_exist(path):
        if reuse:
            rig = unreal.load_asset(path)
            ctl = unreal.IKRigController.get_controller(rig)
            log.append("%s: reused, root %s, chains %s" % (name, ctl.get_retarget_root(),
                                                           [str(c.chain_name) for c in ctl.get_retarget_chains()]))
            return rig
        _delete(path)
    rig = tools.create_asset(name, folder, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    ctl = unreal.IKRigController.get_controller(rig)
    ctl.set_skeletal_mesh(unreal.load_asset(mesh_path))
    for i in reversed(range(ctl.get_num_solvers())):
        ctl.remove_solver(i)
    ctl.set_retarget_root("Hips")
    for cname, start, end in chains:
        got = ctl.add_retarget_chain(cname, start, end, "None")
        if str(got) != cname:
            raise RuntimeError("chain %s on %s came back as %s" % (cname, name, got))
    log.append("%s: root %s, chains %s" % (name, ctl.get_retarget_root(),
                                           [str(c.chain_name) for c in ctl.get_retarget_chains()]))
    return rig


# ---------------------------------------------------------------- per-character tuning
# S8 (warrior plan): static offsets on top of the auto-aligned target retarget pose (FK carries them into every clip).
# Offsets are local (post-multiplied onto the bone's existing offset); axes were found by measuring the hand / finger
# tip in the exported Idle. Mixamo arm bones point along local Y.
#  - ARM_ABDUCT: abduct the upper arms so the short arms clear the fat body and fur collar, and the axe / shield move out.
#  - FINGER_CURL: curl joints 1-3 about the finger-local Z axis (towards the palm) so the paw closes on the handle.
#  - PELVIS_H / PELVIS_V / PELVIS_Z: see the pelvis op below.
# A key without an entry starts from a copy of the warrior values, with PELVIS_Z scaled by the target rest hips height.
TUNE = {
    "warrior": {
        "PELVIS_H": 1.0,
        "PELVIS_V": 0.45,
        "PELVIS_Z": 14.5,
        "ARM_ABDUCT": {"RightArm": ((0.7071, 0.0, -0.7071), 20.0), "LeftArm": ((-0.7071, 0.0, -0.7071), 15.0)},
        "FINGER_CURL": {"Right": 35.0, "Left": 35.0},
    },
}
WARRIOR_HIPS_Z = 29.93  # cm, warrior rest hips height the warrior PELVIS_Z was tuned on
_dst_ref = unreal.load_asset(DST_MESH).skeleton.get_reference_pose()
dst_hips_z = unreal.AnimPoseExtensions.get_bone_pose(_dst_ref, "Hips", unreal.AnimPoseSpaces.WORLD).translation.z
if CHAR in TUNE:
    _tune = copy.deepcopy(TUNE[CHAR])
else:
    _tune = copy.deepcopy(TUNE["warrior"])
    _tune["PELVIS_Z"] = 14.5 * dst_hips_z / WARRIOR_HIPS_Z
if "PELVIS_H" not in globals():
    PELVIS_H = _tune["PELVIS_H"]  # extra multipliers on top of the height-ratio correction (tuning)
if "PELVIS_V" not in globals():
    PELVIS_V = _tune["PELVIS_V"]
if "PELVIS_Z" not in globals():
    PELVIS_Z = _tune["PELVIS_Z"]  # cm, static vertical pelvis offset (see below)
if "ARM_ABDUCT" not in globals():
    ARM_ABDUCT = _tune["ARM_ABDUCT"]
if "FINGER_CURL" not in globals():
    FINGER_CURL = _tune["FINGER_CURL"]
log.append("tuning %s (target hips %.2f cm): PELVIS_H %s PELVIS_V %s PELVIS_Z %.2f" % (
    CHAR, dst_hips_z, PELVIS_H, PELVIS_V, PELVIS_Z))

# ---------------------------------------------------------------- IK Rigs and retargeter
_delete(RTG)
src_rig = make_rig(SRC_RIG, SRC_MESH, SRC_CHAINS, reuse=True)
dst_rig = make_rig(DST_RIG, DST_MESH, DST_CHAINS)

folder, name = _split(RTG)
rtg = tools.create_asset(name, folder, unreal.IKRetargeter, unreal.IKRetargetFactory())
rc = unreal.IKRetargeterController.get_controller(rtg)
S, T = unreal.RetargetSourceOrTarget.SOURCE, unreal.RetargetSourceOrTarget.TARGET
rc.set_ik_rig(S, src_rig)
rc.set_ik_rig(T, dst_rig)
rc.set_preview_mesh(S, unreal.load_asset(SRC_MESH))
rc.set_preview_mesh(T, unreal.load_asset(DST_MESH))
if rc.get_num_retarget_ops() == 0:
    rc.add_default_ops()
rc.assign_ik_rig_to_all_ops(S, src_rig)
rc.assign_ik_rig_to_all_ops(T, dst_rig)

# FK only: drop IK-goal ops (no goals exist in either rig).
FK_ONLY_DROP = ("IKRetargetIKChainsOp", "IKRetargetRunIKRigOp", "IKRetargetSpeedPlantingOp",
                "IKRetargetStrideWarpingOp", "IKRetargetFloorConstraintOp", "IKRetargetScaleGoalsOp",
                "IKRetargetOffsetGoalsOp", "IKRetargetAlignPoleVectorOp", "IKRetargetWeaponGoalOp")
for i in reversed(range(rc.get_num_retarget_ops())):
    t = rc.get_op_type_by_name(rc.get_op_name(i))
    if any(t.endswith(d) for d in FK_ONLY_DROP):
        rc.remove_retarget_op(i)

rc.auto_map_chains(unreal.AutoMapChainType.EXACT, True)
log.append("chain map: %s" % {str(k): str(v) for k, v in rc.get_chain_map().items()})

# Target retarget pose: align the T-posed axolotl to the Viking's A-pose chain by chain.
rc.auto_align_all_bones(T, unreal.RetargetAutoAlignMethod.CHAIN_TO_CHAIN)
# Chain-to-chain cannot aim the last bone of a source chain (the Viking LowerLeg_L / Foot_L end their chains), so
# the shins stayed straight while the Viking rest shin is bent back ~38 deg: align shins, then feet, by global axes.
rc.auto_align_bones(["LeftLeg", "RightLeg"], unreal.RetargetAutoAlignMethod.GLOBAL_ROTATION_AXES, T)
rc.auto_align_bones(["LeftFoot", "RightFoot"], unreal.RetargetAutoAlignMethod.GLOBAL_ROTATION_AXES, T)


def _qaxis(axis, deg):
    import math
    s = math.sin(math.radians(deg) / 2)
    return unreal.Quat(axis[0] * s, axis[1] * s, axis[2] * s, math.cos(math.radians(deg) / 2))


def _add_offset(bone, q):
    pose_name = rc.get_current_retarget_pose_name(T)
    cur = {str(k): v for k, v in rc.get_retarget_pose_bone_rotation_offsets(pose_name, T).items()}
    base = cur.get(bone, unreal.Quat(0.0, 0.0, 0.0, 1.0))
    rc.set_rotation_offset_for_retarget_pose_bone(bone, base.multiply(q), T)


for bone, (axis, deg) in ARM_ABDUCT.items():
    _add_offset(bone, _qaxis(axis, deg))
for side, deg in FINGER_CURL.items():
    for finger in ("Index", "Middle", "Ring", "Pinky"):
        for j in (1, 2, 3):
            _add_offset("%sHand%s%d" % (side, finger, j), _qaxis((0.0, 0.0, 1.0), deg))
log.append("retarget pose: arm abduction %s, finger curl %s" % (ARM_ABDUCT, FINGER_CURL))

# Per-op settings.
_ref = unreal.load_asset(SRC_MESH).skeleton.get_reference_pose()
src_pelvis_h = unreal.AnimPoseExtensions.get_bone_pose(_ref, "Hips", unreal.AnimPoseSpaces.WORLD).translation.z
for i in range(rc.get_num_retarget_ops()):
    opname = rc.get_op_name(i)
    t = rc.get_op_type_by_name(opname)
    ctl = rc.get_op_controller(i)
    if t.endswith("IKRetargetPelvisMotionOp"):
        ctl.set_source_pelvis_bone("Hips")
        ctl.set_target_pelvis_bone("Hips")
        st = ctl.get_settings()
        # With the defaults the exported Hips came out at source_cm * target_pelvis_height (x30 too high; observed,
        # probably the Viking rig's x100 root scale confusing the op's source height). Dividing by the real source
        # pelvis height restores target = source * (target_height / source_height).
        # S8: the vertical pelvis motion is damped (PELVIS_V 0.45 instead of 0.9) and lifted by a static PELVIS_Z.
        # At 0.9 / 0 the soles dipped to -7 cm in Walk and -12 cm in Run (the Viking pelvis bob is too deep for the
        # short axolotl legs) and the fat body sank into the ground at the end of Death (hips 7 cm, feet -12 cm).
        # Measured sole heights at 0.45 / 14.5 (97 cm warrior figure): Idle ~0, Walk min -5, Run min -7.5, Death hips 18.
        st.scale_horizontal = PELVIS_H / src_pelvis_h
        st.scale_vertical = PELVIS_V / src_pelvis_h
        st.translation_offset_global = unreal.Vector(0.0, 0.0, PELVIS_Z)
        ctl.set_settings(st)
        log.append("pelvis: source height %.2f cm, scale_h %.5f scale_v %.5f offset_z %.1f" % (
            src_pelvis_h, st.scale_horizontal, st.scale_vertical, PELVIS_Z))
    elif t.endswith("IKRetargetFKChainsOp"):
        st = ctl.get_settings()
        chains = []  # array elements come back as copies: edit them and assign a new list
        for c in list(st.chains_to_retarget):
            chains.append(c)
            c.enable_fk = True
            # Interpolated samples the source chain by normalised length, but a Viking chain ends AT its last joint
            # (LowerLeg_L, Fist_L), so e.g. the axolotl knee (65% along its leg) got mostly thigh rotation and the
            # shin never bent. Limbs and head have matching joint counts: copy one to one. The spine (2 vs 3 bones)
            # stays interpolated.
            if str(c.target_chain_name) == "Spine":
                c.rotation_mode = unreal.FKChainRotationMode.INTERPOLATED
            else:
                c.rotation_mode = unreal.FKChainRotationMode.ONE_TO_ONE
            c.translation_mode = unreal.FKChainTranslationMode.NONE
            # The Viking foot is a free IK-style bone; copying its rotation pitched the axolotl toes 25-45 deg off
            # and into the ground in Walk. The foot follows the shin in its (aligned) retarget pose instead.
            if str(c.target_chain_name) in ("LeftFoot", "RightFoot"):
                c.enable_fk = False
        st.chains_to_retarget = chains
        ctl.set_settings(st)
        log.append("fk: %s" % [(str(c.target_chain_name), str(c.rotation_mode).split(".")[-1].split(":")[0])
                               for c in ctl.get_settings().chains_to_retarget])
    elif t.endswith("IKRetargetRootMotionOp"):
        # The axolotl has no root bone above Hips (its root IS the pelvis) and the Viking clips are
        # in place, so a root-motion pass would only fight the pelvis op.
        rc.set_retarget_op_enabled(i, False)
    log.append("op %d %s (%s) enabled=%s" % (i, opname, t.rsplit(".", 1)[-1], rc.get_retarget_op_enabled(i)))

EAL.save_loaded_asset(src_rig, True)  # shared source rig: save only if this run created or changed it
for a in (dst_rig, rtg):
    EAL.save_loaded_asset(a, False)

rep = rc.validate_retargeter(unreal.load_asset(SRC_MESH), unreal.load_asset(DST_MESH))
try:
    log.append("validate: %s" % rep.to_dict() if hasattr(rep, "to_dict") else str(rep))
except Exception as e:  # report object shape varies between versions
    log.append("validate: %s" % e)

# ---------------------------------------------------------------- batch export
if RUN_EXPORT:
    # The batch "overwrite" silently writes <name>1 when the old clip is still loaded somewhere, so old outputs
    # (and stray numbered copies from earlier runs) are deleted first and purged from memory.
    import re
    unreal.SystemLibrary.collect_garbage()
    pat = re.compile(r"^%s(%s)\d*$" % (re.escape(DST_PREFIX), "|".join(CLIPS)))
    for p in EAL.list_assets(DST_FOLDER, recursive=False):
        n = str(p).split(".")[0].rsplit("/", 1)[-1]
        if pat.match(n):
            _delete(str(p).split(".")[0])
    unreal.SystemLibrary.collect_garbage()
    src_assets = []
    for c in CLIPS:
        ad = unreal.EditorAssetLibrary.find_asset_data(SRC_FOLDER + "/" + SRC_PREFIX + c)
        if not ad.is_valid():
            raise RuntimeError("missing source clip " + c)
        src_assets.append(ad)
    inp = unreal.IKRetargetBatchOperationInputs()
    inp.assets_to_retarget = src_assets
    inp.ik_retarget_asset = rtg
    inp.source_mesh = unreal.load_asset(SRC_MESH)
    inp.target_mesh = unreal.load_asset(DST_MESH)
    inp.search = "Viking_Male"
    inp.replace = "Axolotl_" + PASCAL
    inp.prefix = ""
    inp.suffix = ""
    inp.target_path = DST_FOLDER
    inp.use_source_path = False
    inp.include_referenced_assets = False
    inp.overwrite_existing_files = True
    made = []
    todo = list(src_assets)
    for attempt in range(2):  # one retry for anything the batch did not retarget
        inp.assets_to_retarget = todo
        results = unreal.IKRetargetBatchOperation.run_batch_retarget_with_results(inp)
        todo = []
        for r in results:
            if r.status == unreal.IKRetargetBatchAssetStatus.RETARGETED:
                made.append(str(r.retargeted_asset.package_name))
            else:
                log.append("attempt %d: %s %s %s" % (attempt, r.source_asset.asset_name, r.status, r.reason))
                todo.append(r.source_asset)
        if not todo:
            break
    log.append("exported %d: %s" % (len(made), [m.rsplit("_", 1)[-1] for m in made]))

    dst_skel = unreal.load_asset(DST_MESH).skeleton
    for c in CLIPS:
        want = DST_FOLDER + "/" + DST_PREFIX + c
        if not EAL.does_asset_exist(want):
            # rename anything the batch produced for this clip to the exact name
            cand = [p for p in made if p.endswith(c) and p.startswith(DST_FOLDER)]
            if cand:
                EAL.rename_asset(cand[0], want)
        seq = unreal.load_asset(want)
        src = unreal.load_asset(SRC_FOLDER + "/" + SRC_PREFIX + c)
        if seq is None:
            log.append("%s MISSING" % want)
            continue
        ok = seq.get_editor_property("skeleton") == dst_skel
        log.append("%-16s src %.3fs dst %.3fs skel_ok=%s" % (c, src.get_play_length(), seq.get_play_length(), ok))
        EAL.save_loaded_asset(seq, False)

RESULT = "\n".join(log)
