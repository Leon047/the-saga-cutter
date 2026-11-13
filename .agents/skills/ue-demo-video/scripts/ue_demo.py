"""Helpers for recording a demo video of a level with Movie Render Queue.

Runs INSIDE the Unreal Editor (unreal-mcp execute_python). Load it with top-level statements only:

    import sys, unreal
    sys.path.insert(0, unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()) + '../.agents/skills/ue-demo-video/scripts')
    import importlib, ue_demo
    importlib.reload(ue_demo)

All APIs below were verified on UE 5.9 (TheSagaCutter, 2026-11-12).
"""
import glob
import math
import os

import unreal

DR = unreal.MovieSceneTimeUnit.DISPLAY_RATE


# ----------------------------------------------------------------------------------------------------------------------
# camera math
# ----------------------------------------------------------------------------------------------------------------------

def look_at(cam, target):
    """(pitch, yaw) in degrees for a camera at `cam` looking at `target` (both (x, y, z))."""
    d = [target[i] - cam[i] for i in range(3)]
    return math.degrees(math.atan2(d[2], math.hypot(d[0], d[1]))), math.degrees(math.atan2(d[1], d[0]))


def _unwrap_yaw(channel):
    """Make consecutive yaw keys differ by at most 180 deg, so the camera never spins the long way round."""
    keys = sorted(channel.get_keys(), key=lambda k: k.get_time(DR).frame_number.value)
    prev, out = None, []
    for k in keys:
        v = k.get_value()
        if prev is not None:
            while v - prev > 180:
                v -= 360
            while v - prev < -180:
                v += 360
        k.set_value(v)
        prev = v
        out.append(round(v))
    return out


def _transform_channels(seq):
    cam = seq.get_spawnables()[0]
    tr = cam.find_tracks_by_exact_type(unreal.MovieScene3DTransformTrack)[0]
    return tr.get_sections()[0].get_all_channels()  # [locX, locY, locZ, roll, pitch, yaw, scaleX, scaleY, scaleZ]


# ----------------------------------------------------------------------------------------------------------------------
# sequence
# ----------------------------------------------------------------------------------------------------------------------

def build_flythrough(asset_path, shots, fps=30, focal=20.0, sub_sequences=(), linear_after=()):
    """Create (or replace) a LevelSequence with one spawnable CineCamera flying through `shots`.

    shots: list of (frame, cam_xyz, target_xyz), sorted by frame; the last frame is the sequence length.
    sub_sequences: LevelSequence asset paths to play underneath (e.g. a wind-gust sequence), looped to fill the length.
    linear_after: frames whose outgoing location segment is linear (use to keep the camera from bulging into a wall).
    Returns the yaw keys after unwrapping.
    """
    folder, name = asset_path.rsplit('/', 1)
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.EditorAssetLibrary.delete_asset(asset_path)
    seq = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, folder, unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
    length = shots[-1][0]
    seq.set_display_rate(unreal.FrameRate(fps, 1))
    seq.set_playback_start(0)
    seq.set_playback_end(length)

    cam = seq.add_spawnable_from_class(unreal.CineCameraActor)
    cam.set_display_name('FlyCam')
    sec = cam.add_track(unreal.MovieScene3DTransformTrack).add_section()
    sec.set_range(0, length)
    ch = sec.get_all_channels()
    for f, c, t in shots:
        p, y = look_at(c, t)
        for i, v in enumerate([c[0], c[1], c[2], 0.0, p, y]):
            k = ch[i].add_key(unreal.FrameNumber(f), v, 0.0, DR)
            mode = unreal.RichCurveInterpMode.RCIM_LINEAR if (f in linear_after and i < 3) else unreal.RichCurveInterpMode.RCIM_CUBIC
            k.set_interpolation_mode(mode)
    yaws = _unwrap_yaw(ch[5])

    # lens: wide, no autofocus (the default focus blurs everything at MRQ resolutions)
    cc = cam.get_object_template().get_editor_property('camera_component')
    fs = cc.get_editor_property('focus_settings')
    fs.set_editor_property('focus_method', unreal.CameraFocusMethod.DISABLE)
    cc.set_editor_property('focus_settings', fs)
    cc.set_editor_property('current_focal_length', float(focal))
    cc.set_editor_property('current_aperture', 16.0)

    cut = seq.add_track(unreal.MovieSceneCameraCutTrack).add_section()
    cut.set_range(0, length)
    cut.set_camera_binding_id(unreal.MovieSceneSequenceExtensions.get_portable_binding_id(seq, seq, cam))

    if sub_sequences:
        sub = seq.add_track(unreal.MovieSceneSubTrack)
        for path in sub_sequences:
            ss = unreal.load_asset(path)
            n = max(1, ss.get_playback_end() - ss.get_playback_start())
            start = 0
            while start < length:
                s = sub.add_section()
                s.set_sequence(ss)
                s.set_range(start, min(start + n, length))
                start += n

    unreal.EditorAssetLibrary.save_loaded_asset(seq)
    return yaws


