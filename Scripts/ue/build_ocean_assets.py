# The Saga Cutter - game jam project.
# Builds or updates the stylized ocean assets. Run inside the editor:  exec(open(r"<repo>/Scripts/ue/build_ocean_assets.py").read())
#   /Game/Vikings/Environment/Ocean/T_OceanPatterns  <- Docs/Art/Ocean/T_OceanPatterns.png (linear, masks)
#   /Game/Vikings/Environment/Ocean/M_SagaOcean      masked Default Lit, waves = the table in TSCOcean.cpp
# Parameters marked * are overridden by ATSCOcean every tick / at build; the rest are look defaults.
import math
import os
import unreal

FOLDER = "/Game/Vikings/Environment/Ocean"
TEX_PATH = FOLDER + "/T_OceanPatterns"
MAT_NAME = "M_SagaOcean"
MAT_PATH = FOLDER + "/" + MAT_NAME
REPO = os.path.abspath(os.path.join(unreal.Paths.project_dir(), ".."))
PNG = os.path.join(REPO, "Docs", "Art", "Ocean", "T_OceanPatterns.png")

# --- Look defaults (tunable) ---
NORMAL_BOOST = 2.0          # S8: 3.0 -> 2.0 (night moon highlights blew out)
FOAM_SCALE = 2000.0         # S8: 1400 -> 2000 (bigger, calmer foam cells)
ROUGH_WATER = 0.55          # S8: 0.22 -> 0.55 (broad soft highlights instead of white patches)
ROUGH_FOAM = 0.95           # S8: 0.9 -> 0.95
SPECULAR = 0.25             # S8: 0.5 -> 0.25
COL_DEEP = (0.010, 0.045, 0.085)
COL_SHALLOW = (0.035, 0.20, 0.24)   # S8: (0.04, 0.26, 0.30) -> less cyan glare at night
COL_FOAM = (0.80, 0.90, 0.92)
COL_ASH_DEEP = (0.030, 0.030, 0.034)
COL_ASH_FOAM = (0.30, 0.28, 0.26)


def wave(dx, dy, length, amp):
    """Same as MakeWave in TSCOcean.cpp: (Dir.X, Dir.Y, K, A) and W."""
    n = math.hypot(dx, dy)
    k = 2.0 * math.pi / length
    return (dx / n, dy / n, k, amp), 0.6 * math.sqrt(980.0 * k)


WAVES = [wave(-1.0, 0.25, 1900.0, 9.0), wave(-0.8, -0.6, 1100.0, 5.0), wave(-0.3, 1.0, 620.0, 2.5), wave(-1.0, -0.1, 330.0, 1.2)]

# Fractal shoreline, same as GShore / ShoreOffset in TSCOcean.cpp: (A, F, P) per octave.
SHORE_PHASES = (1.31, 2.85, 0.07, 4.12, 5.00)
SHORE = [(78.0 * 0.58 ** i, 2.0 * math.pi / 2900.0 * 2.17 ** i, SHORE_PHASES[i]) for i in range(5)]
SHORE_BIAS, SHORE_MIN, SHORE_MAX = 60.0, -60.0, 180.0
# Lapping bands, same as LapRate / LapLength in TSCOcean.cpp (the surf volume swells in time with them).
LAP_RATE, LAP_LENGTH = 0.11, 460.0

# Reuse the pattern texture on material-only updates.
tex = unreal.EditorAssetLibrary.load_asset(TEX_PATH) if unreal.EditorAssetLibrary.does_asset_exist(TEX_PATH) else None
if not tex:
    task = unreal.AssetImportTask()
    task.filename = PNG
    task.destination_path = FOLDER
    task.destination_name = "T_OceanPatterns"
    task.automated = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    tex = unreal.EditorAssetLibrary.load_asset(TEX_PATH)
    if not tex:
        raise RuntimeError("[ocean] texture import failed: " + PNG)
    tex.set_editor_property("srgb", False)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    unreal.EditorAssetLibrary.save_asset(TEX_PATH, only_if_is_dirty=False)

mat = unreal.EditorAssetLibrary.load_asset(MAT_PATH) if unreal.EditorAssetLibrary.does_asset_exist(MAT_PATH) else None
if mat:
    unreal.MaterialEditingLibrary.delete_all_material_expressions(mat)
