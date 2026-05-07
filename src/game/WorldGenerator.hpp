#pragma once

#include <optional>
#include <vector>

namespace ld {

enum class WorldZone {
    Forest,
    Storm,
    Town,
    Service,
    Industrial
};

enum class WorldPropKind {
    Tree,
    Building,
    TrafficLight,
    StreetLamp,
    GasPump,
    Billboard,
    UtilityPole,
    IndustrialTank,
    RoadBarrier,
    WindowWatcher
};

struct WorldContext {
    WorldZone zone = WorldZone::Forest;
    float stormIntensity = 0.0f;
    float wetness = 0.0f;
    float townDensity = 0.0f;
    float lightning = 0.0f;
    bool monsterVoiceWindow = false;
};

struct WorldProp {
    WorldPropKind kind = WorldPropKind::Tree;
    float mile = 0.0f;
    float lateral = 0.0f;
    float width = 1.0f;
    float depth = 1.0f;
    float height = 1.0f;
    unsigned seed = 1;
    bool collidable = false;
};

class WorldGenerator {
public:
    static WorldContext ContextAt(float distanceMiles, float elapsedSeconds);
    static std::vector<WorldProp> PropsAround(float distanceMiles, float behindMiles, float aheadMiles, float roadWidth);
    static std::optional<WorldProp> FindTreeImpact(float distanceMiles, float lateralOffset, float roadWidth);
    static std::optional<WorldProp> FindRoadObstacle(float distanceMiles, float lateralOffset);
    static const char* ZoneName(WorldZone zone);
    static unsigned Hash(unsigned value);
};

} // namespace ld
