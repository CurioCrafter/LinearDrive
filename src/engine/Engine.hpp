#pragma once

#include "engine/Assets.hpp"
#include "engine/AudioSystem.hpp"
#include "engine/Renderer.hpp"
#include "game/Input.hpp"
#include "game/Simulation.hpp"

#include <vector>

namespace ld {

class Engine {
public:
    Engine();
    ~Engine();

    int Run();

private:
    AssetStore assetStore_;
    Simulation simulation_;
    AudioSystem audio_;
    Renderer renderer_;
    std::vector<HudMessage> messages_;
    float lookYaw_ = 0.0f;
    float lookPitch_ = 0.0f;
    bool audioEnabled_ = true;

    InputState BuildInput(float dt);
    void HandleGlobalShortcuts();
    void HandlePressedActions(const InputState& input);
    void ApplyEvents(const std::vector<GameEvent>& events);
    void RestartRun();
};

} // namespace ld
