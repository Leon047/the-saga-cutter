# From a Flat Map to a Viking Fjord

*Cutsalots: Take One environment devlog · Synoecium · November 14, 2026*

The first sea in *Cutsalots: Take One* was the top of a dark box. It marked the edge of the settlement, but in the prologue we ask the player to watch a longship sail in and see where the settlers choose to build. I wanted the coast to do some of that storytelling. The new shoreline sits at the foot of a mountain range, with a river coming in from the south and enough sheltered water for the village to have a dock.

![The longship approaching the shore in the prologue](water-and-landscape-devlog-images/Nordic-Final-Prologue.jpg)

*The new water seen from the prologue, when the settlers approach the shore.*

## Making the longship ride the swell

I built the ocean as a procedural mesh in Unreal. The wave motion combines four swells, and I tuned the larger ones to be visible from the strategy camera. Near the shoreline the waves ease off, where the foam does more of the work of showing the water lapping against the beach.

The water material and the boats share the same wave calculation. Each hull reads the water height at four points: bow, stern, port, and starboard. If the wave reaches the bow first, the ship pitches up. If it reaches one side first, the ship rolls. That gave the longship a motion that follows the water you can see. In the shallows, the settlers’ steps kick up small splashes as they come ashore.

![Low view of surf, gravel beach, and the longship at the village dock](water-and-landscape-devlog-images/current-shoreline.jpg)

*Surf breaks along the gravel shoreline beside the longship and dock.*

## Building the mountains around the camera

The settlement already had a grid for its buildings and paths. I kept that ground level and generated the relief around it. The tall, snow-capped peaks sit to the north. On the camera side I kept the wooded hills low, so the terrain frames the village without getting between the player and the buildings. The mountains use broad, flat-shaded faces that fit the same low-poly style as the rest of the map.

![The northern mountain range seen from the settlement](water-and-landscape-devlog-images/Nordic-Final-North.jpg)

*The northern range frames the fjord beyond the settlement.*

On the coast, I blended the meadow and forest colors into a gravel beach and scattered rock clusters, grass, and ferns along the shoreline. I carried the same palette inland, using patches of moss to guide where forest undergrowth appears.

![Wide view of the village between the fjord, mountain range, and southern river](water-and-landscape-devlog-images/current-fjord.jpg)

*The finished fjord from above, with the mountain range behind the village and the river entering from the south.*

## Where the river meets the sea

I carved a winding riverbed south of the village and let the terrain define its banks. Then I split the river into stretches with different flow. In the rocky reaches, the material builds up whitewater and draws wakes downstream. The rocks also give the water something to splash against. Between the rapids, the material uses riverbed depth to tint the quieter pools.

![Close view of green river water and whitewater flowing around rocks](water-and-landscape-devlog-images/current-rapids.jpg)

*Whitewater gathers around the rocks, leaving quieter green pools between the rapids.*

![Overhead view of green water and white foam in a river pool](water-and-landscape-devlog-images/NordicClearPools.jpg)

*Water color makes changes in depth visible in the quieter pools.*

Near the coast I widened the channel, added gravel bars, and eased off the rapids and rock wakes. The river and the beach share the same channel cut, so the mouth opens through the shoreline and the current fans out into the fjord.

![Overhead view of the river delta, gravel bars, and surf at the fjord](water-and-landscape-devlog-images/current-delta.jpg)

*The river widens around gravel bars and meets the fjord at the break in the surf.*

## Hearing the map from above

The camera also drives the ambience mix. Focus on the river and the current comes up. Move across to the dock and the surf gets louder. There’s wind toward the mountains, birds over the woods by day, and occasional owl calls after dark. I used ElevenLabs to generate the sound effects and mixed the loops in Unreal as the camera moves around the map.

*Cutsalots: Take One was built for the 2026 Epic AI MegaJam by Team Sunshine.*

[Ocean implementation notes](../Plans/stylized-ocean.md) · [Landscape implementation notes](../Plans/landscape-mountains-river.md)
