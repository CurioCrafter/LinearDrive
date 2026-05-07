#include "game/SmokeTest.hpp"

#include "engine/Assets.hpp"
#include "engine/Renderer.hpp"
#include "game/Simulation.hpp"

#include <iostream>
#include <filesystem>
#include <cmath>
#include <string>
#include <vector>

namespace ld {
namespace {

bool Require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    std::cout << "PASS: " << message << '\n';
    return true;
}

} // namespace

int RunSmokeTests() {
    bool ok = true;

    const std::vector<const char*> assets {
        "models/tree_default_dark.obj",
        "models/plant_bushDetailed.obj",
        "models/creature_stalker.glb",
        "models/quaternius_skeleton_tri.obj",
        "models/sedan.glb",
        "audio/radio_static.mp3",
        "audio/glitch_003.wav",
        "audio/jumpscare.wav",
        "audio/thunder.wav",
        "audio/storm_ambience.ogg",
        "textures/menu_v2_backplate.png",
        "textures/tutorial_v2_backplate.png",
        "textures/icons/locked.png"
    };

    for (const char* asset : assets) {
        const std::string path = AssetPath(asset);
        ok &= Require(FileExists(path.c_str()), path.c_str());
    }

    Simulation simulation(1234);
    simulation.Start();
    InputState drive {};
    drive.accelerate = true;
    for (int i = 0; i < 600; ++i) {
        simulation.Update(drive, 1.0f / 60.0f);
    }
    ok &= Require(simulation.State().car.speedMph > 20.0f, "car accelerates");
    ok &= Require(simulation.State().distanceMiles > 0.02f, "distance advances");

    simulation.State().distanceMiles = 0.17f;
    simulation.Update(InputState {}, 1.0f / 60.0f);
    ok &= Require(simulation.State().car.radioOn, "scripted radio beat triggers");

    simulation.State().distanceMiles = 1.44f;
    simulation.Update(InputState {}, 1.0f / 60.0f);
    ok &= Require(simulation.State().car.stalled, "engine stall beat triggers");

    simulation.State().distanceMiles = 4.20f;
    simulation.State().car.stalled = false;
    simulation.State().car.engineOn = true;
    simulation.Update(drive, 1.0f / 60.0f);
    ok &= Require(simulation.State().phase == GamePhase::Ending, "tower ending state triggers");

    Simulation slowKill(4321);
    slowKill.Start();
    slowKill.State().distanceMiles = 1.62f;
    slowKill.State().tension = 82.0f;
    slowKill.State().car.engineOn = false;
    slowKill.State().car.stalled = true;
    slowKill.State().car.speedMph = 0.0f;
    slowKill.State().creature.mode = CreatureMode::Pacing;
    slowKill.State().creature.variant = 1;
    slowKill.State().creature.visibility = 1.0f;
    slowKill.State().creature.distanceAhead = 10.0f;
    for (int i = 0; i < 220; ++i) {
        slowKill.Update(InputState {}, 1.0f / 60.0f);
    }
    ok &= Require(slowKill.State().jumpscareActive || slowKill.State().phase == GamePhase::Dead, "slowing down starts jumpscare consequence");
    for (int i = 0; i < 220; ++i) {
        slowKill.Update(InputState {}, 1.0f / 60.0f);
    }
    ok &= Require(slowKill.State().phase == GamePhase::Dead, "jumpscare resolves to death");

    Simulation treeFast(2468);
    treeFast.Start();
    treeFast.State().distanceMiles = 0.24f;
    treeFast.Update(drive, 1.0f / 60.0f);
    auto fastTree = WorldGenerator::FindTreeImpact(treeFast.State().distanceMiles, treeFast.State().roadWidth * 0.5f + 1.2f, treeFast.State().roadWidth);
    if (!fastTree) {
        const auto props = WorldGenerator::PropsAround(treeFast.State().distanceMiles, 0.02f, 0.16f, treeFast.State().roadWidth);
        for (const WorldProp& prop : props) {
            if (prop.kind == WorldPropKind::Tree) {
                fastTree = prop;
                break;
            }
        }
    }
    ok &= Require(fastTree.has_value(), "procedural tree collision candidate exists");
    if (fastTree) {
        treeFast.State().distanceMiles = fastTree->mile;
        treeFast.State().car.speedMph = 44.0f;
        treeFast.State().car.lateralOffset = fastTree->lateral;
        treeFast.State().crashGraceTime = 0.0f;
        treeFast.Update(InputState {}, 1.0f / 60.0f);
        ok &= Require(treeFast.State().jumpscareActive, "high-speed tree impact starts jumpscare");
    }

    Simulation treeSlow(1357);
    treeSlow.Start();
    treeSlow.State().distanceMiles = 0.24f;
    treeSlow.Update(InputState {}, 1.0f / 60.0f);
    const auto slowProps = WorldGenerator::PropsAround(treeSlow.State().distanceMiles, 0.02f, 0.16f, treeSlow.State().roadWidth);
    for (const WorldProp& prop : slowProps) {
        if (prop.kind == WorldPropKind::Tree) {
            treeSlow.State().distanceMiles = prop.mile;
            treeSlow.State().car.lateralOffset = prop.lateral;
            break;
        }
    }
    treeSlow.State().car.speedMph = 12.0f;
    treeSlow.State().crashGraceTime = 0.0f;
    treeSlow.Update(InputState {}, 1.0f / 60.0f);
    ok &= Require(treeSlow.State().phase == GamePhase::Playing && !treeSlow.State().jumpscareActive && treeSlow.State().car.stalled, "low-speed tree impact damages and stalls without instant death");

    Simulation storm(9753);
    storm.Start();
    storm.State().distanceMiles = 1.0f;
    storm.Update(drive, 1.0f / 60.0f);
    ok &= Require(storm.State().world.stormIntensity > 0.35f && storm.State().world.wetness > 0.25f, "storm zone affects world state");

    Simulation town(8642);
    town.Start();
    town.State().distanceMiles = 2.34f;
    town.Update(drive, 1.0f / 60.0f);
    ok &= Require(town.State().world.townDensity > 0.5f && !WorldGenerator::PropsAround(town.State().distanceMiles, 0.02f, 0.05f, town.State().roadWidth).empty(), "town zone generates procedural props");

    Simulation voice(1111);
    voice.Start();
    voice.State().completedBeats.fill(true);
    voice.State().distanceMiles = 2.35f;
    voice.State().monsterVoiceCooldown = 0.0f;
    voice.State().timeSinceScare = 5.0f;
    for (int i = 0; i < 20; ++i) voice.Update(drive, 1.0f / 60.0f);
    ok &= Require(voice.State().story.monsterSubtitleTimer > 0.0f, "monster voice event surfaces subtitle");

    Simulation radioSkill(2222);
    radioSkill.Start();
    radioSkill.TriggerRadioInterference(0.7f, "test radio pull");
    radioSkill.Update(drive, 1.0f / 60.0f);
    ok &= Require(radioSkill.State().car.radioInterferenceTimer > 0.0f && std::abs(radioSkill.State().car.steeringInterference) > 0.05f, "radio interference creates steering pull");
    radioSkill.Interact(Hotspot::Radio);
    ok &= Require(radioSkill.State().car.radioInterferenceTimer <= 0.0f && std::abs(radioSkill.State().car.steeringInterference) < 0.01f, "clicking radio stabilizes steering pull");

    Simulation shiftSkill(3333);
    shiftSkill.Start();
    shiftSkill.State().car.speedMph = 34.0f;
    shiftSkill.TriggerShifterSlip("test shifter slip");
    shiftSkill.Update(drive, 1.0f / 60.0f);
    ok &= Require(shiftSkill.State().car.gear == 'N' && shiftSkill.State().car.shifterSlipTimer > 0.0f, "shifter slip blocks acceleration in neutral");
    shiftSkill.Interact(Hotspot::GearShift);
    ok &= Require(shiftSkill.State().car.shifterSlipTimer <= 0.0f && shiftSkill.State().car.gear != 'N', "clicking shifter re-seats gear");

    Simulation skillChain(4444);
    skillChain.Start();
    skillChain.State().distanceMiles = 2.35f;
    skillChain.State().car.speedMph = 36.0f;
    skillChain.State().tension = 70.0f;
    skillChain.TriggerRadioInterference(0.55f, "test chain radio");
    skillChain.TriggerShifterSlip("test chain shifter");
    skillChain.State().creature.mode = CreatureMode::Attacking;
    skillChain.State().creature.attackCharge = 0.72f;
    skillChain.State().creature.distanceAhead = 6.0f;
    skillChain.State().car.doorLock = DoorLockState::Jammed;
    skillChain.Update(drive, 1.0f / 60.0f);
    skillChain.Interact(Hotspot::Radio);
    skillChain.Interact(Hotspot::GearShift);
    skillChain.Interact(Hotspot::DoorLock);
    for (int i = 0; i < 90; ++i) skillChain.Update(drive, 1.0f / 60.0f);
    ok &= Require(skillChain.State().phase == GamePhase::Playing && !skillChain.State().jumpscareActive, "skilled counterplay survives hazard chain");

    Simulation settings(5555);
    settings.State().settings.masterVolume = 0.42f;
    settings.State().settings.musicVolume = 0.33f;
    settings.State().settings.sfxVolume = 0.66f;
    ok &= Require(settings.State().settings.masterVolume == 0.42f && settings.State().settings.musicVolume == 0.33f && settings.State().settings.sfxVolume == 0.66f, "settings state exposes live audio volumes");

    return ok ? 0 : 1;
}

