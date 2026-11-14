# The Saga Cutter - game jam project.
# Builds the river material in the running editor (idempotent: updates the existing asset).
# Run inside the editor:  exec(open(r"<repo>/Scripts/ue/build_river_assets.py").read())
#   /Game/Vikings/Environment/River/M_SagaRiverClear   translucent, on the ribbon built by ATSCTerrain
# Reuses /Game/Vikings/Environment/Ocean/T_OceanPatterns (built by build_ocean_assets.py) and the sea's colours.
# UV0 stores across-channel position and downstream travel time. Vertex color stores mouth fade, rock wakes, and rapid reaches in RGB.
# Parameters marked * are set by ATSCTerrain every tick; the rest are look defaults.
import os
import unreal

FOLDER = "/Game/Vikings/Environment/River"
TEX_PATH = "/Game/Vikings/Environment/Ocean/T_OceanPatterns"
MAT_NAME = "M_SagaRiverClear"
MAT_PATH = FOLDER + "/" + MAT_NAME

# --- Look defaults (tunable) ---
FLOW_SPEED = 0.58           # V units per second (1 V = 600 world units at the reference width)
FOAM_SCALE = 0.3            # S7: 1.0 read as fine specks at arm 16500
ROUGH_WATER = 0.5
ROUGH_FOAM = 0.95
SPECULAR = 0.25
COL_SHALLOW = (0.069, 0.139, 0.115)
COL_DEEP = (0.018, 0.044, 0.041)
COL_FOAM = (0.80, 0.86, 0.80)
COL_ASH_DEEP = (0.030, 0.030, 0.034)
COL_ASH_FOAM = (0.30, 0.28, 0.26)

tex = unreal.load_asset(TEX_PATH + ".T_OceanPatterns")
if not tex:
    raise RuntimeError("[river] missing " + TEX_PATH + " (run build_ocean_assets.py first)")

# --- Material ---
if not unreal.EditorAssetLibrary.does_directory_exist(FOLDER):
    unreal.EditorAssetLibrary.make_directory(FOLDER)
mat = unreal.load_asset(MAT_PATH + "." + MAT_NAME)
if mat:
    unreal.MaterialEditingLibrary.delete_all_material_expressions(mat)
