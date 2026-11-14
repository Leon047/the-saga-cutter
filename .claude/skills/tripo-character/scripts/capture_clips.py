# Contact-sheet capture: axolotl (retargeted) next to the Viking source, far above the map.
# PoseableMeshComponents are posed from AnimPose evaluation (the offscreen editor does not tick anims).
# Usage in execute_python:  exec(open(path).read()); await run(CLIPS, frames)
import unreal, asyncio, os, math
# OUT_DIR: set it before exec (a scratch folder for stills / json); default is the system temp folder.
if "OUT_DIR" not in globals():
    import tempfile as _tf
    OUT_DIR = _tf.gettempdir().replace("\\", "/") + "/tripo_character"

OUT = OUT_DIR + "/cap"
BASE = unreal.Vector(0, 0, 60000)
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
SDS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SDL = unreal.SubobjectDataBlueprintFunctionLibrary
AX_MESH = "/Game/Characters/axolotl_%s/Axolotl_%s" % (CHAR, CHAR.capitalize())
AX_H = unreal.load_asset(AX_MESH).get_bounds().box_extent.z * 2
VK_MESH = "/Game/Characters/viking_male/Viking_Male"
LUX = 4.0


def add_comp(actor, cls, parent_index=0):
    h = SDS.k2_gather_subobject_data_for_instance(actor)
    nh, fail = SDS.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=h[parent_index], new_class=cls, blueprint_context=None))
    return SDL.get_object(SDL.get_data(nh))


def spawn_poseable(mesh, loc, scale, label):
    a = EAS.spawn_actor_from_class(unreal.SkeletalMeshActor, loc, unreal.Rotator(0, 0, 0))
    c = a.skeletal_mesh_component
    c.set_skinned_asset_and_update(unreal.load_asset(mesh), True)
    c.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    a.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    a.set_actor_label(label)
    return a, c


def apply_pose(comp, seq, t):
    # The offscreen editor does not tick animation; re-setting the mesh re-runs InitAnim, which evaluates
    # the single-node animation at the saved position and rebuilds the render state.
    m = comp.get_skinned_asset()
    ad = unreal.SingleAnimationPlayData()
    ad.anim_to_play = seq
    ad.saved_position = t
    ad.saved_play_rate = 0.0
    ad.saved_looping = False
    ad.saved_playing = False
    comp.set_editor_property("animation_data", ad)
    comp.set_skinned_asset_and_update(None, True)
    comp.set_skinned_asset_and_update(m, True)


async def run(clips, nframes=4, cam_yaw_deg=35.0, size=512, viking=True, extra=None, dist=330.0, tag="", times=None):
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    os.makedirs(OUT, exist_ok=True)
    temps = []
    try:
        ax, axc = spawn_poseable(AX_MESH, BASE, 120.0 / AX_H, "TMP_CAP_AX")
        temps.append(ax)
        vk = vkc = None
        if viking:
            vk, vkc = spawn_poseable(VK_MESH, BASE + unreal.Vector(-100, 0, 0), 120.0 / 275.0, "TMP_CAP_VK")
            temps.append(vk)
        floor = EAS.spawn_actor_from_class(unreal.StaticMeshActor, BASE + unreal.Vector(-50, 0, 0), unreal.Rotator(0, 0, 0))
        floor.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane"))
        floor.set_actor_scale3d(unreal.Vector(4, 4, 1))
        temps.append(floor)
        sun = EAS.spawn_actor_from_class(unreal.DirectionalLight, BASE + unreal.Vector(0, 0, 500), unreal.Rotator(0, -40, -120))
        sun.light_component.set_intensity(LUX)
        temps.append(sun)
        if extra:
            temps += extra(ax, axc, vk, vkc) or []
        yaw = math.radians(90.0 - cam_yaw_deg)  # figures face +Y
        focus = BASE + unreal.Vector(-50 if viking else 0, 0, 55)
        camloc = focus + unreal.Vector(math.cos(yaw) * dist, math.sin(yaw) * dist, 45)
        rot = unreal.MathLibrary.find_look_at_rotation(camloc, focus)
        cap = EAS.spawn_actor_from_class(unreal.SceneCapture2D, camloc, rot)
        temps.append(cap)
        cc = cap.capture_component2d
        rt = unreal.RenderingLibrary.create_render_target2d(world, size, size, unreal.TextureRenderTargetFormat.RTF_RGBA8)
        cc.texture_target = rt
        cc.capture_source = unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
        cc.capture_every_frame = False
        cc.fov_angle = 40.0
        pp = cc.post_process_settings
        pp.override_auto_exposure_method = True
        pp.auto_exposure_method = unreal.AutoExposureMethod.AEM_MANUAL
        pp.override_auto_exposure_apply_physical_camera_exposure = True
        pp.auto_exposure_apply_physical_camera_exposure = False
        pp.override_auto_exposure_bias = True
        pp.auto_exposure_bias = 0.0
        cc.post_process_settings = pp
        cc.post_process_blend_weight = 1.0
        files = []
        for clip in clips:
            sa = unreal.load_asset("/Game/Characters/axolotl_%s/Axolotl_%sCharacterArmature_" % (CHAR, CHAR.capitalize()) + clip)
            sv = unreal.load_asset("/Game/Characters/viking_male/Viking_MaleCharacterArmature_" + clip)
            L = sa.get_play_length()
            loop = clip in ("Idle", "Walk", "Run", "Walk_Carry", "Run_Carry")
            ts = times or [L * i / (nframes if loop else max(1, nframes - 1)) for i in range(nframes)]
            for i, t in enumerate(ts):
                t = min(t, L - 0.001)
                apply_pose(axc, sa, t)
                if vkc:
                    apply_pose(vkc, sv, t)
                await asyncio.sleep(0.1)
                cc.capture_scene()
                await asyncio.sleep(0.05)
                name = "%s%s_%d" % (tag, clip, i)
                unreal.RenderingLibrary.export_render_target(world, rt, OUT, name)
                files.append(name)
        return files
    finally:
        for a in temps:
            EAS.destroy_actor(a)
