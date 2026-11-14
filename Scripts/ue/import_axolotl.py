# The Saga Cutter - game jam project.
# Imports one Tripo axolotl character into /Game/Characters/axolotl_<key> (idempotent: reimports over existing assets).
# Run inside the editor (CHAR defaults to "warrior"; REPO defaults to the repo path below):
#   CHAR = "archer"; exec(open(r"<repo>/Scripts/ue/import_axolotl.py").read())
#   Axolotl_<Key> (skeletal mesh), Axolotl_<Key>_Skeleton, M_Axolotl<Key>, T_Axolotl<Key>_{BaseColor,Normal,Roughness,Metallic}
# Source: Art/Tripo/axolotl_<key>/ (made by Scripts/blender/prep_axolotl.py). No animation, materials and textures are NOT taken from the FBX.
# Naming: Docs/Plans/axolotl-all-classes.md, "Naming for a new character".
import os
import unreal

if "CHAR" not in globals():
    CHAR = "warrior"
PASCAL = CHAR.capitalize()
if "REPO" not in globals():
    REPO = r"C:\Users\Sega\Documents\Projects\TheSagaCutter"
SRC = os.path.join(REPO, "Art", "Tripo", "axolotl_" + CHAR)
FOLDER = "/Game/Characters/axolotl_" + CHAR
MESH_NAME = "Axolotl_" + PASCAL
SKEL_NAME = MESH_NAME + "_Skeleton"
MAT_NAME = "M_Axolotl" + PASCAL
TEX_PREFIX = "T_Axolotl" + PASCAL + "_"
FBX_NAME = "SK_Axolotl" + PASCAL + ".fbx"
CLIP_PREFIX = MESH_NAME + "CharacterArmature_"
IK_RIG_NAME = "IK_Axolotl_" + PASCAL
RTG_NAME = "RTG_Viking_To_Axolotl" if CHAR == "warrior" else "RTG_Viking_To_Axolotl_" + PASCAL
# The FBX carries real centimetre data (Blender scene scale_length 0.01, exported with FBX_SCALE_NONE): bones and
# vertices in cm, no node scale, UnitScaleFactor 1. Imported as is, with no unit conversion and no uniform scale, so the
# mesh reference pose equals the skeleton reference pose (Hips at ~29.9 cm for the warrior, scale 1 on every bone).
SCALE = 1.0
TEXTURES = ["BaseColor", "Normal", "Roughness", "Metallic"]
# Set FORCE_TEXTURES_AND_MATERIAL = True before exec to reimport the textures and rebuild the material too.
if "FORCE_TEXTURES_AND_MATERIAL" not in globals():
    FORCE_TEXTURES_AND_MATERIAL = False
EAL = unreal.EditorAssetLibrary
CLIPS = ["Idle", "Walk", "Run", "Walk_Carry", "Run_Carry", "SwordSlash",
         "Shoot_OneHanded", "Punch", "PickUp", "RecieveHit", "Death"]

if not unreal.EditorAssetLibrary.does_directory_exist(FOLDER):
    unreal.EditorAssetLibrary.make_directory(FOLDER)


def run_task(filename, name, options=None):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", filename)
    t.set_editor_property("destination_path", FOLDER)
    t.set_editor_property("destination_name", name)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("automated", True)
    t.set_editor_property("save", False)
    if options is not None:
        t.set_editor_property("options", options)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    return t


# --- Textures ---
for kind in TEXTURES:
    name = TEX_PREFIX + kind
    if not FORCE_TEXTURES_AND_MATERIAL and EAL.does_asset_exist("%s/%s" % (FOLDER, name)):
        continue
    run_task(os.path.join(SRC, name + ".jpg"), name)
    tex = unreal.load_asset("%s/%s.%s" % (FOLDER, name, name))
    if kind == "BaseColor":
        tex.set_editor_property("srgb", True)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    elif kind == "Normal":
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
    else:
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP)
    unreal.EditorAssetLibrary.save_loaded_asset(tex)

# --- Skeletal mesh ---
# A rerun over an existing mesh would be an atomic reimport that keeps the old import settings, so the mesh is deleted first (the skeleton stays).
ui = unreal.FbxImportUI()
ui.set_editor_property("import_mesh", True)
ui.set_editor_property("import_as_skeletal", True)
ui.set_editor_property("import_animations", False)
ui.set_editor_property("import_materials", False)
ui.set_editor_property("import_textures", False)
ui.set_editor_property("create_physics_asset", False)
ui.set_editor_property("automated_import_should_detect_type", False)
ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
ui.skeletal_mesh_import_data.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
ui.skeletal_mesh_import_data.set_editor_property("import_morph_targets", False)
ui.skeletal_mesh_import_data.set_editor_property("convert_scene", True)
ui.skeletal_mesh_import_data.set_editor_property("convert_scene_unit", False)
ui.skeletal_mesh_import_data.set_editor_property("import_uniform_scale", SCALE)

