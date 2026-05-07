#include "game/Simulation.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace ld {
namespace {

constexpr float kMaxForwardSpeed = 64.0f;
constexpr float kMaxReverseSpeed = -18.0f;
constexpr float kSpeedToMilesPerSecond = 1.0f / 3600.0f;

struct ChapterDef {
    Chapter chapter;
    float startMiles;
    const char* name;
    const char* objective;
};

struct BeatDef {
    float distanceMiles;
    Chapter chapter;
    int kind;
    float tension;
    const char* message;
};

constexpr std::array<ChapterDef, 6> kChapters {{
    {Chapter::MainRoad, 0.0f, "The Main Road", "Reach the Radio Tower"},
    {Chapter::ElectricalProblems, 0.70f, "Electrical Problems", "Keep the power alive"},
    {Chapter::FirstBreakdown, 1.35f, "The First Breakdown", "Restart the engine"},
    {Chapter::RoadNarrows, 2.05f, "The Road Narrows", "Stay on the road"},
    {Chapter::HandleComesOff, 2.90f, "The Handle Comes Off", "Do not stop"},
    {Chapter::RadioTower, 3.55f, "The Radio Tower", "Reach the signal"}
}};

constexpr std::array<RoadSegment, 9> kRoadSegments {{
    {0.00f, 0.38f, SegmentKind::Straight, 0.00f, 8.2f, 0.28f},
    {0.38f, 0.74f, SegmentKind::Bend, 0.34f, 7.4f, 0.32f},
    {0.74f, 1.18f, SegmentKind::Fog, -0.18f, 7.2f, 0.54f},
    {1.18f, 1.76f, SegmentKind::Straight, 0.08f, 7.0f, 0.42f},
    {1.76f, 2.34f, SegmentKind::Mud, -0.28f, 6.4f, 0.50f},
    {2.34f, 2.68f, SegmentKind::Bridge, 0.16f, 5.1f, 0.40f},
    {2.68f, 3.08f, SegmentKind::FallenTree, -0.08f, 6.1f, 0.58f},
    {3.08f, 3.52f, SegmentKind::Checkpoint, 0.22f, 6.6f, 0.46f},
    {3.52f, 4.20f, SegmentKind::Tower, 0.00f, 7.8f, 0.62f}
}};

constexpr std::array<BeatDef, 13> kBeats {{
    {0.16f, Chapter::MainRoad, 0, 18.0f, "The radio wakes with a burst of dry static."},
    {0.32f, Chapter::MainRoad, 1, 22.0f, "A pair of eyes holds steady between the trees."},
    {0.84f, Chapter::ElectricalProblems, 2, 30.0f, "The dashboard fan begins to wind down."},
    {1.02f, Chapter::ElectricalProblems, 3, 35.0f, "The radio turns itself back on, louder than before."},
    {1.23f, Chapter::ElectricalProblems, 4, 45.0f, "Something crosses the headlights and is gone."},
    {1.43f, Chapter::FirstBreakdown, 5, 58.0f, "The engine coughs hard and dies."},
    {1.55f, Chapter::FirstBreakdown, 6, 62.0f, "The passenger door jumps against its latch."},
    {1.84f, Chapter::FirstBreakdown, 7, 44.0f, "The rear-view mirror catches two points of light behind you."},
    {2.31f, Chapter::RoadNarrows, 8, 54.0f, "Something works the passenger handle from outside."},
    {2.78f, Chapter::RoadNarrows, 9, 45.0f, "The driver handle cracks under your fingers."},
    {3.02f, Chapter::HandleComesOff, 10, 70.0f, "The handle snaps free and drops into the footwell."},
    {3.36f, Chapter::HandleComesOff, 11, 74.0f, "The road is blocked. Reverse before it reaches the glass."},
    {3.82f, Chapter::RadioTower, 12, 88.0f, "The tower beacon pulses through the fog."}
}};

const std::vector<const char*> kRadioLines {
    "...county road seven is closed after dusk...",
    "Turn back when the tower light blinks twice.",
    "The vehicle was reported empty. Engine still warm.",
    "If you can hear this, do not unlock the doors.",
    "Signal marker four-two. Repeat. Four-two.",
    "You are not on the road you started on.",
    "The car has been marked since before you left.",
    "Keep driving. It listens when the engine stops."
};

const std::vector<const char*> kFanLines {
    "The fan cage trembles against its screws.",
    "The bearing squeals, then catches.",
    "The fan slows as if something pinched the current."
};

const std::vector<const char*> kLockLines {
    "The lock pin drops halfway, clicks, then rises again.",
    "Plastic flexes under your thumb. The lock refuses to stay down.",
    "The lock looks seated. It does not feel seated."
};

const std::vector<const char*> kHandleLines {
    "The handle gives a dry little crack.",
    "The handle bends, loose on one screw.",
    "The door shifts, but the latch does not release."
};

const std::vector<const char*> kGloveboxLines {
    "The glovebox opens an inch and vomits old receipts.",
    "A folded towing notice slides out, damp and unreadable.",
    "Something inside knocks once, then goes still."
};

const std::vector<const char*> kMirrorLines {
    "The mirror shakes until the road behind becomes a smear.",
    "A pale reflection appears and folds back into darkness.",
    "The mirror shows empty road. The radio disagrees."
};

const std::vector<const char*> kMonsterVoiceLines {
    "I hear the engine slowing.",
    "The town remembers your headlights.",
    "Turn into the street with no houses.",
    "The trees are closer than the road.",
    "You left the door unlocked before you started.",
    "The tower is not calling you."
};

RoadSegment RoadAt(float distance) {
    for (const RoadSegment& segment : kRoadSegments) {
        if (distance >= segment.startMiles && distance < segment.endMiles) {
            return segment;
        }
    }
    return kRoadSegments.back();
}

std::pair<ChapterDef, int> ChapterAt(float distance) {
    ChapterDef selected = kChapters.front();
    int index = 0;
    for (int i = 0; i < static_cast<int>(kChapters.size()); ++i) {
        if (distance >= kChapters[static_cast<size_t>(i)].startMiles) {
            selected = kChapters[static_cast<size_t>(i)];
            index = i;
        }
    }
    return {selected, index};
}

EventType BeatEventType(int kind) {
    switch (kind) {
        case 0:
        case 3:
            return EventType::Radio;
        case 2:
            return EventType::Power;
        case 5:
            return EventType::Stall;
        case 1:
        case 4:
        case 7:
        case 12:
            return EventType::Creature;
        case 6:
        case 8:
        case 9:
        case 10:
        case 11:
            return EventType::Impact;
        default:
            return EventType::Notice;
    }
}

FanState DeriveFan(FanState current, float battery, bool engineOn, float tension) {
    if (!engineOn || battery <= 3.0f) {
        return FanState::Stopped;
    }
    if (battery < 18.0f || tension > 82.0f) {
        return FanState::Irregular;
    }
    if (battery < 42.0f || current == FanState::Slow) {
        return FanState::Slow;
    }
    return FanState::Normal;
}

} // namespace

Simulation::Simulation(uint32_t seed) {
    Reset(seed);
}

void Simulation::Reset(uint32_t seed) {
    state_ = GameState {};
    state_.randomSeed = seed == 0 ? 1 : seed;
    state_.hazards = {{
        {"mud-hole-1", 1.96f, -1.6f, 2.0f, 0, false, false},
        {"branch-1", 2.74f, 1.1f, 1.8f, 1, false, false},
        {"abandoned-car", 3.17f, -1.15f, 2.4f, 2, false, false},
        {"blocked-road", 3.37f, 0.0f, 7.0f, 3, false, false},
        {"tower-gate", 4.05f, 0.75f, 1.8f, 4, false, false}
    }};
    events_.clear();
}

