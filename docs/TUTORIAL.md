# Linear Drive Tutorial

Linear Drive is a first-person survival-horror driving game. Your goal is to reach the radio tower without letting the car get quiet long enough for the entity to enter.

## Core Rule

When an entity is active, slowing below 12 MPH fills the SLOW RISK meter. If it fills, the entity closes the distance, jumpscares through the windshield, and kills you.

At blocked roads, reversing counts as escape. Sitting still or failing to reverse fills the same risk meter.

Tree impacts now matter. Leaving the road at speed and hitting a tree can break the car immediately and trigger the jumpscare kill sequence. A slower impact can still stall the engine, damage the car, and give the monster a chance to close in.

## Driving

- W: accelerate
- S: brake, or reverse when stopped
- A/D: steer
- Space: handbrake
- Q/E or right mouse drag: look around
- Esc: pause

Stay near the road center. Leaving the road damages the car and can kill the run.

## Defensive Controls

Use the physical Toyota RAV4 cockpit first. Hovered or urgent controls outline in the cabin, and clicking the actual mounted part performs the action. Keyboard backups remain available:

- Shift: hold high beams
- H: horn
- R: ignition restart
- T: radio
- L: door lock
- B: headlights
- V: wipers
- F: dome light
- G: gear shift
- Tab: mirror glance

High beams and horn can repel some approaches, but both depend on battery and timing. Speed is the most reliable defense.

## Cockpit Skill Systems

- Toyota cabin baseline: the windshield, pillars, seats, dash volume, and cabin proportions come from the supplied `Toyotarav6.fbx` model converted for the engine. The horror controls are mounted as readable gameplay affordances inside that interior.
- Calibrated driver view: the cockpit camera is placed from the car's 3D coordinate rig, with the Toyota front aligned to the road and each clickable control projected from a cabin anchor instead of a screen overlay.
- Loose radio: bumps, thunder, monster voice events, and impacts can rattle the radio out of its bracket. The steering will start to pull. Keep the lane with A/D, then click the radio to stabilize it.
- Shifter slip: impacts and console surges can knock the shifter toward neutral. Acceleration will fade or block. Click the shifter to re-seat it.
- Door lock lies: monster pressure can jam the lock or make it look locked when it is not. Click the lock during door attacks, and use the mirror when a cue feels suspicious.
- Ignition stalls: when the engine dies, restart with R or the ignition key, then re-seat the shifter if the console is still loose.
- Feedback: successful counters reduce tension and push the monster back enough to recover, but only if you respond during the warning window.

## UI

- OBJECTIVE: current chapter goal and immediate action
- Center route bar: progress to the radio tower
- CONDITION/FUEL/BATTERY/HEADLIGHTS: vehicle survival systems
- ENTITY: current monster variety and proximity when visible
- SLOW RISK: lethal timer that fills when the car gets too slow during a hunt
- COCKPIT FAULT: active radio pull or shifter slip timer
- Monster speech: red subtitle panel when the radio or cabin carries a distorted voice
- Settings: master, music, and SFX sliders apply live from the main menu

## V2 Route Zones

- Forest Road: dense procedural trees, tree collision danger, early watcher events
- Storm Road: rain, lightning, thunder, wet steering, headlight/radio faults
- Abandoned Town: procedural buildings, dark windows, traffic lights, fake directions
- Service Strip: gas pumps, signs, barricades, closer monster pressure
- Tower Works: industrial props, utility poles, heavier fog, late-game voice events

## Storms And Towns

Rain makes the road slick and drains more power from the car. Lightning can twitch the headlights and spike the radio. In town, low speed is especially dangerous because the buildings give the entity more places to appear.

## Monster Varieties

- Pale Stalker: the main road hunter, fast windshield lunge
- Bone Crawler: bony close-range attacker during stalls and door events
- Winged Watcher: moving silhouette used for crossing and overhead pressure
- Ash Shade: dark late-game entity tied to mirror and tower events

## Practical Survival

1. Keep W held when the road allows it.
2. Use high beams or horn only when the entity is visible or close.
3. Restart stalls immediately with R.
4. Reverse hard when the road is blocked.
5. If SLOW RISK appears, move first and interact second.
6. If the wheel pulls without input, stabilize the radio.
7. If acceleration dies while the engine is on, re-seat the shifter.
8. In the storm, brake earlier and avoid overcorrecting.
9. In town, do not trust radio directions from the monster subtitle panel.