# Mesh AND skeleton are recreated on every run (a reused skeleton keeps its old reference pose). Their dependents go
# first: the retargeted clips, the retargeter and the axolotl IK Rig (Scripts/ue/retarget_axolotl.py rebuilds them).
unreal.SystemLibrary.collect_garbage()
for p in ["%s/%s%s" % (FOLDER, CLIP_PREFIX, c) for c in CLIPS] + [
        FOLDER + "/" + RTG_NAME, FOLDER + "/" + IK_RIG_NAME,
        "%s/%s" % (FOLDER, MESH_NAME), "%s/%s" % (FOLDER, SKEL_NAME)]:
    if EAL.does_asset_exist(p):
        if not EAL.delete_asset(p):
            raise RuntimeError("[axolotl] could not delete " + p)
# Purge the deleted objects from memory, or the importer finds the old skeleton object by name and merges into it
# (keeping its stale reference pose).
unreal.SystemLibrary.collect_garbage()

task = run_task(os.path.join(SRC, FBX_NAME), MESH_NAME, ui)
imported = list(task.get_editor_property("imported_object_paths"))
mesh = unreal.load_asset("%s/%s.%s" % (FOLDER, MESH_NAME, MESH_NAME))
if not mesh:
    raise RuntimeError("[axolotl] mesh import failed: %s" % imported)

# The importer names a new skeleton <mesh>_Skeleton, which is exactly SKEL_NAME. Verify.
skel = mesh.get_editor_property("skeleton")
if skel.get_name() != SKEL_NAME:
    unreal.EditorAssetLibrary.rename_asset(skel.get_path_name().split(".")[0], "%s/%s" % (FOLDER, SKEL_NAME))
    skel = mesh.get_editor_property("skeleton")

# --- Material ---
MAT_PATH = "%s/%s" % (FOLDER, MAT_NAME)
MEL = unreal.MaterialEditingLibrary
mat = unreal.load_asset(MAT_PATH + "." + MAT_NAME)
BUILD_MAT = FORCE_TEXTURES_AND_MATERIAL or not mat
if mat and BUILD_MAT:
    MEL.delete_all_material_expressions(mat)
elif not mat:
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(MAT_NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())


def build_material(mat):
    mat.set_editor_property("two_sided", False)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)

    def expr(cls, x, y):
        return MEL.create_material_expression(mat, getattr(unreal, "MaterialExpression" + cls), x, y)


    def tex_node(kind, x, y, sampler):
        n = expr("TextureSampleParameter2D", x, y)
        n.set_editor_property("parameter_name", kind)
        n.set_editor_property("texture", unreal.load_asset("%s/%s%s.%s%s" % (FOLDER, TEX_PREFIX, kind, TEX_PREFIX, kind)))
        n.set_editor_property("sampler_type", sampler)
        return n


    S = unreal.MaterialSamplerType
    base = tex_node("BaseColor", -900, -300, S.SAMPLERTYPE_COLOR)
    nrm = tex_node("Normal", -900, 0, S.SAMPLERTYPE_NORMAL)
    rough = tex_node("Roughness", -900, 300, S.SAMPLERTYPE_MASKS)
    metal = tex_node("Metallic", -900, 600, S.SAMPLERTYPE_MASKS)

    diffuse = expr("VectorParameter", -900, -550)
    diffuse.set_editor_property("parameter_name", "DiffuseColor")
    diffuse.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))

    metal_scale = expr("ScalarParameter", -900, 850)
    metal_scale.set_editor_property("parameter_name", "MetallicScale")
    metal_scale.set_editor_property("default_value", 0.5)

    mul_col = expr("Multiply", -500, -350)
    MEL.connect_material_expressions(base, "RGB", mul_col, "A")
    MEL.connect_material_expressions(diffuse, "", mul_col, "B")

    mul_met = expr("Multiply", -500, 650)
    MEL.connect_material_expressions(metal, "R", mul_met, "A")
    MEL.connect_material_expressions(metal_scale, "", mul_met, "B")

    MEL.connect_material_property(mul_col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(nrm, "RGB", unreal.MaterialProperty.MP_NORMAL)
    MEL.connect_material_property(rough, "R", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(mul_met, "", unreal.MaterialProperty.MP_METALLIC)

    MEL.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)


if BUILD_MAT:
    build_material(mat)

# --- Assign to slot(s) ---
mats = list(mesh.get_editor_property("materials"))
for i, m in enumerate(mats):
    m.set_editor_property("material_interface", mat)
    mats[i] = m
mesh.set_editor_property("materials", mats)
unreal.EditorAssetLibrary.save_loaded_asset(mesh)
unreal.EditorAssetLibrary.save_loaded_asset(skel)

# --- Report ---
bb = mesh.get_bounds()
names = [str(n) for n in skel.get_reference_pose().get_bone_names()]
print("[axolotl] %s: bounds origin=%s extent=%s (height %.2f cm)" % (CHAR, bb.origin, bb.box_extent, bb.box_extent.z * 2))
print("[axolotl] bones=%d first=%s mixamorig=%s" % (len(names), names[:1], any("mixamorig" in n for n in names)))
print("[axolotl] slots=%s" % [str(m.get_editor_property("material_slot_name")) for m in mats])
unreal.EditorAssetLibrary.save_directory(FOLDER)