void Simulation::Start() {
    state_.phase = GamePhase::Playing;
    state_.jumpscareActive = false;
    state_.jumpscareTimer = 0.0f;
    state_.story.objective = "Reach the Radio Tower";
    state_.story.subObjective = "Keep the engine moving";
    Push(EventType::Notice, "The road ahead narrows to a single black line.", 0.2f);
}

void Simulation::ShowTutorial() {
    if (state_.phase == GamePhase::Menu || state_.phase == GamePhase::Paused || state_.phase == GamePhase::Tutorial || state_.phase == GamePhase::Settings) {
        state_.phase = GamePhase::Tutorial;
    }
}

void Simulation::ShowSettings() {
    if (state_.phase == GamePhase::Menu || state_.phase == GamePhase::Paused || state_.phase == GamePhase::Tutorial || state_.phase == GamePhase::Settings) {
        state_.phase = GamePhase::Settings;
    }
}

void Simulation::ReturnToMenu() {
    if (state_.phase == GamePhase::Tutorial || state_.phase == GamePhase::Settings || state_.phase == GamePhase::Dead) {
        state_.phase = GamePhase::Menu;
    }
}

void Simulation::Pause() {
    if (state_.phase == GamePhase::Playing && !state_.jumpscareActive) {
        state_.phase = GamePhase::Paused;
    }
}

void Simulation::Resume() {
    if (state_.phase == GamePhase::Paused) {
        state_.phase = GamePhase::Playing;
    }
}

void Simulation::Update(const InputState& input, float dt) {
    if (state_.phase != GamePhase::Playing) {
        return;
    }

    dt = std::min(dt, 0.05f);
    state_.elapsed += dt;
    state_.timeSinceScare += dt;
    state_.cooldown = std::max(0.0f, state_.cooldown - dt);
    state_.crashGraceTime = std::max(0.0f, state_.crashGraceTime - dt);
    state_.treeImpactCooldown = std::max(0.0f, state_.treeImpactCooldown - dt);
    state_.stormEventCooldown = std::max(0.0f, state_.stormEventCooldown - dt);
    state_.monsterVoiceCooldown = std::max(0.0f, state_.monsterVoiceCooldown - dt);
    state_.story.messageTimer = std::max(0.0f, state_.story.messageTimer - dt);
    state_.story.monsterSubtitleTimer = std::max(0.0f, state_.story.monsterSubtitleTimer - dt);
    state_.car.radioInterferenceTimer = std::max(0.0f, state_.car.radioInterferenceTimer - dt);
    state_.car.shifterSlipTimer = std::max(0.0f, state_.car.shifterSlipTimer - dt);
    state_.car.shifterSettleTimer = std::max(0.0f, state_.car.shifterSettleTimer - dt);
    state_.car.cockpitFeedbackTimer = std::max(0.0f, state_.car.cockpitFeedbackTimer - dt);
    if (state_.car.radioInterferenceTimer <= 0.0f) {
        state_.car.radioLoose = false;
        state_.car.steeringInterference = Approach(state_.car.steeringInterference, 0.0f, dt * 0.45f);
    }
    if (state_.car.shifterSlipTimer <= 0.0f) {
        state_.car.shifterSlipping = false;
    }

    if (state_.jumpscareActive) {
        UpdateJumpscare(dt);
        return;
    }

    UpdateRoadContext();
    UpdateCar(input, dt);
    UpdateDistance(dt);
    UpdateDirector(dt);
    UpdateStormAndVoices(dt);
    UpdateCreature(input, dt);
    UpdateHazards();
    UpdatePressure(input, dt);
    UpdateObjective();
    CheckDeathAndEnding();
}

void Simulation::Interact(Hotspot hotspot) {
    if (state_.phase != GamePhase::Playing || state_.jumpscareActive) {
        return;
    }

    switch (hotspot) {
        case Hotspot::Radio: {
            if (state_.car.radioInterferenceTimer > 0.0f || state_.car.radioLoose || std::abs(state_.car.steeringInterference) > 0.04f) {
                StabilizeRadio();
                break;
            }
            MarkCockpitFeedback(hotspot);
            state_.car.radioOn = !state_.car.radioOn || state_.tension > 62.0f;
            if (state_.car.radioOn) {
                state_.car.radioVolume = std::min(1.0f, state_.car.radioVolume + 0.22f);
                state_.story.radioSubtitle = PickLine(kRadioLines);
                Push(EventType::Radio, state_.story.radioSubtitle, 0.48f);
            } else {
                state_.car.radioVolume = 0.0f;
                state_.story.radioSubtitle = "The static drops out. The speaker keeps ticking.";
                Push(EventType::Interaction, state_.story.radioSubtitle, 0.28f);
            }
            break;
        }
        case Hotspot::Fan:
            MarkCockpitFeedback(hotspot);
            state_.car.fan = state_.car.battery < 30.0f ? FanState::Irregular : (state_.car.fan == FanState::Normal ? FanState::Slow : FanState::Normal);
            state_.tension += 3.0f;
            Push(EventType::Power, PickLine(kFanLines), 0.34f);
            break;
        case Hotspot::DoorLock:
            MarkCockpitFeedback(hotspot);
            if (state_.car.doorLock == DoorLockState::Broken || state_.car.doorLock == DoorLockState::ForcedOpen) {
                Push(EventType::Interaction, "The lock piece wiggles without moving the latch.", 0.35f);
            } else if (state_.car.doorLock == DoorLockState::Jammed || state_.car.doorLock == DoorLockState::FakeLocked || state_.car.falseLockState || state_.creature.mode == CreatureMode::Attacking) {
                state_.car.doorLock = DoorLockState::Locked;
                state_.car.falseLockState = false;
                state_.creature.attackCharge = std::max(0.0f, state_.creature.attackCharge - 0.36f);
                state_.creature.slowHuntTimer = std::max(0.0f, state_.creature.slowHuntTimer - 0.45f);
                state_.tension = Clamp(state_.tension - 5.0f, 0.0f, 100.0f);
                Push(EventType::Interaction, "You force the lock down. The door attack loses purchase.", 0.42f);
            } else {
                state_.car.doorLock = state_.car.doorLock == DoorLockState::Locked ? DoorLockState::Unlocked : DoorLockState::Locked;
                Push(EventType::Interaction, state_.car.doorLock == DoorLockState::Locked ? "The lock clicks down and almost feels useful." : "The lock rises with a brittle click.", 0.22f);
            }
            break;
        case Hotspot::DoorHandle:
            MarkCockpitFeedback(hotspot);
            if (state_.car.doorHandleBroken) {
                Push(EventType::Interaction, "The broken handle is cold in your palm. The empty socket faces the woods.", 0.42f);
            } else if (state_.distanceMiles > 2.75f || state_.tension > 70.0f) {
                state_.car.doorHandleBroken = true;
                state_.car.condition = Clamp(state_.car.condition - 6.0f, 0.0f, 100.0f);
                state_.tension += 12.0f;
                Push(EventType::Impact, "The handle snaps off and lands below the pedals.", 0.86f);
            } else {
                Push(EventType::Interaction, PickLine(kHandleLines), 0.32f);
            }
            break;
        case Hotspot::Glovebox:
            MarkCockpitFeedback(hotspot);
            state_.tension += 5.0f;
            Push(EventType::Interaction, PickLine(kGloveboxLines), 0.34f);
            break;
        case Hotspot::Mirror:
            MarkCockpitFeedback(hotspot);
            state_.creature.mirrorVisible = state_.tension > 36.0f;
            state_.creature.mode = state_.creature.mirrorVisible ? CreatureMode::Mirror : state_.creature.mode;
            if (state_.story.monsterSubtitleTimer > 0.0f || state_.car.falseLockState) {
                state_.story.monsterSubtitleTimer = std::max(state_.story.monsterSubtitleTimer, 1.6f);
                state_.car.falseLockState = false;
                state_.tension = Clamp(state_.tension - 4.0f, 0.0f, 100.0f);
                Push(EventType::Creature, "The mirror catches the lie before your hand leaves the wheel.", 0.38f);
            } else {
                Push(EventType::Creature, PickLine(kMirrorLines), state_.creature.mirrorVisible ? 0.64f : 0.24f);
            }
            break;
        case Hotspot::Headlights:
            MarkCockpitFeedback(hotspot);
            state_.car.headlightsOn = !state_.car.headlightsOn;
            Push(EventType::Interaction, state_.car.headlightsOn ? "The headlights catch and throw a weak cone down the road." : "The road vanishes beyond the windshield.", state_.car.headlightsOn ? 0.22f : 0.58f);
            break;
        case Hotspot::Ignition:
            MarkCockpitFeedback(hotspot);
            RestartEngine();
            break;
        case Hotspot::Horn:
            MarkCockpitFeedback(hotspot);
            SoundHorn();
            break;
        case Hotspot::Wipers:
            MarkCockpitFeedback(hotspot);
            state_.car.wipersOn = !state_.car.wipersOn;
            Push(EventType::Interaction, "The wipers smear the windshield into greasy arcs.", 0.24f);
            break;
        case Hotspot::WindowCrank:
            MarkCockpitFeedback(hotspot);
            state_.car.windowOpen = std::min(1.0f, state_.car.windowOpen + 0.18f);
            state_.tension += 4.0f;
            Push(EventType::Interaction, "The crank spins too freely. The window drops another inch.", 0.36f);
            break;
        case Hotspot::DomeLight:
            MarkCockpitFeedback(hotspot);
            state_.car.domeLightOn = !state_.car.domeLightOn;
            state_.car.battery = Clamp(state_.car.battery - 1.2f, 0.0f, 100.0f);
            Push(EventType::Power, state_.car.domeLightOn ? "The dome light flickers over the back seat." : "The cabin collapses back into amber darkness.", state_.car.domeLightOn ? 0.5f : 0.2f);
            break;
        case Hotspot::GearShift:
            if (state_.car.shifterSlipTimer > 0.0f || state_.car.shifterSlipping || state_.car.gear == 'N') {
                ReseatShifter();
                break;
            }
            MarkCockpitFeedback(hotspot);
            if (std::abs(state_.car.speedMph) < 8.0f) {
                state_.car.gear = state_.car.gear == 'R' ? 'D' : 'R';
                Push(EventType::Interaction, std::string("The shifter knocks into ") + state_.car.gear + ".", 0.22f);
            } else {
                state_.car.condition = Clamp(state_.car.condition - 2.0f, 0.0f, 100.0f);
                Push(EventType::Warning, "The gearbox grinds and refuses the shift.", 0.42f);
            }
            break;
    }

    if (!events_.empty()) {
        state_.story.lastInteraction = events_.back().message;
        state_.story.messageTimer = 4.0f;
    }
}