int RunCapture(const char* outputPath) {
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "Linear Drive Capture");
    InitAudioDevice();
    SetTargetFPS(60);

    AssetStore assets;
    assets.Load();
    Renderer renderer(&assets.Get());
    Simulation simulation(5678);
    simulation.Start();
    const std::string requestedName(outputPath);
    const bool creatureScene = requestedName.find("creature") != std::string::npos;
    const bool jumpscareScene = requestedName.find("jumpscare") != std::string::npos;
    const bool tutorialScene = requestedName.find("tutorial") != std::string::npos;
    const bool menuScene = requestedName.find("menu") != std::string::npos;
    const bool treeCrashScene = requestedName.find("treecrash") != std::string::npos;
    const bool stormScene = requestedName.find("storm") != std::string::npos;
    const bool townScene = requestedName.find("town") != std::string::npos;
    const bool cockpitScene = requestedName.find("cockpit_controls") != std::string::npos;
    const bool settingsScene = requestedName.find("settings_menu") != std::string::npos;
    const bool radioInterferenceScene = requestedName.find("radio_interference") != std::string::npos;
    const bool shifterSlipScene = requestedName.find("shifter_slip") != std::string::npos;
    const bool restartAudioScene = requestedName.find("restart_audio") != std::string::npos;
    if (cockpitScene) {
        GameState& state = simulation.State();
        state.distanceMiles = 1.08f;
        state.tension = 66.0f;
        state.car.speedMph = 36.0f;
        state.car.radioOn = true;
        state.car.radioVolume = 0.88f;
        state.car.radioInterferenceTimer = 4.4f;
        state.car.radioLoose = true;
        state.car.steeringInterference = 0.44f;
        state.car.shifterSlipTimer = 3.6f;
        state.car.shifterSlipping = true;
        state.car.doorLock = DoorLockState::FakeLocked;
        state.car.falseLockState = true;
        state.story.warning = "The cockpit is alive. Stabilize the right control.";
        state.story.messageTimer = 5.0f;
    } else if (creatureScene) {
        GameState& state = simulation.State();
        state.distanceMiles = 1.24f;
        state.tension = 76.0f;
        state.car.speedMph = 38.0f;
        state.car.headlightPower = 88.0f;
        state.car.highBeams = true;
        state.creature.mode = CreatureMode::Lunging;
        state.creature.variant = 0;
        state.creature.visibility = 1.0f;
        state.creature.distanceAhead = 15.0f;
        state.creature.lateral = 2.6f;
        state.creature.attackCharge = 0.0f;
        state.story.warning = "Something breaks from the right-side trees.";
        state.story.messageTimer = 5.0f;
    } else if (jumpscareScene) {
        GameState& state = simulation.State();
        state.distanceMiles = 2.95f;
        state.tension = 100.0f;
        state.jumpscareActive = true;
        state.jumpscareTimer = 1.06f;
        state.jumpscareDuration = 2.55f;
        state.car.speedMph = 0.0f;
        state.car.engineOn = false;
        state.car.stalled = true;
        state.car.radioOn = true;
        state.car.radioVolume = 1.0f;
        state.creature.mode = CreatureMode::Jumpscare;
        state.creature.variant = 3;
        state.creature.visibility = 1.0f;
        state.creature.distanceAhead = 0.55f;
        state.creature.lateral = 0.0f;
        state.creature.attackCharge = 0.48f;
        state.creature.slowHuntTimer = 2.65f;
        state.story.warning = "You let the car get too quiet.";
        state.story.messageTimer = 5.0f;
    } else if (tutorialScene) {
        simulation.State().phase = GamePhase::Menu;
        simulation.ShowTutorial();
    } else if (settingsScene) {
        simulation.State().phase = GamePhase::Menu;
        simulation.ShowSettings();
    } else if (menuScene) {
        simulation.State().phase = GamePhase::Menu;
    } else if (radioInterferenceScene) {
        GameState& state = simulation.State();
        state.distanceMiles = 1.1f;
        state.tension = 72.0f;
        state.car.speedMph = 42.0f;
        state.car.radioOn = true;
        state.car.radioVolume = 1.0f;
        state.car.radioInterferenceTimer = 5.0f;
        state.car.radioLoose = true;
        state.car.steeringInterference = 0.58f;
        state.story.warning = "The loose radio is pulling the wheel.";
        state.story.messageTimer = 5.0f;
    } else if (shifterSlipScene) {
        GameState& state = simulation.State();
        state.distanceMiles = 2.1f;
        state.tension = 74.0f;
        state.car.speedMph = 25.0f;
        state.car.shifterSlipTimer = 4.2f;
        state.car.shifterSlipping = true;
        state.car.gear = 'N';
        state.story.warning = "The shifter slips out of drive.";
        state.story.messageTimer = 5.0f;
    } else if (restartAudioScene) {
        GameState& state = simulation.State();
        state.phase = GamePhase::Dead;
        state.jumpscareActive = false;
        state.car.radioOn = true;
        state.car.radioVolume = 1.0f;
        state.story.warning = "Restart clears the jumpscare and all active audio.";
    } else if (treeCrashScene) {
        GameState& state = simulation.State();
        state.distanceMiles = 0.32f;
        state.tension = 100.0f;
        state.jumpscareActive = true;
        state.jumpscareTimer = 0.92f;
        state.creature.mode = CreatureMode::Jumpscare;
        state.creature.variant = 1;
        state.creature.visibility = 1.0f;
        state.creature.attackCharge = 0.44f;
        state.story.warning = "The tree caves in the windshield.";
        state.story.messageTimer = 5.0f;
    } else if (stormScene) {
        GameState& state = simulation.State();
        state.distanceMiles = 1.08f;
        state.tension = 68.0f;
        state.car.speedMph = 38.0f;
        state.story.warning = "Thunder rolls over the hood.";
        state.story.messageTimer = 5.0f;
    } else if (townScene) {
        GameState& state = simulation.State();
        state.distanceMiles = 2.38f;
        state.tension = 76.0f;
        state.car.speedMph = 28.0f;
        state.creature.mode = CreatureMode::Watching;
        state.creature.visibility = 0.7f;
        state.creature.variant = 3;
        state.story.monsterSubtitle = "The town remembers your headlights.";
        state.story.monsterSubtitleTimer = 4.0f;
    }

    std::vector<HudMessage> messages;
    InputState input {};
    input.accelerate = true;

    for (int frame = 0; frame < 180; ++frame) {
        simulation.Update(input, 1.0f / 60.0f);
        if (creatureScene) {
            GameState& state = simulation.State();
            state.creature.mode = CreatureMode::Lunging;
            state.creature.variant = frame < 90 ? 0 : 2;
            state.creature.visibility = 1.0f;
            state.creature.distanceAhead = 9.0f;
            state.creature.lateral = 1.55f + std::sin(frame * 0.08f) * 0.25f;
            state.creature.attackCharge = 0.15f;
            state.creature.slowHuntTimer = 0.0f;
            state.tension = 82.0f;
            state.car.highBeams = true;
            state.car.headlightPower = 88.0f;
            state.story.warning = "Something breaks from the right-side trees.";
            state.story.messageTimer = 5.0f;
        } else if (cockpitScene) {
            GameState& state = simulation.State();
            state.distanceMiles = 1.08f;
            state.tension = 70.0f;
            state.car.speedMph = 36.0f;
            state.car.radioInterferenceTimer = 4.4f;
            state.car.radioLoose = true;
            state.car.steeringInterference = 0.42f;
            state.car.shifterSlipTimer = 3.8f;
            state.car.shifterSlipping = true;
            state.car.doorLock = DoorLockState::FakeLocked;
            state.car.falseLockState = true;
            state.story.warning = "Click the physical cockpit controls to counter faults.";
            state.story.messageTimer = 5.0f;
        } else if (jumpscareScene) {
            GameState& state = simulation.State();
            state.phase = GamePhase::Playing;
            state.jumpscareActive = true;
            state.jumpscareTimer = 1.08f + std::sin(frame * 0.04f) * 0.08f;
            state.creature.mode = CreatureMode::Jumpscare;
            state.creature.variant = 3;
            state.creature.visibility = 1.0f;
            state.creature.distanceAhead = 0.42f;
            state.creature.attackCharge = 0.52f;
            state.creature.slowHuntTimer = 2.65f;
            state.tension = 100.0f;
        } else if (treeCrashScene) {
            GameState& state = simulation.State();
            state.phase = GamePhase::Playing;
            state.jumpscareActive = true;
            state.jumpscareTimer = 0.94f + std::sin(frame * 0.05f) * 0.05f;
            state.creature.mode = CreatureMode::Jumpscare;
            state.creature.variant = 1;
            state.creature.visibility = 1.0f;
            state.creature.distanceAhead = 0.5f;
            state.creature.attackCharge = 0.48f;
            state.lastTreeImpactLethal = true;
        } else if (stormScene) {
            GameState& state = simulation.State();
            state.distanceMiles = 1.08f + frame * 0.0001f;
            state.car.speedMph = 38.0f;
            state.tension = 72.0f;
        } else if (townScene) {
            GameState& state = simulation.State();
            state.distanceMiles = 2.38f + frame * 0.0001f;
            state.tension = 78.0f;
            state.story.monsterSubtitle = "The town remembers your headlights.";
            state.story.monsterSubtitleTimer = 4.0f;
        } else if (settingsScene) {
            simulation.State().phase = GamePhase::Settings;
        } else if (radioInterferenceScene) {
            GameState& state = simulation.State();
            state.distanceMiles = 1.1f + frame * 0.0001f;
            state.tension = 72.0f;
            state.car.speedMph = 42.0f;
            state.car.radioInterferenceTimer = 4.8f;
            state.car.radioLoose = true;
            state.car.steeringInterference = 0.56f;
            state.story.warning = "The loose radio is pulling the wheel.";
            state.story.messageTimer = 5.0f;
        } else if (shifterSlipScene) {
            GameState& state = simulation.State();
            state.distanceMiles = 2.1f + frame * 0.0001f;
            state.tension = 74.0f;
            state.car.shifterSlipTimer = 4.2f;
            state.car.shifterSlipping = true;
            state.car.gear = 'N';
            state.story.warning = "The shifter slips out of drive.";
            state.story.messageTimer = 5.0f;
        } else if (restartAudioScene) {
            GameState& state = simulation.State();
            state.phase = GamePhase::Dead;
            state.jumpscareActive = false;
            state.story.warning = "Restart clears the jumpscare and all active audio.";
        }
        std::vector<GameEvent> events = simulation.DrainEvents();
        for (const GameEvent& event : events) {
            messages.insert(messages.begin(), HudMessage {event.type, event.message, 4.0f, event.intensity});
        }
        if (messages.size() > 5) messages.resize(5);
        for (HudMessage& message : messages) message.timer -= 1.0f / 60.0f;
        renderer.Render(simulation.State(), input, messages);
        if (frame == 120) {
            TakeScreenshot("linear_drive_capture.png");
        }
    }

    assets.Unload();
    if (IsAudioDeviceReady()) {
        CloseAudioDevice();
    }
    CloseWindow();

    std::filesystem::path requested(outputPath);
    if (!requested.empty() && requested != "linear_drive_capture.png") {
        if (requested.has_parent_path()) {
            std::filesystem::create_directories(requested.parent_path());
        }
        std::filesystem::copy_file("linear_drive_capture.png", requested, std::filesystem::copy_options::overwrite_existing);
    }

    std::cout << "capture: " << outputPath << '\n';
    return 0;
}

} // namespace ld
