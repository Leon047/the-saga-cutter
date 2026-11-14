# Run in Unreal Editor to create or refresh the runtime terrain and trail materials.
import unreal


FOLDER = "/Game/Vikings/Environment"
MEL = unreal.MaterialEditingLibrary


def build(name, trail=False):
    path = f"{FOLDER}/{name}"
    mat = unreal.EditorAssetLibrary.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if mat:
        MEL.delete_all_material_expressions(mat)
    else:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, FOLDER, unreal.Material, unreal.MaterialFactoryNew()
        )
    mat.set_editor_property(
        "blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT if trail else unreal.BlendMode.BLEND_OPAQUE
    )
    if trail:
        mat.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)

    def node(kind, **props):
        expr = MEL.create_material_expression(mat, getattr(unreal, "MaterialExpression" + kind))
        for key, value in props.items():
            expr.set_editor_property(key, value)
        return expr

    def wire(source, target, pin, output=""):
        if not MEL.connect_material_expressions(source, output, target, pin):
            raise RuntimeError(f"{name}: cannot connect {output} to {pin}")

    def property_pin(source, prop):
        if not MEL.connect_material_property(source, "", getattr(unreal.MaterialProperty, "MP_" + prop)):
            raise RuntimeError(f"{name}: cannot connect {prop}")

    def custom_inputs(expression, names):
        inputs = []
        for pin in names:
            entry = unreal.CustomInput()
            entry.set_editor_property("input_name", pin)
            inputs.append(entry)
        expression.set_editor_property("inputs", inputs)

    vertex = node("VertexColor")
    ash = node("ScalarParameter", parameter_name="Ash", default_value=0.0)
    uv = node("TextureCoordinate")
    color = node("Custom", output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    custom_inputs(color, ("Palette", "Ash", "Gravel", "Forest", "GroundDetail", "MossTexture"))
    color.set_editor_property("code", """
float green = Palette.g - max(Palette.r, Palette.b);
float vegetation = smoothstep(0.012, 0.075, green);
float stone = (1 - vegetation) * (1 - Forest);
float gravelDetail = lerp(0.69, 1.36, smoothstep(0.04, 0.39, Gravel));
float mossDetail = lerp(0.86, 1.14, smoothstep(0.022, 0.26, MossTexture));
float detail = stone * (gravelDetail - 1) + Forest * (mossDetail - 1);
float3 emerald = Palette.rgb * float3(0.64, 1.07, 1.64);
float3 living = lerp(Palette.rgb, emerald, Forest * vegetation)
              * (1 + GroundDetail * detail);
float luma = dot(living, float3(0.3, 0.59, 0.11));
float3 ashen = luma * float3(0.55, 0.50, 0.47) + living * float3(0.08, 0.05, 0.03)
             + float3(0.012, 0.010, 0.008);
return lerp(living, ashen, saturate(Ash));
""")
    if trail:
        custom_inputs(color, ("Palette", "Ash", "UV", "Gravel", "Forest", "GroundDetail", "MossTexture"))
        code = color.get_editor_property("code")
        color.set_editor_property("code", code.replace("float luma =", """
living *= 1 + 0.045 * sin(UV.x * 58 + sin(UV.y * 39)) * sin(UV.y * 51 - sin(UV.x * 21))
            + 0.10 * sin(UV.x * 9.1 + UV.y * 4.7) * sin(UV.y * 8.4 - UV.x * 5.3);
float luma ="""))
        wire(uv, color, "UV")
        wire(node("Constant", r=0), color, "GroundDetail")
        wire(node("Constant", r=0), color, "Gravel")
        wire(node("Constant", r=0), color, "Forest")
        wire(node("Constant", r=0.13), color, "MossTexture")
        palette = node("VectorParameter", parameter_name="Color", default_value=unreal.LinearColor(0.14, 0.09, 0.06, 1))
        wire(palette, color, "Palette")
        edge = node("Custom", output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1)
        custom_inputs(edge, ("Coverage", "UV"))
        edge.set_editor_property("code", """
float breakup = sin(UV.x * 84 + sin(UV.y * 67)) * sin(UV.y * 81);
return saturate(Coverage + 0.62 * Coverage * (1 - Coverage) * breakup);
""")
        wire(vertex, edge, "Coverage", "A")
        wire(uv, edge, "UV")
        property_pin(edge, "OPACITY")
    else:
        root = "/Game/Fab/Beach_Gravel_With_Pebbles/tbvqegas_tier_2/Textures/T_tbvqegas_2K_"
        gravel = unreal.EditorAssetLibrary.load_asset(root + "B")
        gravel_normal = unreal.EditorAssetLibrary.load_asset(root + "N")
        if not gravel or not gravel_normal:
            raise RuntimeError("Fab Beach Gravel With Pebbles textures are missing")

        moss_root = "/Game/Fab/Mossy_Ground/vjoefgo_tier_2/Textures/T_vjoefgo_2K_"
        moss_color_path = moss_root + "B"
        moss_normal_path = moss_root + "N"
        has_moss = all(unreal.EditorAssetLibrary.does_asset_exist(path)
                       for path in (moss_color_path, moss_normal_path))
        moss_color = unreal.EditorAssetLibrary.load_asset(moss_color_path) if has_moss else gravel
        moss_normal = unreal.EditorAssetLibrary.load_asset(moss_normal_path) if has_moss else gravel_normal

        gravel_uv = node("TextureCoordinate", u_tiling=6.0, v_tiling=6.0)
        coordinates = node("Custom", output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
        custom_inputs(coordinates, ("UV",))
        coordinates.set_editor_property("code", """
struct Noise
{
    float hash(float2 p)
    {
        float3 h = frac(float3(p.xyx) * 0.1031);
        h += dot(h, h.yzx + 33.33);
        return frac((h.x + h.y) * h.z);
    }
    float sample(float2 p)
    {
        float2 cell = floor(p);
        float2 t = frac(p);
        t = t*t*(3-2*t);
        return lerp(lerp(hash(cell), hash(cell + float2(1,0)), t.x),
                    lerp(hash(cell + float2(0,1)), hash(cell + 1), t.x), t.y);
    }
};
Noise n;
float2 warp = float2(n.sample(UV * 4.3), n.sample(UV * 4.3 + 41.7)) - 0.5;
return float3(UV + warp * 0.17, smoothstep(0.2, 0.8, n.sample(UV * 2.75 + 16.3)));
""")
        wire(uv, coordinates, "UV")
        mask = node("ComponentMask", r=True, g=True)
        weight = node("ComponentMask", b=True)
        wire(coordinates, mask, "Input")
        wire(coordinates, weight, "Input")
        gravel_sample = node("TextureSample", texture=gravel, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        wire(gravel_uv, gravel_sample, "UVs")
        wire(gravel_sample, color, "Gravel", "G")

        def sample_pair(texture, sampler):
            samples = []
            for frequency, offset in ((9.7, 0.0), (6.13, 0.37)):
                scale = node("Multiply", const_b=frequency)
                shifted = node("Add", const_b=offset)
                wire(mask, scale, "A")
                wire(scale, shifted, "A")
                sample = node("TextureSample", texture=texture, sampler_type=sampler)
                wire(shifted, sample, "UVs")
                samples.append(sample)
            blend = node("LinearInterpolate")
            wire(samples[0], blend, "A", "RGB")
            wire(samples[1], blend, "B", "RGB")
            wire(weight, blend, "Alpha")
            return blend

        moss_sample = sample_pair(moss_color, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        normal_sample = sample_pair(moss_normal, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        moss_green = node("ComponentMask", g=True)
        wire(moss_sample, moss_green, "Input")
        wire(moss_green, color, "MossTexture")
        wire(vertex, color, "Forest", "A")
        wire(node("Constant", r=0.88), color, "GroundDetail")
        wire(vertex, color, "Palette")

        detail_normal = node("Custom", output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
        custom_inputs(detail_normal, ("DetailNormal", "Forest"))
        detail_normal.set_editor_property("code", """
return normalize(float3(DetailNormal.xy * lerp(0.12, 0.62, Forest), max(0.1, DetailNormal.z)));
""")
        wire(normal_sample, detail_normal, "DetailNormal")
        wire(vertex, detail_normal, "Forest", "A")
        property_pin(detail_normal, "NORMAL")

    regional_ash = node("Custom", output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    ash_inputs = ["UV", "AmbientAsh"]
    for region in range(1, 4):
        ash_inputs.extend((f"AshBounds{region}", f"AshWave{region}"))
    custom_inputs(regional_ash, ash_inputs)
    regional_ash.set_editor_property("code", """
struct GroundAsh
{
    float amount(float2 p, float4 bounds, float4 wave)
    {
        float2 edge = min(p - bounds.xy, bounds.zw - p);
        float inRegion = smoothstep(0, 36, min(edge.x, edge.y));
        float delay = min(distance(p, wave.xy) / 2400, 2.4);
        return wave.w * inRegion * smoothstep(0, 1, (wave.z - delay) * 0.9);
    }
};
GroundAsh ground;
float2 p = UV * 1000;
float local = max(ground.amount(p, AshBounds1, AshWave1), ground.amount(p, AshBounds2, AshWave2));
local = max(local, ground.amount(p, AshBounds3, AshWave3));
return max(AmbientAsh, local);
""")
    wire(uv, regional_ash, "UV")
    wire(ash, regional_ash, "AmbientAsh")
    for parameter_name in ash_inputs[2:]:
        parameter = node("VectorParameter", parameter_name=parameter_name, default_value=unreal.LinearColor(0, 0, 0, 0))
        rgba = node("AppendVector")
        wire(parameter, rgba, "A")
        wire(parameter, rgba, "B", "A")
        wire(rgba, regional_ash, parameter_name)
    wire(regional_ash, color, "Ash")
    property_pin(color, "BASE_COLOR")
    property_pin(node("Constant", r=0.88), "ROUGHNESS")
    property_pin(node("Constant", r=0.23), "SPECULAR")
    MEL.layout_material_expressions(mat)
    MEL.recompile_material(mat)
    if MEL.get_material_error_expressions(mat):
        raise RuntimeError(f"{path} has material compile errors")
    if not unreal.EditorAssetLibrary.save_loaded_asset(mat, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save {path}")
    unreal.log(f"[terrain] built {path}")


if __name__ == "__main__":
    build("M_SagaEmeraldGround")
    build("M_SagaForestTrail", trail=True)
