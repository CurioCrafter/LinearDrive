# Linear Drive

Native Windows build of **Linear Drive**, a first-person survival-horror driving game based on the provided design document.

## Build

Prerequisites on this machine:

- CMake 3.24+
- Ninja
- MinGW-w64 g++

```powershell
$env:PATH = "C:\msys64\mingw64\bin;$env:PATH"
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=C:/msys64/mingw64/bin/gcc.exe -DCMAKE_CXX_COMPILER=C:/msys64/mingw64/bin/g++.exe
cmake --build build --config Release
```

The executable is written to:

```text
build/bin/LinearDrive.exe
```

The post-build step copies `assets/` beside the executable.

## Download And Install

For friends who just want to play, use the GitHub Release installer:

1. Download `LinearDriveSetup.exe`.
2. Run it.
3. It installs the game to `%LOCALAPPDATA%\LinearDrive`, creates Desktop and Start Menu shortcuts, and launches the game.

The release also includes `LinearDrive-Windows.zip` for manual extraction.

To rebuild the installer locally after packaging:

```powershell
.\tools\build_installer.ps1
```

## Controls

Primary interaction is diegetic: hover and click the modeled cockpit parts inside the car. The radio, ignition, shifter, lock, handle, mirror, headlights, horn, wipers, fan, dome light, glovebox, and window crank all have physical hit areas. Keyboard controls remain as backups:

- `W/S`: accelerate, brake, reverse when stopped
- `A/D`: steer
- `Shift`: high beams
- `Space`: handbrake
- `R`: ignition
- `H`: horn
- `T`: radio
- `L`: door lock
- `B`: headlights
- `V`: wipers
- `F`: dome light
- `G`: gear shift
- `Tab`: mirror glance
- `Q/E` or right mouse drag: look around
- `Esc`: pause

## Survival Tutorial

The in-game main menu has a tutorial screen. A written guide is also available at `docs/TUTORIAL.md`.

Core rule: when a monster is active, slowing below 12 MPH fills the SLOW RISK meter. If it fills, the monster closes in, jumpscares through the windshield, and kills the run. At blocked roads, reversing counts as escape.

V3 adds the cockpit and skill gameplay pass:

- The first-person cabin now uses the supplied Toyota RAV4 FBX converted to `assets/models/toyota_rav4_cockpit.glb`; the gameplay radio, shifter, ignition, lock, and other diegetic controls are mounted into that cabin.
- The Toyota cockpit is calibrated as a car-local 3D rig: +X passenger/right, +Y up, and -Z road/front. The Toyota asset is rotated into that direction with positive scale so the left-hand-drive cabin is not mirrored. Debug capture scenes verify the exterior orientation, driver-eye camera, and projected cockpit anchors.
- The old right-side gameplay button bar has been replaced with modeled cockpit controls and projected hit areas.
- Loose radio interference can pull the steering. Click the physical radio to stabilize it before the car drifts off the road.
- Shifter slip can dump the car into neutral or block acceleration. Click the shifter to re-seat it.
- Door attacks can fake or jam the lock. Relock the door and use the mirror to identify false cues.
- Hazards are telegraphed before they become lethal, so practiced players can survive by driving, stabilizing, relocking, restarting, looking, and listening.
- The main menu now includes Start, Tutorial, Settings, and Quit. Settings includes live master/music/SFX volume sliders plus subtitles, reduced motion, reduced flashing, and gamma.
- Restarting or returning to menu now stops active scare sounds, radio/static streams, and storm ambience before a new run starts.

V2 added the chapter-slice world pass:

- Tree impacts now use procedural roadside collision. A fast tree hit breaks the car and starts the jumpscare kill sequence; a slower hit damages and stalls the car.
- The route now moves through forest, storm road, abandoned town, service strip, and tower works zones.
- Storms add rain, puddles, lightning, thunder audio, slick steering, and temporary electrical pressure.
- The abandoned town adds procedural buildings, traffic lights, windows, service props, barricades, and monster voice events.
- Monster speech appears as distorted radio/dashboard subtitles with dedicated monster audio.

Accessibility toggles are available from pause:

- `F1`: subtitles
- `F2`: reduced motion
- `F3`: reduced flashing
- `+/-`: gamma

## Engine Shape

The project has its own C++ engine layer over raylib:

- `src/engine`: application loop, asset loading, renderer, audio
- `src/game`: simulation, systems, event director, player interactions
- `assets`: runtime models, textures, and audio with source zips retained

The renderer uses online CC0 GLB models where useful, the supplied Toyota RAV4 cockpit baseline, and procedural road, fog, creature, cockpit-control, and HUD drawing.

## Verification Commands

```powershell
build\bin\LinearDrive.exe --smoke
build\bin\LinearDrive.exe --capture dist\treecrash_capture.png
build\bin\LinearDrive.exe --capture dist\storm_capture.png
build\bin\LinearDrive.exe --capture dist\town_capture.png
build\bin\LinearDrive.exe --capture dist\menu_v2_capture.png
build\bin\LinearDrive.exe --capture dist\jumpscare_capture.png
build\bin\LinearDrive.exe --capture dist\tutorial_capture.png
build\bin\LinearDrive.exe --capture dist\toyota_cockpit_capture.png
build\bin\LinearDrive.exe --capture dist\toyota_calibration_front.png
build\bin\LinearDrive.exe --capture dist\toyota_calibration_left.png
build\bin\LinearDrive.exe --capture dist\toyota_calibration_top.png
build\bin\LinearDrive.exe --capture dist\toyota_cockpit_driver.png
build\bin\LinearDrive.exe --capture dist\toyota_controls_anchor_map.png
build\bin\LinearDrive.exe --capture dist\toyota_console_closeup.png
build\bin\LinearDrive.exe --capture dist\cockpit_controls_capture.png
build\bin\LinearDrive.exe --capture dist\settings_menu_capture.png
build\bin\LinearDrive.exe --capture dist\radio_interference_capture.png
build\bin\LinearDrive.exe --capture dist\shifter_slip_capture.png
build\bin\LinearDrive.exe --capture dist\restart_audio_capture.png
```
