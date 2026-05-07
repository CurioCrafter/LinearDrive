#pragma once

#include "raylib.h"

namespace ld {

struct InputState {
    bool accelerate = false;
    bool brake = false;
    bool steerLeft = false;
    bool steerRight = false;
    bool highBeams = false;
    bool handbrake = false;
    bool restartEngine = false;
    bool horn = false;
    bool toggleRadio = false;
    bool toggleHeadlights = false;
    bool toggleLock = false;
    bool toggleWipers = false;
    bool domeLight = false;
    bool pausePressed = false;
    bool confirmPressed = false;
    bool backPressed = false;
    bool mousePressed = false;
    Vector2 mouse = {0, 0};
    float lookYaw = 0.0f;
    float lookPitch = 0.0f;
};

} // namespace ld
