# PIE helpers. exec(open(path).read()) inside execute_python, then call the functions.
import unreal, math, os, asyncio
# OUT_DIR: set it before exec (a scratch folder for stills / json); default is the system temp folder.
if "OUT_DIR" not in globals():
    import tempfile as _tf
    OUT_DIR = _tf.gettempdir().replace("\\", "/") + "/tripo_character"

OUT9 = OUT_DIR + "/pie"


def pie_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def units(world=None):
    world = world or pie_world()
    res = []
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        for c in a.get_components_by_class(unreal.SkeletalMeshComponent):
            m = c.get_skinned_asset()
            if m:
                res.append((a, c, m.get_name()))
                break
    return res


def find_hero(key, world=None):
    for a, c, n in units(world):
        if n == "Axolotl_" + key.capitalize():
            return a, c
    return None, None


def describe(a, c):
    out = []
    b = c.get_editor_property("bounds") if False else None
    o, ext = a.get_actor_bounds(False)
    an = c.get_animation_mode()
    seq = None
    try:
        inst = c.get_editor_property("animation_data").anim_to_play
        seq = inst.get_name() if inst else None
    except Exception:
        pass
    out.append("%s loc %s rot %s scale %s boundsZ %.1f..%.1f vis %s" % (
        a.get_name(), a.get_actor_location(), a.get_actor_rotation(), c.get_world_scale(), o.z - ext.z, o.z + ext.z,
        not a.is_hidden_ed()))
    for s in c.get_children_components(True):
        if isinstance(s, unreal.StaticMeshComponent):
            sock = s.get_attach_socket_name()
            sl = c.get_socket_location(sock)
            so, se, _ = unreal.SystemLibrary.get_component_bounds(s)
            out.append("   %s socket=%s vis=%s dist=%.1f scale %s" % (
                s.static_mesh.get_name() if s.static_mesh else None, sock, s.is_visible(),
                (so - sl).length(), s.get_world_scale()))
    return "\n".join(out)


