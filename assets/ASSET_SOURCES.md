# Asset Sources

All selected third-party runtime assets are permissively licensed and copied under `assets/`.

## Kenney Nature Kit

- Source: https://kenney.nl/assets/nature-kit
- License: Creative Commons CC0 1.0 Universal
- Used files:
  - `assets/models/tree_default_dark.obj`
  - `assets/models/tree_cone_dark.obj`
  - `assets/models/tree_fat_darkh.obj`
  - `assets/models/rock_largeA.obj`
  - `assets/models/log_large.obj`
  - `assets/models/fence_gate.obj`
  - `assets/models/tree_oak_dark.obj`
  - `assets/models/tree_pineTallA.obj`
  - `assets/models/tree_pineRoundB.obj`
  - `assets/models/plant_bushDetailed.obj`
  - `assets/models/plant_bushLarge.obj`
  - `assets/models/grass_large.obj`
  - `assets/models/stump_oldTall.obj`
  - `assets/models/stump_roundDetailed.obj`
  - `assets/models/sign.obj`
  - `assets/models/ground_pathRocks.obj`
  - `assets/models/bridge_side_wood.obj`
  - `assets/models/fence_planksDouble.obj`
  - `assets/models/log_stackLarge.obj`
  - matching `.mtl` files beside each OBJ

## Kenney Car Kit

- Source: https://kenney.nl/assets/car-kit
- License: Creative Commons CC0 1.0 Universal
- Used files:
  - `assets/models/sedan.glb`
  - `assets/models/debris-door.glb`
  - `assets/models/debris-tire.glb`

## Toyota RAV4 cockpit baseline

- Source: user-provided local model
- Original file: `C:\Users\andre\Desktop\Organized Desktop\03 Creative Assets and References\Blender\ToyotaRav4\Toyotarav6.fbx`
- Runtime conversion: `assets/models/toyota_rav4_cockpit.glb`
- Conversion tool: `FBX2glTF 0.9.7-p1`
- Used as the first-person vehicle interior baseline. Procedural/diegetic control affordances are layered onto the model so gameplay controls line up with visible car parts.

## Kenney Interface Sounds

- Source: https://kenney.nl/assets/interface-sounds
- License: Creative Commons CC0 1.0 Universal
- Used files:
  - `assets/audio/click_002.wav`
  - `assets/audio/glitch_003.wav`
  - `assets/audio/error_006.wav`
  - `assets/audio/glass_003.wav`
  - `assets/audio/scratch_004.wav`
  - `assets/audio/drop_003.wav`

The WAV files are local PCM conversions of the original CC0 OGG files to avoid runtime decoder issues.

## Kenney Game Icons

- Source: https://opengameart.org/content/game-icons
- Author: Kenney
- License: Creative Commons CC0 1.0 Universal
- Used files:
  - `assets/textures/icons/locked.png`
  - `assets/textures/icons/power.png`
  - `assets/textures/icons/warning.png`
  - `assets/textures/icons/audioOn.png`
  - `assets/textures/icons/gear.png`

## Static

- Source: https://opengameart.org/content/static
- Author: xhunterko
- License: Creative Commons CC0 1.0 Universal
- Used file:
  - `assets/audio/radio_static.mp3`

Original downloaded archives are retained in `assets/source/` for provenance.

## Horror SFX

- Source: https://opengameart.org/content/horror-sfx
- Author: TinyWorlds
- License: Creative Commons CC0 1.0 Universal
- Used files:
  - `assets/audio/jumpscare.wav`
  - `assets/audio/thunder.wav`
- Retained source:
  - `assets/source/horror_sfx.zip`

## Monster SFX

- Source: https://opengameart.org/content/monster-sfx
- Author: marcelofg55
- License: Creative Commons CC0 1.0 Universal
- Used file:
  - `assets/audio/monster_voice.wav`
- Retained source:
  - `assets/source/monster_sfx.zip`

## Rain And Thunders

- Source: https://opengameart.org/content/rain-and-thunders
- Author: kindland
- License: Creative Commons CC0 1.0 Universal
- Used file:
  - `assets/audio/storm_ambience.ogg`

## V2 menu backplates

- Source: generated locally for this project
- License: project-owned asset
- Used files:
  - `assets/textures/menu_v2_backplate.png`
  - `assets/textures/tutorial_v2_backplate.png`

## Custom creature model

- Source: generated locally by `tools/generate_creature_asset.py`
- License: project-owned asset
- Used files:
  - `assets/models/creature_stalker.obj`
  - `assets/models/creature_stalker.glb`
  - `assets/models/creature_stalker.mtl`

## OpenGameArt monster source reference

- Source: https://opengameart.org/content/monster-3
- Author: koe-kto
- License: Creative Commons CC0 1.0 Universal
- Retained source files:
  - `assets/source/opengameart_koe-kto_monster.obj`
  - `assets/models/oga_monster_normalized.obj`

`oga_monster_normalized.obj` is a scale/orientation-normalized derivative kept for future testing. The current runtime uses the smaller custom creature model because it is more stable in the native renderer.

## Quaternius LowPoly Animated Monsters

- Source: https://opengameart.org/content/lowpoly-animated-monsters
- Author: quaternius
- License: Creative Commons CC0 1.0 Universal
- Used files:
  - `assets/models/quaternius_skeleton.obj`
  - `assets/models/quaternius_skeleton_tri.obj`
  - `assets/models/quaternius_skeleton.mtl`
  - `assets/models/quaternius_bat.obj`
  - `assets/models/quaternius_bat_tri.obj`
  - `assets/models/quaternius_bat.mtl`
