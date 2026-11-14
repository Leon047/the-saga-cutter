# Cutsalots: Take One — game jam build brief

## Goal
Build a small, complete, playable Viking settlement sim in this Unreal Engine 5.9
project: one forest clearing by the sea, three days and one final night, about
10 minutes per playthrough. A finished short game beats a larger unfinished one:
when in doubt, cut scope, not polish.

## Pitch
Your Cutsalot settlement exists in two possible sagas, and you
switch between them to edit the events that decide your people's fate.

- The Living Saga: a misty, overgrown forest, rich in timber, food and glowing
  mushrooms, but crawling with the things that grow in the fog.
- The Ashen Saga: the same land after fire and war. Open paths, ruins, stronger
  fortifications, little left to harvest.

Both sagas share one map layout. Every tree, gate, building and shoreline spot
has a Living version and an Ashen version.

## Geometry rule: primitives only, no meshes
Do not import, model or generate custom static or skeletal meshes. Build every
object in the game as a set of engine primitive shapes (cube, sphere, cylinder,
cone, plane) grouped into one actor or Blueprint, scaled, rotated and coloured
to form a readable silhouette.

- Each object is a reusable Blueprint made of primitive components, so the same
  house, tree or Viking can be placed many times and swapped between sagas.
- Detail comes from composition, colour and light, not from polygon count.

Two visual layers, deliberately different:

Buildings and constructions are 2.5D and made of tiles.
- The whole settlement sits on one square tile grid. Every house, the longhouse,
  sawmill, healer's hut, shrines, palisade, gate, garden fence, dock and
  construction plot occupies a whole number of grid cells.
- Each of them is assembled from flat tile pieces: planes or very thin boxes of
  one grid-cell size, snapped to the grid. Floor tiles lie flat, wall tiles
  stand upright on cell edges, roof tiles are tilted planes on top.
- A building is a data-driven stack of such tiles (floor, wall, wall-with-window,
  door, roof, roof-ridge), so new buildings are made by listing tiles, not by
  sculpting shapes. Lit windows are emissive tiles.
- Tiles have no real volume or interior depth: they are drawn for the fixed
  isometric camera and are never seen from another angle.
- The Living and Ashen versions of a building use the same tile layout with
  different tile variants (intact or burnt, mossy or charred, missing roof tiles).
- The palisade is a row of upright tiles along cell edges; a fallen tree that
  blocks the gate occupies grid cells like any other construction.

Characters and animals are full 3D.
- Villagers, raiders, mushroom creatures and any animals are volumetric figures
  built from solid primitives, lit and shadowed in 3D, and they move freely
  across the grid rather than cell by cell.
- Characters are primitive figures: capsule or cylinder body, sphere head, cone
  or half-sphere helmet, flat cylinder for the round shield, thin box for the axe.
- Mushroom creatures: a flattened sphere cap with emissive spots and several
  thin cylinders hanging down as tendrils.
- Trees and rocks are natural scenery, not constructions, so they are also 3D:
  a tree is a cylinder trunk with two or three stacked cones, and its Ashen
  version is a short dark cylinder stump with no cones. Each still stands on a
  grid cell so it can block or free a path.
- Animation is done by moving, rotating and scaling the primitive parts (bobbing
  walk, swinging axe arm, drifting tendrils), not with skeletal animation.
- Use simple flat-coloured materials, with emissive ones for fire, windows,
  candles and glowing mushrooms. No textures are required.
- Fog, lighting, particles and UI are not restricted by this rule.

## The settlement (match the concept image)
A dense, hand-crafted diorama seen from a fixed isometric camera:
- Centre: a great longhouse with carved dragon-head gables, an open hearth,
  tables, shields and beds.
- Around it: a sawmill with log piles, several small turf-roofed houses, a fenced
  vegetable garden, a healer's hut hung with drying herbs.
- Two sacred sites: a hammer rune stone with banners and candles to the north,
  and a stag rune stone wrapped in roots to the south. Villagers pray at both.
- West: a palisade of sharpened logs with a gate, facing the fog forest.
- East: a rocky shore, a wooden dock with crates, and a moored longship.
- One marked empty plot (dashed outline, hammer icon) where the next building goes.

## Threats (two fronts)
- From the forest, west: pale glowing mushroom creatures with dangling tendrils
  drift out of the fog and attack the palisade.
