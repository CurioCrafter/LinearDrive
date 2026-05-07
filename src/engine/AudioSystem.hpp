#pragma once

#include "engine/Assets.hpp"
#include "game/Simulation.hpp"

#include <vector>

namespace ld {

class AudioSystem {
public:
    void Attach(Assets* assets);
    void Update(const GameState& state, const std::vector<GameEvent>& events);
    void SetEnabled(bool enabled);
    void Reset();

private:
    Assets* assets_ = nullptr;
    bool enabled_ = true;
    bool radioPlaying_ = false;
    bool stormPlaying_ = false;
    float lastRadioVolume_ = 0.0f;
    float lastStormVolume_ = 0.0f;
    float masterVolume_ = 1.0f;
    float musicVolume_ = 0.8f;
    float sfxVolume_ = 0.9f;

    void PlayEvent(const GameEvent& event);
    void ApplySettings(const SettingsState& settings);
    static bool IsSoundReadySafe(const Sound& sound);
};

} // namespace ld
