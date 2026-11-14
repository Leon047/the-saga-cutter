# Building the Environment for Cutsalots: Take One

*My work on visual references, model generation, and integration into the game*

Most of the work took place on November 12 and 13 and overnight into November 14, 2026. Visual exploration started on the evening of November 11. All times in the chronology are UTC.

I led the settlement’s visual direction, prepared images for 3D generation, organized the modular house kit, and imported and refined the environment in Unreal Engine. I worked on individual props and on how the scene behaved as a whole, including banner cloth, wind in the pines, the movement of the sun, fire, and chimney smoke.

I used ChatGPT and Codex for images, task briefs, and project work, Claude Code for Blender, browser work, and integration, and Tripo to generate 3D models. I set the direction, defined tasks, reviewed outputs, and made successive corrections. These tools contributed geometry, images, and code. The sections below describe my decisions and how I brought the results into the game.

![Generated concept of the settlement](environment-devlog-images/figure-1.jpg)

*Figure 1. The original visual reference supplied by the author. This is a generated concept. [S01]*

## Contents

[1. Reference](#1-the-original-settlement-reference) · [2. Style](#2-establishing-a-consistent-style) · [3. Props](#3-from-the-wide-shot-to-individual-props) · [4. Vegetation](#4-trees-and-small-plants) · [5. Unreal assets](#5-preparing-assets-in-unreal) · [6. Cloth](#6-banners-and-cloth-physics) · [7. Architecture](#7-why-the-houses-needed-to-be-modular) · [8. Input sheets](#8-preparing-image-sheets-for-tripo) · [9. Tripo](#9-tripo-settings-and-failed-attempts) · [10. Geometry](#10-textures-export-and-geometry-replacement) · [11. Construction](#11-integrating-the-kit-into-the-construction-system) · [12. Wind and sky](#12-wind-sky-and-moving-shadows) · [13. Camp props](#13-shields-torches-and-the-brazier) · [14. Smoke](#14-chimney-smoke-and-asset-cleanup) · [15. Contribution](#15-my-contribution-and-the-role-of-the-tools) · [16. Chronology](#16-chronology) · [17. Evidence](#17-verified-results-and-limits) · [18. Sources](#18-work-history-sources) · [19. Commits](#19-commits-for-cross-checking)

## 1. The original settlement reference

The first reference was a wide view of a Viking village. It already brought together the elements of the environment: mossy shingled roofs, wooden houses on stone foundations, carved heads on the roofs, conifers, fortifications, banners, an altar, a dock, a vegetable garden, and warm lights against a cool forest. It gave us a world in which to judge the appearance of a barrel or a wall.

On the evening of November 11, I iterated on the composition in the Codex chat “Create a shot from the game.” I asked for a higher, more top-down camera, a larger village, and more realistic world proportions while preserving stylized characters. I asked to simplify the interface to the resources and quick-build menu. I also explored morning light, evening campfires, and a settlement extending deeper into the forest. [S01]

I initially tried showing a film set, with camera operators and people holding microphones at the edges of the frame. I dropped the visible crew and artificial lighting. These were experiments in presentation. The settlement itself, its materials, and how clearly it read from a high camera became the priorities.

On November 12 I attached the wide reference when requesting simple props such as crates, barrels, chairs, and stones. I reused it as a style reference for house modules. During one browser session Claude used a saved village image with an interior because the other attachment was unavailable in that session. [S03, S07]

## 2. Establishing a consistent style

At 08:42 on November 12, I asked for a JSON prompt to produce a series of high-quality images in one consistent 3D style. The template separated each object description from a reusable style_lock block for art direction, form, materials, palette, lighting, camera, and consistency rules. [S02]

The direction was stylized realism for dark medieval woodland fantasy. Large shapes needed to read from a strategy camera. Wood was weathered, metal matte, stone chipped, and fabric coarse. The palette stayed muted with browns, grays, olive, and moss green. Warm amber light came from fires and lanterns. The generator was asked to show complete objects and omit stray text and game UI.

I carried that template into the next chat and reviewed the actual images each time. Two successive notes were “make the graphics less childish” and “it needs more cuteness.” I wanted a slightly storybook quality with believable materials and proportions. [S03]

For important assets I added visual references, specified the angle and materials, and asked for revisions. I checked silhouettes, relative sizes, detail density, and whether the image would make a usable input for the next stage. A good-looking render could still make a poor input for generating a kit.

## 3. From the wide shot to individual props

I broke the environment into small groups. For the campfire I requested laid firewood ready to light, a burning fire, and a spent fire with burned wood. I then requested an altar and two banner variants. I asked for rear and side views of the altar and flags and specified a consistent motif on the flags. [S03]

In the first simple prop pack chat, I stopped an attempt to make geometry and clarified, “just make an image.” The first sheet showed 12 items: logs, firewood, a stump, stones, a crate, a barrel, a stool, an axe, mushrooms, a basket, a fire pit, and a tent. Next I requested eight larger objects. [S04]

The objects had to retain their relative scale across a sheet. After reviewing eight stone variants, I narrowed them to three sizes and asked for more distinct silhouettes: a massive angular boulder, a low layered slab, and a rounded pebble.

![Generated sheet of eight larger props](environment-devlog-images/figure-2.jpg)

*Figure 2. The second generated prop sheet. The images served as references for later work.*

I continued with mushrooms growing from the ground, fences, and garden crops. For the fortifications I requested three fence modules and separate large gates. I asked to split a gate leaf at the hinges, remove one leaf, and detach the remaining leaf from the frame so it could move. [S03]

When the rear view disagreed with the front, I asked to move the door to the other side and reminded the agent how the logs were fastened. Both views needed to describe the same structure for 3D generation. The split references captured the intended construction for a moving gate.

## 4. Trees and small plants

I requested multiple conifer silhouettes. I wanted the early variants to feel more ominous through their shape as well as their color, then brought an exaggerated revision back toward the intended style. [S03]

To save polygons I asked for simpler branch shapes with denser coverage. I also requested a taller, more realistic tree, a sparse tree with a long trunk, and a small variant. The repository later gained additional pine variants and placement support. [S03, G03]

In another chat I explored bushes and plants that fit the Viking setting. The first results felt too miniature, so I asked for fuller and more realistic growth, followed by three angles and a top view. The source records these images as preparation and leaves individual variants’ final map placement unverified. [S05]

## 5. Preparing assets in Unreal

I requested systematic names for meshes, materials, and textures for the altar, furniture, crates, plants, fences, gates, and pines. I clarified that four plants were distinct objects, explained the purpose of the sprout called Object019, and asked for one group’s textures to be reduced to 1024. [S06]

Descriptive names made imported assets easier to identify and reuse. Git records separate additions for banners, the altar, crates, furniture, plants, mushrooms, fences, gates, and pines. I asked for finished groups to be shared through the repository as work progressed so teammates could use them in batches. The chats document generation briefs, preparation, naming, and integration. [G01, G02]

## 6. Banners and cloth physics

I asked for cloth physics, a wind test scene, and pole collision. The first cloth motion looked unrealistic, so I requested intermittent wind with changing directions. [S06]

I initially wanted to prevent the front and back of the cloth from intersecting. When frame rate fell sharply, I prioritized a cheaper simulation and asked to disable cloth self-collision.

The third flag had a separate defect. A point stayed low, then the banner jerked as if the simulation had restarted. I fixed the mesh in 3ds Max and asked for it to be checked and reimported. I also asked to enlarge pole collision and specified 3 cm in one iteration.

After testing, I asked to transfer the flags and wind to the target level, strengthen the gusts, and add sound. I specified a seamless 15-second loop of heavy fabric with irregular rustling and dull snaps between gusts, without voices or music. I later adjusted the audible radius. On Map_Saga I refined banner scale and orientation in the settlement. [S06, S11, G01]

## 7. Why the houses needed to be modular

On November 13 I returned to the architecture of the original concept. I requested a parts list and reference images with four modules each, followed by a house assembled from those pieces. The proposed parts could not reproduce the reference silhouette, so I asked for a modeling brief for Claude. [S03]

I also commissioned four views of a closed house. The original concept omitted the front wall to reveal the interior. I asked for a facade consistent with the existing architecture, then added a stone chimney with smoke. The four-view sheet established the design before the task moved to editable Blender geometry. [S08]

![Four generated views of a house with a chimney](environment-devlog-images/figure-3.jpg)

*Figure 3. Generated architectural reference for the closed house after the chimney was added.*

In the Blender brief I prioritized architecture, proportions, and silhouette. I asked for a complete house and for all its modules to be laid out separately. That made the kit easy to inspect and gave us a basis for preparing inputs for generation. [S07]

I asked for a longer jarl’s hall with a large central feast table, then two smaller houses, with one door per house. The set expanded to a smithy, sawmill, warehouse, and dock. I wanted the working buildings to have recognizable silhouettes without labels. [S07]

## 8. Preparing image sheets for Tripo

To avoid running the process for each of roughly a hundred pieces, I proposed placing three to eight modules on each image depending on their size. Each group needed two views that showed its construction. I started with a test sheet for the roofs. We called these multi-object sheets “atlases” in the chat. [S07]

The agent initially rotated the models for the second view. I asked it to move the camera around a fixed arrangement. The objects then changed positions naturally in the frame while retaining their spatial relationship. I requested a wider orbit so the underside of the roof cladding would be visible and the slopes would read as volumes.

The technical Blender renders included dimensions and annotations to communicate the construction to ChatGPT. For the final Tripo inputs I asked to preserve the shapes, camera angles, and arrangement while removing labels, dimensions, and long shadows. I asked for materials in the village style, including worked wood, shingles, moss, and soft colors. [S07, S09]

![Dimensioned Blender sheet of roof modules](environment-devlog-images/figure-4.jpg)

*Figure 4. Technical roof sheet before the layout was tightened and annotations were removed for Tripo.*

The first layout left too much background around one row of small objects. I asked for two rows with the parts as large as possible, separated and fully in frame. The added image area made thickness and construction easier to read. [S09]

## 9. Tripo settings and failed attempts

For the roof set I targeted about 1,500 polygons and set an approximate ceiling of 2,500, with textures disabled for the first pass. The browser history records Texture Off, Triangle topology, and a working limit of 2,000 for specific attempts. [S07]

We tried a single image, then Multi View with the two sheets in the Front and Back slots. The agent reported 1,954 triangles and 985 vertices, but the result was distorted. I flagged inconsistencies between the source images and requested a fresh attempt.

The agent then produced four orthographic views of one roof slope. This improved its shape but lost the batch workflow. I reiterated that the sheet needed four models and asked for a return to multiple separate parts with consistent views. [S07]

The side views of the complex set continued to cause problems. I tried leaving both side slots empty and then uploaded a thin strip to the Right slot myself. That appeared to help. Claude’s next report described four distinct parts with no extra layer, 2,416 triangles, and 1,215 vertices. It used ChatGPT’s Front and Back sheets, my strip on the right, and an empty Left slot. I accepted that result. The counts come from the saved report for that attempt.

I worked through the agent and directly in Tripo. Later I generated a wall set myself, passed along export names, and asked the agent to use my latest model. I managed the handoffs and intervened when the result drifted from the brief.

## 10. Textures, export, and geometry replacement

Once I accepted the roofs, the work log records Smart UV, 4K texturing, Remove Lighting, PBR, and export. VikingRoofs_Set01 contained an FBX and base color, normal, roughness, metallic, and RM maps. [S07]

The parts were matched to the Blender modules ROOF_Slope_Bay, ROOF_Slope_Strip, ROOF_Eave_Flare, and ROOF_Eave_Strip. The integration script wrote replacement geometry into existing mesh datablocks, retaining assignment and orientation. Linked instances in the houses and library updated together. The replacements were fitted to the original bounds and 2.4 m grid. [S12]

I requested a render with normal and base color maps, asked to remove the moss as shown, and asked to retain Tripo’s smoothing. The agent reported transferring polygon smooth flags. Preservation of all FBX custom normals remains unverified.

I wanted wall timber with more volume. I rejected flat boards, requested a square sheet with the right-hand pieces moved to the top, and asked for slightly hewn logs. Doors and windows needed open apertures. [S07]

The wall export was initially mistaken for a pine file, so the archive had to be located by the name I supplied. The Tripo scale also differed from the modular grid. Two tiny stray fragments were removed. Pieces were matched by size and filled area, which distinguished a window wall from a solid one regardless of their order in the FBX.

Six WALL modules were replaced, at about 3,870 triangles in the saved report. The foundation and frame were updated similarly. The frame had eight parts and about 3,300 triangles, and its export contained only base color. Material map coverage varied across the kits. [S07, S12]

![Blender render of three assembled houses](environment-devlog-images/figure-5.jpg)

*Figure 5. Working Blender render showing how the geometry and materials evolved from the concept.*

## 11. Integrating the kit into the construction system

At 22:09 on November 13, I asked Claude to export the finished library from Blender and replace the old houses in Unreal. Some modules came from Tripo and others were Blender placeholders slated for replacement. I asked for names that would survive those updates. [S10]

The work covered asset export and C++ building assembly. Stable SM_VH identifiers used the module name, while data and metadata recorded provenance. A procedural piece could be replaced by a Tripo mesh with existing references intact.

At this stage, the kit documentation recorded 75 unique modules, comprising 26 from Tripo and 49 Blender placeholders, along with six buildings and 1,112 layout placements. These figures predate the later shield additions. Positions and dimensions were converted from Blender meters to Unreal centimeters with axis conversion. [S12, G05]

VikingHouseModules recorded sources, paths, and module dimensions. VikingHouses described building variants. VikingHouseLayouts stored the group, module, and transform for each placement. The C++ assembly fitted the kit to existing building footprints and accounted for construction stages and Living or Ashen appearance. The old geometry remained as a fallback when kit data was unavailable. [S12, G05]

After the first report I raised the risk that interiors had been skipped. The data retained Shell, Removable, and Interior groups for the exterior, removable pieces, and furnishings. These groups prepared the assets for independent access. A playable interior cutaway remained unverified.

![Blender interior render with table, hearth, and beds](environment-devlog-images/figure-6.jpg)

*Figure 6. Working Blender render of the interior, including the central table, hearth, and sleeping places.*

## 12. Wind, sky, and moving shadows

On November 13 I asked to move the wind source from the test level to Map_Saga, connect the pines to it, and make the sway more visible. The code passes wind strength and direction into material parameters so the trees respond to the level’s wind. This extended the work begun with the banners. [S10, G03]

I asked to transfer Ultra Dynamic Sky from another Unreal project of mine and hook it into the game. I adjusted fog, cloud shadows, and lighting. The first cloud shadows were too small and high-contrast, so I asked for larger, softer patches. [S10]

The clock advanced from morning to night, but the sun’s movement was hard to read. I asked for shadows to move through the day and follow the fast-forward speed. I also reduced fill light. Git records the connection between the sky and game clock. The source provides no comparative lighting or performance measurements. [G04]

## 13. Shields, torches, and the brazier

Late on November 13, I asked ChatGPT for seven Viking shields with different traditional patterns and separate front and back views. I then requested a metal brazier made of panels with cutouts and removed the fire from the reference so the engine could supply the effect. [S13]

In Unreal I used names such as SM_Shield_01, M_Shield_01, and T_Shield_01. I requested shields inside and outside buildings and a few scattered around camp as if someone had left them there. After the first placement pass, I asked to hang them lower and keep windows clear. The data tables gained seven shield variants and building placements. [S14, G06]

For the torches I selected 1_TorchFire_pt and requested its dependencies. Blueprint variants attached it to SM_Torch_01, SM_Torch_02, and SM_Torch_03. I pointed out old instances on the map and asked to replace them with BP_Torch_01.

For the brazier I selected Fire_03 and requested that Fire_00 also be brought into VFX. I noticed the smoke had become too small, then made a separate request to shrink the flame. Adjusting the two independently let each keep its intended scale. I also asked to remove the generated light sources. The VFX commit records the change to the function that added them. [S14, G07]

## 14. Chimney smoke and asset cleanup

I wanted a narrow white plume rising high and fading gradually. The first screenshot showed red smoke and visible flame elements. I flagged that and asked for the plume to respond to wind. [S14]

I also asked Claude to adjust visibility distance and move the whole plume throughout its lifetime. I requested a trail reaching almost to the clouds, with a 20–25 second particle lifetime. I experimented with the smoke shadow threshold, then asked to disable its shadows. The text reports document those iterations. The final appearance of the white plume still requires visual verification. [S10]

![Interim screenshot with red smoke and a red annotation](environment-devlog-images/figure-7.jpg)

*Figure 7. Interim screenshot supplied with the red-smoke feedback. The red line is an annotation.*

The code attaches smoke to chimneys on completed houses and offsets particles using smoothed wind with gusts. I then requested removal of the original M5VFXVOL2 folder while keeping the working assets under VFX. A local validation report recorded 66 checked assets, zero failures, zero missing dependencies, and a loaded map. The report covers asset and map loading. Visual quality and frame rate require separate checks. [S15, G07]

I saved the camp dressing and VFX work in Git and asked for it to be merged with my teammate’s ocean update. Git records a separate environment commit followed by the merge. Synoecium developed the ocean update. [G07, G08]

## 15. My contribution and the role of the tools

I led the visual and technical work from reference to in-game asset. I chose the reference, set the style and contents of the kits, specified how pieces should fit, accepted or rejected generations, and reviewed camera angles, materials, scale, topology, and placement. I redirected work toward batches of parts, timber with volume, clear windows, and independent controls for flame and smoke.

ChatGPT and image generation helped make and revise references. Claude Code and Codex handled browser operations, Blender scene preparation, import, code changes, and project checks through the tools available to them. Tripo produced models from the prepared images. Blender was used to assemble, match, and replace geometry, and Unreal was used to review assets in the game.

I set requirements and evaluated the result at each handoff. I edited Tripo inputs and generated some sets in the service myself, then delegated export and integration. My teammates contributed game systems, onboarding, music, characters, weapons, and the ocean alongside this environment work.

## 16. Chronology

All times are UTC.

- **November 11, evening.** Explored the wide view, camera height, world scale, UI, and morning and evening lighting. [S01]
- **November 12, 08:42.** Requested the JSON style template. The morning covered campfires, altar, flags, and additional views. During the day I worked on cloth, wind, audio, and moving the banners to the map. After 14:45 I made prop sheets from the main reference, followed in the evening by fences, gates, and tree variants. [S02–S06]
- **November 13, 11:48–15:41.** Requested a modular parts list at 11:48, four views of a closed house at 13:57, and editable Blender geometry at 14:45. After 15:41 I requested the longer jarl’s hall, feast table, and smaller and working buildings. [S03, S07, S08]
- **November 13, 16:49–18:00.** Iterated on the multi-module sheets, camera orbit, roof undersides, sizing, and tighter layout from 16:49 to 17:12. Worked through Tripo settings after 17:12 and accepted the corrected roofs around 18:00. [S07, S09]
- **November 13, after 18:00.** Textured, exported, and replaced the roofs, followed by walls, foundation, and frame. Requested Unreal import at 20:50, Ultra Dynamic Sky transfer at 21:08, and construction-system integration at 22:09. [S07, S10]
- **November 14, 00:20–01:12.** Named shields, worked on torch Blueprints and shield placement, and adjusted the brazier, effects, and generated lights. After 01:17 I iterated on plume color, wind, and visibility. After 06:16 I requested VFX cleanup and validation. [S14, S15]

## 17. Verified results and limits

Files and Git records support the environment assets, pine variants and wind parameters, modular house kit and layouts, sky integration with the game clock, shields, torches, brazier, and selected VFX. The Blender scripts show how geometry was replaced in a shared library. Kit documentation records module sources and counts at integration.

The correspondence records rejected generations, changing settings, direct intervention in Tripo, and review criteria. Triangle counts for individual attempts come from saved Claude reports. The 1,500–2,500 roof target applied to the roof set. The exports varied in material map coverage.

Total Tripo credits, exact hands-on time, and final game performance remain unverified. Reference creation, model generation, import, and in-game review are distinct milestones. Figure captions identify the source images, working renders, and interim screenshot.

## 18. Work history sources

- **S01.** Codex, “Create a shot from the game,” 01a1cc8a-99f5-7ec2-9558-3a47f1adefe0. November 11 requests about the wide view, camera, proportions, film crew, interface, and time of day. The exact generation message for file ea0e96fe was unidentified in the available excerpt. The author supplied the file as the starting reference.
- **S02.** Codex, “Create a JSON prompt for a consistent 3D style,” 01a1ceeb-a0e9-7180-b9aa-d6fa17cf1fb0. November 12 request at 08:42 and response containing style_lock.
- **S03.** Codex, “Create woodland fantasy asset,” 01a1ceed-bd4d-71a0-b915-90216e9a3c35. Local history covering stylization, campfires, altar, flags, props, fences, gates, trees, and the first modular house tasks.
- **S04.** Codex, “Create the first pack of simple props,” 01a1cedc-f655-7df1-bc9e-d95b51be7f02. First prop sheets, relative proportions, and stone silhouette revisions.
- **S05.** Codex, “Create woodland fantasy asset (2),” 01a1d522-be18-7da3-ab0a-14a469a60d66. Bushes, scale and fullness revisions, and additional views.
- **S06.** Codex, “Set up medieval_banner physics,” 01a1cf63-6bbd-7721-bd65-3b16169dcaf7. Local history of cloth, collisions, gusts, the author’s 3ds Max mesh fix, audio, map transfer, and asset naming.
- **S07.** Claude Code, C--Linch-game-x-UE project, session 2e65a088-b5e0-48b0-b5ad-ad04acee923a. November 13 requests and reports on Blender, sheets, ChatGPT, Tripo, settings, failures, exports, and geometry replacement. Blender construction tasks also appear in session 91ffe4b7-ecb7-4ccd-9937-9976b32f91d3.
- **S08.** Codex, “Create views of the house from all sides,” 01a1d532-9ac8-77c1-a721-8704fe1da16b. Four views and chimney.
- **S09.** ChatGPT, “Roof image description,” 6af7431c-52c0-83ed-b69f-3dcbca7057c3. Full two-view and tighter-layout prompts. The chat-reading tool returned the text of the requests.
- **S10.** Claude Code, C--Linch-game-x, session 1ffd7240-e7bf-4f38-ab8c-ec836484e59b. Sky, cloud shadows, game-driven sun, house integration, interior feedback, and smoke iterations. Pines and wind also appear in session 91ffe4b7-ecb7-4ccd-9937-9976b32f91d3.
- **S11.** Codex, “Find the sound of a flag in the wind,” 01a1cfc9-88ba-7e43-aa05-aae34cd6d3b7. Request and audio prompt.
- **S12.** Local files VikingHouseKit.md, VikingHouseModules.tsv, VikingHouses.tsv, and VikingHouseLayouts.tsv, plus integrate_tripo_roofs.py and integrate_tripo_walls.py in the Blender workspace. Counts also use the integration commit’s documentation to capture that point in time.
- **S13.** ChatGPT, “Redrawing four views,” 6af7493f-a414-83eb-bf26-3c71f34f3590. Seven shields, patterns, front and back views, metal brazier, and removal of fire from the image.
- **S14.** Codex, “Add M5 Fire and Flames to game-x,” 01a1d754-2e42-76d2-830f-625007a87896. Shields, torches, brazier, particle selection, smoke, and asset cleanup. The original account continued this chat.
- **S15.** Local github_update_validation.json. Status complete, 66 checked assets, empty failed and missing_dependencies lists, and map_loaded true. It records loading after cleanup and merge.

## 19. Commits for cross-checking

- **G01.** Naidjen, November 12. [Banners and initial environment assets](https://github.com/Leon047/the-saga-cutter/commit/0f2fbcd776225e0f7cd1a90dd4496df8665a8150)
- **G02.** Naidjen. [Altar](https://github.com/Leon047/the-saga-cutter/commit/02601902e5e0ddb7b26426937c989e0118053527), [crates](https://github.com/Leon047/the-saga-cutter/commit/a39d6e012b15d5a782c55304f0581bdc5cb4911b), and [gates](https://github.com/Leon047/the-saga-cutter/commit/e878cc8a70648e7fafc04ef4b25ec4dc4264610e)
- **G03.** Naidjen, November 13. [Pine variants and wind](https://github.com/Leon047/the-saga-cutter/commit/db234c44f515c5bd574a3f1dd15a319a07680a78)
- **G04.** Naidjen, November 13. [Ultra Dynamic Sky and the game clock](https://github.com/Leon047/the-saga-cutter/commit/bda3f04d80037414f7b36a61a74c481e7954da44)
- **G05.** Naidjen, November 13. [Modular house kit and assembly](https://github.com/Leon047/the-saga-cutter/commit/973ef839bd2609f2bd98a609eb077fcb9e8aaa39)
- **G06.** Naidjen, November 14. [Shields](https://github.com/Leon047/the-saga-cutter/commit/1868d901fe3ed834d4b57c6767ee0233aa5649ba)
- **G07.** Naidjen, November 14. [Camp dressing, torches, and wind-driven smoke](https://github.com/Leon047/the-saga-cutter/commit/27dfc379a8816a46aa669c228be05ca198261045)
- **G08.** [Naidjen’s merge of the ocean and local work](https://github.com/Leon047/the-saga-cutter/commit/b54fc8641b63fe613d66b0ec377edadb19a5e850) and [Synoecium’s ocean update](https://github.com/Leon047/the-saga-cutter/commit/fc90199edb10cccb5384e4649a4fc7da2edcf8d4)