else:
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(MAT_NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
mat.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
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
        raise RuntimeError("[river] link failed: %s.%s -> %s.%s (inputs %s)" % (
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
def append(a, b): return binop("AppendVector", a, b)


def unary(cls, a, **props):
    n = add(cls)
    for k, v in props.items():
        n.set_editor_property(k, v)
    link(a, n, "")
    return n


def sat(a): return unary("Saturate", a)
def absn(a): return unary("Abs", a)


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


def power(base, exponent):
    n = add("Power")
    link(base, n, "Base")
    link(exponent, n, "Exponent")
    return n


def tex_sample(uv):
    t = add("TextureSample")
    t.set_editor_property("texture", tex)
    t.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    link(uv, t, "UVs")
    return t


# Parameters
p_time = scalar("RiverTime", 0.0)     # *
p_ash = scalar("Ash", 0.0)            # *
p_flow = scalar("FlowSpeed", FLOW_SPEED)
p_fscale = scalar("FoamScale", FOAM_SCALE)
p_shallow = vector("ColShallow", COL_SHALLOW)
p_deep = vector("ColDeep", COL_DEEP)
p_cfoam = vector("ColFoam", COL_FOAM)
p_adeep = vector("ColAshDeep", COL_ASH_DEEP)
p_afoam = vector("ColAshFoam", COL_ASH_FOAM)

uv = add("TextureCoordinate")
U = mask(uv, "r")
V = mask(uv, "g")
vertex = add("VertexColor")
vcol_r = mask(vertex, "r")
wake = mask(vertex, "g")
rapid = mask(vertex, "b")
depth = (vertex, "A")

# 1. Across profile: c = 0 centre .. 1 edge; deeper colour in the middle
c = mul(absn(sub(U, 0.5)), 2.0)
water = lerp(p_deep, p_shallow, power(c, 1.5))

# 2. Foam net, panned downstream (toward growing V)
flow = mul(p_time, p_flow)
uv1 = plus(mul(append(mul(U, 1.4), V), p_fscale), append(0.0, mul(flow, -1.0)))
t1 = tex_sample(uv1)
R1 = mask(t1, "r")

# 3. Patchy foam within the rapid reaches and close to the rocks.
foam_texture = add("Custom")
foam_texture.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
foam_inputs = []
for name in ("UV", "Flow"):
    entry = unreal.CustomInput()
    entry.set_editor_property("input_name", name)
    foam_inputs.append(entry)
foam_texture.set_editor_property("inputs", foam_inputs)
foam_texture.set_editor_property("code", """
float2 p = float2(UV.x, UV.y - Flow * 1.65);
float n = 0;
float weight = 0.53;
for (int octave = 0; octave < 4; ++octave)
{
    float2 cell = floor(p * 13);
    float2 f = frac(p * 13);
    f = f * f * (3 - 2 * f);
    float4 x = cell.x + float4(0, 1, 0, 1);
    float4 y = cell.y + float4(0, 0, 1, 1);
    float4 seed = frac(sin(x * 127.1 + y * 311.7) * 43758.5453);
    n += weight * lerp(lerp(seed.x, seed.y, f.x), lerp(seed.z, seed.w, f.x), f.y);
    p = float2(p.x + p.y * 0.16, p.y - p.x * 0.16) * 2.12 + 0.37;
    weight *= 0.5;
}
return n;
""")
link(uv, foam_texture, "UV")
link(flow, foam_texture, "Flow")
froth = smoothstep(0.24, 0.72, foam_texture)
breaking = mul(power(wake, 0.89), plus(0.30, mul(froth, 1.24)))
riffles = mul(rapid, mul(smoothstep(0.31, 0.72, foam_texture), plus(0.14, mul(froth, 0.34))))
foam = mul(sat(plus(breaking, riffles)), sub(1.0, vcol_r))
water = lerp(mul(water, plus(0.96, mul(R1, 0.08))), vector("MouthTint", (0.032, 0.088, 0.076)), vcol_r)

def wave_node(code):
    node = add("Custom")
    node.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs = []
    for name in ("UV", "Flow", "Wake", "Rapid", "Mouth"):
        entry = unreal.CustomInput()
        entry.set_editor_property("input_name", name)
        inputs.append(entry)
    node.set_editor_property("inputs", inputs)
    node.set_editor_property("code", code)
    for value, name in ((uv, "UV"), (flow, "Flow"), (wake, "Wake"), (rapid, "Rapid"), (vcol_r, "Mouth")):
        link(value, node, name)
    return node

# Broad, rounded crests in the mesh. Higher frequency detail comes from the material normal.
displacement = wave_node("""
float a = sin(UV.y * 11.8 - Flow * 8.2 + sin(UV.x * 10.5 + UV.y * 4.6) * 1.05);
float b = cos(UV.y * 16.7 - Flow * 10.1 - UV.x * 14.0);
float energy = saturate(Rapid * 0.58 + Wake * 0.75);
float banks = smoothstep(0, 0.11, min(UV.x, 1 - UV.x));
float crest = 16.0 * pow(saturate(0.52 + a * 0.36 + b * 0.17), 2) - 3.0;
return float3(0, 0, (0.75 * b + energy * crest) * banks * (1 - Mouth));
""")
MEL.connect_material_property(displacement, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

ripple = wave_node("""
float a = cos(UV.y * 12.2 - Flow * 8.2 + sin(UV.x * 10.5 + UV.y * 4.6));
float b = sin(UV.y * 16.7 - Flow * 10.1 - UV.x * 14.0);
float c = sin(UV.y * 57 - Flow * 22 + cos(UV.x * 43.0 + UV.y * 8.0) * 2.0);
float energy = saturate(Rapid * 0.60 + Wake * 0.80);
float2 slope = float2(0.16 * a + 0.055 * c, 0.11 * b - 0.05 * c);
return normalize(float3(slope * (0.19 + energy) * (1 - Mouth), 1));
""")
MEL.connect_material_property(ripple, "", unreal.MaterialProperty.MP_NORMAL)

# Keep the riverbed visible on the shelves, with a stronger green tint down the channel.
water = lerp(water, vector("ChannelGreen", (0.027, 0.147, 0.064)), mul(depth, sub(1.0, vcol_r)))
base = lerp(lerp(water, p_adeep, mul(p_ash, 0.85)), lerp(p_cfoam, p_afoam, p_ash), foam)
MEL.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
surface_opacity = lerp(lerp(0.09, 0.66, power(depth, 2.25)), 0.97, power(foam, 0.82))
surface_opacity = mul(surface_opacity, sub(1.0, power(vcol_r, 3.0)))
MEL.connect_material_property(surface_opacity, "", unreal.MaterialProperty.MP_OPACITY)
MEL.connect_material_property(lerp(lerp(0.29, ROUGH_WATER, rapid), ROUGH_FOAM, foam), "", unreal.MaterialProperty.MP_ROUGHNESS)
MEL.connect_material_property(lerp(0.045, SPECULAR, sat(plus(mul(rapid, 0.65), foam))), "", unreal.MaterialProperty.MP_SPECULAR)
MEL.connect_material_property(pin(0.0)[0], "", unreal.MaterialProperty.MP_METALLIC)

MEL.layout_material_expressions(mat)
MEL.recompile_material(mat)
errs = MEL.get_material_error_expressions(mat)
if errs or not unreal.EditorAssetLibrary.save_loaded_asset(mat, only_if_is_dirty=False):
    raise RuntimeError("[river] could not compile and save " + MAT_PATH)
summary = "[river] material built, %d expressions%s" % (
    MEL.get_num_material_expressions(mat), "" if not errs else ", %d ERROR expressions" % len(errs))
print(summary)
summary
