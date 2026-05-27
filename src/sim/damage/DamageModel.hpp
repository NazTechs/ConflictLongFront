#pragma once

#include <string>
#include <cstdint>

#include <glm/vec2.hpp>

namespace clf::sim::damage {

struct Armor;
struct DamageState;

enum class HitZone {
    HullFront,
    HullSide,
    HullRear,
    TurretFront,
    TurretSide,
    TurretRear,
};

struct DamageResult final {
    bool penetrated = false;
    HitZone zone = HitZone::HullFront;
    std::string summary;
};

HitZone ClassifyZone(double targetHullHeading_rad, const glm::dvec2& shotDirWorld);

DamageResult ApplyPenetration(const Armor& armor,
                             DamageState& inOutDamage,
                             HitZone zone,
                             double penetration_mm,
                             double impactAngleRad,
                             std::uint32_t rngSeed);

double ArmorThicknessForZone(const Armor& armor, HitZone zone);

} // namespace clf::sim::damage