void Simulation::ChooseEnding(int choice) {
    if (state_.phase != GamePhase::Ending) {
        return;
    }

    if (choice == 2) {
        state_.story.ending = "Never Left the Road";
        state_.story.endingDetail = "The tower slides past the windshield. The road does not end.";
    } else if (choice == 1) {
        state_.story.ending = state_.car.doorHandleBroken ? "Wrong Signal" : "Signal Sent";
        state_.story.endingDetail = state_.car.doorHandleBroken
            ? "You step out through the passenger side. The radio begins broadcasting before you reach the tower."
            : "You leave the engine idling and send the warning through the tower relay.";
    } else if (state_.car.condition > 32.0f && state_.car.fuel > 12.0f && state_.car.battery > 16.0f) {
        state_.story.ending = "The Car Survives";
        state_.story.endingDetail = "The warning goes out. The car starts again on the first try.";
    } else {
        state_.story.ending = "Signal Sent";
        state_.story.endingDetail = "The broadcast leaves the tower as the car dies under the beacon.";
    }
}

std::vector<GameEvent> Simulation::DrainEvents() {
    std::vector<GameEvent> copy = events_;
    events_.clear();
    return copy;
}

void Simulation::Push(EventType type, std::string message, float intensity) {
    if (type == EventType::MonsterVoice) {
        state_.story.monsterSubtitle = message;
        state_.story.monsterSubtitleTimer = std::max(state_.story.monsterSubtitleTimer, 4.2f + intensity);
        state_.car.radioOn = true;
        state_.car.radioVolume = std::max(state_.car.radioVolume, 0.78f);
    }
    state_.story.warning = message;
    state_.story.messageTimer = std::max(state_.story.messageTimer, 3.2f + intensity);
    events_.push_back({type, std::move(message), intensity});
}

void Simulation::UpdateRoadContext() {
    const RoadSegment segment = RoadAt(state_.distanceMiles);
    auto [chapter, index] = ChapterAt(state_.distanceMiles);
    state_.world = WorldGenerator::ContextAt(state_.distanceMiles, state_.elapsed);
    state_.currentSegment = segment.kind;
    state_.roadWidth = segment.width;
    state_.roadCurvature = segment.curvature;
    state_.roadFog = Clamp(segment.fog + state_.world.stormIntensity * 0.24f + state_.world.townDensity * 0.08f, 0.0f, 0.82f);
    state_.chapter = chapter.chapter;
    state_.chapterIndex = index;
}

