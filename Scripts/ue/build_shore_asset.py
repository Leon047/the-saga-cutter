# Generate the Saga shoreline material from the free Fab gravel surface.
import unreal

library = unreal.EditorAssetLibrary
editing = unreal.MaterialEditingLibrary
folder = "/Game/Vikings/Environment"
name = "M_SagaShore"
path = f"{folder}/{name}"
material = library.load_asset(path) if library.does_asset_exist(path) else None
if material:
    editing.delete_all_material_expressions(material)
else:
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, folder, unreal.Material, unreal.MaterialFactoryNew()
    )


def node(kind, **properties):
    expression = editing.create_material_expression(material, getattr(unreal, "MaterialExpression" + kind))
    for key, value in properties.items():
        expression.set_editor_property(key, value)
    return expression


def wire(source, target, pin, output=""):
    if not editing.connect_material_expressions(source, output, target, pin):
        raise RuntimeError(f"Could not connect {source.get_name()} to {pin}")


def custom(inputs, code, output_type):
    expression = node("Custom", code=code, output_type=output_type)
    pins = []
    for name in inputs:
        pin = unreal.CustomInput()
        pin.set_editor_property("input_name", name)
        pins.append(pin)
    expression.set_editor_property("inputs", pins)
    return expression


def output(expression, prop):
    if not editing.connect_material_property(expression, "", getattr(unreal.MaterialProperty, "MP_" + prop)):
        raise RuntimeError(f"Could not connect {prop}")


world = node("WorldPosition")
uv = custom(("Position",), "return Position.xy / 300.0;", unreal.CustomMaterialOutputType.CMOT_FLOAT2)
wire(world, uv, "Position")
texture_folder = "/Game/Fab/Beach_Gravel_With_Pebbles/tbvqegas_tier_2/Textures"


def sample(suffix, sampler):
    texture = library.load_asset(f"{texture_folder}/T_tbvqegas_2K_{suffix}")
    if not texture:
        raise RuntimeError(f"Missing Fab gravel texture {suffix}")
    expression = node("TextureSample", texture=texture, sampler_type=sampler)
    wire(uv, expression, "UVs")
    return expression


gravel = sample("B", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
normal = sample("N", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
palette = node("VectorParameter", parameter_name="Color", default_value=unreal.LinearColor(0.20, 0.19, 0.18, 1.0))
meadow = node("VectorParameter", parameter_name="Meadow", default_value=unreal.LinearColor(0.055, 0.095, 0.05, 1.0))
forest = node("VectorParameter", parameter_name="ForestFloor", default_value=unreal.LinearColor(0.028, 0.056, 0.032, 1.0))
ash = node("ScalarParameter", parameter_name="Ash", default_value=0.0)
vertex = node("VertexColor")
blend = custom(
    ("UV", "Gravel", "Mesh"),
    """
float2 p = UV * 300.0;
float edge = 1650 + 115 * sin(p.y / 418.0) + 70 * sin(p.y / 148.0 + 0.4);
float patches = sin(p.x / 73.0 + sin(p.y / 85.0)) * sin(p.y / 68.0);
float luma = dot(Gravel.rgb, float3(0.3, 0.59, 0.11));
float stones = smoothstep(-285, 245, p.x - edge + 76 * patches + 83 * (luma - 0.22));
return float3(stones, Mesh.r, Mesh.g);
""",
    unreal.CustomMaterialOutputType.CMOT_FLOAT3,
)
wire(uv, blend, "UV")
wire(gravel, blend, "Gravel")
wire(vertex, blend, "Mesh")
color = custom(
    ("Palette", "Gravel", "Blend", "Meadow", "Forest", "Ash"),
    """
float luma = dot(Gravel.rgb, float3(0.3, 0.59, 0.11));
float3 rock = Palette.rgb * lerp(0.63, 1.32, smoothstep(0.035, 0.39, luma));
float gray = dot(rock, float3(0.3, 0.59, 0.11));
float3 ashen = gray * float3(0.55, 0.50, 0.47) + rock * float3(0.08, 0.05, 0.03) + float3(0.012, 0.010, 0.008);
rock = lerp(rock, ashen, Ash) * lerp(1.0, 0.6, Blend.g);
float3 grass = lerp(Forest.rgb, Meadow.rgb, Blend.b);
return lerp(grass, rock, Blend.r);
""",
    unreal.CustomMaterialOutputType.CMOT_FLOAT3,
)
wire(palette, color, "Palette")
wire(gravel, color, "Gravel")
wire(blend, color, "Blend")
wire(meadow, color, "Meadow")
wire(forest, color, "Forest")
wire(ash, color, "Ash")
output(color, "BASE_COLOR")
normal_strength = custom(
    ("Detail", "Blend"), "return normalize(lerp(float3(0,0,1), Detail.rgb, Blend.r * lerp(0.28,0.20,Blend.g)));",
    unreal.CustomMaterialOutputType.CMOT_FLOAT3,
)
wire(normal, normal_strength, "Detail")
wire(blend, normal_strength, "Blend")
output(normal_strength, "NORMAL")
roughness = custom(("Blend",), "return lerp(0.9, 0.56, Blend.r * Blend.g);", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
wire(blend, roughness, "Blend")
output(roughness, "ROUGHNESS")
output(node("Constant", r=0.23), "SPECULAR")
editing.layout_material_expressions(material)
editing.recompile_material(material)
if editing.get_material_error_expressions(material) or not library.save_loaded_asset(material, False):
    raise RuntimeError(f"Could not compile and save {path}")
