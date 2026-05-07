#pragma once

#include "engine/Assets.hpp"
#include "game/Input.hpp"
#include "game/Simulation.hpp"

#include <optional>
#include <string>
#include <vector>
#include <array>

namespace ld {

struct HudMessage {
    EventType type = EventType::Notice;
    std::string text;
    float timer = 0.0f;
    float intensity = 0.0f;
};

class Renderer {
public:
    explicit Renderer(Assets* assets);

    void Render(const GameState& state, const InputState& input, const std::vector<HudMessage>& messages);
    std::optional<Hotspot> HitTestHotspot(Vector2 mouse) const;
    std::optional<Hotspot> HitTestCockpitControl(Vector2 mouse) const;
    bool HitStart(Vector2 mouse) const;
    bool HitTutorial(Vector2 mouse) const;
    bool HitSettings(Vector2 mouse) const;
    bool HitQuit(Vector2 mouse) const;
    bool HitBack(Vector2 mouse) const;
    bool HitResume(Vector2 mouse) const;
    bool HitRestart(Vector2 mouse) const;
    std::optional<int> HitSettingsSlider(Vector2 mouse) const;
    std::optional<int> HitSettingsToggle(Vector2 mouse) const;
    std::optional<int> HitEndingChoice(Vector2 mouse) const;
    float SliderValueFromMouse(int slider, Vector2 mouse) const;

private:
    struct CockpitControl {
        Rectangle rect {};
        Hotspot hotspot = Hotspot::Radio;
        std::string label;
        Vector3 world {};
        bool urgent = false;
    };

    Assets* assets_ = nullptr;
    Camera3D camera_ {};
    std::vector<CockpitControl> cockpitControls_;
    Rectangle startButton_ {};
    Rectangle tutorialButton_ {};
    Rectangle settingsButton_ {};
    Rectangle quitButton_ {};
    Rectangle backButton_ {};
    Rectangle resumeButton_ {};
    Rectangle restartButton_ {};
    std::array<Rectangle, 4> settingsSliders_ {};
    std::array<Rectangle, 3> settingsToggles_ {};
    std::array<Rectangle, 3> endingButtons_ {};

    void SetupCamera(const GameState& state, const InputState& input);
    void BuildCockpitControls(const GameState& state);
    void DrawWorld(const GameState& state);
    void DrawRoad(const GameState& state);
    void DrawForest(const GameState& state);
    void DrawRoadsideDressing(const GameState& state);
    void DrawWorldProp(const GameState& state, const WorldProp& prop);
    void DrawHazards(const GameState& state);
    void DrawCreature(const GameState& state);
    void DrawCreatureAppendages(Vector3 position, float scale, float alpha, float phase, bool close) const;
    void DrawCreatureEyes(Vector3 position, float width, float size, float alpha) const;
    void DrawCockpit3D(const GameState& state);
    void DrawHud(const GameState& state, const std::vector<HudMessage>& messages);
    void DrawCockpitControlHighlights(const GameState& state, Vector2 mouse);
    void DrawMenu(const GameState& state);
    void DrawSettings(const GameState& state);
    void DrawTutorial(const GameState& state);
    void DrawPause(const GameState& state);
    void DrawDeath(const GameState& state);
    void DrawEnding(const GameState& state);
    void DrawJumpscare(const GameState& state);
    void DrawStormEffects(const GameState& state);
    void DrawFilmEffects(const GameState& state);
    void DrawPanel(Rectangle rect, Color color, Color border) const;
    void DrawMenuButton(Rectangle rect, const char* label, Color fill) const;
    void DrawSlider(Rectangle rect, const char* label, float value, Color fill) const;
    void DrawModelIfReady(const Model& model, Vector3 position, Vector3 axis, float angle, Vector3 scale, Color tint) const;
    static const char* HotspotLabel(Hotspot hotspot);
    static unsigned Hash(unsigned value);
};

} // namespace ld