void Simulation::UpdateCar(const InputState& input, float dt) {
    CarState& car = state_.car;
    const float wetGrip = 1.0f - state_.world.wetness * 0.24f;
    const float segmentGrip = (state_.currentSegment == SegmentKind::Mud ? 0.62f : (state_.currentSegment == SegmentKind::Bridge ? 0.82f : 1.0f)) * wetGrip;
    const float powerDrain = (car.headlightsOn ? 0.7f : 0.12f) + (car.highBeams ? 1.1f : 0.0f) + (car.radioOn ? 0.28f : 0.0f) + (car.domeLightOn ? 0.2f : 0.0f) + state_.world.stormIntensity * 0.22f;

    car.highBeams = input.highBeams && car.headlightsOn && car.battery > 0.0f;

    if (input.toggleHeadlights) car.headlightsOn = !car.headlightsOn;
    if (input.toggleRadio) car.radioOn = !car.radioOn;
    if (input.toggleWipers) car.wipersOn = !car.wipersOn;
    car.domeLightOn = input.domeLight || car.domeLightOn;

    car.battery = Clamp(car.battery - powerDrain * dt * (car.highBeams ? 1.35f : 1.0f), 0.0f, 100.0f);
    car.headlightPower = Clamp(car.headlightPower + (car.headlightsOn ? -0.18f : 0.08f) * dt - (car.highBeams ? 0.62f * dt : 0.0f), 0.0f, 100.0f);
    if (state_.world.lightning > 0.55f && !state_.settings.reducedFlashing) {
        car.headlightPower = Clamp(car.headlightPower - 3.8f * dt, 0.0f, 100.0f);
        car.hoodRattle = std::max(car.hoodRattle, 0.45f);
    }
    car.fuel = Clamp(car.fuel - std::max(0.0f, std::abs(car.speedMph)) * 0.0018f * dt - (car.engineOn ? 0.004f * dt : 0.0f), 0.0f, 100.0f);

    if (car.battery < 4.0f && car.engineOn) {
        car.engineOn = false;
        car.stalled = true;
        car.fan = FanState::Stopped;
        Push(EventType::Stall, "The electronics die in one flat click.", 0.72f);
    }

    if (car.radioInterferenceTimer > 0.0f) {
        car.radioLoose = true;
        const float wobble = std::sin(state_.elapsed * 8.7f) * 0.55f + std::sin(state_.elapsed * 3.4f) * 0.35f;
        car.steeringInterference = Approach(car.steeringInterference, Clamp(car.steeringInterference + wobble * 0.018f, -0.82f, 0.82f), dt * 1.1f);
        car.radioVolume = std::max(car.radioVolume, 0.86f);
        if (std::fmod(state_.elapsed, 1.1f) < dt) {
            state_.story.warning = "The loose radio is pulling the wheel. Click it to stabilize.";
            state_.story.messageTimer = std::max(state_.story.messageTimer, 1.2f);
        }
    }

    if (car.shifterSlipTimer > 0.0f) {
        car.shifterSlipping = true;
        if (std::fmod(state_.elapsed + 0.37f, 1.25f) < dt) {
            state_.story.warning = "The shifter is slipping toward neutral. Re-seat it.";
            state_.story.messageTimer = std::max(state_.story.messageTimer, 1.2f);
        }
    }

    const float steerIntent = (input.steerRight ? 1.0f : 0.0f) - (input.steerLeft ? 1.0f : 0.0f);
    car.steering = Approach(car.steering, steerIntent, dt * 5.6f);
    const float roadBendDrift = state_.roadCurvature * std::max(0.0f, car.speedMph) * 0.0018f;
    const float radioPull = car.radioInterferenceTimer > 0.0f ? car.steeringInterference * std::max(0.25f, std::abs(car.speedMph) / 45.0f) : car.steeringInterference;
    car.lateralOffset += (car.steering * std::max(6.0f, std::abs(car.speedMph)) * 0.08f * segmentGrip + car.steeringBias + roadBendDrift + radioPull) * dt;
    car.lateralOffset = Approach(car.lateralOffset, 0.0f, dt * 0.12f);

    const bool shifterBlocked = car.shifterSlipTimer > 0.0f && car.shifterSettleTimer <= 0.0f;
    if (!car.engineOn || car.stalled) {
        car.speedMph = Approach(car.speedMph, 0.0f, dt * 13.0f);
    } else if (shifterBlocked) {
        car.gear = 'N';
        car.speedMph = Approach(car.speedMph, input.brake ? 0.0f : (input.accelerate ? 10.0f : 0.0f), dt * 10.0f);
        state_.tension = Clamp(state_.tension + 1.6f * dt, 0.0f, 100.0f);
    } else if (state_.reverseEscapeMeters > 0.0f) {
        const float target = input.brake ? kMaxReverseSpeed : (input.accelerate ? 12.0f : 0.0f);
        car.gear = input.brake ? 'R' : 'D';
        car.speedMph = Approach(car.speedMph, target, dt * (input.brake ? 24.0f : 14.0f));
    } else if (input.brake && std::abs(car.speedMph) < 2.0f && !input.accelerate) {
        car.gear = 'R';
        car.speedMph = Approach(car.speedMph, kMaxReverseSpeed * 0.7f, dt * 16.0f);
    } else {
        car.gear = car.speedMph < -1.0f ? 'R' : 'D';
        const float targetSpeed = input.accelerate ? kMaxForwardSpeed - state_.tension * 0.13f : (input.brake ? 0.0f : 18.0f);
        car.speedMph = Approach(car.speedMph, targetSpeed, dt * (input.accelerate ? 12.0f : 8.0f));
    }

    if (input.handbrake) {
        car.speedMph = Approach(car.speedMph, 0.0f, dt * 28.0f);
        car.lateralOffset += car.steering * std::abs(car.speedMph) * 0.035f * dt;
    }

    if (car.speedMph > 49.0f && state_.currentSegment != SegmentKind::Straight && state_.currentSegment != SegmentKind::Tower) {
        car.condition = Clamp(car.condition - (car.speedMph - 49.0f) * 0.014f * dt, 0.0f, 100.0f);
        state_.tension = Clamp(state_.tension + 0.9f * dt, 0.0f, 100.0f);
    }

    car.fan = DeriveFan(car.fan, car.battery, car.engineOn, state_.tension);
    car.radioVolume = Approach(car.radioVolume, car.radioOn ? Clamp(0.2f + state_.tension / 120.0f, 0.2f, 1.0f) : 0.0f, dt * 1.5f);
    car.hoodRattle = std::max(0.0f, car.hoodRattle - dt);
}

void Simulation::UpdateDistance(float dt) {
    if (state_.reverseEscapeMeters > 0.0f) {
        if (state_.car.speedMph < -3.0f) {
            state_.reverseEscapeMeters = std::max(0.0f, state_.reverseEscapeMeters - std::abs(state_.car.speedMph) * 0.45f * dt);
        }
        if (state_.reverseEscapeMeters <= 0.0f) {
            state_.distanceMiles += 0.08f;
            state_.hazards[3].cleared = true;
            state_.creature.mode = CreatureMode::Retreating;
            Push(EventType::Notice, "The road opens past the fallen trunk.", 0.3f);
        }
        return;
    }

    state_.distanceMiles = Clamp(state_.distanceMiles + std::max(0.0f, state_.car.speedMph) * kSpeedToMilesPerSecond * dt, 0.0f, state_.towerDistanceMiles);
}

void Simulation::UpdateDirector(float dt) {
    (void)dt;
    for (size_t i = 0; i < kBeats.size(); ++i) {
        if (!state_.completedBeats[i] && state_.distanceMiles >= kBeats[i].distanceMiles) {
            TriggerBeat(i);
        }
    }

    if (Random01() < 0.005f && state_.cooldown <= 0.0f && state_.timeSinceScare > 9.0f && state_.distanceMiles > 0.25f) {
        static const std::vector<const char*> minor {
            "dashboard rattle", "distant branch snap", "static tick", "door rubber creak"
        };
        state_.tension = Clamp(state_.tension + 4.0f, 0.0f, 100.0f);
        state_.cooldown = 5.0f;
        Push(EventType::Notice, std::string("A ") + PickLine(minor) + " cuts through the engine noise.", 0.22f);
    }

    const float calmRate = state_.cooldown > 0.0f ? 0.9f : 0.22f;
    state_.tension = Clamp(state_.tension - calmRate * 0.016f, 0.0f, 100.0f);
}

void Simulation::UpdateStormAndVoices(float dt) {
    (void)dt;
    if (state_.world.stormIntensity > 0.35f) {
        state_.tension = Clamp(state_.tension + state_.world.stormIntensity * 0.7f * dt, 0.0f, 100.0f);
        if (state_.stormEventCooldown <= 0.0f && state_.world.lightning > 0.65f) {
            state_.stormEventCooldown = 4.5f + Random01() * 3.0f;
            state_.car.radioOn = true;
            state_.car.radioVolume = std::max(state_.car.radioVolume, 0.72f);
            state_.car.headlightPower = Clamp(state_.car.headlightPower - 2.0f, 0.0f, 100.0f);
            Push(EventType::Thunder, "Thunder rolls over the hood and the headlights twitch.", 0.78f);
            if (Random01() < 0.62f) {
                TriggerRadioInterference(0.62f, "The thunder kicks the radio loose. The steering starts to pull.");
            } else {
                TriggerShifterSlip("The console jumps under your hand. The shifter is not seated.");
            }
        }
    }

    if (state_.world.townDensity > 0.0f && std::abs(state_.car.speedMph) < 18.0f) {
        state_.tension = Clamp(state_.tension + state_.world.townDensity * 0.55f * dt, 0.0f, 100.0f);
    }

    if (state_.world.monsterVoiceWindow && state_.monsterVoiceCooldown <= 0.0f && state_.timeSinceScare > 4.0f) {
        state_.monsterVoiceCooldown = 8.0f + Random01() * 5.0f;
        state_.timeSinceScare = 0.0f;
        Push(EventType::MonsterVoice, PickLine(kMonsterVoiceLines), 0.74f);
        TriggerRadioInterference(0.48f, "The voice rattles the radio loose. Hold the lane and click it stable.");
        if (Random01() < 0.36f) {
            state_.car.falseLockState = true;
            state_.car.doorLock = DoorLockState::FakeLocked;
            Push(EventType::Warning, "The lock pin looks down, but the mirror says it is lying.", 0.44f);
        }
    }
}

