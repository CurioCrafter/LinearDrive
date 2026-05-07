#pragma once

#include "game/Input.hpp"
#include "game/WorldGenerator.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ld {

enum class GamePhase {
    Menu,
    Settings,
    Tutorial,
    Playing,
    Paused,
    Dead,
    Ending
};

enum class Chapter {
    MainRoad,
    ElectricalProblems,
    FirstBreakdown,
    RoadNarrows,
    HandleComesOff,
    RadioTower
};

enum class SegmentKind {
    Straight,
    Bend,
    Mud,
    Bridge,
    FallenTree,
    Fog,
    Checkpoint,
    Tower
};

enum class CreatureMode {
    Hidden,
    Watching,
    Pacing,
    Crossing,
    Mirror,
    Lunging,
    Attacking,
    Jumpscare,
    Retreating
};

enum class DoorLockState {
    Unlocked,
    Locked,
    Jammed,
    FakeLocked,
    Broken,
    ForcedOpen
};

enum class FanState {
    Normal,
    Slow,
    Stopped,
    Irregular
};

enum class Hotspot {
    Radio,
    Fan,
    DoorLock,
    DoorHandle,
    Glovebox,
    Mirror,
    Headlights,
    Ignition,
    Horn,
    Wipers,
    WindowCrank,
    DomeLight,
    GearShift
};

enum class EventType {
    Notice,
    Radio,
    Static,
    Power,
    Stall,
    Impact,
    Creature,
    MonsterVoice,
    Thunder,
    Jumpscare,
    Interaction,
    Warning,
    Death,
    Ending
};

struct GameEvent {
    EventType type = EventType::Notice;
    std::string message;
    float intensity = 0.0f;
};

struct RoadSegment {
    float startMiles = 0.0f;
    float endMiles = 0.0f;
    SegmentKind kind = SegmentKind::Straight;
    float curvature = 0.0f;
    float width = 8.0f;
    float fog = 0.35f;
};

struct RoadHazard {
    const char* id = "";
    float distanceMiles = 0.0f;
    float laneOffset = 0.0f;
    float width = 1.0f;
    int kind = 0;
    bool cleared = false;
    bool checked = false;
};

struct CarState {
    float speedMph = 0.0f;
    float lateralOffset = 0.0f;
    float steering = 0.0f;
    char gear = 'D';
    bool engineOn = true;
    bool stalled = false;
    float condition = 100.0f;
    float fuel = 52.0f;
    float battery = 86.0f;
    float headlightPower = 76.0f;
    bool headlightsOn = true;
    bool highBeams = false;
    bool radioOn = false;
    float radioVolume = 0.0f;
    FanState fan = FanState::Normal;
    DoorLockState doorLock = DoorLockState::Unlocked;
    bool doorHandleBroken = false;
    float windowOpen = 0.0f;
    bool wipersOn = false;
    bool domeLightOn = false;
    float steeringBias = 0.03f;
    float hoodRattle = 0.0f;
    float radioInterferenceTimer = 0.0f;
    float steeringInterference = 0.0f;
    float shifterSlipTimer = 0.0f;
    float shifterSettleTimer = 0.0f;
    float cockpitFeedbackTimer = 0.0f;
    Hotspot lastCockpitControl = Hotspot::Radio;
    bool radioLoose = false;
    bool shifterSlipping = false;
    bool falseLockState = false;
};

struct CreatureState {
    CreatureMode mode = CreatureMode::Hidden;
    int side = 1;
    float distanceAhead = 72.0f;
    float lateral = 8.0f;
    float visibility = 0.0f;
    float attackCharge = 0.0f;
    float slowHuntTimer = 0.0f;
    int variant = 0;
    bool mirrorVisible = false;
    bool eyesVisible = false;
};

struct SettingsState {
    bool subtitles = true;
    bool reducedMotion = false;
    bool reducedFlashing = false;
    float masterVolume = 1.0f;
    float musicVolume = 0.8f;
    float sfxVolume = 0.9f;
    float gamma = 1.0f;
};

struct StoryState {
    std::string objective = "Reach the Radio Tower";
    std::string subObjective = "Start the engine and drive forward";
    std::string radioSubtitle;
    std::string monsterSubtitle;
    std::string warning;
    std::string lastInteraction;
    std::string ending;
    std::string endingDetail;
    float messageTimer = 0.0f;
    float monsterSubtitleTimer = 0.0f;
};

struct GameState {
    GamePhase phase = GamePhase::Menu;
    float elapsed = 0.0f;
    float distanceMiles = 0.0f;
    float towerDistanceMiles = 4.2f;
    Chapter chapter = Chapter::MainRoad;
    int chapterIndex = 0;
    SegmentKind currentSegment = SegmentKind::Straight;
    WorldContext world;
    float roadWidth = 8.2f;
    float roadCurvature = 0.0f;
    float roadFog = 0.28f;
    float reverseEscapeMeters = 0.0f;
    float roadDepartureTime = 0.0f;
    float crashGraceTime = 2.0f;
    CarState car;
    CreatureState creature;
    SettingsState settings;
    StoryState story;
    float tension = 8.0f;
    float cooldown = 4.0f;
    float timeSinceScare = 0.0f;
    bool jumpscareActive = false;
    float jumpscareTimer = 0.0f;
    float jumpscareDuration = 2.55f;
    float treeImpactCooldown = 0.0f;
    float stormEventCooldown = 0.0f;
    float monsterVoiceCooldown = 3.0f;
    bool lastTreeImpactLethal = false;
    uint32_t randomSeed = 1;
    std::array<bool, 13> completedBeats {};
    std::array<RoadHazard, 5> hazards {};
};

class Simulation {
public:
    explicit Simulation(uint32_t seed = 1);

    void Reset(uint32_t seed);
    void Start();
    void ShowTutorial();
    void ShowSettings();
    void ReturnToMenu();
    void Pause();
    void Resume();
    void Update(const InputState& input, float dt);
    void Interact(Hotspot hotspot);
    void TriggerRadioInterference(float strength, std::string message);
    void TriggerShifterSlip(std::string message);
    void ChooseEnding(int choice);

    const GameState& State() const { return state_; }
    GameState& State() { return state_; }
    std::vector<GameEvent> DrainEvents();

    static const char* ChapterName(Chapter chapter);
    static const char* SegmentName(SegmentKind segment);
    static const char* FanName(FanState fan);
    static const char* LockName(DoorLockState lock);

private:
    GameState state_;
    std::vector<GameEvent> events_;

    void Push(EventType type, std::string message, float intensity = 0.0f);
    void UpdateRoadContext();
    void UpdateCar(const InputState& input, float dt);
    void UpdateDistance(float dt);
    void UpdateDirector(float dt);
    void UpdateJumpscare(float dt);
    void UpdateStormAndVoices(float dt);
    void UpdateCreature(const InputState& input, float dt);
    void UpdateHazards();
    void UpdatePressure(const InputState& input, float dt);
    void UpdateObjective();
    void CheckDeathAndEnding();
    void TriggerBeat(size_t index);
    void RestartEngine();
    void SoundHorn();
    void StabilizeRadio();
    void ReseatShifter();
    void MarkCockpitFeedback(Hotspot hotspot);
    void HandleTreeImpact(const WorldProp& prop);
    void BeginJumpscare(std::string message);
    void Kill(std::string message);
    float Random01();
    std::string PickLine(const std::vector<const char*>& lines);
};

float Clamp(float value, float minimum, float maximum);
float Approach(float value, float target, float amount);

} // namespace ld
