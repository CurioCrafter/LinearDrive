#pragma once

#include "game/Simulation.hpp"

#include "raylib.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace ld {

enum class CockpitCameraMode {
    Driver,
    ExteriorFront,
    ExteriorLeft,
    TopDown,
    ConsoleCloseup
};

struct CockpitAnchor {
    Hotspot hotspot = Hotspot::Radio;
    const char* name = "";
    Vector3 local {};
    Vector2 hitSize {};
};

struct CockpitRig {
    Vector3 modelPosition {0.0f, -0.62f, 2.86f};
    // The Toyota asset is authored with its nose on raw +Z. Rotate it into the
    // game convention (-Z road/front) without mirroring the left-hand-drive cabin.
    Vector3 modelScale {3.15f, 3.15f, 3.15f};
    float modelYawDegrees = 180.0f;
    float carYawDegrees = 0.0f;
    Vector3 modelBoundsMin {-0.461823f, 0.0f, -0.950000f};
    Vector3 modelBoundsMax {0.461823f, 0.860898f, 0.950000f};
    Vector3 driverEye {-0.55f, 1.34f, 3.28f};
    Vector3 driverTarget {-0.30f, 1.12f, -7.6f};
    float driverFov = 66.0f;
};

inline const CockpitRig& ToyotaCockpitRig() {
    static const CockpitRig rig {};
    return rig;
}

inline const std::array<CockpitAnchor, 13>& CockpitAnchors() {
    static const std::array<CockpitAnchor, 13> anchors {{
        {Hotspot::Horn, "Horn", {-0.54f, 0.79f, 2.42f}, {74.0f, 58.0f}},
        {Hotspot::Radio, "Radio", {0.28f, 1.02f, 2.08f}, {100.0f, 60.0f}},
        {Hotspot::Ignition, "Ignition", {-0.18f, 0.77f, 2.18f}, {58.0f, 48.0f}},
        {Hotspot::GearShift, "Shifter", {0.00f, 0.78f, 2.35f}, {78.0f, 88.0f}},
        {Hotspot::DoorLock, "Lock", {-1.14f, 1.13f, 2.18f}, {62.0f, 48.0f}},
        {Hotspot::DoorHandle, "Handle", {-1.17f, 0.88f, 2.24f}, {80.0f, 50.0f}},
        {Hotspot::Mirror, "Mirror", {-0.04f, 1.80f, 1.92f}, {118.0f, 50.0f}},
        {Hotspot::Headlights, "Lights", {-0.82f, 0.93f, 2.16f}, {62.0f, 46.0f}},
        {Hotspot::Wipers, "Wipers", {-0.36f, 0.88f, 2.18f}, {62.0f, 46.0f}},
        {Hotspot::Fan, "Fan", {0.72f, 1.05f, 2.10f}, {64.0f, 56.0f}},
        {Hotspot::DomeLight, "Dome", {0.0f, 1.92f, 2.08f}, {96.0f, 46.0f}},
        {Hotspot::Glovebox, "Glovebox", {0.72f, 0.72f, 2.14f}, {96.0f, 50.0f}},
        {Hotspot::WindowCrank, "Window", {-1.22f, 1.00f, 2.34f}, {62.0f, 60.0f}}
    }};
    return anchors;
}

inline const CockpitAnchor& CockpitAnchorFor(Hotspot hotspot) {
    for (const CockpitAnchor& anchor : CockpitAnchors()) {
        if (anchor.hotspot == hotspot) {
            return anchor;
        }
    }
    return CockpitAnchors().front();
}

inline Vector3 Add3(Vector3 a, Vector3 b) {
    return Vector3 {a.x + b.x, a.y + b.y, a.z + b.z};
}