def ground_z(world, loc):
    hit = unreal.SystemLibrary.line_trace_single(world, loc + unreal.Vector(0, 0, 300), loc - unreal.Vector(0, 0, 600),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
    if hit:
        t = hit.to_tuple()
        return t[4].z  # impact point... fallback below
    return None


async def capture(key_or_actor, name, dist=260.0, yaw_off=30.0, height=60.0, fov=40.0, size=768, focus_z=55.0, world=None):
    world = world or pie_world()
    if isinstance(key_or_actor, str):
        a, c = find_hero(key_or_actor, world)
    else:
        a = key_or_actor
    loc = a.get_actor_location()
    fwd = a.get_actor_forward_vector()
    yaw = math.atan2(fwd.y, fwd.x) + math.radians(yaw_off)
    focus = loc + unreal.Vector(0, 0, focus_z)
    o, ext = a.get_actor_bounds(False)
    focus = unreal.Vector(o.x, o.y, o.z)
    camloc = focus + unreal.Vector(math.cos(yaw) * dist, math.sin(yaw) * dist, height)
    rot = unreal.MathLibrary.find_look_at_rotation(camloc, focus)
    return await shoot(world, camloc, rot, name, fov, size)


def seg_box(p0, p1, o, e):
    # slab test, segment p0->p1 vs AABB centre o extent e
    t0, t1 = 0.0, 1.0
    for ax in ("x", "y", "z"):
        a0, d = getattr(p0, ax), getattr(p1, ax) - getattr(p0, ax)
        lo, hi = getattr(o, ax) - getattr(e, ax), getattr(o, ax) + getattr(e, ax)
        if abs(d) < 1e-6:
            if a0 < lo or a0 > hi:
                return False
        else:
            ta, tb = (lo - a0) / d, (hi - a0) / d
            if ta > tb:
                ta, tb = tb, ta
            t0, t1 = max(t0, ta), min(t1, tb)
            if t0 > t1:
                return False
    return True


_PRIMS = None


def prims(world, hero):
    res = []
    for act in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        if act == hero:
            continue
        for pc in act.get_components_by_class(unreal.PrimitiveComponent):
            if not pc.is_visible():
                continue
            if isinstance(pc, unreal.InstancedStaticMeshComponent):
                res.append(("ism", pc))
                continue
            o, e, _ = unreal.SystemLibrary.get_component_bounds(pc)
            if max(e.x, e.y, e.z) > 1500:
                continue
            res.append((o, e, pc))
    return res


def _seg_dist2d(p, a, b):
    dx, dy = b.x - a.x, b.y - a.y
    L = dx * dx + dy * dy
    t = 0.0 if L < 1e-6 else max(0.0, min(1.0, ((p.x - a.x) * dx + (p.y - a.y) * dy) / L))
    return math.hypot(a.x + dx * t - p.x, a.y + dy * t - p.y), t


def ism_hits(plist, frm, to):
    n = 0
    for item in plist:
        if item[0] != "ism":
            continue
        pc = item[1]
        o, e, _ = unreal.SystemLibrary.get_component_bounds(pc)
        if not seg_box(frm, to, o, e):
            continue
        sm = pc.static_mesh
        if sm is None:
            continue
        be = sm.get_bounds().box_extent
        if be.z * 2 < 60:  # grass / ferns
            continue
        rad = max(be.x, be.y)
        for i in range(pc.get_instance_count()):
            t = pc.get_instance_transform(i, True)
            s = t.scale3d
            d, _ = _seg_dist2d(t.translation, frm, to)
            if d < rad * max(s.x, s.y) * 0.6 + 10:
                n += 1
    return n


def blockers(plist, frm, to):
    out = []
    for item in plist:
        if item[0] == "ism":
            continue
        o, e, pc = item
        hit = False
        for dz in (0, 35, -35):
            if seg_box(frm, to + unreal.Vector(0, 0, dz), o, e):
                hit = True
                break
        if hit:
            out.append(pc)
    return out


def clear_line(world, a, frm, to):
    pts = [to, to + unreal.Vector(0, 0, 40), to - unreal.Vector(0, 0, 40)]
    for p in pts:
        hit = unreal.SystemLibrary.line_trace_single(world, frm, p, unreal.TraceTypeQuery.TRACE_TYPE_QUERY2, True, [a],
                                                     unreal.DrawDebugTrace.NONE, True)
        if hit is not None and (not isinstance(hit, tuple) or hit[0]):
            return False
    return True


async def best(key, name, dist=200.0, height=30.0, fov=40.0, prefer=(-30, 30, -60, 60, 0, -90, 90, 150, -150, 180)):
    world = pie_world()
    a, c = find_hero(key, world) if isinstance(key, str) else (key, None)
    o, ext = a.get_actor_bounds(False)
    fwd = a.get_actor_forward_vector()
    base = math.atan2(fwd.y, fwd.x)
    plist = prims(world, a)
    cands = []
    for rank, off in enumerate(prefer):
        yaw = base + math.radians(off)
        camloc = o + unreal.Vector(math.cos(yaw) * dist, math.sin(yaw) * dist, height)
        n = ism_hits(plist, camloc, o)
        bl = blockers(plist, camloc, o)
        cands.append((n, len(bl), rank, off, camloc, bl))
        if n == 0 and not bl:
            break
    cands.sort(key=lambda x: (x[0], x[1], x[2]))
    n, nb, _, off, camloc, bl = cands[0]
    for pc in bl:
        pc.set_visibility(False, False)
    try:
        await asyncio.sleep(0.05)
        p = await shoot(world, camloc, unreal.MathLibrary.find_look_at_rotation(camloc, o), name, fov)
    finally:
        for pc in bl:
            pc.set_visibility(True, False)
    return "%s off %d trees %d hidden %s" % (p, off, n, [pc.get_owner().get_name() + "." + pc.get_name() for pc in bl][:6])


async def shoot(world, camloc, rot, name, fov=40.0, size=768, mode="Lit"):
    import base64
    res = await call_tool("SceneCaptureToolset.SceneCaptureToolset.Capture", world={"refPath": world.get_path_name()},
                          camera={"location": {"x": camloc.x, "y": camloc.y, "z": camloc.z},
                                  "rotation": {"pitch": rot.pitch, "yaw": rot.yaw, "roll": rot.roll},
                                  "projectionMode": "Perspective", "fOV": fov},
                          mode=mode, options={"width": size, "height": size, "screenPercentage": 1.0, "visualizationSubMode": ""})
    img = res["image"]
    os.makedirs(OUT9, exist_ok=True)
    p = os.path.join(OUT9, name + ".png")
    if isinstance(img, dict):
        img = img.get("data") or img.get("Data")
    if isinstance(img, str) and img.startswith("data:"):
        img = img.split(",", 1)[1]
    with open(p, "wb") as f:
        f.write(base64.b64decode(img))
    return p


async def pair(key, name, maxd=500.0, dist=330.0, height=45.0, fov=45.0, prefer=(-30, 30, -60, 60, 0, -90, 90, 150, -150, 180)):
    """Hero with the nearest worker (if within maxd) in frame; picks the least blocked angle."""
    world = pie_world()
    a, c = find_hero(key, world)
    o, _ = a.get_actor_bounds(False)
    best_w, bd = None, 1e9
    for u, uc, n in units(world):
        if n == "Axolotl_Warrior":
            d = (u.get_actor_location() - a.get_actor_location()).length()
            if d < bd:
                best_w, bd = u, d
    focus = o
    extra = ""
    if best_w is not None and bd < maxd:
        wo, _ = best_w.get_actor_bounds(False)
        focus = (o + wo) * 0.5
        dist = dist + bd * 0.6
        _, wc = best_w, best_w.get_components_by_class(unreal.SkeletalMeshComponent)[0]
        wb = wc.get_skinned_asset().get_imported_bounds() if hasattr(wc.get_skinned_asset(), "get_imported_bounds") else None
        extra = " worker %.0f cm away, worker scale %.3f" % (bd, wc.get_world_scale().x)
    fwd = a.get_actor_forward_vector()
    base = math.atan2(fwd.y, fwd.x)
    plist = prims(world, a)
    cands = []
    for rank, off in enumerate(prefer):
        yaw = base + math.radians(off)
        camloc = focus + unreal.Vector(math.cos(yaw) * dist, math.sin(yaw) * dist, height)
        n = ism_hits(plist, camloc, focus)
        bl = blockers(plist, camloc, focus)
        cands.append((n, len(bl), rank, off, camloc, bl))
        if n == 0 and not bl:
            break
    cands.sort(key=lambda x: (x[0], x[1], x[2]))
    n, nb, _, off, camloc, bl = cands[0]
    for pc in bl:
        pc.set_visibility(False, False)
    try:
        p = await shoot(world, camloc, unreal.MathLibrary.find_look_at_rotation(camloc, focus), name, fov)
    finally:
        for pc in bl:
            pc.set_visibility(True, False)
    return "%s off %d trees %d hidden %d%s" % (p, off, n, len(bl), extra)
