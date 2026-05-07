#include "engine/Engine.hpp"

#include <algorithm>
#include <ctime>

namespace ld {

Engine::Engine() : simulation_(static_cast<uint32_t>(std::time(nullptr))), renderer_(&assetStore_.Get()) {}

Engine::~Engine() {
    assetStore_.Unload();
    if (IsAudioDeviceReady()) {
        CloseAudioDevice();
    }
    if (IsWindowReady()) {
        CloseWindow();
    }
}

int Engine::Run() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "Linear Drive");
    SetWindowMinSize(960, 540);
    InitAudioDevice();
    SetExitKey(KEY_NULL);

    assetStore_.Load();
    audio_.Attach(&assetStore_.Get());

    while (!WindowShouldClose()) {
        const float dt = GetFrameTime();
        HandleGlobalShortcuts();
        InputState input = BuildInput(dt);
        HandlePressedActions(input);

        if (simulation_.State().phase == GamePhase::Playing) {
            simulation_.Update(input, dt);
        }

        std::vector<GameEvent> events = simulation_.DrainEvents();
        ApplyEvents(events);
        audio_.Update(simulation_.State(), events);

        for (HudMessage& message : messages_) {
            message.timer -= dt;
        }
        messages_.erase(
            std::remove_if(messages_.begin(), messages_.end(), [](const HudMessage& message) { return message.timer <= 0.0f; }),
            messages_.end()
        );

        renderer_.Render(simulation_.State(), input, messages_);
    }

    return 0;
}

InputState Engine::BuildInput(float dt) {
    InputState input {};
    input.accelerate = IsKeyDown(KEY_W) || IsKeyDown(KEY_UP);
    input.brake = IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN);
    input.steerLeft = IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT);
    input.steerRight = IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT);
    input.highBeams = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    input.handbrake = IsKeyDown(KEY_SPACE);
    input.restartEngine = IsKeyPressed(KEY_R);
    input.horn = IsKeyPressed(KEY_H);
    input.pausePressed = IsKeyPressed(KEY_ESCAPE);
    input.confirmPressed = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);
    input.backPressed = IsKeyPressed(KEY_BACKSPACE);
    input.mousePressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    input.mouse = GetMousePosition();

    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        Vector2 delta = GetMouseDelta();
        lookYaw_ = Clamp(lookYaw_ - delta.x * 0.003f, -0.75f, 0.75f);
        lookPitch_ = Clamp(lookPitch_ - delta.y * 0.0024f, -0.32f, 0.32f);
    } else {
        lookYaw_ = Approach(lookYaw_, 0.0f, dt * 0.65f);
        lookPitch_ = Approach(lookPitch_, 0.0f, dt * 0.55f);
    }

    if (IsKeyDown(KEY_Q)) lookYaw_ = Approach(lookYaw_, 0.55f, dt * 2.6f);
    if (IsKeyDown(KEY_E)) lookYaw_ = Approach(lookYaw_, -0.55f, dt * 2.6f);

    input.lookYaw = lookYaw_;
    input.lookPitch = lookPitch_;
    return input;
}

void Engine::HandleGlobalShortcuts() {
    GameState& state = simulation_.State();
    if (IsKeyPressed(KEY_F1)) state.settings.subtitles = !state.settings.subtitles;
    if (IsKeyPressed(KEY_F2)) state.settings.reducedMotion = !state.settings.reducedMotion;
    if (IsKeyPressed(KEY_F3)) state.settings.reducedFlashing = !state.settings.reducedFlashing;
    if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) state.settings.gamma = Clamp(state.settings.gamma + 0.08f, 0.75f, 1.45f);
    if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) state.settings.gamma = Clamp(state.settings.gamma - 0.08f, 0.75f, 1.45f);
    if (IsKeyPressed(KEY_M)) {
        audioEnabled_ = !audioEnabled_;
        audio_.SetEnabled(audioEnabled_);
    }
}

