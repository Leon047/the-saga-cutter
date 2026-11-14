# Run in Unreal Editor after importing the Nordic Megascans meshes.
import unreal


library = unreal.EditorAssetLibrary
editing = unreal.MaterialEditingLibrary
assets = unreal.AssetToolsHelpers.get_asset_tools()
folder = "/Game/Vikings/Environment"

for name, scan, code in (
    ("M_SagaNordicCluster", "Nordic_Forest_Cluster_Rock_Small", "xjijdiz"),
    ("M_SagaNordicLedge", "Nordic_Forest_Ledge_Rock_Medium", "xislecsqx"),
):
    root = f"/Game/Fab/{scan}/{code}_tier_2"
    path = f"{folder}/{name}"
    material = library.load_asset(path) if library.does_asset_exist(path) else assets.create_asset(
        name, folder, unreal.Material, unreal.MaterialFactoryNew()
    )
    editing.delete_all_material_expressions(material)
    material.set_editor_property("used_with_instanced_static_meshes", True)
    material.set_editor_property("used_with_nanite", True)

    def sample(suffix, sampler):
        texture = library.load_asset(f"{root}/Textures/T_{code}_2K_{suffix}")
        if not texture:
            raise RuntimeError(f"Missing Nordic scan texture {suffix} for {scan}")
        node = editing.create_material_expression(material, unreal.MaterialExpressionTextureSample)
        node.set_editor_property("texture", texture)
        node.set_editor_property("sampler_type", sampler)
        return node

    base = sample("B", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    normal = sample("N", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    orm = sample("ORM", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    tint = editing.create_material_expression(material, unreal.MaterialExpressionVectorParameter)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(0.83, 0.88, 0.77, 1))
    amount = editing.create_material_expression(material, unreal.MaterialExpressionScalarParameter)
    amount.set_editor_property("parameter_name", "MossAmount")
    amount.set_editor_property("default_value", 1.0)
    moss = editing.create_material_expression(material, unreal.MaterialExpressionCustom)
    moss.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    inputs = []
    for pin in ("Position", "Facing", "Coverage"):
        entry = unreal.CustomInput()
        entry.set_editor_property("input_name", pin)
        inputs.append(entry)
    moss.set_editor_property("inputs", inputs)
    moss.set_editor_property("code", """
float3 p = Position.xyz;
float broad = sin(p.x * 0.035 + cos(p.y * 0.046) * 1.4) * cos(p.y * 0.052 + p.z * 0.028);
float fine = cos(p.x * 0.191 - p.z * 0.153) * sin(p.y * 0.166 + p.x * 0.064);
return Coverage * smoothstep(-0.28, 0.46, Facing.z + broad * 0.53 + fine * 0.13);
""")
    position = editing.create_material_expression(material, unreal.MaterialExpressionWorldPosition)
    facing = editing.create_material_expression(material, unreal.MaterialExpressionVertexNormalWS)
    assert editing.connect_material_expressions(position, "", moss, "Position")
    assert editing.connect_material_expressions(facing, "", moss, "Facing")
    assert editing.connect_material_expressions(amount, "", moss, "Coverage")

    rock = editing.create_material_expression(material, unreal.MaterialExpressionMultiply)
    assert editing.connect_material_expressions(base, "RGB", rock, "A")
    assert editing.connect_material_expressions(tint, "RGB", rock, "B")
    green = editing.create_material_expression(material, unreal.MaterialExpressionCustom)
    grain = unreal.CustomInput()
    grain.set_editor_property("input_name", "Rock")
    green.set_editor_property("inputs", [grain])
    green.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    green.set_editor_property("code", """
float detail = smoothstep(0.022, 0.19, dot(Rock.rgb, float3(0.3, 0.59, 0.11)));
return lerp(float3(0.031, 0.055, 0.009), float3(0.187, 0.273, 0.042), detail);
""")
    assert editing.connect_material_expressions(base, "RGB", green, "Rock")
    color = editing.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate)
    assert editing.connect_material_expressions(rock, "", color, "A")
    assert editing.connect_material_expressions(green, "", color, "B")
    assert editing.connect_material_expressions(moss, "", color, "Alpha")
    assert editing.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    assert editing.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    assert editing.connect_material_property(orm, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    roughness = editing.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate)
    roughness.set_editor_property("const_b", 0.96)
    assert editing.connect_material_expressions(orm, "G", roughness, "A")
    assert editing.connect_material_expressions(moss, "", roughness, "Alpha")
    assert editing.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    editing.layout_material_expressions(material)
    editing.recompile_material(material)
    assert not editing.get_material_error_expressions(material) and library.save_loaded_asset(material, False)

    mesh = library.load_asset(f"{root}/StaticMeshes/{code}_tier_2")
    assert mesh
    mesh.set_material(0, material)
    assert library.save_loaded_asset(mesh, False)
