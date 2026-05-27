#include "sim/damage/DamageModel.hpp"

#include <algorithm>
#include <cmath>
#include <random>

#include "sim/damage/ArmorComponent.hpp"
#include "sim/damage/DamageComponent.hpp"

namespace clf::sim::damage {

namespace {

double WrapAngleRad(double a)
{
    while (a > 3.141592653589793) a -= 2.0 * 3.141592653589793;
    while (a < -3.141592653589793) a += 2.0 * 3.141592653589793;
    return a;
}

const char* ZoneName(HitZone z)
{
    switch (z) {
        case HitZone::HullFront: return "HullFront";
        case HitZone::HullSide: return "HullSide";
        case HitZone::HullRear: return "HullRear";
        case HitZone::TurretFront: return "TurretFront";
        case HitZone::TurretSide: return "TurretSide";
        case HitZone::TurretRear: return "TurretRear";
    }
    return "HullFront";
}

} // namespace

double ArmorThicknessForZone(const Armor& armor, HitZone zone)
{
    switch (zone) {
        case HitZone::HullFront: return armor.front_mm;
        case HitZone::HullSide: return armor.side_mm;
        case HitZone::HullRear: return armor.rear_mm;
        case HitZone::TurretFront: return armor.turret_front_mm;
        case HitZone::TurretSide: return armor.turret_side_mm;
        case HitZone::TurretRear: return armor.turret_rear_mm;
    }
    return armor.front_mm;
}

HitZone ClassifyZone(double targetHullHeading_rad, const glm::dvec2& shotDirWorld)
{
    // shotDirWorld points from shooter -> target. We want the impact “from” direction on target.
    const double incoming = std::atan2(-shotDirWorld.y, -shotDirWorld.x);
    const double rel = std::abs(WrapAngleRad(incoming - targetHullHeading_rad));

    // Front within 60 deg, rear within 60 deg, else side.
    if (rel <= (60.0 * 3.141592653589793 / 180.0)) {
        return HitZone::HullFront;
    }
    if (std::abs(rel - 3.141592653589793) <= (60.0 * 3.141592653589793 / 180.0)) {
        return HitZone::HullRear;
    }
    return HitZone::HullSide;
}

DamageResult ApplyPenetration(const Armor& armor,
                             DamageState& inOutDamage,
                             HitZone zone,
                             double penetration_mm,
                             double impactAngleRad,
                             std::uint32_t rngSeed)
{
    DamageResult out{};
    out.zone = zone;

    // Effective penetration decreases with cosine of impact angle (very simplified).
    const double angleFactor = std::clamp(std::cos(std::abs(impactAngleRad)), 0.2, 1.0);
    const double effectivePen = penetration_mm * angleFactor;
    const double thickness = ArmorThicknessForZone(armor, zone);

    if (effectivePen < thickness) {
        out.penetrated = false;
        out.summary = std::string("No penetration (") + ZoneName(zone) + ")";
        return out;
    }

    out.penetrated = true;

    std::mt19937 rng(rngSeed);
    std::uniform_real_distribution<double> u(0.0, 1.0);

    // Internal effects (prototype):
    // - Small chance of outright kill
    // - Otherwise degrade subsystems based on zone
    const double roll = u(rng);
    if (roll < 0.06) {
        inOutDamage.destroyed = true;
        inOutDamage.mobility = SubsystemState::Destroyed;
        inOutDamage.firepower = SubsystemState::Destroyed;
        inOutDamage.optics = SubsystemState::Destroyed;
        inOutDamage.engine = SubsystemState::Destroyed;
        inOutDamage.ammo = SubsystemState::Destroyed;
        out.summary = std::string("Catastrophic kill (") + ZoneName(zone) + ")";
        return out;
    }

    auto worsen = [&](SubsystemState& s) {
        if (s == SubsystemState::Ok) s = SubsystemState::Damaged;
        else if (s == SubsystemState::Damaged) s = SubsystemState::Disabled;
        else if (s == SubsystemState::Disabled) s = SubsystemState::Destroyed;
    };

    switch (zone) {
        case HitZone::HullFront:
        case HitZone::HullSide:
        case HitZone::HullRear:
            worsen(inOutDamage.mobility);
            if (u(rng) < 0.35) worsen(inOutDamage.engine);
            if (u(rng) < 0.25) worsen(inOutDamage.optics);
            if (u(rng) < 0.18) worsen(inOutDamage.ammo);
            break;
        case HitZone::TurretFront:
        case HitZone::TurretSide:
        case HitZone::TurretRear:
            worsen(inOutDamage.firepower);
            if (u(rng) < 0.35) worsen(inOutDamage.optics);
            if (u(rng) < 0.18) worsen(inOutDamage.ammo);
            break;
    }

    if (inOutDamage.ammo >= SubsystemState::Disabled && u(rng) < 0.25) {
        inOutDamage.destroyed = true;
        out.summary = std::string("Ammo cookoff -> destroyed (") + ZoneName(zone) + ")";
        return out;
    }

    out.summary = std::string("Penetration (") + ZoneName(zone) + ")";
    return out;
}

} // namespace clf::sim::damage

