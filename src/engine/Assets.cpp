#include "engine/Assets.hpp"

#include <array>

namespace ld {

std::string AssetPath(const char* relative) {
    const std::array<std::string, 3> prefixes {
        std::string("assets/"),
        std::string("../assets/"),
        std::string("../../assets/")
    };

    for (const std::string& prefix : prefixes) {
        std::string candidate = prefix + relative;
        if (FileExists(candidate.c_str())) {
            return candidate;
        }
    }

    return prefixes.front() + relative;
}

bool AssetStore::Load() {
    assets_.treeDefault = LoadModelSafe(AssetPath("models/tree_default_dark.obj").c_str());
    assets_.treeCone = LoadModelSafe(AssetPath("models/tree_cone_dark.obj").c_str());
    assets_.treeFat = LoadModelSafe(AssetPath("models/tree_fat_darkh.obj").c_str());
    assets_.treeOak = LoadModelSafe(AssetPath("models/tree_oak_dark.obj").c_str());
    assets_.treePineTall = LoadModelSafe(AssetPath("models/tree_pineTallA.obj").c_str());
    assets_.treePineRound = LoadModelSafe(AssetPath("models/tree_pineRoundB.obj").c_str());
    assets_.rockLarge = LoadModelSafe(AssetPath("models/rock_largeA.obj").c_str());
    assets_.logLarge = LoadModelSafe(AssetPath("models/log_large.obj").c_str());
    assets_.logStack = LoadModelSafe(AssetPath("models/log_stackLarge.obj").c_str());
    assets_.fenceGate = LoadModelSafe(AssetPath("models/fence_gate.obj").c_str());
    assets_.fencePlanks = LoadModelSafe(AssetPath("models/fence_planksDouble.obj").c_str());
    assets_.bridgeSide = LoadModelSafe(AssetPath("models/bridge_side_wood.obj").c_str());
    assets_.pathRocks = LoadModelSafe(AssetPath("models/ground_pathRocks.obj").c_str());
    assets_.bushDetailed = LoadModelSafe(AssetPath("models/plant_bushDetailed.obj").c_str());
    assets_.bushLarge = LoadModelSafe(AssetPath("models/plant_bushLarge.obj").c_str());
    assets_.grassLarge = LoadModelSafe(AssetPath("models/grass_large.obj").c_str());
    assets_.stumpOld = LoadModelSafe(AssetPath("models/stump_oldTall.obj").c_str());
    assets_.stumpRound = LoadModelSafe(AssetPath("models/stump_roundDetailed.obj").c_str());
    assets_.roadSign = LoadModelSafe(AssetPath("models/sign.obj").c_str());
    assets_.sedan = LoadModelSafe(AssetPath("models/sedan.glb").c_str());
    assets_.toyotaCockpit = LoadModelSafe(AssetPath("models/toyota_rav4_cockpit.glb").c_str());
    assets_.debrisDoor = LoadModelSafe(AssetPath("models/debris-door.glb").c_str());
    assets_.debrisTire = LoadModelSafe(AssetPath("models/debris-tire.glb").c_str());
    assets_.horrorMonster = LoadModelSafe(AssetPath("models/creature_stalker.glb").c_str());
    assets_.skeleton = LoadModelSafe(AssetPath("models/quaternius_skeleton_tri.obj").c_str());
    assets_.wingedSilhouette = LoadModelSafe(AssetPath("models/quaternius_bat_tri.obj").c_str());

    assets_.iconLocked = LoadTextureSafe(AssetPath("textures/icons/locked.png").c_str());
    assets_.iconPower = LoadTextureSafe(AssetPath("textures/icons/power.png").c_str());
    assets_.iconWarning = LoadTextureSafe(AssetPath("textures/icons/warning.png").c_str());
    assets_.iconAudio = LoadTextureSafe(AssetPath("textures/icons/audioOn.png").c_str());
    assets_.iconGear = LoadTextureSafe(AssetPath("textures/icons/gear.png").c_str());
    assets_.menuBackplate = LoadTextureSafe(AssetPath("textures/menu_v2_backplate.png").c_str());
    assets_.tutorialBackplate = LoadTextureSafe(AssetPath("textures/tutorial_v2_backplate.png").c_str());

    assets_.click = LoadSoundSafe(AssetPath("audio/click_002.wav").c_str());
    assets_.glitch = LoadSoundSafe(AssetPath("audio/glitch_003.wav").c_str());
    assets_.error = LoadSoundSafe(AssetPath("audio/error_006.wav").c_str());
    assets_.glass = LoadSoundSafe(AssetPath("audio/glass_003.wav").c_str());
    assets_.scratch = LoadSoundSafe(AssetPath("audio/scratch_004.wav").c_str());
    assets_.drop = LoadSoundSafe(AssetPath("audio/drop_003.wav").c_str());
    assets_.jumpscare = LoadSoundSafe(AssetPath("audio/jumpscare.wav").c_str());
    assets_.thunder = LoadSoundSafe(AssetPath("audio/thunder.wav").c_str());
    assets_.monsterVoice = LoadSoundSafe(AssetPath("audio/monster_voice.wav").c_str());

    const std::string radioPath = AssetPath("audio/radio_static.mp3");
    if (FileExists(radioPath.c_str())) {
        assets_.radioStatic = LoadMusicStream(radioPath.c_str());
        assets_.radioStatic.looping = true;
    }
    const std::string stormPath = AssetPath("audio/storm_ambience.ogg");
    if (FileExists(stormPath.c_str())) {
        assets_.stormAmbience = LoadMusicStream(stormPath.c_str());
        assets_.stormAmbience.looping = true;
    }

    assets_.loaded = true;
    return true;
}

void AssetStore::Unload() {
    if (!assets_.loaded) return;

    UnloadModel(assets_.treeDefault);
    UnloadModel(assets_.treeCone);
    UnloadModel(assets_.treeFat);
    UnloadModel(assets_.treeOak);
    UnloadModel(assets_.treePineTall);
    UnloadModel(assets_.treePineRound);
    UnloadModel(assets_.rockLarge);
    UnloadModel(assets_.logLarge);
    UnloadModel(assets_.logStack);
    UnloadModel(assets_.fenceGate);
    UnloadModel(assets_.fencePlanks);
    UnloadModel(assets_.bridgeSide);
    UnloadModel(assets_.pathRocks);
    UnloadModel(assets_.bushDetailed);
    UnloadModel(assets_.bushLarge);
    UnloadModel(assets_.grassLarge);
    UnloadModel(assets_.stumpOld);
    UnloadModel(assets_.stumpRound);
    UnloadModel(assets_.roadSign);
    UnloadModel(assets_.sedan);
    UnloadModel(assets_.toyotaCockpit);
    UnloadModel(assets_.debrisDoor);
    UnloadModel(assets_.debrisTire);
    UnloadModel(assets_.horrorMonster);
    UnloadModel(assets_.skeleton);
    UnloadModel(assets_.wingedSilhouette);

    UnloadTexture(assets_.iconLocked);
    UnloadTexture(assets_.iconPower);
    UnloadTexture(assets_.iconWarning);
    UnloadTexture(assets_.iconAudio);
    UnloadTexture(assets_.iconGear);
    UnloadTexture(assets_.menuBackplate);
    UnloadTexture(assets_.tutorialBackplate);

    UnloadSound(assets_.click);
    UnloadSound(assets_.glitch);
    UnloadSound(assets_.error);
    UnloadSound(assets_.glass);
    UnloadSound(assets_.scratch);
    UnloadSound(assets_.drop);
    UnloadSound(assets_.jumpscare);
    UnloadSound(assets_.thunder);
    UnloadSound(assets_.monsterVoice);
    UnloadMusicStream(assets_.radioStatic);
    UnloadMusicStream(assets_.stormAmbience);

    assets_ = Assets {};
}

Model AssetStore::LoadModelSafe(const char* path) {
    if (!FileExists(path)) {
        TraceLog(LOG_WARNING, "Missing model: %s", path);
        return Model {};
    }
    return LoadModel(path);
}

Texture2D AssetStore::LoadTextureSafe(const char* path) {
    if (!FileExists(path)) {
        TraceLog(LOG_WARNING, "Missing texture: %s", path);
        return Texture2D {};
    }
    return LoadTexture(path);
}

Sound AssetStore::LoadSoundSafe(const char* path) {
    if (!FileExists(path)) {
        TraceLog(LOG_WARNING, "Missing sound: %s", path);
        return Sound {};
    }
    return LoadSound(path);
}

} // namespace ld