void Engine::HandlePressedActions(const InputState& input) {
    const GameState& state = simulation_.State();

    if (input.pausePressed) {
        if (state.phase == GamePhase::Playing && !state.jumpscareActive) simulation_.Pause();
        else if (state.phase == GamePhase::Paused) simulation_.Resume();
        else if (state.phase == GamePhase::Tutorial) simulation_.ReturnToMenu();
        else if (state.phase == GamePhase::Settings) simulation_.ReturnToMenu();
        return;
    }

    if (state.phase == GamePhase::Menu) {
        if (input.confirmPressed || (input.mousePressed && renderer_.HitStart(input.mouse))) {
            RestartRun();
        } else if (IsKeyPressed(KEY_I) || (input.mousePressed && renderer_.HitTutorial(input.mouse))) {
            simulation_.ShowTutorial();
        } else if (IsKeyPressed(KEY_O) || (input.mousePressed && renderer_.HitSettings(input.mouse))) {
            simulation_.ShowSettings();
        } else if (input.mousePressed && renderer_.HitQuit(input.mouse)) {
            CloseWindow();
        }
        return;
    }

    if (state.phase == GamePhase::Settings) {
        GameState& editable = simulation_.State();
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            if (auto slider = renderer_.HitSettingsSlider(input.mouse)) {
                const float value = renderer_.SliderValueFromMouse(*slider, input.mouse);
                switch (*slider) {
                    case 0:
                        editable.settings.masterVolume = value;
                        break;
                    case 1:
                        editable.settings.musicVolume = value;
                        break;
                    case 2:
                        editable.settings.sfxVolume = value;
                        break;
                    case 3:
                        editable.settings.gamma = 0.75f + value * 0.70f;
                        break;
                    default:
                        break;
                }
            }
        }
        if (input.mousePressed) {
            if (auto toggle = renderer_.HitSettingsToggle(input.mouse)) {
                switch (*toggle) {
                    case 0:
                        editable.settings.subtitles = !editable.settings.subtitles;
                        break;
                    case 1:
                        editable.settings.reducedMotion = !editable.settings.reducedMotion;
                        break;
                    case 2:
                        editable.settings.reducedFlashing = !editable.settings.reducedFlashing;
                        break;
                    default:
                        break;
                }
            } else if (renderer_.HitBack(input.mouse)) {
                simulation_.ReturnToMenu();
            }
        }
        if (input.backPressed) {
            simulation_.ReturnToMenu();
        }
        return;
    }

    if (state.phase == GamePhase::Tutorial) {
        if (input.confirmPressed || (input.mousePressed && renderer_.HitStart(input.mouse))) {
            RestartRun();
        } else if (input.backPressed || (input.mousePressed && renderer_.HitBack(input.mouse))) {
            simulation_.ReturnToMenu();
        }
        return;
    }

    if (state.phase == GamePhase::Paused) {
        if (input.confirmPressed || (input.mousePressed && renderer_.HitResume(input.mouse))) {
            simulation_.Resume();
        } else if (IsKeyPressed(KEY_R) || (input.mousePressed && renderer_.HitRestart(input.mouse))) {
            RestartRun();
        }
        return;
    }

    if (state.phase == GamePhase::Dead) {
        if (input.confirmPressed || IsKeyPressed(KEY_R) || (input.mousePressed && renderer_.HitRestart(input.mouse))) {
            RestartRun();
        } else if (input.backPressed || (input.mousePressed && renderer_.HitBack(input.mouse))) {
            audio_.Reset();
            messages_.clear();
            simulation_.ReturnToMenu();
        }
        return;
    }

    if (state.phase == GamePhase::Ending) {
        if (input.mousePressed) {
            if (auto choice = renderer_.HitEndingChoice(input.mouse)) {
                simulation_.ChooseEnding(*choice);
            } else if (renderer_.HitRestart(input.mouse)) {
                RestartRun();
            }
        }
        if (IsKeyPressed(KEY_ONE)) simulation_.ChooseEnding(0);
        if (IsKeyPressed(KEY_TWO)) simulation_.ChooseEnding(1);
        if (IsKeyPressed(KEY_THREE)) simulation_.ChooseEnding(2);
        if (IsKeyPressed(KEY_R) && !state.story.ending.empty()) RestartRun();
        return;
    }

    if (state.phase != GamePhase::Playing) {
        return;
    }

    if (state.jumpscareActive) {
        return;
    }

    if (IsKeyPressed(KEY_T)) simulation_.Interact(Hotspot::Radio);
    if (IsKeyPressed(KEY_L)) simulation_.Interact(Hotspot::DoorLock);
    if (IsKeyPressed(KEY_B)) simulation_.Interact(Hotspot::Headlights);
    if (IsKeyPressed(KEY_V)) simulation_.Interact(Hotspot::Wipers);
    if (IsKeyPressed(KEY_F)) simulation_.Interact(Hotspot::DomeLight);
    if (IsKeyPressed(KEY_G)) simulation_.Interact(Hotspot::GearShift);
    if (IsKeyPressed(KEY_TAB)) simulation_.Interact(Hotspot::Mirror);
    if (input.restartEngine) simulation_.Interact(Hotspot::Ignition);
    if (input.horn) simulation_.Interact(Hotspot::Horn);
    if (input.mousePressed) {
        if (auto hotspot = renderer_.HitTestCockpitControl(input.mouse)) {
            simulation_.Interact(*hotspot);
        }
    }
}

void Engine::ApplyEvents(const std::vector<GameEvent>& events) {
    for (const GameEvent& event : events) {
        messages_.insert(messages_.begin(), HudMessage {event.type, event.message, 5.0f + event.intensity * 2.0f, event.intensity});
    }
    if (messages_.size() > 6) {
        messages_.resize(6);
    }
}

void Engine::RestartRun() {
    const SettingsState settings = simulation_.State().settings;
    audio_.Reset();
    simulation_.Reset(static_cast<uint32_t>(std::time(nullptr)));
    simulation_.State().settings = settings;
    simulation_.Start();
    messages_.clear();
    lookYaw_ = 0.0f;
    lookPitch_ = 0.0f;
}

} // namespace ld
