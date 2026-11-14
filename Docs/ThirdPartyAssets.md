# Third-party assets and credits

## Asset packs

Add one entry per third-party pack used in the project. Include the pack name, creator, source link, where it’s used, and the license that applied when you acquired it. If the pack includes a license or attribution file, add it to `Docs/` and link it from the entry. If you still need to verify the source or license, record it under [Provenance to confirm](#provenance-to-confirm).

Example entry (replace the placeholders)

```markdown
- [Pack name](SOURCE_URL) by Creator. Used for shoreline props in `Map_Saga`. [License name and version](LICENSE_URL_OR_FILE).
```

- [Ultra Dynamic Sky](https://www.fab.com/listings/84fda27a-c79f-49c9-8458-82401fb37cfb) by Everett Gunther. Sky and lighting in `Map_Saga`.

The Megascans assets were imported at the medium quality tier. The game applies its own palette.

- [Nordic Forest Ledge Rock Medium](https://www.fab.com/listings/034fcd08-2e37-407f-9e32-26291a8d18a3) by Quixel Megascans. Mountain slopes.
- [Nordic Forest Cluster Rock Small](https://www.fab.com/listings/f186c225-178a-482b-a122-5508a318a4f0) by Quixel Megascans. Riverbanks.
- [Nordic Forest Tree Stump Birch Medium](https://www.fab.com/listings/140c2265-c446-4080-8f03-dd58299c3f8c) by Quixel Megascans. Ashen forest stumps.
- [Coast African Debris Driftwood S 03](https://www.fab.com/listings/34a31ebb-9cda-484e-8a69-e8ea263b3678) by Quixel Megascans. Burned dock debris.
- [Beach Gravel With Pebbles](https://www.fab.com/listings/8fbcf4f7-a969-46f5-b786-acd5aad0054a) by Quixel Megascans. Shoreline surface.
- [Mossy Ground](https://www.fab.com/listings/aa33d733-44e7-4a7e-945b-be3c443afc18) by Quixel Megascans. The 2K base color and normal add detail to the shared forest and colony ground.
- [Watermills / Nature Environment](https://www.fab.com/listings/a2ab9a3c-4862-421a-8f58-47463695c6ba) by Leartes Studios. Shoreline grass and ferns with Saga materials.

## Other credited content

- **Roboto** by Google. The HUD uses the Roboto font bundled with Unreal Engine. See the [bundled Roboto license](Roboto-LICENSE.txt).
- **Tripo**. The Viking house kit includes Tripo-generated meshes alongside team-built Blender modules. See the [module inventory](VikingHouseKit.md). The Cutsalot warrior villager (`Content/Characters/axolotl_warrior`) is a Tripo-generated mesh, texture set and rig, made from the team's reference art in `Docs/References/FurryAxolotlClasses` with a T-pose sheet drawn in ChatGPT. The archer, druid, blacksmith and jarl heroes (`Content/Characters/axolotl_archer`, `axolotl_druid`, `axolotl_blacksmith`, `axolotl_jarl`) are made the same way, each from the team's reference art with a T-pose sheet drawn in ChatGPT. Their animations are retargeted from the Viking character clips.
- **ElevenLabs**. The repository includes generation scripts for the soundtrack and environmental sound effects.

## Provenance to confirm

- Viking character and weapon assets have import filenames and repository history. The original creator or pack and its license are unverified.
- Imported props and pine variants have Tripo references in package data. Those references alone do not establish the source of each mesh, texture, or embedded audio clip.
- The original account entitlements and license versions for marketplace assets are not recorded here. The listing links identify the products. Acquisition records establish the terms for each copy.
