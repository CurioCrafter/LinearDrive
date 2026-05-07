#include "engine/Renderer.hpp"

#include "rlgl.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>

namespace ld {
namespace {

constexpr float kMetersPerMile = 1609.344f;

Color UiPanel() { return Color {9, 12, 12, 216}; }
Color UiBorder() { return Color {188, 168, 119, 155}; }
Color Amber() { return Color {255, 186, 88, 255}; }
Color PaleEye() { return Color {214, 245, 214, 255}; }

std::string Percent(float value) {
    std::ostringstream stream;
    stream << static_cast<int>(std::round(value)) << "%";
    return stream.str();
}

std::string Speed(float value) {
    std::ostringstream stream;
    stream << static_cast<int>(std::round(value)) << " MPH";
    return stream.str();
}

const char* MonsterName(int variant) {
    switch (variant % 4) {
        case 1:
            return "BONE CRAWLER";
        case 2:
            return "WINGED WATCHER";
        case 3:
            return "ASH SHADE";
        default:
            return "PALE STALKER";
    }
}

float Smooth01(float value) {
    value = Clamp(value, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
}

void DrawMeter(Rectangle rect, const char* label, float value, Color fill, Color back = Color {23, 25, 24, 235}) {
    value = Clamp(value, 0.0f, 1.0f);
    DrawRectangleRounded(rect, 0.08f, 8, back);
    DrawRectangleRounded(Rectangle {rect.x, rect.y, rect.width * value, rect.height}, 0.08f, 8, fill);
    DrawRectangleRoundedLines(rect, 0.08f, 8, Color {129, 115, 82, 150});
    DrawText(label, static_cast<int>(rect.x + 8), static_cast<int>(rect.y + rect.height * 0.5f - 6.0f), 12, Color {232, 221, 188, 230});
}

bool TextureReady(const Texture2D& texture) {
    return texture.id != 0;
}

bool ModelReady(const Model& model) {
    return model.meshCount > 0 && model.meshes != nullptr;
}

float RelativeZ(float objectMeters, float distanceMeters) {
    return -(objectMeters - distanceMeters) - 14.0f;
}

} // namespace

Renderer::Renderer(Assets* assets) : assets_(assets) {
    camera_.fovy = 62.0f;
    camera_.projection = CAMERA_PERSPECTIVE;
}

void Renderer::Render(const GameState& state, const InputState& input, const std::vector<HudMessage>& messages, const RenderOptions& options) {
    renderOptions_ = options;
    SetupCamera(state, input);
    BeginDrawing();
    ClearBackground(Color {3, 5, 5, 255});

    BeginMode3D(camera_);
    DrawWorld(state);
    DrawCockpit3D(state);
    if (renderOptions_.drawCalibrationRig) {
        DrawCalibrationRig3D(state);
    }
    EndMode3D();
    BuildCockpitControls(state);

    if (!renderOptions_.drawCalibrationRig) {
        DrawFilmEffects(state);
        DrawStormEffects(state);
    }
    if (state.jumpscareActive) {
        DrawJumpscare(state);
    } else {
        if (!renderOptions_.hideHud) {
            DrawHud(state, messages);
            DrawCockpitControlHighlights(state, input.mouse);
        }
        if (renderOptions_.drawCalibrationRig) {
            DrawCockpitControlHighlights(state, Vector2 {-1000.0f, -1000.0f});
            DrawCalibrationRigLabels(state);
        }
    }

    switch (state.phase) {
        case GamePhase::Menu:
            DrawMenu(state);
            break;
        case GamePhase::Settings:
            DrawSettings(state);
            break;
        case GamePhase::Tutorial:
            DrawTutorial(state);
            break;
        case GamePhase::Paused:
            DrawPause(state);
            break;
        case GamePhase::Dead:
            DrawDeath(state);
            break;
        case GamePhase::Ending:
            DrawEnding(state);
            break;
        case GamePhase::Playing:
            break;
    }

    EndDrawing();
}

std::optional<Hotspot> Renderer::HitTestHotspot(Vector2 mouse) const {
    return HitTestCockpitControl(mouse);
}

std::optional<Hotspot> Renderer::HitTestCockpitControl(Vector2 mouse) const {
    for (const CockpitControl& control : cockpitControls_) {
        if (CheckCollisionPointRec(mouse, control.rect)) {
            return control.hotspot;
        }
    }
    return std::nullopt;
}

bool Renderer::HitStart(Vector2 mouse) const {
    return CheckCollisionPointRec(mouse, startButton_);
}

bool Renderer::HitTutorial(Vector2 mouse) const {
    return CheckCollisionPointRec(mouse, tutorialButton_);
}

bool Renderer::HitSettings(Vector2 mouse) const {
    return CheckCollisionPointRec(mouse, settingsButton_);
}

bool Renderer::HitQuit(Vector2 mouse) const {
    return CheckCollisionPointRec(mouse, quitButton_);
}

bool Renderer::HitBack(Vector2 mouse) const {
    return CheckCollisionPointRec(mouse, backButton_);
}

bool Renderer::HitResume(Vector2 mouse) const {
    return CheckCollisionPointRec(mouse, resumeButton_);
}

bool Renderer::HitRestart(Vector2 mouse) const {
    return CheckCollisionPointRec(mouse, restartButton_);
}

std::optional<int> Renderer::HitSettingsSlider(Vector2 mouse) const {
    for (int i = 0; i < static_cast<int>(settingsSliders_.size()); ++i) {
        if (CheckCollisionPointRec(mouse, settingsSliders_[static_cast<size_t>(i)])) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<int> Renderer::HitSettingsToggle(Vector2 mouse) const {
    for (int i = 0; i < static_cast<int>(settingsToggles_.size()); ++i) {
        if (CheckCollisionPointRec(mouse, settingsToggles_[static_cast<size_t>(i)])) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<int> Renderer::HitEndingChoice(Vector2 mouse) const {
    for (int i = 0; i < static_cast<int>(endingButtons_.size()); ++i) {
        if (CheckCollisionPointRec(mouse, endingButtons_[static_cast<size_t>(i)])) {
            return i;
        }
    }
    return std::nullopt;
}

float Renderer::SliderValueFromMouse(int slider, Vector2 mouse) const {
    if (slider < 0 || slider >= static_cast<int>(settingsSliders_.size())) {
        return 0.0f;
    }
    const Rectangle rect = settingsSliders_[static_cast<size_t>(slider)];
    return Clamp((mouse.x - rect.x) / std::max(1.0f, rect.width), 0.0f, 1.0f);
}

void Renderer::SetupCamera(const GameState& state, const InputState& input) {
    if (renderOptions_.cameraOverride) {
        camera_ = renderOptions_.overrideCamera;
        return;
    }

    const float scareShake = state.jumpscareActive ? (0.06f + state.creature.attackCharge * 0.08f) : 0.0f;
    const float shake = state.settings.reducedMotion ? 0.0f : (state.tension / 100.0f) * 0.015f + state.car.hoodRattle * 0.025f + scareShake;
    const float rumble = std::sin(state.elapsed * 31.0f) * shake;
    const float yaw = input.lookYaw + (input.lookYaw == 0.0f ? 0.0f : std::sin(state.elapsed * 4.0f) * shake);
    const float pitch = input.lookPitch + rumble * 0.35f;

    camera_ = CockpitCamera(state, CockpitCameraMode::Driver);
    camera_.position = Add3(camera_.position, Vector3 {rumble, rumble * 0.5f, 0.0f});
    camera_.target = Add3(camera_.target, Vector3 {std::sin(yaw) * 8.5f + rumble * 0.35f, pitch * 5.0f, 0.0f});
}

void Renderer::BuildCockpitControls(const GameState& state) {
    cockpitControls_.clear();
    if (state.phase != GamePhase::Playing || state.jumpscareActive) {
        return;
    }

    const int w = GetScreenWidth();
    const int h = GetScreenHeight();
    for (const CockpitAnchor& anchor : CockpitAnchors()) {
        const Vector3 world = CarToWorld(state, anchor.local);
        const Vector2 screen = GetWorldToScreen(world, camera_);
        if (screen.x < -80.0f || screen.x > static_cast<float>(w) + 80.0f || screen.y < -80.0f || screen.y > static_cast<float>(h) + 80.0f) {
            continue;
        }
        const bool urgent =
            (anchor.hotspot == Hotspot::Radio && state.car.radioInterferenceTimer > 0.0f) ||
            (anchor.hotspot == Hotspot::GearShift && state.car.shifterSlipTimer > 0.0f) ||
            (anchor.hotspot == Hotspot::DoorLock && (state.car.doorLock == DoorLockState::Jammed || state.car.doorLock == DoorLockState::FakeLocked || state.car.falseLockState)) ||
            (anchor.hotspot == Hotspot::Ignition && state.car.stalled) ||
            (anchor.hotspot == Hotspot::Mirror && state.story.monsterSubtitleTimer > 0.0f);
        cockpitControls_.push_back(CockpitControl {
            Rectangle {screen.x - anchor.hitSize.x * 0.5f, screen.y - anchor.hitSize.y * 0.5f, anchor.hitSize.x, anchor.hitSize.y},
            anchor.hotspot,
            anchor.name,
            world,
            urgent
        });
    }
}

void Renderer::DrawWorld(const GameState& state) {
    DrawRoad(state);
    DrawForest(state);
    DrawRoadsideDressing(state);
    DrawHazards(state);
    DrawCreature(state);

    const float distanceMeters = state.distanceMiles * kMetersPerMile;
    const float towerMeters = state.towerDistanceMiles * kMetersPerMile;
    const float towerZ = RelativeZ(towerMeters, distanceMeters);
    if (towerZ < -20.0f && towerZ > -420.0f) {
        DrawCylinderEx(Vector3 {0.0f, 0.0f, towerZ}, Vector3 {0.0f, 42.0f, towerZ}, 0.35f, 0.2f, 8, Color {70, 75, 76, 255});
        DrawSphere(Vector3 {0.0f, 43.0f + std::sin(state.elapsed * 5.0f) * 0.8f, towerZ}, 2.1f, Color {166, 28, 22, 210});
        DrawCube(Vector3 {0.0f, 1.2f, towerZ + 4.0f}, 9.0f, 2.2f, 0.5f, Color {30, 26, 22, 255});
    }

    if (state.car.headlightsOn) {
        const float reach = 42.0f + state.car.headlightPower * 1.4f + (state.car.highBeams ? 34.0f : 0.0f);
        const float alpha = state.settings.reducedFlashing ? 0.13f : 0.08f + state.car.headlightPower / 900.0f;
        BeginBlendMode(BLEND_ALPHA);
        DrawCylinderEx(Vector3 {state.car.lateralOffset - 1.1f, 0.64f, 1.0f}, Vector3 {state.car.lateralOffset - 2.8f, 0.45f, -reach}, 0.28f, 10.0f, 24, Fade(Color {255, 219, 143, 255}, alpha));
        DrawCylinderEx(Vector3 {state.car.lateralOffset + 1.1f, 0.64f, 1.0f}, Vector3 {state.car.lateralOffset + 2.8f, 0.45f, -reach}, 0.28f, 10.0f, 24, Fade(Color {255, 219, 143, 255}, alpha));
        EndBlendMode();
    }
}

void Renderer::DrawRoad(const GameState& state) {
    const float distanceMeters = state.distanceMiles * kMetersPerMile;
    const float segmentLength = 18.0f;
    const int first = static_cast<int>(std::floor(distanceMeters / segmentLength));

    for (int i = 0; i < 22; ++i) {
        const int index = first + i;
        const float z = -((static_cast<float>(index) * segmentLength) - distanceMeters) - 9.0f;
        const float curve = std::sin((static_cast<float>(index) * 0.37f) + state.roadCurvature * 4.0f) * state.roadCurvature * 3.8f;
        const float width = state.roadWidth + (state.currentSegment == SegmentKind::Bridge ? -0.8f : 0.0f);
        Color road = state.currentSegment == SegmentKind::Mud ? Color {39, 27, 20, 255} : Color {34, 31, 27, 255};
        if (state.currentSegment == SegmentKind::Bridge) road = Color {45, 37, 30, 255};
        if (state.world.wetness > 0.1f) {
            road = Color {
                static_cast<unsigned char>(road.r * (1.0f - state.world.wetness * 0.22f)),
                static_cast<unsigned char>(road.g * (1.0f - state.world.wetness * 0.08f) + 12.0f * state.world.wetness),
                static_cast<unsigned char>(road.b + 24.0f * state.world.wetness),
                255
            };
        }
        DrawCube(Vector3 {curve, -0.055f, z}, width, 0.08f, segmentLength + 0.35f, road);
        DrawCube(Vector3 {curve - width * 0.5f - 0.45f, 0.02f, z}, 0.18f, 0.12f, segmentLength, Color {74, 65, 48, 180});
        DrawCube(Vector3 {curve + width * 0.5f + 0.45f, 0.02f, z}, 0.18f, 0.12f, segmentLength, Color {74, 65, 48, 180});
    }

    DrawPlane(Vector3 {0.0f, -0.08f, -110.0f}, Vector2 {320.0f, 260.0f}, Color {8, 14, 10, 255});

    if (state.world.wetness > 0.28f) {
        BeginBlendMode(BLEND_ADDITIVE);
        for (int i = 0; i < 12; ++i) {
            const unsigned h = Hash(static_cast<unsigned>(i) * 7757u + static_cast<unsigned>(distanceMeters));
            const float z = -18.0f - static_cast<float>((h >> 5u) % 150u);
            const float x = -state.roadWidth * 0.38f + static_cast<float>((h >> 12u) % 600u) / 100.0f;
            DrawCylinder(Vector3 {x, 0.01f, z}, 0.24f, 0.36f, 0.01f, 16, Fade(Color {115, 150, 160, 255}, state.world.wetness * 0.12f));
        }
        EndBlendMode();
    }
}

void Renderer::DrawForest(const GameState& state) {
    const float distanceMeters = state.distanceMiles * kMetersPerMile;
    const float spacing = 8.5f;
    const int first = static_cast<int>(std::floor(distanceMeters / spacing));

    for (int i = 0; i < 88; ++i) {
        const int index = first + i;
        const unsigned hash = Hash(static_cast<unsigned>(index) * 7919u);
        const int side = (hash & 1u) ? 1 : -1;
        const float z = -((static_cast<float>(index) * spacing) - distanceMeters) - 16.0f;
        if (z > 4.0f || z < -230.0f) continue;

        const float x = static_cast<float>(side) * (state.roadWidth * 0.5f + 3.3f + static_cast<float>((hash >> 3u) % 900u) / 100.0f);
        const float scale = 1.2f + static_cast<float>((hash >> 12u) % 170u) / 100.0f;
        const Color tint = Color {
            static_cast<unsigned char>(34 + ((hash >> 5u) % 18u)),
            static_cast<unsigned char>(48 + ((hash >> 9u) % 20u)),
            static_cast<unsigned char>(37 + ((hash >> 13u) % 16u)),
            255
        };

        const unsigned treeChoice = (hash >> 2u) % 6u;
        const Model& model =
            treeChoice == 0u ? assets_->treeDefault :
            treeChoice == 1u ? assets_->treeCone :
            treeChoice == 2u ? assets_->treeFat :
            treeChoice == 3u ? assets_->treeOak :
            treeChoice == 4u ? assets_->treePineTall :
                                assets_->treePineRound;
        DrawModelIfReady(model, Vector3 {x, 0.0f, z}, Vector3 {0, 1, 0}, static_cast<float>((hash >> 18u) % 360u), Vector3 {scale, scale, scale}, tint);

        if ((hash % 13u) == 0u) {
            DrawModelIfReady(assets_->rockLarge, Vector3 {-x * 0.92f, 0.0f, z - 3.5f}, Vector3 {0, 1, 0}, 0.0f, Vector3 {0.8f, 0.8f, 0.8f}, Color {58, 58, 56, 255});
        }
        if ((hash % 9u) == 0u) {
            const float undergrowthX = static_cast<float>(side) * (state.roadWidth * 0.5f + 1.6f + static_cast<float>((hash >> 21u) % 240u) / 100.0f);
            const Model& bush = ((hash >> 24u) & 1u) ? assets_->bushDetailed : assets_->bushLarge;
            DrawModelIfReady(bush, Vector3 {undergrowthX, 0.0f, z + 1.2f}, Vector3 {0, 1, 0}, static_cast<float>((hash >> 10u) % 360u), Vector3 {1.1f, 1.1f, 1.1f}, Color {26, 50, 32, 255});
        }
        if ((hash % 17u) == 0u) {
            const float grassX = static_cast<float>(side) * (state.roadWidth * 0.5f + 0.95f);
            DrawModelIfReady(assets_->grassLarge, Vector3 {grassX, -0.02f, z - 2.0f}, Vector3 {0, 1, 0}, static_cast<float>((hash >> 16u) % 360u), Vector3 {0.75f, 0.75f, 0.75f}, Color {33, 54, 32, 230});
        }
    }
}

void Renderer::DrawRoadsideDressing(const GameState& state) {
    const float distanceMeters = state.distanceMiles * kMetersPerMile;

    struct Dressing {
        float mile;
        float side;
        float offset;
        int kind;
        float angle;
    };

    const std::array<Dressing, 14> props {{
        {0.18f, 1.0f, 6.4f, 0, -8.0f},
        {0.55f, -1.0f, 5.7f, 1, 18.0f},
        {0.92f, 1.0f, 7.1f, 2, 37.0f},
        {1.18f, -1.0f, 6.2f, 3, -22.0f},
        {1.68f, 1.0f, 5.9f, 4, 8.0f},
        {2.35f, -1.0f, 3.6f, 5, 0.0f},
        {2.38f, 1.0f, 3.6f, 5, 180.0f},
        {2.62f, -1.0f, 5.1f, 1, 64.0f},
        {2.86f, 1.0f, 5.5f, 6, 35.0f},
        {3.10f, -1.0f, 5.2f, 0, -12.0f},
        {3.22f, 1.0f, 5.8f, 7, 0.0f},
        {3.58f, -1.0f, 7.3f, 8, 24.0f},
        {3.88f, 1.0f, 6.6f, 2, -18.0f},
        {4.03f, -1.0f, 4.8f, 4, 0.0f}
    }};

    for (const Dressing& prop : props) {
        const float z = RelativeZ(prop.mile * kMetersPerMile, distanceMeters);
        if (z > 8.0f || z < -175.0f) continue;
        const Vector3 pos {prop.side * prop.offset, 0.0f, z};
        switch (prop.kind) {
            case 0:
                DrawModelIfReady(assets_->roadSign, pos, Vector3 {0, 1, 0}, prop.angle, Vector3 {1.15f, 1.15f, 1.15f}, Color {116, 105, 78, 255});
                break;
            case 1:
                DrawModelIfReady(assets_->stumpOld, pos, Vector3 {0, 1, 0}, prop.angle, Vector3 {1.2f, 1.2f, 1.2f}, Color {66, 46, 31, 255});
                break;
            case 2:
                DrawModelIfReady(assets_->logStack, pos, Vector3 {0, 1, 0}, prop.angle, Vector3 {1.15f, 1.15f, 1.15f}, Color {82, 57, 39, 255});
                break;
            case 3:
                DrawModelIfReady(assets_->fencePlanks, pos, Vector3 {0, 1, 0}, prop.angle, Vector3 {1.4f, 1.4f, 1.4f}, Color {66, 50, 38, 255});
                break;
            case 4:
                DrawModelIfReady(assets_->pathRocks, pos, Vector3 {0, 1, 0}, prop.angle, Vector3 {1.3f, 1.3f, 1.3f}, Color {66, 64, 59, 255});
                break;
            case 5:
                DrawModelIfReady(assets_->bridgeSide, pos, Vector3 {0, 1, 0}, prop.angle, Vector3 {1.8f, 1.8f, 1.8f}, Color {67, 50, 35, 255});
                break;
            case 6:
                DrawModelIfReady(assets_->stumpRound, pos, Vector3 {0, 1, 0}, prop.angle, Vector3 {1.0f, 1.0f, 1.0f}, Color {73, 50, 33, 255});
                break;
            case 7:
                DrawModelIfReady(assets_->skeleton, Vector3 {pos.x, 0.05f, pos.z}, Vector3 {0, 1, 0}, prop.angle, Vector3 {0.82f, 0.82f, 0.82f}, Color {118, 113, 96, 210});
                break;
            case 8:
                DrawModelIfReady(assets_->fenceGate, pos, Vector3 {0, 1, 0}, prop.angle, Vector3 {1.6f, 1.6f, 1.6f}, Color {75, 55, 38, 255});
                break;
            default:
                break;
        }
    }

    if (state.tension > 38.0f) {
        for (int i = 0; i < 5; ++i) {
            const unsigned hash = Hash(static_cast<unsigned>(i) * 3457u + static_cast<unsigned>(state.chapterIndex) * 119u);
            const float z = -48.0f - static_cast<float>((hash >> 4u) % 120u);
            const float x = -18.0f + static_cast<float>((hash >> 12u) % 360u) / 10.0f;
            const float y = 9.0f + static_cast<float>((hash >> 22u) % 120u) / 20.0f + std::sin(state.elapsed * 1.7f + i) * 0.55f;
            DrawModelIfReady(assets_->wingedSilhouette, Vector3 {x, y, z}, Vector3 {0, 1, 0}, 180.0f + std::sin(state.elapsed + i) * 18.0f, Vector3 {0.45f, 0.45f, 0.45f}, Fade(Color {3, 5, 5, 255}, 0.62f));
        }
    }

    for (const WorldProp& prop : WorldGenerator::PropsAround(state.distanceMiles, 0.04f, 0.16f, state.roadWidth)) {
        DrawWorldProp(state, prop);
    }
}

void Renderer::DrawWorldProp(const GameState& state, const WorldProp& prop) {
    const float distanceMeters = state.distanceMiles * kMetersPerMile;
    const float z = RelativeZ(prop.mile * kMetersPerMile, distanceMeters);
    if (z > 8.0f || z < -210.0f) return;

    const Vector3 pos {prop.lateral, 0.0f, z};
    const float flicker = (std::sin(state.elapsed * 11.0f + static_cast<float>(prop.seed % 31u)) + 1.0f) * 0.5f;
    switch (prop.kind) {
        case WorldPropKind::Tree: {
            const Model& model = (prop.seed & 1u) ? assets_->treePineTall : ((prop.seed & 2u) ? assets_->treeOak : assets_->treeDefault);
            const float scale = 0.7f + prop.height * 0.22f;
            DrawModelIfReady(model, pos, Vector3 {0, 1, 0}, static_cast<float>(prop.seed % 360u), Vector3 {scale, scale, scale}, Color {27, 43, 31, 255});
            break;
        }
        case WorldPropKind::Building: {
            const Color wall = Color {28, 30, 29, 255};
            DrawCube(Vector3 {pos.x, prop.height * 0.5f, pos.z}, prop.width, prop.height, prop.depth, wall);
            DrawCubeWires(Vector3 {pos.x, prop.height * 0.5f, pos.z}, prop.width, prop.height, prop.depth, Color {62, 57, 49, 150});
            for (int floor = 0; floor < 3; ++floor) {
                for (int col = -1; col <= 1; ++col) {
                    if (((prop.seed + floor * 3u + static_cast<unsigned>(col + 2)) % 4u) == 0u) continue;
                    DrawCube(Vector3 {pos.x + col * prop.width * 0.22f, 0.85f + floor * 0.82f, pos.z - prop.depth * 0.51f}, 0.32f, 0.24f, 0.04f, Color {122, 103, 61, static_cast<unsigned char>(110 + flicker * 90)});
                }
            }
            break;
        }
        case WorldPropKind::IndustrialTank:
            DrawCylinder(Vector3 {pos.x, prop.height * 0.5f, pos.z}, prop.width * 0.42f, prop.width * 0.45f, prop.height, 18, Color {42, 48, 48, 255});
            DrawCube(Vector3 {pos.x, prop.height + 0.2f, pos.z}, prop.width * 0.72f, 0.25f, prop.depth * 0.72f, Color {63, 67, 64, 255});
            break;
        case WorldPropKind::TrafficLight:
            DrawCube(Vector3 {0.0f, 2.3f, pos.z}, 0.16f, 4.4f, 0.16f, Color {22, 24, 22, 255});
            DrawCube(Vector3 {0.0f, 4.2f, pos.z}, 2.4f, 0.12f, 0.12f, Color {22, 24, 22, 255});
            DrawSphere(Vector3 {0.72f, 4.08f, pos.z - 0.08f}, 0.12f, flicker > 0.52f ? Color {115, 15, 9, 255} : Color {34, 8, 7, 255});
            break;
        case WorldPropKind::StreetLamp:
            DrawCube(Vector3 {pos.x, 1.85f, pos.z}, 0.12f, 3.7f, 0.12f, Color {27, 29, 28, 255});
            BeginBlendMode(BLEND_ADDITIVE);
            DrawSphere(Vector3 {pos.x, 3.78f, pos.z}, 0.22f, Fade(Color {255, 199, 112, 255}, 0.16f + flicker * 0.16f));
            EndBlendMode();
            break;
        case WorldPropKind::GasPump:
            DrawCube(Vector3 {pos.x, 0.62f, pos.z}, 0.52f, 1.24f, 0.38f, Color {116, 31, 24, 255});
            DrawCube(Vector3 {pos.x, 1.15f, pos.z - 0.21f}, 0.34f, 0.2f, 0.04f, Color {34, 37, 33, 255});
            break;
        case WorldPropKind::Billboard:
            DrawCube(Vector3 {pos.x, 1.1f, pos.z}, 0.12f, 2.2f, 0.12f, Color {49, 42, 34, 255});
            DrawCube(Vector3 {pos.x, 2.2f, pos.z}, prop.width, prop.height, 0.12f, Color {44, 36, 31, 255});
            break;
        case WorldPropKind::UtilityPole:
            DrawCube(Vector3 {pos.x, prop.height * 0.5f, pos.z}, 0.18f, prop.height, 0.18f, Color {50, 38, 27, 255});
            DrawCube(Vector3 {pos.x, prop.height - 0.7f, pos.z}, 2.1f, 0.09f, 0.09f, Color {49, 38, 29, 255});
            break;
        case WorldPropKind::RoadBarrier:
            DrawCube(Vector3 {pos.x, 0.34f, pos.z}, prop.width, prop.height, prop.depth, Color {102, 72, 43, 255});
            DrawCube(Vector3 {pos.x, 0.62f, pos.z - 0.01f}, prop.width * 0.82f, 0.08f, prop.depth * 1.05f, Color {151, 37, 28, 255});
            break;
        case WorldPropKind::WindowWatcher:
            BeginBlendMode(BLEND_ADDITIVE);
            DrawSphere(Vector3 {pos.x - 0.13f, prop.height + 1.0f, pos.z - 0.45f}, 0.055f, Fade(PaleEye(), 0.7f));
            DrawSphere(Vector3 {pos.x + 0.13f, prop.height + 1.0f, pos.z - 0.45f}, 0.055f, Fade(PaleEye(), 0.7f));
            EndBlendMode();
            break;
    }
}

void Renderer::DrawHazards(const GameState& state) {
    const float distanceMeters = state.distanceMiles * kMetersPerMile;
    for (const RoadHazard& hazard : state.hazards) {
        if (hazard.cleared) continue;
        const float z = RelativeZ(hazard.distanceMiles * kMetersPerMile, distanceMeters);
        if (z > 6.0f || z < -170.0f) continue;

        const Vector3 pos {hazard.laneOffset, 0.05f, z};
        if (hazard.kind == 0) {
            DrawCube(pos, hazard.width * 1.4f, 0.04f, 3.2f, Color {26, 17, 13, 255});
        } else if (hazard.kind == 1 || hazard.kind == 3) {
            DrawModelIfReady(assets_->logLarge, pos, Vector3 {0, 1, 0}, 92.0f, Vector3 {1.8f, 1.8f, 1.8f}, Color {78, 56, 39, 255});
        } else if (hazard.kind == 2) {
            DrawModelIfReady(assets_->sedan, pos, Vector3 {0, 1, 0}, -18.0f, Vector3 {1.4f, 1.4f, 1.4f}, Color {55, 62, 66, 255});
        } else {
            DrawModelIfReady(assets_->fenceGate, pos, Vector3 {0, 1, 0}, 12.0f, Vector3 {1.5f, 1.5f, 1.5f}, Color {66, 54, 40, 255});
        }
    }
}

void Renderer::DrawCreature(const GameState& state) {
    if (state.creature.visibility <= 0.01f) return;

    float z = -state.creature.distanceAhead;
    float x = state.creature.lateral;
    float y = 0.0f;
    if (state.creature.mode == CreatureMode::Mirror) {
        z = 3.9f;
        x = state.car.lateralOffset + 0.0f;
        y = 1.53f;
    } else if (state.creature.mode == CreatureMode::Jumpscare) {
        const float t = Smooth01(state.creature.attackCharge);
        z = 1.28f - t * 0.96f;
        x = state.car.lateralOffset + std::sin(state.elapsed * 28.0f) * 0.12f;
        y = -0.16f + t * 0.08f;
    }

    const float alpha = Clamp(state.creature.visibility, 0.0f, 1.0f);
    const float phase = state.elapsed * (state.creature.mode == CreatureMode::Lunging || state.creature.mode == CreatureMode::Jumpscare ? 9.0f : 4.8f);
    const bool close = state.creature.mode == CreatureMode::Lunging || state.creature.mode == CreatureMode::Attacking || state.creature.mode == CreatureMode::Crossing || state.creature.mode == CreatureMode::Jumpscare;
    const int variant = state.creature.variant % 4;
    const Color variantTint =
        variant == 1 ? Color {215, 206, 178, 255} :
        variant == 2 ? Color {95, 107, 110, 255} :
        variant == 3 ? Color {122, 126, 116, 255} :
                       Color {191, 198, 184, 255};
    const Color body = Fade(variantTint, alpha * (close ? 0.76f : 0.44f));
    const Vector3 pos {x, y, z};

    if (state.creature.mode == CreatureMode::Mirror) {
        DrawCreatureEyes(Vector3 {pos.x, pos.y + 0.22f, pos.z}, 0.28f, 0.04f, alpha);
        BeginBlendMode(BLEND_ADDITIVE);
        DrawSphere(Vector3 {pos.x, pos.y + 0.22f, pos.z}, 0.26f, Fade(PaleEye(), alpha * 0.12f));
        EndBlendMode();
        return;
    }

    if (variant == 1 && ModelReady(assets_->skeleton)) {
        const float scale = (state.creature.mode == CreatureMode::Jumpscare ? 1.55f : (close ? 1.05f : 0.82f));
        DrawModelIfReady(assets_->skeleton, Vector3 {pos.x, 0.02f, pos.z}, Vector3 {0, 1, 0}, 180.0f + std::sin(phase) * 9.0f, Vector3 {scale, scale, scale}, body);
    } else if (variant == 2 && ModelReady(assets_->wingedSilhouette)) {
        const float scale = (state.creature.mode == CreatureMode::Jumpscare ? 2.15f : (close ? 1.35f : 0.9f));
        DrawModelIfReady(assets_->wingedSilhouette, Vector3 {pos.x, 1.05f, pos.z}, Vector3 {0, 1, 0}, 180.0f + std::sin(phase) * 15.0f, Vector3 {scale, scale, scale}, Fade(body, 0.9f));
        DrawCapsule(Vector3 {pos.x, 0.4f, pos.z + 0.05f}, Vector3 {pos.x, 1.95f, pos.z - 0.18f}, 0.16f * scale, 8, 6, Fade(Color {16, 18, 18, 255}, alpha * 0.78f));
    } else if (ModelReady(assets_->horrorMonster)) {
        const float lean = state.creature.mode == CreatureMode::Lunging || state.creature.mode == CreatureMode::Jumpscare ? -18.0f : (state.creature.mode == CreatureMode::Crossing ? 20.0f : 0.0f);
        const float scale = state.creature.mode == CreatureMode::Jumpscare ? 2.1f + state.creature.attackCharge * 0.25f : (close ? 1.08f + std::sin(phase) * 0.03f : 0.86f);
        DrawModelIfReady(assets_->horrorMonster, Vector3 {pos.x, 0.0f, pos.z}, Vector3 {0, 1, 0}, 180.0f + lean, Vector3 {scale, scale, scale}, body);
    } else {
        DrawCapsule(Vector3 {x, 0.35f, z}, Vector3 {x, 1.78f, z - 0.2f}, 0.22f, 8, 8, body);
    }

    DrawCreatureAppendages(pos, state.creature.mode == CreatureMode::Jumpscare ? 1.85f : (close ? 1.0f : 0.72f), alpha, phase, close);
    DrawCreatureEyes(Vector3 {pos.x, 1.92f + (close ? std::sin(phase * 0.6f) * 0.04f : 0.0f), pos.z - 0.1f}, close ? 0.36f : 0.24f, state.creature.mode == CreatureMode::Jumpscare ? 0.12f : (close ? 0.075f : 0.06f), alpha);

    BeginBlendMode(BLEND_ADDITIVE);
    DrawSphere(Vector3 {x, 1.95f, z - 0.05f}, close ? 0.55f : 0.34f, Fade(PaleEye(), alpha * (close ? 0.2f : 0.13f)));
    EndBlendMode();
}

void Renderer::DrawCreatureAppendages(Vector3 position, float scale, float alpha, float phase, bool close) const {
    const Color limb = Fade(Color {172, 181, 167, 255}, alpha * 0.78f);
    const Color dark = Fade(Color {35, 38, 34, 255}, alpha * 0.72f);
    const float lunge = close ? 1.0f : 0.55f;
    const float sway = std::sin(phase) * 0.18f;

    const Vector3 spineBase {position.x, 0.18f, position.z + 0.05f};
    const Vector3 spineTop {position.x + sway * 0.25f, 2.26f * scale, position.z - 0.22f};
    DrawCapsule(spineBase, spineTop, 0.06f * scale, 7, 5, dark);

    for (int side : {-1, 1}) {
        const float s = static_cast<float>(side);
        Vector3 shoulder {position.x + s * 0.32f * scale, 1.62f * scale, position.z - 0.06f};
        Vector3 elbow {position.x + s * (0.92f + 0.22f * lunge) * scale, (1.05f + sway * s * 0.2f) * scale, position.z - 0.38f * lunge};
        Vector3 hand {position.x + s * (1.35f + 0.42f * lunge) * scale, (0.48f + std::sin(phase + s) * 0.08f) * scale, position.z - 0.7f * lunge};
        DrawCapsule(shoulder, elbow, 0.055f * scale, 6, 5, limb);
        DrawCapsule(elbow, hand, 0.045f * scale, 6, 5, limb);
        for (int claw = 0; claw < 3; ++claw) {
            const float clawOffset = (static_cast<float>(claw) - 1.0f) * 0.07f;
            DrawCapsule(hand, Vector3 {hand.x + s * 0.22f, hand.y - 0.08f + clawOffset, hand.z - 0.18f}, 0.012f * scale, 4, 3, Fade(Color {215, 218, 203, 255}, alpha));
        }

        Vector3 hip {position.x + s * 0.22f * scale, 0.82f * scale, position.z + 0.03f};
        Vector3 knee {position.x + s * (0.42f + 0.08f * lunge) * scale, 0.34f * scale, position.z - 0.32f};
        Vector3 foot {position.x + s * (0.64f + 0.18f * lunge) * scale, 0.04f, position.z - 0.74f};
        DrawCapsule(hip, knee, 0.065f * scale, 6, 5, limb);
        DrawCapsule(knee, foot, 0.05f * scale, 6, 5, limb);
    }

    for (int spike = 0; spike < 5; ++spike) {
        const float s = static_cast<float>(spike - 2);
        const Vector3 root {position.x + s * 0.08f * scale, (1.82f + spike * 0.06f) * scale, position.z - 0.16f};
        const Vector3 tip {position.x + s * 0.24f * scale, (2.35f + (2 - std::abs(spike - 2)) * 0.08f) * scale, position.z - 0.36f};
        DrawCapsule(root, tip, 0.018f * scale, 4, 3, Fade(Color {61, 55, 48, 255}, alpha * 0.85f));
    }
}

void Renderer::DrawCreatureEyes(Vector3 position, float width, float size, float alpha) const {
    const Color eye = Fade(PaleEye(), alpha);
    DrawSphere(Vector3 {position.x - width * 0.5f, position.y, position.z}, size, eye);
    DrawSphere(Vector3 {position.x + width * 0.5f, position.y, position.z}, size, eye);
    BeginBlendMode(BLEND_ADDITIVE);
    DrawSphere(Vector3 {position.x - width * 0.5f, position.y, position.z}, size * 2.3f, Fade(PaleEye(), alpha * 0.24f));
    DrawSphere(Vector3 {position.x + width * 0.5f, position.y, position.z}, size * 2.3f, Fade(PaleEye(), alpha * 0.24f));
    EndBlendMode();
}

void Renderer::DrawCockpit3D(const GameState& state) {
    const CockpitRig& rig = ToyotaCockpitRig();
    const auto P = [&](Vector3 local) { return CarToWorld(state, local, rig); };
    const auto A = [&](Hotspot hotspot) { return CockpitAnchorFor(hotspot).local; };

    if (ModelReady(assets_->toyotaCockpit)) {
        DrawModelIfReady(
            assets_->toyotaCockpit,
            P(rig.modelPosition),
            Vector3 {0, 1, 0},
            rig.modelYawDegrees,
            rig.modelScale,
            renderOptions_.brightenCockpit ? Color {235, 226, 204, 255} : Color {178, 170, 150, 255}
        );
    } else {
        DrawCube(P({0.0f, 0.48f, 1.18f}), 5.4f, 0.78f, 0.55f, Color {22, 24, 22, 255});
        DrawCube(P({0.0f, 0.92f, 0.86f}), 5.7f, 0.12f, 0.18f, Color {8, 10, 10, 255});
        DrawCube(P({-2.85f, 1.45f, 0.7f}), 0.16f, 2.3f, 0.26f, Color {10, 12, 12, 255});
        DrawCube(P({2.85f, 1.45f, 0.7f}), 0.16f, 2.3f, 0.26f, Color {10, 12, 12, 255});
        DrawCube(P({0.0f, 2.46f, 0.58f}), 5.7f, 0.18f, 0.28f, Color {6, 7, 7, 255});
    }

    const Color gaugeGlow = state.car.engineOn ? Color {235, 153, 71, 255} : Color {94, 60, 44, 255};
    const Color trim = Color {82, 76, 63, 255};
    const Color softBlack = Color {10, 12, 12, 245};
    const Color panel = Color {20, 22, 20, 238};
    const float wheelPull = state.car.steering * 12.0f + state.car.steeringInterference * 18.0f;

    rlDisableDepthTest();

    const Vector3 horn = A(Hotspot::Horn);
    const Vector3 radio = A(Hotspot::Radio);
    const Vector3 ignition = A(Hotspot::Ignition);
    const Vector3 shifter = A(Hotspot::GearShift);
    const Vector3 handbrake = A(Hotspot::Handbrake);
    const Vector3 lock = A(Hotspot::DoorLock);
    const Vector3 handle = A(Hotspot::DoorHandle);
    const Vector3 mirror = A(Hotspot::Mirror);
    const Vector3 lights = A(Hotspot::Headlights);
    const Vector3 wipers = A(Hotspot::Wipers);
    const Vector3 fan = A(Hotspot::Fan);
    const Vector3 dome = A(Hotspot::DomeLight);
    const Vector3 glovebox = A(Hotspot::Glovebox);
    const Vector3 window = A(Hotspot::WindowCrank);
    const Color vinyl = Color {24, 25, 23, 242};
    const Color oldPlastic = Color {34, 34, 31, 246};
    const Color recessed = Color {4, 5, 5, 250};
    const Color dust = Color {118, 98, 72, 164};

    DrawCube(P({0.10f, 0.94f, 2.03f}), 1.06f, 0.82f, 0.12f, Color {22, 23, 21, 242});
    DrawCube(P({0.58f, 0.86f, 2.08f}), 0.88f, 0.46f, 0.08f, oldPlastic);
    DrawCube(P({-0.48f, 0.87f, 2.13f}), 0.98f, 0.24f, 0.06f, Color {16, 18, 17, 232});
    DrawCube(P(Add3(shifter, {0.10f, -0.45f, 0.33f})), 0.86f, 0.22f, 0.82f, Color {12, 13, 12, 240});
    DrawCube(P(Add3(shifter, {0.10f, -0.31f, 0.58f})), 0.74f, 0.07f, 0.42f, Color {25, 25, 22, 245});
    DrawCylinder(P(Add3(shifter, {0.05f, -0.25f, 0.74f})), 0.14f, 0.14f, 0.035f, 24, recessed);
    DrawCylinder(P(Add3(shifter, {0.30f, -0.25f, 0.74f})), 0.13f, 0.13f, 0.035f, 24, recessed);

    DrawCube(P({-0.58f, 0.98f, 2.10f}), 0.72f, 0.30f, 0.08f, Color {8, 9, 9, 242});
    DrawSphere(P({-0.78f, 1.00f, 2.14f}), 0.105f, Color {21, 25, 24, 255});
    DrawSphere(P({-0.58f, 1.00f, 2.14f}), 0.092f, Color {21, 25, 24, 255});
    DrawSphere(P({-0.39f, 0.99f, 2.14f}), 0.064f, Color {21, 25, 24, 255});
    DrawSphere(P({-0.78f, 1.00f, 2.18f}), 0.038f, gaugeGlow);
    DrawSphere(P({-0.58f, 1.00f, 2.18f}), 0.034f, gaugeGlow);
    DrawSphere(P({-0.39f, 0.99f, 2.18f}), 0.026f, gaugeGlow);
    DrawCube(P({-0.56f, 0.82f, 2.16f}), 0.48f, 0.045f, 0.035f, Color {102, 76, 49, 255});

    DrawCylinderEx(P(Add3(horn, {0.0f, 0.0f, 0.15f})), P(Add3(horn, {0.0f, 0.0f, -0.13f})), 0.38f, 0.38f, 36, softBlack);
    DrawCylinderEx(P(Add3(horn, {0.0f, 0.0f, 0.17f})), P(Add3(horn, {0.0f, 0.0f, -0.15f})), 0.24f, 0.24f, 36, Color {20, 22, 21, 255});
    DrawCube(P(Add3(horn, {std::sin(wheelPull * DEG2RAD) * 0.055f, 0.0f, 0.02f})), 0.58f, 0.06f, 0.055f, trim);
    DrawCube(P(Add3(horn, {0.0f, std::cos(wheelPull * DEG2RAD) * 0.045f, 0.02f})), 0.06f, 0.50f, 0.055f, trim);
    DrawSphere(P(Add3(horn, {0.0f, 0.0f, -0.20f})), 0.105f, state.car.cockpitFeedbackTimer > 0.0f && state.car.lastCockpitControl == Hotspot::Horn ? Amber() : Color {24, 25, 24, 255});

    DrawSphere(P(ignition), 0.075f, state.car.engineOn ? Amber() : Color {62, 58, 49, 255});
    DrawCube(P(lights), 0.20f, 0.13f, 0.05f, state.car.headlightsOn ? Color {198, 173, 112, 255} : Color {41, 43, 39, 255});
    DrawCube(P(wipers), 0.25f, 0.07f, 0.05f, state.car.wipersOn ? Color {145, 171, 172, 255} : Color {55, 57, 53, 255});

    const float radioShake = state.car.radioInterferenceTimer > 0.0f ? std::sin(state.elapsed * 28.0f) * 0.035f : 0.0f;
    for (int side = -1; side <= 1; side += 2) {
        const Vector3 vent = Add3(radio, {side * 0.23f, 0.34f, -0.01f});
        DrawCube(P(vent), 0.38f, 0.16f, 0.055f, recessed);
        for (int slat = 0; slat < 4; ++slat) {
            DrawCube(P(Add3(vent, {0.0f, -0.055f + slat * 0.035f, 0.04f})), 0.32f, 0.008f, 0.025f, Color {82, 82, 72, 255});
        }
        DrawCube(P(Add3(vent, {0.0f, 0.0f, 0.055f})), 0.055f, 0.028f, 0.025f, Color {126, 118, 96, 255});
    }
    DrawCube(P(Add3(radio, {radioShake, 0.20f, 0.025f})), 0.34f, 0.09f, 0.045f, Color {4, 8, 7, 250});
    DrawCube(P(Add3(radio, {radioShake, 0.20f, 0.055f})), 0.16f, 0.032f, 0.022f, state.car.radioOn ? Color {90, 211, 135, 255} : Color {70, 86, 61, 255});
    for (int i = 0; i < 5; ++i) {
        const float x = -0.30f + i * 0.15f;
        DrawCube(P(Add3(radio, {x, 0.115f, 0.055f})), 0.075f, 0.055f, 0.028f, i == 0 ? Color {92, 43, 36, 255} : Color {50, 52, 47, 255});
    }
    DrawCube(P(Add3(radio, {radioShake, 0.0f, 0.0f})), 0.84f, 0.32f, 0.10f, state.car.radioLoose ? Color {112, 58, 43, 255} : Color {30, 31, 28, 248});
    DrawCube(P(Add3(radio, {radioShake, 0.038f, 0.068f})), 0.54f, 0.090f, 0.035f, state.car.radioOn ? Color {75, 214, 164, 255} : Color {42, 54, 38, 255});
    DrawCube(P(Add3(radio, {radioShake, -0.055f, 0.061f})), 0.50f, 0.026f, 0.030f, recessed);
    for (int i = 0; i < 6; ++i) {
        DrawCube(P(Add3(radio, {-0.235f + i * 0.094f + radioShake, -0.105f, 0.065f})), 0.060f, 0.035f, 0.026f, Color {72, 66, 55, 255});
    }
    DrawCylinderEx(P(Add3(radio, {-0.37f + radioShake, 0.0f, 0.055f})), P(Add3(radio, {-0.37f + radioShake, 0.0f, 0.115f})), 0.064f, 0.056f, 22, trim);
    DrawCylinderEx(P(Add3(radio, {0.37f + radioShake, 0.0f, 0.055f})), P(Add3(radio, {0.37f + radioShake, 0.0f, 0.115f})), 0.064f, 0.056f, 22, trim);

    const float fanSpin = state.car.fan == FanState::Stopped ? 0.0f : (state.car.fan == FanState::Slow ? 0.35f : (state.car.fan == FanState::Irregular ? 0.66f + std::sin(state.elapsed * 13.0f) * 0.25f : 1.0f));
    DrawCube(P(Add3(fan, {-0.22f, 0.0f, -0.015f})), 0.74f, 0.26f, 0.07f, Color {17, 18, 17, 246});
    for (int knob = 0; knob < 3; ++knob) {
        const Vector3 knobCenter = Add3(fan, {-0.49f + knob * 0.27f, 0.01f, 0.055f});
        DrawCylinderEx(P(Add3(knobCenter, {0.0f, 0.0f, -0.025f})), P(Add3(knobCenter, {0.0f, 0.0f, 0.055f})), 0.072f, 0.062f, 22, Color {53, 55, 50, 255});
        DrawCube(P(Add3(knobCenter, {0.0f, 0.035f, 0.08f})), 0.030f, 0.075f, 0.025f, Color {133, 123, 96, 255});
    }
    DrawCube(P(Add3(fan, {-0.49f, -0.095f, 0.063f})), 0.13f, 0.025f, 0.020f, Color {67, 123, 163, 255});
    DrawCube(P(Add3(fan, {0.05f, -0.095f, 0.063f})), 0.13f, 0.025f, 0.020f, Color {169, 70, 54, 255});
    for (int i = 0; i < 4; ++i) {
        const float angle = state.elapsed * 9.0f * fanSpin + static_cast<float>(i) * 1.5707963f;
        DrawCube(P(Add3(fan, {0.18f + std::cos(angle) * 0.08f, 0.01f + std::sin(angle) * 0.08f, 0.08f})), 0.16f, 0.03f, 0.03f, Color {116, 119, 112, 255});
    }
    DrawCube(P(Add3(fan, {-0.22f, -0.27f, -0.015f})), 0.64f, 0.15f, 0.06f, recessed);
    DrawCube(P(Add3(fan, {-0.22f, -0.27f, 0.035f})), 0.54f, 0.030f, 0.025f, Color {66, 61, 51, 255});
    DrawCube(P(glovebox), 0.64f, 0.24f, 0.07f, panel);
    DrawCube(P(Add3(glovebox, {0.0f, 0.03f, 0.05f})), 0.50f, 0.03f, 0.025f, Color {83, 76, 61, 255});

    DrawCube(P({-0.52f, 0.31f, 2.18f}), 0.74f, 0.045f, 0.44f, Color {16, 16, 14, 244});
    for (int pedal = 0; pedal < 3; ++pedal) {
        const float x = -0.78f + pedal * 0.22f;
        const float h = pedal == 2 ? 0.22f : 0.17f;
        DrawCube(P({x, 0.36f, 2.08f}), 0.11f, h, 0.035f, Color {58, 55, 47, 255});
        for (int rib = 0; rib < 3; ++rib) {
            DrawCube(P({x, 0.30f + rib * 0.045f, 2.11f}), 0.085f, 0.006f, 0.018f, Color {128, 116, 88, 190});
        }
    }
    for (int rib = 0; rib < 8; ++rib) {
        DrawCube(P({-0.36f, 0.235f, 2.40f + rib * 0.045f}), 0.62f, 0.012f, 0.010f, dust);
    }

    DrawCube(P(Add3(handle, {-0.02f, 0.05f, 0.08f})), 0.20f, 0.74f, 0.74f, Color {18, 18, 17, 238});
    DrawCube(P({-1.30f, 0.95f, 2.36f}), 0.16f, 0.50f, 0.74f, vinyl);
    DrawCube(P({-1.21f, 0.84f, 2.32f}), 0.11f, 0.12f, 0.54f, Color {62, 58, 48, 255});
    DrawCube(P({-1.18f, 0.70f, 2.44f}), 0.10f, 0.08f, 0.62f, Color {22, 23, 21, 244});
    DrawCube(P(lock), 0.10f, 0.10f, 0.10f, state.car.doorLock == DoorLockState::Locked ? Amber() : Color {70, 70, 66, 255});
    DrawCube(P(handle), state.car.doorHandleBroken ? 0.08f : 0.35f, 0.075f, 0.13f, state.car.doorHandleBroken ? Color {112, 63, 52, 255} : Color {116, 104, 83, 255});
    DrawCube(P(Add3(handle, {0.0f, -0.16f, 0.10f})), 0.46f, 0.055f, 0.08f, Color {77, 72, 60, 255});
    DrawCylinderEx(P(Add3(window, {0.0f, 0.0f, -0.025f})), P(Add3(window, {0.0f, 0.0f, 0.045f})), 0.09f, 0.09f, 20, Color {72, 68, 58, 255});
    DrawCylinderEx(P(Add3(window, {0.02f, -0.02f, 0.045f})), P(Add3(window, {0.02f, -0.14f, 0.08f})), 0.024f, 0.020f, 10, trim);
    DrawSphere(P(Add3(window, {0.02f, -0.16f, 0.09f})), 0.040f, trim);

    DrawCube(P(mirror), 0.96f, 0.18f, 0.05f, softBlack);
    if (state.creature.mirrorVisible) {
        DrawSphere(P(Add3(mirror, {-0.14f, 0.0f, -0.05f})), 0.035f, PaleEye());
        DrawSphere(P(Add3(mirror, {0.14f, 0.0f, -0.05f})), 0.035f, PaleEye());
    }

    DrawCube(P(dome), 0.64f, 0.07f, 0.18f, state.car.domeLightOn ? Color {214, 190, 130, 255} : Color {29, 30, 27, 255});

    const float slip = state.car.shifterSlipTimer > 0.0f ? std::sin(state.elapsed * 18.0f) * 0.16f : 0.0f;
    DrawCube(P(Add3(shifter, {0.0f, -0.40f, 0.22f})), 0.56f, 0.13f, 0.38f, Color {19, 20, 19, 255});
    for (int boot = 0; boot < 4; ++boot) {
        const float width = 0.34f - boot * 0.045f;
        DrawCube(P(Add3(shifter, {slip * 0.20f, -0.30f + boot * 0.055f, 0.13f - boot * 0.025f})), width, 0.035f, width * 0.78f, Color {31, 30, 27, 255});
    }
    DrawCylinderEx(P(Add3(shifter, {slip, -0.25f, 0.09f})), P(Add3(shifter, {slip * 1.35f, 0.07f, -0.02f})), 0.044f, 0.034f, 14, Color {92, 87, 75, 255});
    DrawSphere(P(Add3(shifter, {slip * 1.5f, 0.13f, -0.05f})), 0.095f, state.car.shifterSlipping ? Color {179, 63, 48, 255} : trim);
    DrawCube(P(Add3(shifter, {slip * 1.5f, 0.18f, -0.05f})), 0.12f, 0.015f, 0.045f, Color {192, 181, 145, 180});

    const bool handbrakeRecent = state.car.cockpitFeedbackTimer > 0.0f && state.car.lastCockpitControl == Hotspot::Handbrake;
    DrawCube(P(Add3(handbrake, {-0.04f, -0.35f, 0.26f})), 0.26f, 0.08f, 0.40f, Color {13, 14, 13, 252});
    DrawCylinderEx(P(Add3(handbrake, {-0.12f, -0.30f, 0.30f})), P(Add3(handbrake, {0.13f, 0.00f, -0.10f})), 0.030f, 0.032f, 12, Color {32, 31, 27, 255});
    DrawCylinderEx(P(Add3(handbrake, {0.08f, -0.03f, -0.06f})), P(Add3(handbrake, {0.23f, 0.06f, -0.17f})), 0.056f, 0.060f, 14, handbrakeRecent ? Amber() : Color {14, 15, 14, 255});
    DrawCube(P(Add3(handbrake, {0.24f, 0.07f, -0.18f})), 0.15f, 0.058f, 0.11f, handbrakeRecent ? Amber() : Color {24, 25, 22, 255});
    DrawSphere(P(Add3(handbrake, {0.31f, 0.08f, -0.22f})), 0.032f, Color {162, 46, 36, 255});

    for (int speck = 0; speck < 18; ++speck) {
        const float x = -0.90f + static_cast<float>((speck * 37) % 140) * 0.010f;
        const float y = 0.92f + static_cast<float>((speck * 19) % 26) * 0.006f;
        const float z = 2.04f + static_cast<float>((speck * 23) % 38) * 0.003f;
        DrawCube(P({x, y, z}), 0.026f, 0.006f, 0.006f, dust);
    }

    rlEnableDepthTest();
}

void Renderer::DrawCalibrationRig3D(const GameState& state) {
    const CockpitRig& rig = ToyotaCockpitRig();
    const auto P = [&](Vector3 local) { return CarToWorld(state, local, rig); };
    const Vector3 origin = P({0.0f, 0.04f, 0.0f});

    DrawLine3D(origin, P({1.25f, 0.04f, 0.0f}), RED);
    DrawLine3D(origin, P({0.0f, 1.29f, 0.0f}), GREEN);
    DrawLine3D(origin, P({0.0f, 0.04f, -1.25f}), BLUE);
    DrawSphere(origin, 0.045f, RAYWHITE);

    DrawWireBox(ToyotaModelBoundsWorld(state, rig), Color {255, 214, 126, 210});

    for (const CockpitAnchor& anchor : CockpitAnchors()) {
        const Vector3 world = P(anchor.local);
        Color color = Color {105, 230, 205, 255};
        if (anchor.hotspot == Hotspot::Horn || anchor.hotspot == Hotspot::Ignition) color = Color {255, 184, 83, 255};
        if (anchor.hotspot == Hotspot::DoorLock || anchor.hotspot == Hotspot::DoorHandle || anchor.hotspot == Hotspot::WindowCrank) color = Color {231, 96, 75, 255};
        if (anchor.hotspot == Hotspot::GearShift || anchor.hotspot == Hotspot::Handbrake) color = Color {180, 135, 255, 255};
        DrawSphere(world, 0.055f, color);
        DrawLine3D(world, Add3(world, {0.0f, 0.16f, 0.0f}), color);
    }
}

void Renderer::DrawCalibrationRigLabels(const GameState& state) {
    const CockpitRig& rig = ToyotaCockpitRig();
    const BoundingBox box = ToyotaModelBoundsWorld(state, rig);
    const Vector3 driverEye = CockpitCamera(state, CockpitCameraMode::Driver, rig).position;
    const Vector3 driverTarget = CockpitCamera(state, CockpitCameraMode::Driver, rig).target;

    DrawRectangle(12, 12, 520, 88, Color {4, 6, 6, 214});
    DrawRectangleLines(12, 12, 520, 88, Color {255, 214, 126, 185});
    DrawText("TOYOTA COCKPIT CALIBRATION", 28, 28, 16, RAYWHITE);
    DrawText("+X passenger/right   +Y up   -Z road/front", 28, 50, 13, Color {207, 220, 203, 255});
    std::ostringstream cameraInfo;
    cameraInfo.setf(std::ios::fixed);
    cameraInfo.precision(2);
    cameraInfo << "driver eye " << driverEye.x << "," << driverEye.y << "," << driverEye.z
               << "  target " << driverTarget.x << "," << driverTarget.y << "," << driverTarget.z;
    DrawText(cameraInfo.str().c_str(), 28, 72, 12, Color {193, 206, 196, 255});

    const std::array<std::pair<const char*, Vector3>, 3> axes {{
        {"X+", CarToWorld(state, {1.25f, 0.04f, 0.0f}, rig)},
        {"Y+", CarToWorld(state, {0.0f, 1.29f, 0.0f}, rig)},
        {"FRONT -Z", CarToWorld(state, {0.0f, 0.04f, -1.25f}, rig)}
    }};
    for (const auto& axis : axes) {
        Vector2 screen {};
        if (ProjectWorldToScreen(axis.second, camera_, GetScreenWidth(), GetScreenHeight(), screen)) {
            DrawText(axis.first, static_cast<int>(screen.x) + 6, static_cast<int>(screen.y) - 8, 12, RAYWHITE);
        }
    }

    Vector2 minScreen {};
    Vector2 maxScreen {};
    if (ProjectWorldToScreen(box.min, camera_, GetScreenWidth(), GetScreenHeight(), minScreen) &&
        ProjectWorldToScreen(box.max, camera_, GetScreenWidth(), GetScreenHeight(), maxScreen)) {
        std::ostringstream boundsText;
        boundsText.setf(std::ios::fixed);
        boundsText.precision(2);
        boundsText << "bounds min " << box.min.x << "," << box.min.y << "," << box.min.z
                   << " max " << box.max.x << "," << box.max.y << "," << box.max.z;
        DrawText(boundsText.str().c_str(), 28, 94, 12, Color {255, 214, 126, 220});
    }

    for (const CockpitAnchor& anchor : CockpitAnchors()) {
        const Vector3 world = CarToWorld(state, anchor.local, rig);
        Vector2 screen {};
        if (!ProjectWorldToScreen(world, camera_, GetScreenWidth(), GetScreenHeight(), screen)) {
            continue;
        }
        if (screen.x < -120.0f || screen.x > GetScreenWidth() + 120.0f || screen.y < -80.0f || screen.y > GetScreenHeight() + 80.0f) {
            continue;
        }
        std::ostringstream text;
        text.setf(std::ios::fixed);
        text.precision(2);
        text << anchor.name << " " << anchor.local.x << "," << anchor.local.y << "," << anchor.local.z
             << " [" << static_cast<int>(screen.x) << "," << static_cast<int>(screen.y) << "]";
        const std::string label = text.str();
        const int tw = MeasureText(label.c_str(), 10);
        const int x = static_cast<int>(screen.x) + 8;
        const int y = static_cast<int>(screen.y) - 9;
        DrawRectangle(x - 3, y - 2, tw + 6, 14, Color {4, 4, 4, 205});
        DrawText(label.c_str(), x, y, 10, Color {235, 225, 192, 255});
    }
}

void Renderer::DrawWireBox(BoundingBox box, Color color) const {
    const Vector3 a {box.min.x, box.min.y, box.min.z};
    const Vector3 b {box.max.x, box.min.y, box.min.z};
    const Vector3 c {box.max.x, box.min.y, box.max.z};
    const Vector3 d {box.min.x, box.min.y, box.max.z};
    const Vector3 e {box.min.x, box.max.y, box.min.z};
    const Vector3 f {box.max.x, box.max.y, box.min.z};
    const Vector3 g {box.max.x, box.max.y, box.max.z};
    const Vector3 h {box.min.x, box.max.y, box.max.z};

    DrawLine3D(a, b, color);
    DrawLine3D(b, c, color);
    DrawLine3D(c, d, color);
    DrawLine3D(d, a, color);
    DrawLine3D(e, f, color);
    DrawLine3D(f, g, color);
    DrawLine3D(g, h, color);
    DrawLine3D(h, e, color);
    DrawLine3D(a, e, color);
    DrawLine3D(b, f, color);
    DrawLine3D(c, g, color);
    DrawLine3D(d, h, color);
}

void Renderer::DrawHud(const GameState& state, const std::vector<HudMessage>& messages) {
    if (state.phase == GamePhase::Menu || state.phase == GamePhase::Settings || state.phase == GamePhase::Tutorial || state.jumpscareActive) return;

    const int w = GetScreenWidth();
    const int h = GetScreenHeight();
    DrawPanel(Rectangle {24, 24, 380, 86}, UiPanel(), UiBorder());
    DrawText("OBJECTIVE", 42, 38, 12, Color {148, 139, 112, 255});
    DrawText(state.story.objective.c_str(), 42, 56, 22, RAYWHITE);
    DrawText(state.story.subObjective.c_str(), 42, 82, 14, Color {202, 189, 152, 255});

    DrawPanel(Rectangle {w / 2.0f - 178.0f, 24, 356, 58}, UiPanel(), UiBorder());
    DrawText("N", w / 2 - 126, 39, 16, Color {188, 178, 150, 255});
    DrawText("^", w / 2 - 4, 38, 20, Amber());
    std::ostringstream distance;
    distance.setf(std::ios::fixed);
    distance.precision(2);
    distance << std::max(0.0f, state.towerDistanceMiles - state.distanceMiles) << " MI";
    DrawText(distance.str().c_str(), w / 2 + 70, 39, 16, Color {188, 178, 150, 255});
    const float progress = Clamp(state.distanceMiles / std::max(0.01f, state.towerDistanceMiles), 0.0f, 1.0f);
    DrawMeter(Rectangle {w / 2.0f - 146.0f, 63.0f, 292.0f, 8.0f}, "", progress, Color {157, 206, 196, 210}, Color {18, 20, 19, 220});

    DrawPanel(Rectangle {24, static_cast<float>(h - 172), 430, 124}, UiPanel(), UiBorder());
    DrawText((std::string("CONDITION ") + Percent(state.car.condition)).c_str(), 42, h - 156, 17, state.car.condition < 30.0f ? Color {255, 96, 74, 255} : RAYWHITE);
    DrawText((std::string("FUEL ") + Percent(state.car.fuel)).c_str(), 42, h - 132, 15, Color {207, 193, 151, 255});
    DrawText((std::string("BATTERY ") + Percent(state.car.battery)).c_str(), 180, h - 132, 15, state.car.battery < 25.0f ? Color {255, 114, 87, 255} : Color {207, 193, 151, 255});
    DrawText((std::string("HEADLIGHTS ") + Percent(state.car.headlightPower)).c_str(), 42, h - 108, 15, state.car.headlightPower < 25.0f ? Color {255, 114, 87, 255} : Color {207, 193, 151, 255});
    DrawText((std::string("FAN ") + Simulation::FanName(state.car.fan)).c_str(), 42, h - 84, 14, Color {157, 206, 196, 255});
    DrawText((std::string("LOCK ") + Simulation::LockName(state.car.doorLock)).c_str(), 180, h - 84, 14, Color {157, 206, 196, 255});
    DrawMeter(Rectangle {270.0f, static_cast<float>(h - 156), 158.0f, 17.0f}, "THREAT", state.tension / 100.0f, Color {169, 52, 39, 225});

    DrawPanel(Rectangle {static_cast<float>(w - 270), static_cast<float>(h - 148), 246, 100}, UiPanel(), UiBorder());
    const bool slowDanger = state.creature.slowHuntTimer > 0.1f;
    DrawText(Speed(state.car.speedMph).c_str(), w - 250, h - 131, 30, slowDanger ? Color {255, 91, 68, 255} : RAYWHITE);
    DrawText((std::string("GEAR ") + state.car.gear).c_str(), w - 120, h - 102, 18, Amber());
    DrawMeter(Rectangle {static_cast<float>(w - 250), static_cast<float>(h - 74), 202.0f, 14.0f}, "SLOW RISK", state.creature.slowHuntTimer / 2.65f, Color {210, 47, 32, 235});

    if (state.car.radioInterferenceTimer > 0.0f || state.car.shifterSlipTimer > 0.0f) {
        const int faultY = 112;
        DrawPanel(Rectangle {static_cast<float>(w - 330), static_cast<float>(faultY), 306, 88}, Color {18, 8, 7, 224}, Color {205, 75, 49, 172});
        DrawText("COCKPIT FAULT", w - 310, faultY + 18, 12, Color {255, 164, 124, 255});
        if (state.car.radioInterferenceTimer > 0.0f) {
            DrawText("Radio pull: click radio", w - 310, faultY + 44, 17, Color {255, 226, 191, 255});
            DrawMeter(Rectangle {static_cast<float>(w - 310), static_cast<float>(faultY + 72), 246.0f, 12.0f}, "", state.car.radioInterferenceTimer / 5.6f, Color {203, 83, 52, 230});
        } else {
            DrawText("Shifter slip: re-seat lever", w - 310, faultY + 44, 17, Color {255, 226, 191, 255});
            DrawMeter(Rectangle {static_cast<float>(w - 310), static_cast<float>(faultY + 72), 246.0f, 12.0f}, "", state.car.shifterSlipTimer / 4.2f, Color {203, 83, 52, 230});
        }
    }

    if (state.creature.mode != CreatureMode::Hidden && state.creature.mode != CreatureMode::Retreating) {
        DrawPanel(Rectangle {static_cast<float>(w - 318), 24.0f, 294.0f, 76.0f}, Color {11, 8, 8, 220}, Color {184, 67, 49, 160});
        DrawText("ENTITY", w - 300, 38, 12, Color {177, 130, 106, 255});
        DrawText(MonsterName(state.creature.variant), w - 300, 56, 18, Color {255, 213, 186, 255});
        const float proximity = Clamp(1.0f - state.creature.distanceAhead / 24.0f, 0.0f, 1.0f);
        DrawMeter(Rectangle {static_cast<float>(w - 300), 82.0f, 246.0f, 10.0f}, "", proximity, Color {207, 44, 31, 230});
    }

    if (state.story.messageTimer > 0.0f && !state.story.warning.empty()) {
        const int tw = MeasureText(state.story.warning.c_str(), 18);
        DrawPanel(Rectangle {w / 2.0f - tw / 2.0f - 24.0f, h - 226.0f, static_cast<float>(tw + 48), 42}, Color {20, 12, 10, 220}, Color {192, 83, 60, 160});
        DrawText(state.story.warning.c_str(), w / 2 - tw / 2, h - 213, 18, Color {255, 213, 186, 255});
    }

    if (state.settings.subtitles && !state.story.radioSubtitle.empty() && state.car.radioOn) {
        const int tw = MeasureText(state.story.radioSubtitle.c_str(), 18);
        DrawPanel(Rectangle {w / 2.0f - tw / 2.0f - 22.0f, h - 280.0f, static_cast<float>(tw + 44), 38}, Color {10, 18, 18, 205}, Color {72, 144, 135, 140});
        DrawText(state.story.radioSubtitle.c_str(), w / 2 - tw / 2, h - 268, 18, Color {161, 226, 214, 255});
    }

    int y = 116;
    for (const HudMessage& msg : messages) {
        if (msg.timer <= 0.0f) continue;
        Color text = msg.type == EventType::Death || msg.type == EventType::Impact || msg.type == EventType::Jumpscare ? Color {255, 150, 118, 230} : (msg.type == EventType::MonsterVoice ? Color {255, 185, 166, 230} : Color {218, 207, 173, 220});
        DrawText(msg.text.c_str(), 28, y, 14, text);
        y += 19;
    }

}

void Renderer::DrawCockpitControlHighlights(const GameState& state, Vector2 mouse) {
    if (state.phase != GamePhase::Playing || state.jumpscareActive) return;

    for (const CockpitControl& control : cockpitControls_) {
        const bool hover = CheckCollisionPointRec(mouse, control.rect);
        const bool recent = state.car.cockpitFeedbackTimer > 0.0f && state.car.lastCockpitControl == control.hotspot;
        if (!hover && !control.urgent && !recent) continue;

        const float pulse = 0.5f + std::sin(state.elapsed * 12.0f) * 0.5f;
        const Color border = control.urgent
            ? Fade(Color {255, 91, 62, 255}, 0.72f + pulse * 0.22f)
            : (recent ? Color {138, 220, 184, 220} : Color {231, 202, 141, 210});
        DrawRectangleRoundedLines(control.rect, 0.22f, 8, border);
        if (control.urgent) {
            DrawRectangleRounded(control.rect, 0.22f, 8, Fade(Color {128, 28, 18, 255}, 0.16f + pulse * 0.10f));
        }

        const char* label = control.label.c_str();
        const int tw = MeasureText(label, 14);
        Rectangle tag {control.rect.x + control.rect.width * 0.5f - tw * 0.5f - 9.0f, control.rect.y - 28.0f, static_cast<float>(tw + 18), 24.0f};
        DrawRectangleRounded(tag, 0.18f, 8, control.urgent ? Color {45, 13, 10, 230} : Color {9, 12, 12, 218});
        DrawRectangleRoundedLines(tag, 0.18f, 8, border);
        DrawText(label, static_cast<int>(tag.x + 9), static_cast<int>(tag.y + 6), 14, control.urgent ? Color {255, 209, 182, 255} : Color {231, 220, 184, 255});
    }
}

void Renderer::DrawMenu(const GameState& state) {
    const int w = GetScreenWidth();
    const int h = GetScreenHeight();
    if (TextureReady(assets_->menuBackplate)) {
        DrawTexturePro(assets_->menuBackplate, Rectangle {0, 0, static_cast<float>(assets_->menuBackplate.width), static_cast<float>(assets_->menuBackplate.height)}, Rectangle {0, 0, static_cast<float>(w), static_cast<float>(h)}, Vector2 {0, 0}, 0.0f, Fade(RAYWHITE, 0.74f));
    }
    DrawRectangle(0, 0, w, h, Color {0, 0, 0, 165});
    DrawRectangleGradientV(0, 0, w, h, Color {18, 10, 9, 120}, Color {0, 0, 0, 225});

    const float left = w < 980 ? 54.0f : 86.0f;
    const float top = h < 680 ? 62.0f : 88.0f;
    DrawText("LINEAR DRIVE", static_cast<int>(left), static_cast<int>(top), 58, RAYWHITE);
    DrawText("Keep moving. The engine noise keeps it outside.", static_cast<int>(left + 3), static_cast<int>(top + 70), 20, Color {224, 206, 159, 255});
    DrawText("SLOW BELOW 12 MPH DURING A HUNT AND IT CLOSES.", static_cast<int>(left + 3), static_cast<int>(top + 104), 17, Color {255, 139, 101, 255});

    startButton_ = Rectangle {left + 3.0f, top + 158.0f, 216.0f, 52.0f};
    tutorialButton_ = Rectangle {left + 231.0f, top + 158.0f, 162.0f, 52.0f};
    settingsButton_ = Rectangle {left + 405.0f, top + 158.0f, 164.0f, 52.0f};
    quitButton_ = Rectangle {left + 581.0f, top + 158.0f, 118.0f, 52.0f};
    DrawMenuButton(startButton_, "START DRIVE", Color {149, 45, 30, 245});
    DrawMenuButton(tutorialButton_, "TUTORIAL", Color {32, 45, 43, 238});
    DrawMenuButton(settingsButton_, "SETTINGS", Color {42, 52, 49, 238});
    DrawMenuButton(quitButton_, "QUIT", Color {54, 35, 32, 238});

    DrawPanel(Rectangle {left + 3.0f, top + 250.0f, 430.0f, 138.0f}, Color {8, 11, 11, 220}, UiBorder());
    DrawText("SURVIVAL LOOP", static_cast<int>(left + 24), static_cast<int>(top + 268), 13, Color {151, 139, 105, 255});
    DrawText("Drive hard through each road chapter.", static_cast<int>(left + 24), static_cast<int>(top + 294), 17, Color {226, 214, 177, 255});
    DrawText("Use the cockpit: radio, shifter, locks,", static_cast<int>(left + 24), static_cast<int>(top + 322), 14, Color {173, 194, 184, 255});
    DrawText("ignition, mirror, beams, horn, and wipers.", static_cast<int>(left + 24), static_cast<int>(top + 344), 14, Color {173, 194, 184, 255});
    DrawText((std::string("Known entity: ") + MonsterName(state.creature.variant)).c_str(), static_cast<int>(left + 24), static_cast<int>(top + 368), 14, Color {255, 166, 127, 255});

    const float right = std::max(left + 500.0f, static_cast<float>(w) - 420.0f);
    DrawPanel(Rectangle {right, top + 16.0f, 330.0f, 252.0f}, Color {7, 9, 9, 218}, Color {116, 78, 61, 160});
    DrawText("RUN BRIEF", static_cast<int>(right + 22), static_cast<int>(top + 36), 13, Color {151, 139, 105, 255});
    DrawText("Tower distance", static_cast<int>(right + 22), static_cast<int>(top + 70), 15, Color {202, 189, 152, 255});
    DrawText("4.20 MI", static_cast<int>(right + 216), static_cast<int>(top + 68), 20, RAYWHITE);
    DrawText("Entity pressure", static_cast<int>(right + 22), static_cast<int>(top + 110), 15, Color {202, 189, 152, 255});
    DrawMeter(Rectangle {right + 22.0f, top + 136.0f, 286.0f, 14.0f}, "", 0.76f, Color {178, 46, 35, 230});
    DrawText("Survival verbs", static_cast<int>(right + 22), static_cast<int>(top + 176), 15, Color {202, 189, 152, 255});
    DrawText("Drive  Stabilize  Re-seat  Relock", static_cast<int>(right + 22), static_cast<int>(top + 204), 14, Color {220, 213, 186, 255});
    DrawText("Restart  Look  Listen", static_cast<int>(right + 22), static_cast<int>(top + 226), 14, Color {220, 213, 186, 255});
}

void Renderer::DrawSettings(const GameState& state) {
    const int w = GetScreenWidth();
    const int h = GetScreenHeight();
    if (TextureReady(assets_->menuBackplate)) {
        DrawTexturePro(assets_->menuBackplate, Rectangle {0, 0, static_cast<float>(assets_->menuBackplate.width), static_cast<float>(assets_->menuBackplate.height)}, Rectangle {0, 0, static_cast<float>(w), static_cast<float>(h)}, Vector2 {0, 0}, 0.0f, Fade(RAYWHITE, 0.42f));
    }
    DrawRectangle(0, 0, w, h, Color {0, 0, 0, 218});

    const float panelW = std::min(620.0f, static_cast<float>(w) - 96.0f);
    const float panelH = std::min(520.0f, static_cast<float>(h) - 86.0f);
    const float x = w * 0.5f - panelW * 0.5f;
    const float y = h * 0.5f - panelH * 0.5f;
    DrawPanel(Rectangle {x, y, panelW, panelH}, Color {8, 11, 11, 238}, UiBorder());
    DrawText("SETTINGS", static_cast<int>(x + 30), static_cast<int>(y + 28), 34, RAYWHITE);
    DrawText("Audio applies live. Accessibility remains available in pause and on the road.", static_cast<int>(x + 32), static_cast<int>(y + 72), 15, Color {202, 189, 152, 255});

    const float sliderX = x + 186.0f;
    const float sliderW = panelW - 240.0f;
    const std::array<const char*, 4> sliderLabels {{"Master", "Music", "SFX", "Gamma"}};
    const std::array<float, 4> sliderValues {{
        state.settings.masterVolume,
        state.settings.musicVolume,
        state.settings.sfxVolume,
        (state.settings.gamma - 0.75f) / 0.70f
    }};
    for (int i = 0; i < 4; ++i) {
        settingsSliders_[static_cast<size_t>(i)] = Rectangle {sliderX, y + 130.0f + i * 54.0f, sliderW, 16.0f};
        DrawText(sliderLabels[static_cast<size_t>(i)], static_cast<int>(x + 32), static_cast<int>(settingsSliders_[static_cast<size_t>(i)].y - 5), 17, Color {226, 214, 177, 255});
        DrawSlider(settingsSliders_[static_cast<size_t>(i)], "", sliderValues[static_cast<size_t>(i)], i == 3 ? Color {175, 153, 91, 240} : Color {127, 201, 188, 238});
    }

    const std::array<const char*, 3> toggleLabels {{"Subtitles", "Reduced motion", "Reduced flashing"}};
    const std::array<bool, 3> toggleValues {{state.settings.subtitles, state.settings.reducedMotion, state.settings.reducedFlashing}};
    for (int i = 0; i < 3; ++i) {
        settingsToggles_[static_cast<size_t>(i)] = Rectangle {x + 32.0f, y + 370.0f + i * 36.0f, 28.0f, 24.0f};
        DrawRectangleRounded(settingsToggles_[static_cast<size_t>(i)], 0.2f, 8, toggleValues[static_cast<size_t>(i)] ? Color {109, 166, 141, 238} : Color {25, 30, 29, 238});
        DrawRectangleRoundedLines(settingsToggles_[static_cast<size_t>(i)], 0.2f, 8, UiBorder());
        if (toggleValues[static_cast<size_t>(i)]) {
            DrawText("ON", static_cast<int>(settingsToggles_[static_cast<size_t>(i)].x + 3), static_cast<int>(settingsToggles_[static_cast<size_t>(i)].y + 6), 11, Color {7, 10, 9, 255});
        }
        DrawText(toggleLabels[static_cast<size_t>(i)], static_cast<int>(x + 72), static_cast<int>(settingsToggles_[static_cast<size_t>(i)].y + 3), 17, Color {226, 214, 177, 255});
    }

    backButton_ = Rectangle {x + panelW - 178.0f, y + panelH - 72.0f, 146.0f, 46.0f};
    DrawMenuButton(backButton_, "BACK", Color {42, 52, 49, 238});
}

void Renderer::DrawTutorial(const GameState&) {
    const int w = GetScreenWidth();
    const int h = GetScreenHeight();
    if (TextureReady(assets_->tutorialBackplate)) {
        DrawTexturePro(assets_->tutorialBackplate, Rectangle {0, 0, static_cast<float>(assets_->tutorialBackplate.width), static_cast<float>(assets_->tutorialBackplate.height)}, Rectangle {0, 0, static_cast<float>(w), static_cast<float>(h)}, Vector2 {0, 0}, 0.0f, Fade(RAYWHITE, 0.58f));
    }
    DrawRectangle(0, 0, w, h, Color {0, 0, 0, 210});
    DrawRectangleGradientV(0, 0, w, h, Color {5, 12, 12, 180}, Color {0, 0, 0, 245});

    const int titleX = w / 2 - MeasureText("SURVIVE THE LINE", 44) / 2;
    DrawText("SURVIVE THE LINE", titleX, 58, 44, RAYWHITE);
    DrawText("Survive by reading cockpit cues, then using the right control under pressure.", w / 2 - MeasureText("Survive by reading cockpit cues, then using the right control under pressure.", 18) / 2, 112, 18, Color {224, 206, 159, 255});

    const float panelW = std::min(320.0f, (w - 120.0f) / 3.0f);
    const float gap = 22.0f;
    const float startX = w / 2.0f - (panelW * 3.0f + gap * 2.0f) * 0.5f;
    const float y = 166.0f;
    const std::array<const char*, 3> heads {"DRIVE", "STABILIZE", "ESCAPE"};
    const std::array<std::array<const char*, 4>, 3> lines {{
        {{"W accelerates", "S brakes or reverses", "A/D keep centered", "Do not coast during a hunt"}},
        {{"Click radio to stop pull", "Click shifter to re-seat", "Click lock during attacks", "Mirror exposes fake cues"}},
        {{"R or ignition restarts", "Space or handbrake tugs", "H or horn buys space", "Use speed before risk fills"}}
    }};

    for (int i = 0; i < 3; ++i) {
        const float x = startX + static_cast<float>(i) * (panelW + gap);
        DrawPanel(Rectangle {x, y, panelW, 236.0f}, Color {8, 11, 11, 224}, i == 1 ? Color {184, 67, 49, 175} : UiBorder());
        DrawText(heads[static_cast<size_t>(i)], static_cast<int>(x + 22), static_cast<int>(y + 24), 22, RAYWHITE);
        for (int line = 0; line < 4; ++line) {
            DrawText(lines[static_cast<size_t>(i)][static_cast<size_t>(line)], static_cast<int>(x + 24), static_cast<int>(y + 76 + line * 34), 16, Color {214, 203, 170, 255});
        }
    }

    DrawPanel(Rectangle {w / 2.0f - 360.0f, h - 182.0f, 720.0f, 72.0f}, Color {20, 10, 9, 224}, Color {196, 69, 45, 180});
    DrawText("Loose radio pulls the wheel. Shifter slip blocks speed. Bad locks invite door attacks.", w / 2 - MeasureText("Loose radio pulls the wheel. Shifter slip blocks speed. Bad locks invite door attacks.", 16) / 2, h - 166, 16, Color {255, 216, 188, 255});
    DrawText("Survive by stabilizing, re-seating, relocking, restarting, looking, and listening.", w / 2 - MeasureText("Survive by stabilizing, re-seating, relocking, restarting, looking, and listening.", 16) / 2, h - 142, 16, Color {255, 216, 188, 255});

    startButton_ = Rectangle {w / 2.0f - 238.0f, h - 86.0f, 220.0f, 50.0f};
    backButton_ = Rectangle {w / 2.0f + 18.0f, h - 86.0f, 220.0f, 50.0f};
    DrawRectangleRounded(startButton_, 0.1f, 8, Color {149, 45, 30, 245});
    DrawRectangleRoundedLines(startButton_, 0.1f, 8, UiBorder());
    DrawText("START DRIVE", static_cast<int>(startButton_.x + 40), static_cast<int>(startButton_.y + 16), 20, RAYWHITE);
    DrawRectangleRounded(backButton_, 0.1f, 8, Color {31, 42, 40, 238});
    DrawRectangleRoundedLines(backButton_, 0.1f, 8, UiBorder());
    DrawText("BACK TO MENU", static_cast<int>(backButton_.x + 36), static_cast<int>(backButton_.y + 16), 20, RAYWHITE);
}

void Renderer::DrawPause(const GameState& state) {
    const int w = GetScreenWidth();
    const int h = GetScreenHeight();
    DrawRectangle(0, 0, w, h, Color {0, 0, 0, 160});
    DrawPanel(Rectangle {w / 2.0f - 250.0f, h / 2.0f - 180.0f, 500.0f, 310.0f}, Color {8, 10, 10, 235}, UiBorder());
    DrawText("PAUSED", w / 2 - MeasureText("PAUSED", 34) / 2, h / 2 - 150, 34, RAYWHITE);
    DrawText((std::string("Chapter: ") + Simulation::ChapterName(state.chapter)).c_str(), w / 2 - 200, h / 2 - 100, 18, Color {213, 196, 153, 255});
    DrawText((std::string("Road: ") + Simulation::SegmentName(state.currentSegment)).c_str(), w / 2 - 200, h / 2 - 72, 18, Color {213, 196, 153, 255});
    DrawText("F1 subtitles  F2 reduced motion  F3 reduced flashing  +/- gamma", w / 2 - 200, h / 2 - 32, 16, Color {171, 165, 139, 255});
    DrawText((std::string("Subtitles: ") + (state.settings.subtitles ? "on" : "off")).c_str(), w / 2 - 200, h / 2 + 4, 16, Color {171, 165, 139, 255});
    DrawText((std::string("Reduced motion: ") + (state.settings.reducedMotion ? "on" : "off")).c_str(), w / 2 - 200, h / 2 + 28, 16, Color {171, 165, 139, 255});
    resumeButton_ = Rectangle {w / 2.0f - 170.0f, h / 2.0f + 74.0f, 150.0f, 42.0f};
    restartButton_ = Rectangle {w / 2.0f + 20.0f, h / 2.0f + 74.0f, 150.0f, 42.0f};
    DrawRectangleRounded(resumeButton_, 0.12f, 8, Color {72, 95, 80, 235});
    DrawRectangleRounded(restartButton_, 0.12f, 8, Color {95, 56, 45, 235});
    DrawText("RESUME", w / 2 - 132, h / 2 + 86, 18, RAYWHITE);
    DrawText("RESTART", w / 2 + 53, h / 2 + 86, 18, RAYWHITE);
}

void Renderer::DrawDeath(const GameState& state) {
    const int w = GetScreenWidth();
    const int h = GetScreenHeight();
    DrawRectangle(0, 0, w, h, Color {0, 0, 0, 215});
    DrawText("SIGNAL LOST", w / 2 - MeasureText("SIGNAL LOST", 48) / 2, h / 2 - 92, 48, Color {255, 117, 89, 255});
    DrawText(state.story.warning.c_str(), w / 2 - MeasureText(state.story.warning.c_str(), 20) / 2, h / 2 - 34, 20, Color {226, 210, 174, 255});
    restartButton_ = Rectangle {w / 2.0f - 228.0f, h / 2.0f + 32.0f, 218.0f, 48.0f};
    backButton_ = Rectangle {w / 2.0f + 10.0f, h / 2.0f + 32.0f, 218.0f, 48.0f};
    DrawMenuButton(restartButton_, "RESTART DRIVE", Color {116, 42, 32, 240});
    DrawMenuButton(backButton_, "MAIN MENU", Color {42, 52, 49, 238});
}

void Renderer::DrawEnding(const GameState& state) {
    const int w = GetScreenWidth();
    const int h = GetScreenHeight();
    DrawRectangle(0, 0, w, h, Color {0, 0, 0, 150});
    DrawPanel(Rectangle {w / 2.0f - 310.0f, h / 2.0f - 190.0f, 620.0f, 355.0f}, Color {7, 10, 10, 235}, UiBorder());

    if (!state.story.ending.empty()) {
        DrawText(state.story.ending.c_str(), w / 2 - MeasureText(state.story.ending.c_str(), 36) / 2, h / 2 - 145, 36, RAYWHITE);
        DrawText(state.story.endingDetail.c_str(), w / 2 - MeasureText(state.story.endingDetail.c_str(), 18) / 2, h / 2 - 82, 18, Color {219, 204, 166, 255});
        restartButton_ = Rectangle {w / 2.0f - 108.0f, h / 2.0f + 60.0f, 216.0f, 48.0f};
        DrawRectangleRounded(restartButton_, 0.12f, 8, Color {111, 48, 36, 240});
        DrawText("DRIVE AGAIN", w / 2 - MeasureText("DRIVE AGAIN", 20) / 2, h / 2 + 74, 20, RAYWHITE);
        return;
    }

    DrawText("THE TOWER", w / 2 - MeasureText("THE TOWER", 42) / 2, h / 2 - 152, 42, RAYWHITE);
    DrawText("The signal is live. The engine is still ticking.", w / 2 - MeasureText("The signal is live. The engine is still ticking.", 18) / 2, h / 2 - 100, 18, Color {219, 204, 166, 255});

    const std::array<const char*, 3> labels {"Broadcast warning", "Leave the car", "Keep driving"};
    for (int i = 0; i < 3; ++i) {
        endingButtons_[static_cast<size_t>(i)] = Rectangle {w / 2.0f - 210.0f, h / 2.0f - 48.0f + i * 58.0f, 420.0f, 44.0f};
        DrawRectangleRounded(endingButtons_[static_cast<size_t>(i)], 0.1f, 8, Color {38, 48, 45, 236});
        DrawRectangleRoundedLines(endingButtons_[static_cast<size_t>(i)], 0.1f, 8, UiBorder());
        DrawText(labels[static_cast<size_t>(i)], w / 2 - MeasureText(labels[static_cast<size_t>(i)], 19) / 2, static_cast<int>(h / 2 - 36 + i * 58), 19, RAYWHITE);
    }
}

void Renderer::DrawJumpscare(const GameState& state) {
    const int w = GetScreenWidth();
    const int h = GetScreenHeight();
    const float t = Clamp(state.jumpscareTimer / std::max(0.01f, state.jumpscareDuration), 0.0f, 1.0f);
    const float bite = Smooth01(t);
    const float shake = state.settings.reducedMotion ? 0.0f : std::sin(state.elapsed * 71.0f) * (8.0f + bite * 22.0f);
    const float cx = w * 0.5f + shake;
    const float cy = h * (0.45f + 0.04f * std::sin(state.elapsed * 39.0f));

    DrawRectangle(0, 0, w, h, Fade(Color {24, 0, 0, 255}, 0.18f + bite * 0.28f));
    DrawCircleGradient(static_cast<int>(cx - 90.0f - bite * 42.0f), static_cast<int>(cy - 84.0f), 52.0f + bite * 44.0f, Fade(PaleEye(), 0.86f), Fade(Color {192, 18, 12, 255}, 0.0f));
    DrawCircleGradient(static_cast<int>(cx + 90.0f + bite * 42.0f), static_cast<int>(cy - 84.0f), 52.0f + bite * 44.0f, Fade(PaleEye(), 0.86f), Fade(Color {192, 18, 12, 255}, 0.0f));
    DrawCircle(static_cast<int>(cx - 90.0f - bite * 42.0f), static_cast<int>(cy - 84.0f), 12.0f + bite * 12.0f, Color {18, 12, 8, 235});
    DrawCircle(static_cast<int>(cx + 90.0f + bite * 42.0f), static_cast<int>(cy - 84.0f), 12.0f + bite * 12.0f, Color {18, 12, 8, 235});

    const float topY = h * (0.05f + bite * 0.27f);
    const float bottomY = h * (0.95f - bite * 0.28f);
    const int toothCount = 13;
    for (int i = 0; i < toothCount; ++i) {
        const float x0 = static_cast<float>(i) * w / toothCount;
        const float x1 = static_cast<float>(i + 1) * w / toothCount;
        const float mid = (x0 + x1) * 0.5f + std::sin(state.elapsed * 22.0f + i) * 12.0f;
        const float topTip = topY + 104.0f + bite * 102.0f + (i % 2) * 28.0f;
        const float bottomTip = bottomY - 104.0f - bite * 102.0f - (i % 2) * 28.0f;
        DrawTriangle(Vector2 {x0, topY}, Vector2 {x1, topY}, Vector2 {mid, topTip}, Color {222, 214, 188, 235});
        DrawTriangle(Vector2 {x1, bottomY}, Vector2 {x0, bottomY}, Vector2 {mid, bottomTip}, Color {222, 214, 188, 235});
    }

    DrawRectangle(0, 0, w, static_cast<int>(topY), Color {0, 0, 0, 235});
    DrawRectangle(0, static_cast<int>(bottomY), w, h - static_cast<int>(bottomY), Color {0, 0, 0, 235});
    DrawRectangleGradientV(0, static_cast<int>(topY), w, static_cast<int>(std::max(1.0f, bottomY - topY)), Fade(Color {0, 0, 0, 0}, 0.0f), Fade(Color {0, 0, 0, 255}, 0.38f + bite * 0.32f));

    for (int slash = 0; slash < 6; ++slash) {
        const float sx = w * (0.16f + slash * 0.14f) + std::sin(state.elapsed * 18.0f + slash) * 40.0f;
        const float sy = h * (0.18f + (slash % 3) * 0.18f);
        DrawLineEx(Vector2 {sx, sy}, Vector2 {sx + 90.0f + bite * 80.0f, sy + 230.0f}, 3.0f + bite * 5.0f, Fade(Color {255, 82, 49, 255}, 0.28f + bite * 0.42f));
    }

    if (t > 0.72f) {
        DrawRectangle(0, 0, w, h, Fade(Color {0, 0, 0, 255}, (t - 0.72f) / 0.28f));
    }
}

void Renderer::DrawStormEffects(const GameState& state) {
    const int w = GetScreenWidth();
    const int h = GetScreenHeight();
    if (state.world.stormIntensity <= 0.02f && state.world.lightning <= 0.01f && state.story.monsterSubtitleTimer <= 0.0f) {
        return;
    }

    if (state.world.stormIntensity > 0.02f) {
        const int drops = static_cast<int>(90 + state.world.stormIntensity * 180.0f);
        for (int i = 0; i < drops; ++i) {
            const unsigned hash = Hash(static_cast<unsigned>(i) * 1009u + static_cast<unsigned>(state.elapsed * 60.0f) * 9176u);
            const float x = static_cast<float>((hash >> 5u) % static_cast<unsigned>(std::max(1, w)));
            const float y = static_cast<float>((hash >> 17u) % static_cast<unsigned>(std::max(1, h)));
            const float len = 12.0f + static_cast<float>((hash >> 10u) % 24u);
            DrawLineEx(Vector2 {x, y}, Vector2 {x - len * 0.35f, y + len}, 1.0f, Fade(Color {139, 177, 190, 255}, 0.22f + state.world.stormIntensity * 0.22f));
        }
        DrawRectangle(0, 0, w, h, Fade(Color {7, 18, 22, 255}, state.world.stormIntensity * 0.16f));
    }

    if (state.world.lightning > 0.01f && !state.settings.reducedFlashing) {
        DrawRectangle(0, 0, w, h, Fade(Color {198, 222, 230, 255}, state.world.lightning * 0.36f));
    }

    if (state.story.monsterSubtitleTimer > 0.0f && state.settings.subtitles) {
        const char* text = state.story.monsterSubtitle.c_str();
        const int fontSize = 20;
        const int tw = MeasureText(text, fontSize);
        const float y = h * 0.30f + std::sin(state.elapsed * 24.0f) * 2.0f;
        DrawPanel(Rectangle {w / 2.0f - tw / 2.0f - 24.0f, y - 12.0f, static_cast<float>(tw + 48), 48.0f}, Color {8, 3, 3, 225}, Color {190, 56, 47, 180});
        DrawText(text, w / 2 - tw / 2, static_cast<int>(y), fontSize, Color {255, 199, 181, 255});
    }
}

void Renderer::DrawFilmEffects(const GameState& state) {
    const int w = GetScreenWidth();
    const int h = GetScreenHeight();
    const float darkness = Clamp(0.08f + state.roadFog * 0.32f - (state.settings.gamma - 1.0f) * 0.18f, 0.0f, 0.48f);
    DrawRectangle(0, 0, w, h, Fade(Color {0, 0, 0, 255}, darkness));

    if (!state.settings.reducedFlashing && (state.car.headlightPower < 28.0f || state.car.fan == FanState::Irregular)) {
        const float pulse = (std::sin(state.elapsed * 18.0f) + 1.0f) * 0.5f;
        DrawRectangle(0, 0, w, h, Fade(Color {0, 0, 0, 255}, pulse * 0.08f));
    }
}

void Renderer::DrawPanel(Rectangle rect, Color color, Color border) const {
    DrawRectangleRounded(rect, 0.08f, 8, color);
    DrawRectangleRoundedLines(rect, 0.08f, 8, border);
}

void Renderer::DrawMenuButton(Rectangle rect, const char* label, Color fill) const {
    DrawRectangleRounded(rect, 0.1f, 8, fill);
    DrawRectangleRoundedLines(rect, 0.1f, 8, UiBorder());
    const int fontSize = rect.width < 130.0f ? 18 : 20;
    DrawText(label, static_cast<int>(rect.x + rect.width * 0.5f - MeasureText(label, fontSize) * 0.5f), static_cast<int>(rect.y + rect.height * 0.5f - fontSize * 0.45f), fontSize, RAYWHITE);
}

void Renderer::DrawSlider(Rectangle rect, const char* label, float value, Color fill) const {
    value = Clamp(value, 0.0f, 1.0f);
    if (label && label[0] != '\0') {
        DrawText(label, static_cast<int>(rect.x), static_cast<int>(rect.y - 22.0f), 14, Color {218, 207, 174, 255});
    }
    DrawRectangleRounded(rect, 0.5f, 12, Color {24, 28, 27, 245});
    DrawRectangleRounded(Rectangle {rect.x, rect.y, rect.width * value, rect.height}, 0.5f, 12, fill);
    DrawRectangleRoundedLines(rect, 0.5f, 12, Color {118, 103, 78, 170});
    DrawCircle(static_cast<int>(rect.x + rect.width * value), static_cast<int>(rect.y + rect.height * 0.5f), 10.0f, Color {236, 225, 192, 255});
}

void Renderer::DrawModelIfReady(const Model& model, Vector3 position, Vector3 axis, float angle, Vector3 scale, Color tint) const {
    if (ModelReady(model)) {
        DrawModelEx(model, position, axis, angle, scale, tint);
    }
}

const char* Renderer::HotspotLabel(Hotspot hotspot) {
    switch (hotspot) {
        case Hotspot::Radio: return "Radio";
        case Hotspot::Fan: return "Fan";
        case Hotspot::DoorLock: return "Lock";
        case Hotspot::DoorHandle: return "Handle";
        case Hotspot::Glovebox: return "Glovebox";
        case Hotspot::Mirror: return "Mirror";
        case Hotspot::Headlights: return "Lights";
        case Hotspot::Ignition: return "Ignition";
        case Hotspot::Horn: return "Horn";
        case Hotspot::Wipers: return "Wipers";
        case Hotspot::WindowCrank: return "Window";
        case Hotspot::DomeLight: return "Dome";
        case Hotspot::GearShift: return "Shifter";
        case Hotspot::Handbrake: return "Handbrake";
    }
    return "Control";
}

unsigned Renderer::Hash(unsigned value) {
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value;
}

} // namespace ld
