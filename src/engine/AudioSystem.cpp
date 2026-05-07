#include "engine/AudioSystem.hpp"

namespace ld {

void AudioSystem::Attach(Assets* assets) {
    assets_ = assets;
}

void AudioSystem::SetEnabled(bool enabled) {
    enabled_ = enabled;
    if (!enabled_) Reset();
}

void AudioSystem::Reset() {
    if (!assets_) {
        radioPlaying_ = false;
        stormPlaying_ = false;
        lastRadioVolume_ = 0.0f;
        lastStormVolume_ = 0.0f;
        return;
    }

    if (radioPlaying_ && assets_->radioStatic.stream.buffer != nullptr) {
        StopMusicStream(assets_->radioStatic);
    }
    if (stormPlaying_ && assets_->stormAmbience.stream.buffer != nullptr) {
        StopMusicStream(assets_->stormAmbience);
    }

    Sound* sounds[] {
        &assets_->click,
        &assets_->glitch,
        &assets_->error,
        &assets_->glass,
        &assets_->scratch,
        &assets_->drop,
        &assets_->jumpscare,
        &assets_->thunder,
        &assets_->monsterVoice
    };
    for (Sound* sound : sounds) {
        if (sound && IsSoundReadySafe(*sound)) {
            StopSound(*sound);
        }
    }

    radioPlaying_ = false;
    stormPlaying_ = false;
    lastRadioVolume_ = 0.0f;
    lastStormVolume_ = 0.0f;
}

void AudioSystem::Update(const GameState& state, const std::vector<GameEvent>& events) {
    if (!assets_) return;
    ApplySettings(state.settings);
    if (!enabled_) return;

    if (state.car.radioOn && assets_->radioStatic.stream.buffer != nullptr) {
        if (!radioPlaying_) {
            PlayMusicStream(assets_->radioStatic);
            radioPlaying_ = true;
        }
        UpdateMusicStream(assets_->radioStatic);
        lastRadioVolume_ = Approach(lastRadioVolume_, state.car.radioVolume * 0.35f * masterVolume_ * musicVolume_, 0.04f);
        SetMusicVolume(assets_->radioStatic, lastRadioVolume_);
    } else if (radioPlaying_) {
        StopMusicStream(assets_->radioStatic);
        radioPlaying_ = false;
        lastRadioVolume_ = 0.0f;
    }

    const float stormTarget = state.phase == GamePhase::Playing ? state.world.stormIntensity * 0.42f * masterVolume_ * musicVolume_ : 0.0f;
    if (stormTarget > 0.02f && assets_->stormAmbience.stream.buffer != nullptr) {
        if (!stormPlaying_) {
            PlayMusicStream(assets_->stormAmbience);
            stormPlaying_ = true;
        }
        UpdateMusicStream(assets_->stormAmbience);
        lastStormVolume_ = Approach(lastStormVolume_, stormTarget, 0.025f);
        SetMusicVolume(assets_->stormAmbience, lastStormVolume_);
    } else if (stormPlaying_) {
        StopMusicStream(assets_->stormAmbience);
        stormPlaying_ = false;
        lastStormVolume_ = 0.0f;
    }

    for (const GameEvent& event : events) {
        PlayEvent(event);
    }
}

void AudioSystem::PlayEvent(const GameEvent& event) {
    if (!assets_) return;

    Sound* sound = nullptr;
    switch (event.type) {
        case EventType::Radio:
        case EventType::Static:
            sound = &assets_->glitch;
            break;
        case EventType::Power:
            sound = &assets_->scratch;
            break;
        case EventType::Stall:
        case EventType::Warning:
            sound = &assets_->error;
            break;
        case EventType::Impact:
        case EventType::Death:
            sound = &assets_->glass;
            break;
        case EventType::Jumpscare:
            sound = &assets_->jumpscare;
            break;
        case EventType::Thunder:
            sound = &assets_->thunder;
            break;
        case EventType::MonsterVoice:
            sound = &assets_->monsterVoice;
            break;
        case EventType::Creature:
            sound = &assets_->drop;
            break;
        case EventType::Interaction:
        case EventType::Ending:
        case EventType::Notice:
            sound = &assets_->click;
            break;
    }

    if (!sound || !IsSoundReadySafe(*sound)) return;
    SetSoundVolume(*sound, (0.25f + event.intensity * 0.55f) * masterVolume_ * sfxVolume_);
    SetSoundPitch(*sound, 0.82f + event.intensity * 0.4f);
    PlaySound(*sound);
}

void AudioSystem::ApplySettings(const SettingsState& settings) {
    masterVolume_ = Clamp(settings.masterVolume, 0.0f, 1.0f);
    musicVolume_ = Clamp(settings.musicVolume, 0.0f, 1.0f);
    sfxVolume_ = Clamp(settings.sfxVolume, 0.0f, 1.0f);
}

bool AudioSystem::IsSoundReadySafe(const Sound& sound) {
    return sound.stream.buffer != nullptr;
}

} // namespace ld