- From the sea, east: raiders land at the dock.
Defenders are small shield-and-axe Vikings. Every fighter shows a health bar
above their head: green for yours, red for enemies.

## Villagers
Population of about 14, capped by housing (HUD shows "14/18"). Most are anonymous
workers, but four are named, with one specialty each (for example hunter,
woodcutter, healer, shield-bearer). Edits and the epilogue revolve around those
four. Villagers show their needs as speech-bubble icons over their heads: log
(needs wood), leaf (tending crops), green cross (needs healing).

## Economy
Exactly two resources, shown top-left with their income rate ("112 +7"):
- Wood: from the sawmill and log piles.
- Food: from the garden and foraged mushrooms.
Plus the population counter.

## Buildings (bottom build bar, five buttons)
1. House: raises the population cap.
2. Sawmill: wood income.
3. Healer's hut: heals wounded villagers.
4. Hammer shrine: blessing for defenders.
5. Stag shrine: blessing for the forest and harvest.
Placing a building creates a construction plot that villagers then build.
Buildings snap to the tile grid and go up tile by tile as construction progresses.

## Core mechanic: cutting a scene
At scripted key events the player can press "CUT!".
1. The world freezes.
2. A film-strip timeline appears showing the current scene and its alternate take.
3. The player picks a take and confirms the edit.
4. The world visibly transitions to the other saga's version of that scene, and
   the simulation resumes with the consequences applied permanently.

Reference sequence (this must work exactly, and is the contest video):
In the Living Saga a woodcutter fells a tree. It falls across the west gate,
trapping the hunter outside as mushroom creatures drift out of the fog. The
player presses "CUT!" and picks the Ashen take: there the tree burned down years
ago, so the gate is clear, but that timber can never be harvested. The forest
turns to scorched ground, the hunter gets home safely, and the settlement carries
on with less wood. On the final night, the rescued hunter holds the gate.

Design rule, non-negotiable: every edit solves one problem and creates or keeps
another. An edit must never be a free "undo mistake" button. Edits are limited
(start with 3 for the whole game; make it a tunable value).

## Game structure
- Day 1, Settling in: assign villagers to food and wood. An early low-stakes
  accident teaches the player to switch takes.
- Day 2, Growing the village: choose what to build on the empty plot. A forest
  event forces a choice between saving a named villager and keeping a resource.
- Day 3, Preparing: a warning announces that the fog is thickening and sails
  have been sighted. The player finishes preparations and spends remaining edits.
- Final Night, the Director's Cut: a short, mostly automatic finale. Creatures
  hit the palisade while raiders hit the dock. Buildings, shrines, saved people
  and edited scenes determine what survives.
- Epilogue: a skald narrates the player's version of the saga: who lived, what
  was lost, what the settlement became. It must reference specific, recognisable
  player decisions, not a generic win/lose screen.

## Art direction
- Mood: cold blue-grey mist and dark spruce forest against warm orange firelight
  from windows, torches, candles and the longhouse hearth. Dusk, wet ground.
- Palette stands in for materials: weathered brown timber, moss green roofs,
  grey stone foundations, bioluminescent green-white mushrooms in the forest.
- Characters: small, stocky, readable silhouettes with helmets and round shields.
- UI: dark carved-wood bars with iron corner brackets and Celtic knot ornaments;
  resources top-left, build bar bottom-centre with square iron-framed icon buttons.
- The Living to Ashen transition is the visual centrepiece: colour drain, tree
  cones vanishing to leave charred stumps, embers, ash, mushrooms going dark.

## Out of scope
No custom or imported meshes, no skeletal animation, no tech tree, no manual
combat control, no procedural maps, no save system, no third resource, no more
than five building types, no more than three or four editable scenes in total.

## Build order
1. The tile grid and the clearing in both saga states, with tile-built 2.5D
   constructions and 3D primitive characters, and a working swap between them.
2. The Day 1 tree, gate and hunter event with "CUT!", timeline UI and consequences.
3. Job assignment and the two-resource loop with the HUD.
4. Build bar and construction on the empty plot.
5. Day 2 and Day 3 events, then the two-front Final Night.
6. Skald epilogue assembled from the recorded decisions.
After step 2 the game must already be demo-able; keep it runnable at every step.

## Done means
A player can start, play three days and the final night, use edits with real
trade-offs, and get an epilogue that reflects their choices, with no editor
intervention and no blocking bugs.
