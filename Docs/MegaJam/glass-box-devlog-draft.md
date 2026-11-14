# The Spruce at the Gate | Making Cutsalots: Take One

On the first day in *Cutsalots: Take One*, a spruce crashes across the west gate and traps Hakon, the village hunter, outside. Shapes are coming through the fog. Press **CUT!** and you can choose a version of the story in which the tree never grew.

![Overhead view of the torchlit west gate in a curved timber wall, with pine forest outside and village houses inside](../Screenshots/Saga_RoundPalisade_Gate.png)

*The forest road leads through the west gate, where the village makes its stand on the final night.*

## One scene, two consequences

That scene is how we turned the jam theme, **Director’s Cut**, into a decision for our Viking colony sim. **Keep the Living take** and Hakon faces the fog-born outside the gate. At dawn, the village saws up the spruce for 40 wood, and the forest keeps producing timber. **Choose the Ashen take** and the forest burned years ago. Hakon walks home through a clear gate. The village gives up the 40 wood and earns 40% less timber from the forest for the rest of the run.

We built both takes in the same settlement in Unreal Engine 5.9. In the Ashen take, the game swaps the forest to its burned state, clears the blocked tiles at the gate, sends Hakon back inside, and reduces forest income. Saving him changes the rest of the run because the village still needs timber to prepare for the siege.

[Watch the 20-second gameplay clip](The-Saga-Cutter-Spruce-at-the-Gate.mp4). The spruce blocks the west gate, the **CUT!** menu shows both takes, and Hakon walks home after the Ashen cut.

## Building the west gate

The first palisade used separate straight wall sections. We rebuilt it as one curve that runs from the north shore around the west gate to the south shore. The wall had to work at two scales. From the isometric camera you need to see where the forest road enters the village. At ground level the simulation needs to know which cells are blocked and which routes are open. The C++ wall builder follows the curve when placing the logs and marking blocked cells, so the route through the gate follows the wall you see.

![Earlier west gate with a straight palisade](../Screenshots/Saga_WestGate_Front.png)

*An earlier version of the west gate and its straight wall.*

![Overhead view of the curved palisade wrapping around the village](../Screenshots/Saga_RoundPalisade_Top.png)

*The rebuilt wall wraps around the settlement between the two shorelines.*

We connected Claude Code to the Unreal Editor through Unreal MCP. The palisade revision touched both the map and the C++ wall builder. The editor views let us inspect the gate from the forest road and check the full wall from above.

## Getting the player to the first cut

Behind the title screen, a short prologue shows the settlers land, clear the forest, and raise the village. Once you start playing, Day 1 introduces the settlement controls and then frames the west gate when the first cut becomes available. That puts the blocked route and the stranded hunter in view when you choose which take to keep.

## Following the choice to dawn

After the siege, the epilogue checks which take you kept and whether Hakon survived. The skald tells a different version of his story based on those two outcomes.

We checked that connection with an autoplay run that followed a fixed sequence of Living and Ashen takes. We chose Ashen at the gate, and on the final night the village spent 200 wood bracing it. The gate held until dawn. When the skald told the story in the epilogue, he remembered the hunter who walked home through ash and whose bow helped hold the gate.

*Cutsalots: Take One was built for the 2026 Epic AI MegaJam by Sunshine.* The team is [synoecium](https://synoecium.itch.io/), [vidiaikin](https://itch.io/profile/vidiaikin), and [mrleon47](https://itch.io/profile/mrleon47).
