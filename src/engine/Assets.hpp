#pragma once

#include "raylib.h"

#include <string>

namespace ld {

struct Assets {
    Model treeDefault {};
    Model treeCone {};
    Model treeFat {};
    Model treeOak {};
    Model treePineTall {};
    Model treePineRound {};
    Model rockLarge {};
    Model logLarge {};
    Model logStack {};
    Model fenceGate {};
    Model fencePlanks {};
    Model bridgeSide {};
    Model pathRocks {};
    Model bushDetailed {};
    Model bushLarge {};
    Model grassLarge {};
    Model stumpOld {};
    Model stumpRound {};
    Model roadSign {};
    Model sedan {};
    Model debrisDoor {};
    Model debrisTire {};
    Model horrorMonster {};
    Model skeleton {};
    Model wingedSilhouette {};

    Texture2D iconLocked {};
    Texture2D iconPower {};
    Texture2D iconWarning {};
    Texture2D iconAudio {};
    Texture2D iconGear {};
    Texture2D menuBackplate {};
    Texture2D tutorialBackplate {};

    Sound click {};
    Sound glitch {};
    Sound error {};
    Sound glass {};
    Sound scratch {};
    Sound drop {};
    Sound jumpscare {};
    Sound thunder {};
    Sound monsterVoice {};
    Music radioStatic {};
    Music stormAmbience {};

    bool loaded = false;
};

class AssetStore {
public:
    bool Load();
    void Unload();
    Assets& Get() { return assets_; }
    const Assets& Get() const { return assets_; }

private:
    Assets assets_;

    static Model LoadModelSafe(const char* path);
    static Texture2D LoadTextureSafe(const char* path);
    static Sound LoadSoundSafe(const char* path);
};

std::string AssetPath(const char* relative);

} // namespace ld