void Simulation::TriggerBeat(size_t index) {
    const BeatDef& beat = kBeats[index];
    state_.completedBeats[index] = true;
    state_.tension = Clamp(std::max(state_.tension, beat.tension), 0.0f, 100.0f);
    state_.cooldown = beat.tension > 60.0f ? 6.0f : 3.0f;
    state_.timeSinceScare = 0.0f;
    Push(BeatEventType(beat.kind), beat.message, beat.tension / 100.0f);

    switch (beat.kind) {
        case 0:
        case 3:
            state_.car.radioOn = true;
            state_.car.radioVolume = beat.kind == 3 ? 0.9f : 0.48f;
            state_.story.radioSubtitle = kRadioLines[beat.kind == 3 ? 3 : 0];
            if (beat.kind == 3) {
                TriggerRadioInterference(0.42f, "The console speaker bucks in its mount. The wheel leans with the static.");
            }
            break;
        case 1:
            state_.creature.mode = CreatureMode::Watching;
            state_.creature.eyesVisible = true;
            state_.creature.variant = 0;
            state_.creature.side = 1;
            state_.creature.distanceAhead = 64.0f;
            state_.creature.lateral = 9.0f;
            state_.creature.visibility = 0.75f;
            break;
        case 2:
            state_.car.fan = FanState::Slow;
            state_.car.battery = std::min(state_.car.battery, 42.0f);
            state_.car.headlightPower = std::min(state_.car.headlightPower, 55.0f);
            break;
        case 4:
            state_.creature.mode = CreatureMode::Crossing;
            state_.creature.variant = 2;
            state_.creature.side = -1;
            state_.creature.distanceAhead = 38.0f;
            state_.creature.lateral = -7.0f;
            state_.creature.visibility = 0.95f;
            break;
        case 5:
            state_.car.engineOn = false;
            state_.car.stalled = true;
            state_.car.fan = FanState::Stopped;
            state_.car.radioOn = true;
            state_.creature.mode = CreatureMode::Pacing;
            state_.creature.variant = 1;
            state_.creature.distanceAhead = 24.0f;
            state_.creature.visibility = 0.55f;
            state_.story.subObjective = "Hold R or click ignition to restart";
            break;
        case 6:
            state_.car.doorLock = DoorLockState::Jammed;
            state_.creature.mode = CreatureMode::Attacking;
            state_.creature.variant = 1;
            state_.creature.attackCharge = 0.52f;
            state_.creature.side = 1;
            state_.creature.distanceAhead = 8.0f;
            state_.creature.visibility = 0.8f;
            break;
        case 7:
            state_.creature.mode = CreatureMode::Mirror;
            state_.creature.variant = 3;
            state_.creature.mirrorVisible = true;
            break;
        case 8:
            state_.car.doorLock = state_.car.doorLock == DoorLockState::Locked ? DoorLockState::FakeLocked : DoorLockState::Jammed;
            state_.car.falseLockState = state_.car.doorLock == DoorLockState::FakeLocked;
            state_.car.condition = Clamp(state_.car.condition - 3.0f, 0.0f, 100.0f);
            break;
        case 9:
            Push(EventType::Interaction, "The handle is no longer trustworthy.", 0.45f);
            break;
        case 10:
            state_.car.doorHandleBroken = true;
            state_.car.condition = Clamp(state_.car.condition - 6.0f, 0.0f, 100.0f);
            state_.car.domeLightOn = true;
            state_.car.fan = FanState::Irregular;
            TriggerShifterSlip("The broken handle drops into the console and knocks the shifter loose.");
            break;
        case 11:
            state_.reverseEscapeMeters = 38.0f;
            state_.car.speedMph = 0.0f;
            state_.story.subObjective = "Hold S to reverse from the blocked road";
            state_.creature.mode = CreatureMode::Lunging;
            state_.creature.variant = 0;
            state_.creature.distanceAhead = 22.0f;
            state_.creature.visibility = 1.0f;
            break;
        case 12:
            state_.car.radioOn = true;
            state_.car.radioVolume = 1.0f;
            state_.car.headlightPower = std::min(state_.car.headlightPower, 24.0f);
            state_.car.battery = std::min(state_.car.battery, 28.0f);
            state_.creature.mode = CreatureMode::Attacking;
            state_.creature.variant = 3;
            state_.creature.visibility = 1.0f;
            break;
        default:
            break;
    }
}

void Simulation::UpdateJumpscare(float dt) {
    state_.jumpscareTimer += dt;
    const float t = Clamp(state_.jumpscareTimer / std::max(0.01f, state_.jumpscareDuration), 0.0f, 1.0f);

    state_.car.speedMph = Approach(state_.car.speedMph, 0.0f, dt * 96.0f);
    state_.car.engineOn = false;
    state_.car.stalled = true;
    state_.car.fan = FanState::Stopped;
    state_.car.radioOn = true;
    state_.car.radioVolume = 1.0f;
    state_.car.doorLock = DoorLockState::ForcedOpen;
    state_.car.headlightPower = Clamp(state_.car.headlightPower - dt * 18.0f, 0.0f, 100.0f);
    state_.car.hoodRattle = 2.8f;

    state_.tension = 100.0f;
    state_.story.objective = "Monster breach";
    state_.story.subObjective = t < 0.72f ? "Too slow" : "It is inside";
    state_.story.messageTimer = std::max(state_.story.messageTimer, 0.12f);

    CreatureState& creature = state_.creature;
    creature.mode = CreatureMode::Jumpscare;
    creature.visibility = 1.0f;
    creature.mirrorVisible = false;
    creature.eyesVisible = true;
    creature.attackCharge = t;
    creature.slowHuntTimer = std::max(creature.slowHuntTimer, 3.0f);
    creature.distanceAhead = Approach(creature.distanceAhead, 0.28f + (1.0f - t) * 1.28f, dt * 7.5f);
    creature.lateral = state_.car.lateralOffset + std::sin(state_.elapsed * 29.0f) * (0.18f * (1.0f - t));

    if (state_.jumpscareTimer >= state_.jumpscareDuration) {
        Kill("You slowed down. It breaks through the glass and drags you into the dark.");
    }
}