else:
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(MAT_NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
mat.set_editor_property("tangent_space_normal", False)
mat.set_editor_property("two_sided", False)

MEL = unreal.MaterialEditingLibrary
E = unreal  # expression classes live on the unreal module
_pos = [0]


def add(cls, x=None, y=None):
    """Creates an expression; without a position it is placed in a running column (layout is done at the end)."""
    if x is None:
        _pos[0] += 1
        x, y = -400 * (1 + _pos[0] // 40), 120 * (_pos[0] % 40)
    return MEL.create_material_expression(mat, getattr(E, "MaterialExpression" + cls), x, y)


def pin(v):
    """Accepts an expression, (expression, output name) or a float (becomes a Constant)."""
    if isinstance(v, tuple):
        return v
    if isinstance(v, (int, float)):
        c = add("Constant")
        c.set_editor_property("r", float(v))
        return (c, "")
    return (v, "")


def link(a, b, inp):
    node, out = pin(a)
    if not MEL.connect_material_expressions(node, out, b, inp):
        raise RuntimeError("[ocean] link failed: %s.%s -> %s.%s (inputs %s)" % (
            node.get_class().get_name(), out, b.get_class().get_name(), inp, MEL.get_material_expression_input_names(b)))


def scalar(name, default):
    p = add("ScalarParameter")
    p.set_editor_property("parameter_name", name)
    p.set_editor_property("default_value", float(default))
    return p


def vector(name, default):
    p = add("VectorParameter")
    p.set_editor_property("parameter_name", name)
    d = list(default) + [0.0] * (4 - len(default))
    if len(default) == 3:
        d[3] = 1.0
    p.set_editor_property("default_value", unreal.LinearColor(*d))
    return p


def mask(v, chans):
    # A vector parameter's default output is RGB only, so read its channel outputs (R, G, B, A) directly.
    if isinstance(v, unreal.MaterialExpressionVectorParameter):
        outs = [(v, c.upper()) for c in chans]
        res = outs[0]
        for o in outs[1:]:
            res = append(res, o)
        return res
    m = add("ComponentMask")
    for c in "rgba":
        m.set_editor_property(c, c in chans)
    link(v, m, "")
    return m


def binop(cls, a, b):
    n = add(cls)
    link(a, n, "A")
    link(b, n, "B")
    return n


def mul(a, b): return binop("Multiply", a, b)
def plus(a, b): return binop("Add", a, b)
def sub(a, b): return binop("Subtract", a, b)
def div(a, b): return binop("Divide", a, b)
def dot(a, b): return binop("DotProduct", a, b)
def append(a, b): return binop("AppendVector", a, b)
def maxn(a, b): return binop("Max", a, b)


def unary(cls, a, **props):
    n = add(cls)
    for k, v in props.items():
        n.set_editor_property(k, v)
    link(a, n, "")
    return n


def sat(a): return unary("Saturate", a)
def frac(a): return unary("Frac", a)
def absn(a): return unary("Abs", a)
def sin(a): return unary("Sine", a, period=2.0 * math.pi)   # sin in radians, matches C++
def cos(a): return unary("Cosine", a, period=2.0 * math.pi)


def lerp(a, b, t):
    n = add("LinearInterpolate")
    link(a, n, "A")
    link(b, n, "B")
    link(t, n, "Alpha")
    return n


def smoothstep(lo, hi, v):
    n = add("SmoothStep")
    link(lo, n, "Min")
    link(hi, n, "Max")
    link(v, n, "Value")
    return n


def step(edge, v):
    """step(edge, v) = v >= edge ? 1 : 0 (UE Step node: Y = edge, X = value)."""
    n = add("Step")
    link(edge, n, "Y")
    link(v, n, "X")
    return n


def power(base, exponent):
    n = add("Power")
    link(base, n, "Base")
    link(exponent, n, "Exponent")
    return n


def const2(x, y):
    c = add("Constant2Vector")
    c.set_editor_property("r", float(x))
    c.set_editor_property("g", float(y))
    return c


def tex_sample(uv):
    t = add("TextureSample")
    t.set_editor_property("texture", tex)
    t.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    link(uv, t, "UVs")
    return t


# Parameters
p_time = scalar("OceanTime", 0.0)
p_shore = scalar("ShoreX", 2200.0)
scalar("SurfaceZ", 0.0)  # set by C++; the mesh is already built at SurfaceZ, kept for completeness
p_ash = scalar("Ash", 0.0)
p_nboost = scalar("NormalBoost", NORMAL_BOOST)
p_fscale = scalar("FoamScale", FOAM_SCALE)
p_waves = [vector("Wave%d" % i, WAVES[i][0]) for i in range(4)]
p_speed = vector("WaveSpeed", tuple(w[1] for w in WAVES))
p_glint = vector("SunGlint", (1.0, 1.0, 1.0))
p_foam = [vector("Foam0", (0.0, 0.0, 0.0, 0.0)), vector("Foam1", (0.0, 0.0, 0.0, 0.0))]
p_deep = vector("ColDeep", COL_DEEP)
p_shallow = vector("ColShallow", COL_SHALLOW)
p_cfoam = vector("ColFoam", COL_FOAM)
p_adeep = vector("ColAshDeep", COL_ASH_DEEP)
p_afoam = vector("ColAshFoam", COL_ASH_FOAM)
p_shore_oct = [vector("Shore%d" % i, (SHORE[i][0], SHORE[i][1], SHORE[i][2], 0.0)) for i in range(5)]
p_sbias = scalar("ShoreBias", SHORE_BIAS)
p_smin = scalar("ShoreMin", SHORE_MIN)
p_smax = scalar("ShoreMax", SHORE_MAX)
p_lap_rate = scalar("LapRate", LAP_RATE)
p_lap_len = scalar("LapLength", LAP_LENGTH)

wp = add("WorldPosition")
wp.set_editor_property("world_position_shader_offset", unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
P = mask(wp, "rg")
Px = mask(wp, "r")
Py = mask(wp, "g")
vertex = add("VertexColor")
vcol_r = mask(vertex, "r")
river_opening = mask(vertex, "g")

# 1. Waves: H = sum A sin(ph), G = sum A K cos(ph) Dir
H = None
G = None
for i in range(4):
    w = p_waves[i]
    dirv = mask(w, "rg")
    k = mask(w, "b")
    a = mask(w, "a")
    spd = mask(p_speed, "rgba"[i])
    ph = sub(mul(dot(P, dirv), k), mul(p_time, spd))
    h = mul(a, sin(ph))
    g = mul(mul(mul(a, k), cos(ph)), dirv)
    H = h if H is None else plus(H, h)
    G = g if G is None else plus(G, g)

# Fractal waterline offset Sy = soft clamp of Bias + sum A sin(F y + P) to [ShoreMin, ShoreMax] (ShoreOffset in C++)
s_raw = p_sbias
for o in p_shore_oct:
    s_raw = plus(s_raw, mul(mask(o, "r"), sin(plus(mul(mask(o, "g"), Py), mask(o, "b")))))
s_range = sub(p_smax, p_smin)
s_pad = mul(s_range, 0.25)
Sy = plus(p_smin, mul(s_range, smoothstep(sub(p_smin, s_pad), plus(p_smax, s_pad), s_raw)))
d = sub(sub(Px, p_shore), Sy)  # distance out to sea from the fractal waterline
beach = sub(1.0, smoothstep(0.06, 0.97, river_opening))
atten = mul(mul(lerp(0.15, 1.0, smoothstep(0.0, 1600.0, d)), vcol_r),
            lerp(1.0, smoothstep(-12.0, 65.0, d), beach))
HA = mul(H, atten)
GA = mul(G, atten)
MEL.connect_material_property(append(const2(0.0, 0.0), HA), "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
normal = unary("Normalize", append(mul(mul(GA, -1.0), p_nboost), 1.0))
MEL.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL)

# 2. Depth colour, lighter crests
depth = power(sat(div(d, 2600.0)), 0.6)
water = plus(lerp(p_shallow, p_deep, depth), mul(mul(p_shallow, 0.25), sat(div(HA, 14.0))))
plume = mul(smoothstep(0.03, 0.86, river_opening), sub(1.0, smoothstep(-30.0, 1320.0, d)))
water = lerp(water, vector("RiverPlumeTint", (0.027, 0.105, 0.076)), plume)

# 3. Foam net: two panned layers, each UV distorted by the G channel of a first lookup
uv0 = div(P, p_fscale)


def foam_layer(scale, pan):
    uv = plus(mul(uv0, scale) if scale != 1.0 else uv0, mul(const2(*pan), p_time))
    g = mask(tex_sample(uv), "g")
    uv2 = plus(uv, mul(sub(g, 0.5), 0.08))  # +-0.04
    t = tex_sample(uv2)
    return g, mask(t, "r"), mask(t, "b")


G1, R1, B1 = foam_layer(1.0, (0.012, 0.004))
G2, R2, B2 = foam_layer(1.37, (-0.007, 0.010))
# Break up the feather through the outlet with the existing water pattern.
# The mask stays fully opaque across the rest of the ocean.
feather = mask(vertex, "b")
coverage = sub(1.0, smoothstep(0.0, 1.0, plus(feather, mul(mul(feather, sub(1.0, feather)), sub(G1, 0.5)))))
# Clip the ocean apron where it reaches inland under the beach. The river opening keeps its own fade.
coverage = mul(coverage, lerp(1.0, smoothstep(-38.0, -12.0, d), beach))
dither = add("MaterialFunctionCall")
dither.set_material_function(unreal.load_asset(
    "/Engine/Functions/Engine_MaterialFunctions02/Utility/DitherTemporalAA"))
link(coverage, dither, "Alpha Threshold")
MEL.connect_material_property(dither, "", unreal.MaterialProperty.MP_OPACITY_MASK)

# S8: sparser, softer net that lives on the crests (was smoothstep(0.55, 0.75) * (0.25 + 0.75 * crest), read as cracked ice)
net = smoothstep(0.66, 0.9, mul(mul(R1, R2), 2.0))
foam_open = mul(net, plus(0.05, mul(0.65, sat(div(sub(HA, 3.0), 8.0)))))

# 4. Shore: lapping bands travelling in, and the edge line
dn = plus(d, mul(sub(G1, 0.5), 85.0))
phase = frac(plus(div(dn, p_lap_len), mul(p_time, p_lap_rate)))
lap = mul(smoothstep(0.77, 0.87, phase), sub(1.0, smoothstep(0.87, 0.97, phase)))
lap = mul(mul(lap, sub(1.0, smoothstep(170.0, 680.0, d))), lerp(0.12, 0.46, G2))
dne = plus(d, mul(sub(G1, 0.5), 28.0))
edge_hi = plus(27.0, mul(8.0, sin(plus(mul(p_time, 0.9), div(Py, 300.0)))))
edge = mul(sub(1.0, smoothstep(3.0, edge_hi, dne)), lerp(0.38, 0.76, G2))

# 5. Boat rings
rings = []
for f in p_foam:
    r = div(unary("Length", sub(P, mask(f, "rg"))), maxn(mask(f, "b"), 1.0))
    # S8: narrower ring broken up by the soft noise (was * 3.5 and step(0.35, B1), read as a cracked-ice disc)
    ring = mul(mul(mul(sat(sub(1.0, mul(absn(sub(r, 1.0)), 6.0))), smoothstep(0.35, 0.6, G2)), 0.85), mask(f, "a"))
    rings.append(ring)

# 6. Combine
shore_foam = mul(mul(maxn(lap, edge), smoothstep(-8.0, 8.0, d)), beach)
foam = sat(plus(plus(plus(foam_open, shore_foam), rings[0]), rings[1]))
base = lerp(lerp(water, p_adeep, mul(p_ash, 0.85)), lerp(p_cfoam, p_afoam, p_ash), foam)
MEL.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
MEL.connect_material_property(lerp(ROUGH_WATER, ROUGH_FOAM, foam), "", unreal.MaterialProperty.MP_ROUGHNESS)
MEL.connect_material_property(pin(SPECULAR)[0], "", unreal.MaterialProperty.MP_SPECULAR)
MEL.connect_material_property(pin(0.0)[0], "", unreal.MaterialProperty.MP_METALLIC)
sparkle = step(0.93, mul(mul(B1, B2), 4.0))
emissive = mul(mul(mul(mul(p_glint, 0.05), sparkle), sub(1.0, foam)), sub(1.0, p_ash))
MEL.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

MEL.layout_material_expressions(mat)
MEL.recompile_material(mat)
errs = MEL.get_material_error_expressions(mat)
if errs or not unreal.EditorAssetLibrary.save_asset(MAT_PATH, only_if_is_dirty=False):
    raise RuntimeError("[ocean] could not compile and save " + MAT_PATH)
summary = "[ocean] material built, %d expressions%s" % (
    MEL.get_num_material_expressions(mat), "" if not errs else ", %d ERROR expressions" % len(errs))
print(summary)
summary
