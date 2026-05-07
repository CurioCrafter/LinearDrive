#include "game/WorldGenerator.hpp"

#include "game/Simulation.hpp"

#include <algorithm>
#include <cmath>

namespace ld {
namespace {

constexpr float kChunkMiles = 0.0325f;

float SmoothBand(float value, float start, float end) {
    if (value <= start) return 0.0f;
    if (value >= end) return 1.0f;
    const float t = (value - start) / (end - start);
    return t * t * (3.0f - 2.0f * t);
}

float Hash01(unsigned value) {
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return static_cast<float>(value & 0x00ffffffu) / static_cast<float>(0x00ffffffu);
}

void PushTree(std::vector<WorldProp>& props, int chunk, int side, float roadWidth) {
    const unsigned h = WorldGenerator::Hash(static_cast<unsigned>(chunk) * 92821u + static_cast<unsigned>(side + 3) * 311u);
    const float mile = (static_cast<float>(chunk) + 0.2f + Hash01(h) * 0.65f) * kChunkMiles;
    const float edge = roadWidth * 0.5f;
    const float tight = Hash01(h >> 4u) < 0.32f ? 0.55f : 1.45f;
    const float lateral = static_cast<float>(side) * (edge + tight + Hash01(h >> 8u) * 3.8f);
    const float width = 0.7f + Hash01(h >> 12u) * 0.55f;
    props.push_back(WorldProp {WorldPropKind::Tree, mile, lateral, width, width, 3.2f + Hash01(h >> 16u) * 4.6f, h, true});
}

void PushTownBlock(std::vector<WorldProp>& props, int chunk, int side, float roadWidth, WorldZone zone) {
    const unsigned h = WorldGenerator::Hash(static_cast<unsigned>(chunk) * 57269u + static_cast<unsigned>(side + 7) * 1867u);
    const float mile = (static_cast<float>(chunk) + 0.08f + Hash01(h) * 0.78f) * kChunkMiles;
    const float edge = roadWidth * 0.5f;
    const float lateral = static_cast<float>(side) * (edge + 3.0f + Hash01(h >> 5u) * 2.8f);
    const float width = 2.8f + Hash01(h >> 10u) * 3.2f;
    const float depth = 2.2f + Hash01(h >> 14u) * 2.6f;
    const float height = zone == WorldZone::Industrial ? 4.2f + Hash01(h >> 18u) * 7.5f : 2.0f + Hash01(h >> 18u) * 3.4f;
    props.push_back(WorldProp {zone == WorldZone::Industrial ? WorldPropKind::IndustrialTank : WorldPropKind::Building, mile, lateral, width, depth, height, h, false});

    if ((h % 5u) == 0u) {
        props.push_back(WorldProp {WorldPropKind::WindowWatcher, mile + 0.004f, lateral - static_cast<float>(side) * 0.16f, 0.7f, 0.2f, 0.6f, h ^ 0xa11ceu, false});
    }
    if ((h % 7u) == 0u) {
        props.push_back(WorldProp {WorldPropKind::StreetLamp, mile - 0.006f, static_cast<float>(side) * (edge + 0.85f), 0.18f, 0.18f, 3.8f, h ^ 0x51a7eu, false});
    }
}

} // namespace

WorldContext WorldGenerator::ContextAt(float distanceMiles, float elapsedSeconds) {
    WorldContext context {};

    if (distanceMiles >= 3.45f) {
        context.zone = WorldZone::Industrial;
    } else if (distanceMiles >= 2.78f && distanceMiles < 3.16f) {
        context.zone = WorldZone::Service;
    } else if (distanceMiles >= 2.08f && distanceMiles < 2.78f) {
        context.zone = WorldZone::Town;
    } else if (distanceMiles >= 0.72f && distanceMiles < 1.82f) {
        context.zone = WorldZone::Storm;
    } else {
        context.zone = WorldZone::Forest;
    }

    const float stormIn = SmoothBand(distanceMiles, 0.66f, 0.88f);
    const float stormOut = 1.0f - SmoothBand(distanceMiles, 1.68f, 1.92f);
    context.stormIntensity = Clamp(stormIn * stormOut, 0.0f, 1.0f);
    context.wetness = Clamp(context.stormIntensity + SmoothBand(distanceMiles, 1.65f, 2.24f) * 0.42f, 0.0f, 1.0f);
    context.townDensity = context.zone == WorldZone::Town ? 1.0f : (context.zone == WorldZone::Service ? 0.64f : (context.zone == WorldZone::Industrial ? 0.82f : 0.0f));

    const float stormClock = std::fmod(elapsedSeconds * 0.42f + distanceMiles * 7.0f, 5.0f);
    if (context.stormIntensity > 0.35f && stormClock < 0.22f) {
        context.lightning = (1.0f - stormClock / 0.22f) * context.stormIntensity;
    }

    context.monsterVoiceWindow =
        (distanceMiles > 1.05f && distanceMiles < 1.38f) ||
        (distanceMiles > 2.18f && distanceMiles < 2.58f) ||
        (distanceMiles > 3.28f && distanceMiles < 3.72f);
    return context;
}

std::vector<WorldProp> WorldGenerator::PropsAround(float distanceMiles, float behindMiles, float aheadMiles, float roadWidth) {
    std::vector<WorldProp> props;
    const int first = static_cast<int>(std::floor((distanceMiles - behindMiles) / kChunkMiles));
    const int last = static_cast<int>(std::ceil((distanceMiles + aheadMiles) / kChunkMiles));

    for (int chunk = first; chunk <= last; ++chunk) {
        if (chunk < 0) continue;
        const float chunkMile = static_cast<float>(chunk) * kChunkMiles;
        const WorldZone zone = ContextAt(chunkMile, 0.0f).zone;
        const unsigned h = Hash(static_cast<unsigned>(chunk) * 2654435761u);

        if (zone == WorldZone::Forest || zone == WorldZone::Storm) {
            PushTree(props, chunk, -1, roadWidth);
            PushTree(props, chunk, 1, roadWidth);
            if ((h % 3u) == 0u) PushTree(props, chunk, (h & 1u) ? 1 : -1, roadWidth);
        }

        if (zone == WorldZone::Town || zone == WorldZone::Service || zone == WorldZone::Industrial) {
            PushTownBlock(props, chunk, -1, roadWidth, zone);
            PushTownBlock(props, chunk, 1, roadWidth, zone);
            if (zone == WorldZone::Service && chunkMile > 2.86f && chunkMile < 3.02f) {
                props.push_back(WorldProp {WorldPropKind::GasPump, chunkMile + 0.012f, 3.4f, 0.45f, 0.45f, 1.25f, h ^ 0x6a5u, false});
                props.push_back(WorldProp {WorldPropKind::Billboard, chunkMile + 0.004f, -5.1f, 3.2f, 0.3f, 2.1f, h ^ 0xb01du, false});
            }
            if ((h % 11u) == 0u) {
                props.push_back(WorldProp {WorldPropKind::RoadBarrier, chunkMile + 0.018f, (h & 1u) ? 1.05f : -1.05f, 1.7f, 0.8f, 0.7f, h ^ 0xba771eu, true});
            }
            if ((h % 9u) == 0u) {
                props.push_back(WorldProp {WorldPropKind::TrafficLight, chunkMile + 0.007f, 0.0f, 0.35f, 0.35f, 4.4f, h ^ 0x7aaff1cu, false});
            }
        }

        if (zone == WorldZone::Industrial && (h % 4u) == 0u) {
            const float lateral = ((h >> 2u) & 1u ? 1.0f : -1.0f) * (roadWidth * 0.5f + 1.3f + Hash01(h >> 6u) * 2.0f);
            props.push_back(WorldProp {WorldPropKind::UtilityPole, chunkMile + 0.016f, lateral, 0.22f, 0.22f, 7.0f, h ^ 0x907eu, false});
        }
    }

    return props;
}

std::optional<WorldProp> WorldGenerator::FindTreeImpact(float distanceMiles, float lateralOffset, float roadWidth) {
    const auto props = PropsAround(distanceMiles, 0.018f, 0.02f, roadWidth);
    for (const WorldProp& prop : props) {
        if (prop.kind != WorldPropKind::Tree) continue;
        if (std::abs(prop.mile - distanceMiles) > 0.014f) continue;
        if (std::abs(prop.lateral - lateralOffset) < prop.width + 0.65f) {
            return prop;
        }
    }
    return std::nullopt;
}

std::optional<WorldProp> WorldGenerator::FindRoadObstacle(float distanceMiles, float lateralOffset) {
    const auto props = PropsAround(distanceMiles, 0.012f, 0.018f, 7.2f);
    for (const WorldProp& prop : props) {
        if (prop.kind != WorldPropKind::RoadBarrier) continue;
        if (std::abs(prop.mile - distanceMiles) > 0.012f) continue;
        if (std::abs(prop.lateral - lateralOffset) < prop.width) {
            return prop;
        }
    }
    return std::nullopt;
}

const char* WorldGenerator::ZoneName(WorldZone zone) {
    switch (zone) {
        case WorldZone::Storm:
            return "Storm Road";
        case WorldZone::Town:
            return "Abandoned Town";
        case WorldZone::Service:
            return "Service Strip";
        case WorldZone::Industrial:
            return "Tower Works";
        case WorldZone::Forest:
        default:
            return "Forest Road";
    }
}

unsigned WorldGenerator::Hash(unsigned value) {
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value;
}

} // namespace ld