void Simulation::UpdateCreature(const InputState& input, float dt) {
    CreatureState& creature = state_.creature;
    CarState& car = state_.car;
    const float lightPressure = car.headlightsOn ? (car.highBeams ? 1.2f : 0.62f) : 0.08f;
    const float hornPressure = input.horn ? 0.8f : 0.0f;
    const float speedPressure = std::max(0.0f, car.speedMph - 32.0f) / 40.0f;
    const float repel = lightPressure + hornPressure + speedPressure;

    if (creature.mode == CreatureMode::Hidden && state_.tension > 72.0f && state_.distanceMiles > 0.55f && car.speedMph < 10.0f) {
        creature.mode = CreatureMode::Attacking;
        creature.variant = static_cast<int>((state_.randomSeed >> 9u) % 4u);
        creature.visibility = 0.72f;
        creature.distanceAhead = 13.0f;
        creature.lateral = static_cast<float>(creature.side) * 4.4f;
        creature.attackCharge = 0.28f;
        Push(EventType::Warning, "The quiet lets it find the car.", 0.72f);
    }

    const bool hostile =
        creature.mode == CreatureMode::Watching ||
        creature.mode == CreatureMode::Pacing ||
        creature.mode == CreatureMode::Crossing ||
        creature.mode == CreatureMode::Lunging ||
        creature.mode == CreatureMode::Attacking;
    if (hostile) {
        const bool blockedRoad = state_.reverseEscapeMeters > 0.0f;
        const bool dangerouslySlow = blockedRoad ? car.speedMph > -4.0f : car.speedMph < 12.0f;
        const bool pullingAway = blockedRoad ? car.speedMph < -7.0f : car.speedMph > 24.0f;
        const float before = creature.slowHuntTimer;
        if (dangerouslySlow && repel < 1.95f) {
            creature.slowHuntTimer = Clamp(creature.slowHuntTimer + dt * (car.speedMph < 3.0f ? 1.45f : 1.0f), 0.0f, 3.2f);
            state_.story.warning = blockedRoad ? "Reverse now. It is closing." : "Keep moving. It is closing.";
            state_.story.messageTimer = std::max(state_.story.messageTimer, 1.0f);
            if (before <= 0.65f && creature.slowHuntTimer > 0.65f) {
                Push(EventType::Warning, "The creature moves when the engine drops.", 0.72f);
            }
            if (creature.slowHuntTimer > 0.95f && creature.mode != CreatureMode::Attacking && creature.mode != CreatureMode::Lunging) {
                creature.mode = CreatureMode::Attacking;
                creature.visibility = std::max(creature.visibility, 0.85f);
                creature.distanceAhead = std::min(creature.distanceAhead, 9.5f);
                creature.lateral = Approach(creature.lateral, static_cast<float>(creature.side) * 1.8f, dt * 4.0f);
                creature.attackCharge = std::max(creature.attackCharge, 0.64f);
            }
            if (creature.slowHuntTimer >= 2.65f) {
                BeginJumpscare(blockedRoad ? "You wait too long at the blocked road." : "You let the car get too quiet.");
                return;
            }
        } else {
            creature.slowHuntTimer = Approach(creature.slowHuntTimer, 0.0f, dt * (pullingAway ? 1.8f : 0.7f));
        }
    }

    switch (creature.mode) {
        case CreatureMode::Hidden:
            creature.visibility = Approach(creature.visibility, 0.0f, dt * 0.8f);
            break;
        case CreatureMode::Watching:
            creature.visibility = Approach(creature.visibility, 0.74f, dt * 0.7f);
            creature.distanceAhead = Approach(creature.distanceAhead, 52.0f, dt * 8.0f);
            if (state_.tension > 42.0f && state_.cooldown <= 0.0f) creature.mode = CreatureMode::Pacing;
            break;
        case CreatureMode::Pacing:
            creature.visibility = Approach(creature.visibility, 0.42f + state_.tension / 180.0f, dt * 0.9f);
            creature.distanceAhead = Approach(creature.distanceAhead, repel > 1.2f ? 44.0f : 18.0f, dt * 7.0f);
            creature.lateral = Approach(creature.lateral, static_cast<float>(creature.side) * (repel > 1.2f ? 12.0f : 5.6f), dt * 2.0f);
            break;
        case CreatureMode::Crossing:
            creature.lateral += dt * 20.0f;
            creature.distanceAhead = Approach(creature.distanceAhead, 20.0f, dt * 22.0f);
            creature.visibility = Approach(creature.visibility, 1.0f, dt * 2.0f);
            if (creature.lateral > 7.5f) creature.mode = CreatureMode::Retreating;
            break;
        case CreatureMode::Mirror:
            creature.mirrorVisible = true;
            creature.visibility = Approach(creature.visibility, 0.78f, dt * 1.2f);
            if (state_.cooldown <= 0.0f && state_.tension < 38.0f) {
                creature.mode = CreatureMode::Hidden;
                creature.mirrorVisible = false;
            }
            break;
        case CreatureMode::Lunging:
            creature.visibility = Approach(creature.visibility, 1.0f, dt * 3.0f);
            if (state_.reverseEscapeMeters > 0.0f && car.speedMph < -6.0f) {
                creature.distanceAhead = Approach(creature.distanceAhead, 11.0f, dt * 19.0f);
                creature.attackCharge += dt * -1.0f;
            } else {
                creature.distanceAhead = Approach(creature.distanceAhead, 1.25f, dt * 24.0f);
                creature.attackCharge += dt * (repel > 1.75f ? -0.95f : (car.speedMph < 16.0f ? 0.92f : 0.48f));
            }
            if (creature.attackCharge > 1.0f || creature.distanceAhead < 1.65f) {
                car.condition = Clamp(car.condition - 12.0f, 0.0f, 100.0f);
                BeginJumpscare("It reaches the windshield before you pull away.");
                return;
            } else if (creature.attackCharge < -0.2f) {
                creature.mode = CreatureMode::Retreating;
                creature.slowHuntTimer = 0.0f;
                Push(EventType::Creature, "The high beams drive it back into the trees.", 0.42f);
            }
            break;
        case CreatureMode::Attacking:
            creature.visibility = Approach(creature.visibility, 1.0f, dt * 1.7f);
            creature.distanceAhead = Approach(creature.distanceAhead, car.speedMph > 28.0f ? 12.0f : 1.9f, dt * 13.0f);
            creature.lateral = Approach(creature.lateral, state_.car.lateralOffset + static_cast<float>(creature.side) * 0.45f, dt * 2.7f);
            creature.attackCharge += dt * (car.speedMph < 14.0f ? 0.64f : -0.22f);
            if (creature.attackCharge > 1.25f || creature.distanceAhead < 2.05f) {
                BeginJumpscare("The door gives way before the engine catches.");
                return;
            } else if (repel > 1.8f || car.speedMph > 42.0f) {
                creature.mode = CreatureMode::Retreating;
                creature.attackCharge = 0.0f;
                creature.slowHuntTimer = 0.0f;
                Push(EventType::Creature, "The creature breaks away from the headlight cone.", 0.5f);
            }
            break;
        case CreatureMode::Jumpscare:
            break;
        case CreatureMode::Retreating:
            creature.visibility = Approach(creature.visibility, 0.0f, dt * 1.5f);
            creature.distanceAhead = Approach(creature.distanceAhead, 72.0f, dt * 12.0f);
            if (creature.visibility < 0.05f) {
                creature.mode = CreatureMode::Hidden;
                creature.eyesVisible = false;
                creature.mirrorVisible = false;
                creature.slowHuntTimer = 0.0f;
            }
            break;
    }
}

void Simulation::UpdateHazards() {
    if (state_.treeImpactCooldown <= 0.0f) {
    if (auto obstacle = WorldGenerator::FindRoadObstacle(state_.distanceMiles, state_.car.lateralOffset)) {
        if (state_.car.speedMph > 16.0f) {
            state_.treeImpactCooldown = 1.25f;
            state_.car.condition = Clamp(state_.car.condition - 13.0f, 0.0f, 100.0f);
            state_.car.speedMph *= 0.42f;
            state_.car.hoodRattle = 1.6f;
            state_.tension = Clamp(state_.tension + 15.0f, 0.0f, 100.0f);
            Push(EventType::Impact, "The car punches through a barricade in the abandoned street.", 0.72f);
            TriggerShifterSlip("The impact knocks the shifter half out of gear.");
        }
    }
    }

    for (RoadHazard& hazard : state_.hazards) {
        if (hazard.cleared || hazard.checked) continue;
        const float relative = hazard.distanceMiles - state_.distanceMiles;
        if (relative > 0.035f || relative < -0.03f) continue;

        if (hazard.kind == 3) {
            if (state_.reverseEscapeMeters <= 0.0f && state_.distanceMiles > 3.32f) {
                state_.reverseEscapeMeters = 38.0f;
                state_.car.speedMph = 0.0f;
                state_.story.subObjective = "Hold S to reverse from the blocked road";
                Push(EventType::Warning, "The road is blocked by a fallen trunk.", 0.72f);
            }
            hazard.checked = true;
            continue;
        }

        const bool overlap = std::abs(state_.car.lateralOffset - hazard.laneOffset) < hazard.width;
        if (overlap) {
            const float damage = hazard.kind == 0 ? 4.0f : (hazard.kind == 4 ? 10.0f : 9.0f);
            state_.car.condition = Clamp(state_.car.condition - damage, 0.0f, 100.0f);
            state_.car.speedMph *= hazard.kind == 0 ? 0.72f : 0.5f;
            state_.tension = Clamp(state_.tension + 10.0f, 0.0f, 100.0f);
            Push(EventType::Impact, hazard.kind == 0 ? "The tires slap through deep mud and the steering goes soft." : "The car hits something hard in the dark.", 0.55f);
            if (hazard.kind == 0) {
                TriggerRadioInterference(0.26f, "The mud shakes the radio loose. The wheel starts pulling.");
            } else {
                TriggerShifterSlip("The impact kicks the shifter toward neutral.");
            }
        } else if (hazard.kind != 0) {
            Push(EventType::Notice, "The obstacle slips past the fender.", 0.18f);
        }
        hazard.cleared = true;
        hazard.checked = true;
    }
}