inline Vector3 Sub3(Vector3 a, Vector3 b) {
    return Vector3 {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Vector3 Scale3(Vector3 v, float scale) {
    return Vector3 {v.x * scale, v.y * scale, v.z * scale};
}

inline float Dot3(Vector3 a, Vector3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vector3 Cross3(Vector3 a, Vector3 b) {
    return Vector3 {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

inline Vector3 Normalize3(Vector3 v) {
    const float length = std::sqrt(std::max(0.000001f, Dot3(v, v)));
    return Vector3 {v.x / length, v.y / length, v.z / length};
}

inline Vector3 RotateY(Vector3 point, float degrees) {
    const float radians = degrees * DEG2RAD;
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return Vector3 {
        point.x * c + point.z * s,
        point.y,
        -point.x * s + point.z * c
    };
}

inline Vector3 CarToWorld(const GameState& state, Vector3 local, const CockpitRig& rig = ToyotaCockpitRig()) {
    const Vector3 rotated = RotateY(local, rig.carYawDegrees);
    return Vector3 {state.car.lateralOffset + rotated.x, rotated.y, rotated.z};
}

inline BoundingBox ToyotaModelBoundsWorld(const GameState& state, const CockpitRig& rig = ToyotaCockpitRig()) {
    BoundingBox box {};
    box.min = Vector3 {99999.0f, 99999.0f, 99999.0f};
    box.max = Vector3 {-99999.0f, -99999.0f, -99999.0f};

    const std::array<Vector3, 8> corners {{
        {rig.modelBoundsMin.x, rig.modelBoundsMin.y, rig.modelBoundsMin.z},
        {rig.modelBoundsMin.x, rig.modelBoundsMin.y, rig.modelBoundsMax.z},
        {rig.modelBoundsMin.x, rig.modelBoundsMax.y, rig.modelBoundsMin.z},
        {rig.modelBoundsMin.x, rig.modelBoundsMax.y, rig.modelBoundsMax.z},
        {rig.modelBoundsMax.x, rig.modelBoundsMin.y, rig.modelBoundsMin.z},
        {rig.modelBoundsMax.x, rig.modelBoundsMin.y, rig.modelBoundsMax.z},
        {rig.modelBoundsMax.x, rig.modelBoundsMax.y, rig.modelBoundsMin.z},
        {rig.modelBoundsMax.x, rig.modelBoundsMax.y, rig.modelBoundsMax.z}
    }};

    for (Vector3 corner : corners) {
        corner = Vector3 {
            corner.x * rig.modelScale.x,
            corner.y * rig.modelScale.y,
            corner.z * rig.modelScale.z
        };
        corner = RotateY(corner, rig.modelYawDegrees);
        corner = Add3(rig.modelPosition, corner);
        const Vector3 world = CarToWorld(state, corner, rig);
        box.min.x = std::min(box.min.x, world.x);
        box.min.y = std::min(box.min.y, world.y);
        box.min.z = std::min(box.min.z, world.z);
        box.max.x = std::max(box.max.x, world.x);
        box.max.y = std::max(box.max.y, world.y);
        box.max.z = std::max(box.max.z, world.z);
    }

    return box;
}

inline bool PointInBox(Vector3 point, BoundingBox box) {
    return point.x >= box.min.x && point.x <= box.max.x &&
           point.y >= box.min.y && point.y <= box.max.y &&
           point.z >= box.min.z && point.z <= box.max.z;
}

inline Camera3D CockpitCamera(const GameState& state, CockpitCameraMode mode, const CockpitRig& rig = ToyotaCockpitRig()) {
    Camera3D camera {};
    camera.projection = CAMERA_PERSPECTIVE;
    camera.up = Vector3 {0.0f, 1.0f, 0.0f};

    switch (mode) {
        case CockpitCameraMode::ExteriorFront:
            camera.position = CarToWorld(state, Vector3 {0.0f, 1.35f, -5.6f}, rig);
            camera.target = CarToWorld(state, Vector3 {0.0f, 0.92f, 2.45f}, rig);
            camera.fovy = 44.0f;
            break;
        case CockpitCameraMode::ExteriorLeft:
            camera.position = CarToWorld(state, Vector3 {-5.2f, 1.35f, 2.55f}, rig);
            camera.target = CarToWorld(state, Vector3 {-0.25f, 0.95f, 2.45f}, rig);
            camera.fovy = 45.0f;
            break;
        case CockpitCameraMode::TopDown:
            camera.position = CarToWorld(state, Vector3 {0.0f, 8.3f, 2.65f}, rig);
            camera.target = CarToWorld(state, Vector3 {0.0f, 0.25f, 2.65f}, rig);
            camera.up = Vector3 {0.0f, 0.0f, -1.0f};
            camera.fovy = 38.0f;
            break;
        case CockpitCameraMode::ConsoleCloseup:
            camera.position = CarToWorld(state, Vector3 {-0.32f, 1.12f, 3.08f}, rig);
            camera.target = CarToWorld(state, Vector3 {0.28f, 0.83f, 2.18f}, rig);
            camera.fovy = 42.0f;
            break;
        case CockpitCameraMode::Driver:
        default:
            camera.position = CarToWorld(state, rig.driverEye, rig);
            camera.target = CarToWorld(state, rig.driverTarget, rig);
            camera.fovy = rig.driverFov;
            break;
    }

    return camera;
}

inline bool ProjectWorldToScreen(Vector3 point, const Camera3D& camera, int screenWidth, int screenHeight, Vector2& out) {
    const Vector3 forward = Normalize3(Sub3(camera.target, camera.position));
    const Vector3 right = Normalize3(Cross3(forward, camera.up));
    const Vector3 up = Normalize3(Cross3(right, forward));
    const Vector3 delta = Sub3(point, camera.position);
    const float z = Dot3(delta, forward);
    if (z <= 0.001f) {
        return false;
    }
    const float f = (static_cast<float>(screenHeight) * 0.5f) / std::tan(camera.fovy * DEG2RAD * 0.5f);
    out = Vector2 {
        static_cast<float>(screenWidth) * 0.5f + Dot3(delta, right) * f / z,
        static_cast<float>(screenHeight) * 0.5f - Dot3(delta, up) * f / z
    };
    return true;
}

} // namespace ld
