# From a Viking Village Concept to a Playable Settlement

*The Saga Cutter environment devlog · Naidjen · November 12–14, 2026*

The first image of our Viking village had moss-covered roofs, pine trees, and warm lanterns against a cool forest. It also posed a practical question for *The Saga Cutter*: from the high strategy camera, would you be able to pick out the jarl’s hall, the smithy, and the sawmill at a glance?

![Early settlement concept with timber buildings and pine forest](environment-devlog-images/figure-1.jpg)

*The generated concept that set the visual direction for the village.*

I used that image to guide everything from the roofs to the crates and gates. A reusable prompt kept the materials and palette consistent across reference sheets. Big silhouettes mattered most from the gameplay camera. Up close, I wanted rough cloth, weathered wood, and chipped stone, with amber light reserved for fires and lanterns.

## Four roof pieces in one generation

My first set of house parts couldn’t reproduce the shape of the reference. I moved the design into Blender and asked for both an assembled house and a layout showing each piece on its own. The jarl’s hall got a longer footprint and a large feast table. Smaller houses and working buildings each needed a recognizable outline.

I wanted to generate several pieces at once in Tripo. The tricky part was getting both reference images to describe the same set. Early views rotated the parts individually. Moving the camera around a fixed layout preserved their relationship and exposed the underside of the shingles. I arranged the pieces in two rows so the roof sections occupied more of the image, then removed the dimensions and labels from the generation inputs.

![Blender reference sheet with four dimensioned roof modules](environment-devlog-images/figure-4.jpg)

*An early Blender sheet. The Tripo inputs used a tighter layout and omitted the annotations.*

One attempt came in under 2,000 triangles and still warped the pieces. Another produced a better shape for a single roof slope, which would have left us generating the library one part at a time. I adjusted the Tripo inputs directly and accepted a batch of four separate roof pieces. The saved generation report put it at 2,416 triangles. After texturing and export, we fitted the new geometry to the existing Blender modules on a 2.4 m grid. Replacing the shared meshes updated the houses that already used them.

## Keeping the kit editable in Unreal

At integration, the kit had **75 modules across six buildings**, with **1,112 placements** in the layouts. Twenty-six modules came from Tripo and 49 were Blender placeholders. Stable asset names let us replace a placeholder without rebuilding the layouts. In Unreal, the C++ assembler fit the modules to the existing building footprints and handled construction stages and Living and Ashen variants. We also kept separate groups for the shell, removable parts, and interior furnishings. [House kit documentation](../VikingHouseKit.md)

![Working Blender render of three village houses](environment-devlog-images/figure-5.jpg)

*Three houses assembled from the modular library in Blender.*

## Wind, fire, and smoke

The first banner test looked stiff, so I asked for gusts whose direction changed. Cloth self-collision caused a sharp frame-rate drop during testing, so I dropped it and kept the pole collision. I also repaired a faulty flag mesh in 3ds Max before reimport. In the village, the level’s wind drives the pines and pushes chimney smoke. We tied the sun to the game clock so shadows move as the day advances, including during fast-forward.

Small placement fixes mattered from the gameplay camera. I lowered wall-mounted shields to keep windows clear and adjusted brazier flame and smoke independently so I could shrink the fire without losing the plume. The post-cleanup validation checked 66 assets, reported zero failures and zero missing dependencies, and loaded the map.

I set the visual direction, prepared and revised generation briefs, worked directly in Tripo, and reviewed the results at each step. ChatGPT helped make visual references, Codex and Claude Code assisted with Blender preparation and Unreal integration, and Tripo generated meshes from the inputs. Synoecium developed the ocean update that we merged with this work.