void Simulation::UpdatePressure(const InputState& input, float dt) {
    CarState& car = state_.car;
    const float edge = state_.roadWidth * 0.5f;
    if (std::abs(car.lateralOffset) > edge) {
        state_.roadDepartureTime += dt;
        state_.story.warning = "Tires are leaving the road";
        car.condition = Clamp(car.condition - 3.2f * dt, 0.0f, 100.0f);
        state_.tension = Clamp(state_.tension + 6.0f * dt, 0.0f, 100.0f);
        if (state_.treeImpactCooldown <= 0.0f && std::abs(car.lateralOffset) > edge + 0.65f) {
            if (auto tree = WorldGenerator::FindTreeImpact(state_.distanceMiles, car.lateralOffset, state_.roadWidth)) {
                HandleTreeImpact(*tree);
                return;
            }
        }
        if (state_.roadDepartureTime > 1.25f && state_.crashGraceTime <= 0.0f) {
            Kill("The car drops off the road into the trees.");
        }
    } else {
        state_.roadDepartureTime = std::max(0.0f, state_.roadDepartureTime - dt * 2.0f);
    }

    if (!car.engineOn && car.speedMph < 1.0f && state_.creature.mode != CreatureMode::Hidden) {
        state_.tension = Clamp(state_.tension + 6.0f * dt, 0.0f, 100.0f);
    }

    if (car.condition < 26.0f && car.engineOn && std::fmod(state_.elapsed, 3.0f) < dt) {
        car.speedMph *= 0.92f;
        car.hoodRattle = std::max(car.hoodRattle, 0.5f);
        Push(EventType::Warning, "The engine stumbles under the hood.", 0.36f);
    }

    if (car.fuel <= 0.0f) {
        car.engineOn = false;
        car.stalled = true;
        Kill("The tank runs dry before the tower appears.");
    }

    if (input.highBeams && car.battery < 18.0f && std::fmod(state_.elapsed, 2.0f) < dt) {
        car.headlightPower = std::max(0.0f, car.headlightPower - 1.5f);
        Push(EventType::Power, "High beams pull too hard from the dying battery.", 0.3f);
    }
}

void Simulation::UpdateObjective() {
    const auto [chapter, index] = ChapterAt(state_.distanceMiles);
    (void)index;
    state_.story.objective = chapter.objective;

    if (state_.jumpscareActive) {
        state_.story.objective = "Monster breach";
        state_.story.subObjective = "It is inside";
    } else if (state_.car.radioInterferenceTimer > 0.0f) {
        state_.story.subObjective = "Stabilize the radio";
    } else if (state_.car.shifterSlipTimer > 0.0f) {
        state_.story.subObjective = "Re-seat the shifter";
    } else if (state_.creature.slowHuntTimer > 0.25f) {
        std::ostringstream text;
        text << "Move now - " << static_cast<int>(std::ceil((2.65f - state_.creature.slowHuntTimer) * 10.0f)) / 10.0f << " s";
        state_.story.subObjective = text.str();
    } else if (state_.reverseEscapeMeters > 0.0f) {
        std::ostringstream text;
        text << "Reverse " << static_cast<int>(std::ceil(state_.reverseEscapeMeters)) << " m";
        state_.story.subObjective = text.str();
    } else if (state_.car.stalled) {
        state_.story.subObjective = "Restart ignition";
    } else if (state_.car.condition < 30.0f) {
        state_.story.subObjective = "Preserve the car";
    } else if (state_.creature.mode == CreatureMode::Attacking || state_.creature.mode == CreatureMode::Lunging) {
        state_.story.subObjective = "Use speed, horn, or high beams";
    } else if (state_.world.stormIntensity > 0.4f) {
        state_.story.subObjective = "Hold the wet road through the storm";
    } else if (state_.world.townDensity > 0.2f) {
        state_.story.subObjective = "Cross " + std::string(WorldGenerator::ZoneName(state_.world.zone));
    } else {
        std::ostringstream text;
        text.setf(std::ios::fixed);
        text.precision(2);
        text << std::max(0.0f, state_.towerDistanceMiles - state_.distanceMiles) << " mi remaining";
        state_.story.subObjective = text.str();
    }
}

void Simulation::CheckDeathAndEnding() {
    if (state_.phase != GamePhase::Playing) return;
    if (state_.jumpscareActive) return;
    if (state_.car.condition <= 0.0f) {
        Kill("The car folds around the last impact.");
        return;
    }
    if (state_.distanceMiles >= state_.towerDistanceMiles) {
        state_.phase = GamePhase::Ending;
        state_.car.speedMph = 0.0f;
        state_.story.objective = "Choose what happens at the tower";
        state_.story.subObjective = "The signal is live";
        Push(EventType::Ending, "The radio tower fills the windshield.", 0.7f);
    }
}

void Simulation::RestartEngine() {
    CarState& car = state_.car;
    if (!car.stalled && car.engineOn) {
        Push(EventType::Interaction, "The ignition coughs against an engine already running.", 0.2f);
        return;
    }

    const float chance = 0.32f + car.condition / 220.0f + car.battery / 260.0f - state_.tension / 240.0f - (state_.creature.mode == CreatureMode::Attacking ? 0.14f : 0.0f);
    if (Random01() < chance) {
        car.engineOn = true;
        car.stalled = false;
        car.fan = car.battery > 20.0f ? FanState::Irregular : FanState::Slow;
        car.battery = Clamp(car.battery - 2.2f, 0.0f, 100.0f);
        state_.tension = Clamp(state_.tension - 12.0f, 0.0f, 100.0f);
        Push(EventType::Power, "The engine catches after a hard metallic cough.", 0.48f);
    } else {
        car.battery = Clamp(car.battery - 4.5f, 0.0f, 100.0f);
        state_.tension = Clamp(state_.tension + 8.0f, 0.0f, 100.0f);
        Push(EventType::Stall, "The starter whines without catching.", 0.58f);
    }
}

void Simulation::SoundHorn() {
    state_.car.battery = Clamp(state_.car.battery - 0.4f, 0.0f, 100.0f);
    const bool weak = state_.car.battery < 24.0f || state_.tension > 75.0f;
    Push(EventType::Interaction, weak ? "The horn croaks once and dies." : "The horn blasts through the trees.", weak ? 0.3f : 0.42f);
}

void Simulation::TriggerRadioInterference(float strength, std::string message) {
    CarState& car = state_.car;
    car.radioOn = true;
    car.radioLoose = true;
    car.radioVolume = std::max(car.radioVolume, 0.86f);
    car.radioInterferenceTimer = std::max(car.radioInterferenceTimer, 3.2f + strength * 2.4f);
    const float sign = (Random01() < 0.5f) ? -1.0f : 1.0f;
    car.steeringInterference = sign * Clamp(0.16f + strength * 0.38f, 0.12f, 0.72f);
    car.hoodRattle = std::max(car.hoodRattle, 0.55f + strength * 0.35f);
    car.lastCockpitControl = Hotspot::Radio;
    state_.tension = Clamp(state_.tension + 4.0f + strength * 5.0f, 0.0f, 100.0f);
    Push(EventType::Warning, std::move(message), 0.42f + strength * 0.36f);
}

