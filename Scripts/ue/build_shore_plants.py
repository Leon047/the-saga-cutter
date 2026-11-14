# Apply Saga colors to the selected Watermills foliage while retaining its cutout silhouettes.
import unreal

library = unreal.EditorAssetLibrary
editing = unreal.MaterialEditingLibrary
folder = "/Game/Vikings/Environment/ShorePlants"
for mesh_name, texture_name, material_name in (
    ("SM_Grass05", "T_Grass02_B", "M_SagaShoreGrass"),
    ("SM_Fern02", "T_Fern_B", "M_SagaShoreFern"),
):
    path = f"{folder}/{material_name}"
    mat = library.load_asset(path) if library.does_asset_exist(path) else None
    if mat:
        editing.delete_all_material_expressions(mat)
    else:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            material_name, folder, unreal.Material, unreal.MaterialFactoryNew()
        )
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    sample = editing.create_material_expression(mat, unreal.MaterialExpressionTextureSample)
    sample.set_editor_property("texture", library.load_asset(f"{folder}/{texture_name}"))
    sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    tint = editing.create_material_expression(mat, unreal.MaterialExpressionVectorParameter)
    tint.set_editor_property("parameter_name", "Color")
    tint.set_editor_property("default_value", unreal.LinearColor(0.075, 0.13, 0.055, 1))
    scale = editing.create_material_expression(mat, unreal.MaterialExpressionCustom)
    scale.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs = []
    for name in ("Color", "Leaf"):
        pin = unreal.CustomInput()
        pin.set_editor_property("input_name", name)
        inputs.append(pin)
    scale.set_editor_property("inputs", inputs)
    scale.set_editor_property("code", "return Color.rgb * lerp(0.60, 1.38, saturate(dot(Leaf.rgb, float3(0.3,0.59,0.11)) * 3.5));")
    assert editing.connect_material_expressions(tint, "", scale, "Color")
    assert editing.connect_material_expressions(sample, "", scale, "Leaf")
    assert editing.connect_material_property(scale, "", unreal.MaterialProperty.MP_BASE_COLOR)
    assert editing.connect_material_property(sample, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
    rough = editing.create_material_expression(mat, unreal.MaterialExpressionConstant)
    rough.set_editor_property("r", 0.82)
    assert editing.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    editing.layout_material_expressions(mat)
    editing.recompile_material(mat)
    assert not editing.get_material_error_expressions(mat) and library.save_loaded_asset(mat, False)
    mesh = library.load_asset(f"{folder}/{mesh_name}")
    mesh.set_material(0, mat)
    assert library.save_loaded_asset(mesh, False)