def set_shot(asset_path, frame, cam_xyz=None, target_xyz=None, add=False, linear=None):
    """Move (or with add=True, insert) the camera key at `frame`, re-unwrap yaw, save. Returns yaw keys."""
    seq = unreal.load_asset(asset_path)
    ch = _transform_channels(seq)
    p, y = look_at(cam_xyz, target_xyz)
    vals = [cam_xyz[0], cam_xyz[1], cam_xyz[2], 0.0, p, y]
    for i, v in enumerate(vals):
        if add:
            k = ch[i].add_key(unreal.FrameNumber(frame), v, 0.0, DR)
            k.set_interpolation_mode(unreal.RichCurveInterpMode.RCIM_CUBIC)
        else:
            for k in ch[i].get_keys():
                if k.get_time(DR).frame_number.value == frame:
                    k.set_value(v)
        if linear is not None and i < 3:
            for k in ch[i].get_keys():
                if k.get_time(DR).frame_number.value == frame:
                    k.set_interpolation_mode(unreal.RichCurveInterpMode.RCIM_LINEAR if linear else unreal.RichCurveInterpMode.RCIM_CUBIC)
    yaws = _unwrap_yaw(ch[5])
    unreal.EditorAssetLibrary.save_loaded_asset(seq)
    return yaws


# ----------------------------------------------------------------------------------------------------------------------
# render
# ----------------------------------------------------------------------------------------------------------------------

def output_dir(job_name):
    return unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()) + 'MovieRenders/' + job_name


def render(seq_path, map_path, job_name, game_mode=None, res=(1280, 720), warmup=90):
    """Queue one MRQ job (PNG sequence) and start it in a PIE executor. Returns immediately; the render runs on.

    game_mode: class path such as '/Script/TheSagaCutter.TSCGameMode'. REQUIRED when the level content is spawned at
    runtime by a game mode: MRQ otherwise runs MoviePipelineGameMode and you get black frames (no world, no lights).
    Wait for completion outside the editor: frame count in output_dir == length, plus the log line
    'MoviePipelineLinearExecutorBase finished'.
    """
    out = output_dir(job_name)
    for f in glob.glob(out + '/*.png'):
        os.remove(f)
    seq_obj = seq_path if '.' in seq_path.rsplit('/', 1)[1] else seq_path + '.' + seq_path.rsplit('/', 1)[1]
    map_obj = map_path if '.' in map_path.rsplit('/', 1)[1] else map_path + '.' + map_path.rsplit('/', 1)[1]

    qs = unreal.get_editor_subsystem(unreal.MoviePipelineQueueSubsystem)
    q = qs.get_queue()
    for j in list(q.get_jobs()):
        q.delete_job(j)
    job = q.allocate_new_job(unreal.MoviePipelineExecutorJob)
    job.job_name = job_name
    job.sequence = unreal.SoftObjectPath(seq_obj)
    job.map = unreal.SoftObjectPath(map_obj)
    cfg = job.get_configuration()

    o = cfg.find_or_add_setting_by_class(unreal.MoviePipelineOutputSetting)
    o.output_resolution = unreal.IntPoint(*res)
    o.output_directory = unreal.DirectoryPath(out)
    o.file_name_format = 'frame.{frame_number}'
    o.zero_pad_frame_numbers = 4
    cfg.find_or_add_setting_by_class(unreal.MoviePipelineDeferredPassBase)
    cfg.find_or_add_setting_by_class(unreal.MoviePipelineImageSequenceOutput_PNG)
    aa = cfg.find_or_add_setting_by_class(unreal.MoviePipelineAntiAliasingSetting)
    aa.engine_warm_up_count = warmup  # lets BeginPlay build the world before frame 0
    aa.render_warm_up_count = 32
    aa.spatial_sample_count = 1
    aa.temporal_sample_count = 1
    go = cfg.find_or_add_setting_by_class(unreal.MoviePipelineGameOverrideSetting)
    go.cinematic_quality_settings = True
    if game_mode:
        # 'game_mode_override' is deprecated and silently ignored; the soft property needs a loaded UClass
        go.set_editor_property('soft_game_mode_override', unreal.load_class(None, game_mode))

    qs.render_queue_with_executor_instance(unreal.MoviePipelinePIEExecutor())
    return out