void Simulation::TriggerShifterSlip(std::string message) {
    CarState& car = state_.car;
    car.shifterSlipping = true;
    car.shifterSlipTimer = std::max(car.shifterSlipTimer, 3.4f);
    car.shifterSettleTimer = 0.0f;
    car.lastCockpitControl = Hotspot::GearShift;
    car.hoodRattle = std::max(car.hoodRattle, 0.75f);
    state_.tension = Clamp(state_.tension + 7.0f, 0.0f, 100.0f);
    Push(EventType::Warning, std::move(message), 0.62f);
}

void Simulation::StabilizeRadio() {
    CarState& car = state_.car;
    MarkCockpitFeedback(Hotspot::Radio);
    car.radioInterferenceTimer = 0.0f;
    car.steeringInterference = 0.0f;
    car.radioLoose = false;
    car.radioVolume = std::min(car.radioVolume, 0.34f);
    state_.tension = Clamp(state_.tension - 8.0f, 0.0f, 100.0f);
    Push(EventType::Interaction, "You palm the radio back into its bracket. The wheel steadies.", 0.36f);
}

void Simulation::ReseatShifter() {
    CarState& car = state_.car;
    MarkCockpitFeedback(Hotspot::GearShift);
    car.shifterSlipTimer = 0.0f;
    car.shifterSettleTimer = 1.2f;
    car.shifterSlipping = false;
    if (car.gear == 'N') {
        car.gear = state_.reverseEscapeMeters > 0.0f ? 'R' : 'D';
    }
    state_.tension = Clamp(state_.tension - 6.0f, 0.0f, 100.0f);
    Push(EventType::Interaction, "You slam the shifter back into its gate. Power returns cleanly.", 0.34f);
}

void Simulation::MarkCockpitFeedback(Hotspot hotspot) {
    state_.car.lastCockpitControl = hotspot;
    state_.car.cockpitFeedbackTimer = 0.65f;
}

void Simulation::HandleTreeImpact(const WorldProp& prop) {
    (void)prop;
    state_.treeImpactCooldown = 2.5f;
    state_.roadDepartureTime = 0.0f;
    const float speed = std::abs(state_.car.speedMph);
    const bool lethal = speed > 31.0f || state_.car.condition < 24.0f;
    state_.lastTreeImpactLethal = lethal;
    state_.car.hoodRattle = lethal ? 3.4f : 1.9f;
    state_.car.speedMph = 0.0f;
    state_.car.engineOn = false;
    state_.car.stalled = true;
    state_.car.fan = FanState::Stopped;
    state_.car.condition = Clamp(state_.car.condition - (lethal ? 54.0f : 22.0f), 0.0f, 100.0f);
    state_.tension = 100.0f;

    if (lethal) {
        state_.creature.variant = 1;
        BeginJumpscare("The tree caves in the windshield. Something waiting outside climbs through.");
    } else {
        state_.creature.mode = CreatureMode::Attacking;
        state_.creature.variant = 1;
        state_.creature.visibility = 1.0f;
        state_.creature.distanceAhead = 5.0f;
        state_.creature.attackCharge = 0.72f;
        state_.creature.slowHuntTimer = 1.3f;
        Push(EventType::Impact, "The car cracks into a tree. The engine dies and something answers.", 0.86f);
        TriggerShifterSlip("The floor buckles around the shifter. Re-seat it after the ignition catches.");
        TriggerRadioInterference(0.52f, "The crash throws the radio loose and drags the steering off line.");
    }
}

void Simulation::BeginJumpscare(std::string message) {
    if (state_.phase != GamePhase::Playing || state_.jumpscareActive) return;

    state_.jumpscareActive = true;
    state_.jumpscareTimer = 0.0f;
    state_.jumpscareDuration = state_.settings.reducedMotion ? 2.15f : 2.55f;
    state_.cooldown = state_.jumpscareDuration;
    state_.timeSinceScare = 0.0f;
    state_.tension = 100.0f;

    state_.car.speedMph = 0.0f;
    state_.car.engineOn = false;
    state_.car.stalled = true;
    state_.car.fan = FanState::Stopped;
    state_.car.doorLock = DoorLockState::ForcedOpen;
    state_.car.radioOn = true;
    state_.car.radioVolume = 1.0f;
    state_.car.hoodRattle = 2.8f;

    state_.creature.mode = CreatureMode::Jumpscare;
    state_.creature.visibility = 1.0f;
    state_.creature.attackCharge = 0.0f;
    state_.creature.slowHuntTimer = 3.0f;
    state_.creature.distanceAhead = std::min(state_.creature.distanceAhead, 1.8f);
    state_.creature.lateral = state_.car.lateralOffset;
    state_.creature.mirrorVisible = false;
    state_.creature.eyesVisible = true;

    state_.story.objective = "Monster breach";
    state_.story.subObjective = "Too slow";
    state_.story.warning = message;
    state_.story.messageTimer = state_.jumpscareDuration;
    Push(EventType::Jumpscare, std::move(message), 1.0f);
}

void Simulation::Kill(std::string message) {
    if (state_.phase == GamePhase::Dead) return;
    state_.jumpscareActive = false;
    state_.jumpscareTimer = state_.jumpscareDuration;
    state_.phase = GamePhase::Dead;
    state_.car.speedMph = 0.0f;
    state_.car.engineOn = false;
    state_.car.fan = FanState::Stopped;
    state_.story.warning = message;
    Push(EventType::Death, std::move(message), 1.0f);
}

float Simulation::Random01() {
    state_.randomSeed = state_.randomSeed * 1664525u + 1013904223u;
    return static_cast<float>(state_.randomSeed) / static_cast<float>(0xffffffffu);
}

std::string Simulation::PickLine(const std::vector<const char*>& lines) {
    if (lines.empty()) return {};
    const size_t index = std::min(lines.size() - 1, static_cast<size_t>(Random01() * static_cast<float>(lines.size())));
    return lines[index];
}

const char* Simulation::ChapterName(Chapter chapter) {
    for (const ChapterDef& entry : kChapters) {
        if (entry.chapter == chapter) return entry.name;
    }
    return "Unknown";
}

const char* Simulation::SegmentName(SegmentKind segment) {
    switch (segment) {
        case SegmentKind::Straight: return "Straight forest road";
        case SegmentKind::Bend: return "Blind bend";
        case SegmentKind::Mud: return "Muddy stretch";
        case SegmentKind::Bridge: return "Narrow bridge";
        case SegmentKind::FallenTree: return "Fallen branches";
        case SegmentKind::Fog: return "Fog pocket";
        case SegmentKind::Checkpoint: return "Abandoned checkpoint";
        case SegmentKind::Tower: return "Tower approach";
    }
    return "Road";
}

const char* Simulation::FanName(FanState fan) {
    switch (fan) {
        case FanState::Normal: return "normal";
        case FanState::Slow: return "slowing";
        case FanState::Stopped: return "stopped";
        case FanState::Irregular: return "irregular";
    }
    return "unknown";
}

const char* Simulation::LockName(DoorLockState lock) {
    switch (lock) {
        case DoorLockState::Unlocked: return "unlocked";
        case DoorLockState::Locked: return "locked";
        case DoorLockState::Jammed: return "jammed";
        case DoorLockState::FakeLocked: return "fake locked";
        case DoorLockState::Broken: return "broken";
        case DoorLockState::ForcedOpen: return "forced";
    }
    return "unknown";
}

float Clamp(float value, float minimum, float maximum) {
    return std::max(minimum, std::min(value, maximum));
}

float Approach(float value, float target, float amount) {
    if (value < target) return std::min(target, value + amount);
    return std::max(target, value - amount);
}

} // namespace ld
