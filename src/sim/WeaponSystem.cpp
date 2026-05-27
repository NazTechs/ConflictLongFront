#include "sim/WeaponSystem.hpp"

#include <limits>
#include <random>
#include <string>

#include <glm/geometric.hpp>

#include <spdlog/spdlog.h>

#include <entt/entity/entity.hpp>

#include "sim/Components.hpp"
#include "sim/DebugDraw.hpp"
#include "sim/LineOfSight.hpp"
#include "sim/components/TransformComponent.hpp"
#include "sim/components/DetectionComponent.hpp"
#include "sim/components/VehicleComponent.hpp"
#include "sim/damage/ArmorComponent.hpp"
#include "sim/damage/DamageComponent.hpp"
#include "sim/damage/DamageModel.hpp"
#include "sim/damage/PenetrationModel.hpp"
#include "sim/weapons/CombatLog.hpp"
#include "sim/weapons/ShotEvent.hpp"
#include "sim/terrain/Terrain.hpp"

namespace clf::sim::weapon {

namespace {

const char* TeamName(int teamId)
{
    return (teamId == 0) ? "Blue" : (teamId == 1) ? "Red" : "Unknown";
}

std::string GetUnitName(entt::registry& registry, entt::entity e, const Tank& tank)
{
    if (const auto* name = registry.try_get<UnitName>(e)) {
        if (!name->value.empty()) {
            return name->value;
        }
    }
    return std::string(TeamName(tank.team_id));
}

double WrapAngleRad(double a)
{
    while (a > 3.141592653589793) a -= 2.0 * 3.141592653589793;
    while (a < -3.141592653589793) a += 2.0 * 3.141592653589793;
    return a;
}

} // namespace

void UpdateDirectFire(entt::registry& registry,
                      const terrain::Terrain& terrain,
                      double dtSeconds,
                      double simTimeSeconds,
                      weapons::CombatLog* combatLog,
                      std::vector<DebugLine>& inOutDebugLines)
{
    auto view = registry.view<Tank, Transform, Vehicle, DirectFireGun>();

    // Update reload timers first.
    for (const auto e : view) {
        auto& gun = view.get<DirectFireGun>(e);
        gun.reload_remaining_s = std::max(0.0, gun.reload_remaining_s - dtSeconds);
    }

    // Direct fire selection: nearest enemy tank.
    for (const auto shooter : view) {
        const auto& shooterTank = view.get<Tank>(shooter);
        const auto& shooterXf = view.get<Transform>(shooter);
        const auto& shooterVeh = view.get<Vehicle>(shooter);
        auto& shooterGun = view.get<DirectFireGun>(shooter);

        if (const auto* dmg = registry.try_get<damage::DamageState>(shooter)) {
            if (dmg->destroyed || dmg->firepower >= damage::SubsystemState::Disabled) {
                EngagementInfo info{};
                info.target = entt::null;
                registry.emplace_or_replace<EngagementInfo>(shooter, info);
                continue;
            }
        }

        // Prefer sensor-detected target if available.
        entt::entity bestTarget = entt::null;
        if (const auto* det = registry.try_get<Detection>(shooter)) {
            if (det->target_detected && det->current_target != entt::null) {
                bestTarget = det->current_target;
            }
        }

        EngagementInfo info{};
        info.target = bestTarget;

        if (bestTarget != entt::null) {
            const auto& targetTank = registry.get<const Tank>(bestTarget);
            const auto& targetXf = registry.get<const Transform>(bestTarget);
            const glm::dvec2 toTarget = targetXf.position_m - shooterXf.position_m;
            const double dist_m = glm::length(toTarget);

            const double bearing = std::atan2(toTarget.y, toTarget.x);
            const double turretDelta = std::abs(WrapAngleRad(bearing - shooterVeh.turret_heading_rad));
            const bool turretAligned = turretDelta <= 0.026; // ~1.5 deg

            LosResult los{};
            const bool losClear = hasLineOfSight(
                terrain,
                shooterXf.position_m,
                shooterTank.sensor_height_m,
                targetXf.position_m,
                targetTank.sensor_height_m,
                &los);

            info.target_distance_m = dist_m;
            info.target_in_visual_range = dist_m <= shooterTank.visual_range_m;
            const double maxRange = (shooterGun.max_effective_range_m > 0.0) ? shooterGun.max_effective_range_m : shooterGun.max_range_m;
            info.target_in_weapon_range = dist_m <= maxRange;
            info.has_line_of_sight = losClear;
            info.has_blocked_point = !losClear;
            info.blocked_point_m = los.blocked_point_m;

            const bool canFire =
                info.target_in_weapon_range &&
                info.has_line_of_sight &&
                turretAligned &&
                (shooterGun.ammo > 0) &&
                (shooterGun.reload_remaining_s <= 0.0);

            if (canFire) {
                shooterGun.ammo -= 1;
                shooterGun.reload_remaining_s = shooterGun.reload_time_s;

                const std::string shooterName = GetUnitName(registry, shooter, shooterTank);
                const std::string targetName = GetUnitName(registry, bestTarget, targetTank);

                // Prototype hit model:
                // - Miss distance is Gaussian with sigma = dispersion(mrad) * range.
                const double sigma_m = std::max(0.1, (shooterGun.dispersion_mrad * 0.001) * dist_m);
                const double targetRadius_m = targetTank.radius_m;

                const std::uint32_t baseSeed =
                    static_cast<std::uint32_t>(simTimeSeconds * 1000.0) ^
                    (static_cast<std::uint32_t>(entt::to_integral(shooter)) * 1664525u) ^
                    (static_cast<std::uint32_t>(entt::to_integral(bestTarget)) * 1013904223u);

                std::mt19937 rng(baseSeed);
                std::normal_distribution<double> n(0.0, sigma_m);
                const double mx = n(rng);
                const double my = n(rng);
                const double miss_m = std::sqrt(mx * mx + my * my);

                weapons::ShotEvent ev{};
                ev.sim_time_s = simTimeSeconds;
                ev.shooter = shooter;
                ev.target = bestTarget;
                ev.weapon_id = shooterGun.weapon_id;
                ev.projectile_name = shooterGun.projectile_name;
                ev.distance_m = dist_m;
                ev.origin_m = shooterXf.position_m;
                ev.aim_point_m = targetXf.position_m;

                const bool hit = miss_m <= targetRadius_m;
                ev.hit = hit;

                if (hit) {
                    ev.impact_point_m = targetXf.position_m;

                    damage::PenetrationProfile prof{};
                    prof.pen_at_0m_mm = shooterGun.pen_at_0m_mm;
                    prof.pen_at_1000m_mm = shooterGun.pen_at_1000m_mm;
                    prof.pen_at_2000m_mm = shooterGun.pen_at_2000m_mm;
                    prof.pen_at_3000m_mm = shooterGun.pen_at_3000m_mm;
                    const double pen_mm = damage::PenetrationAtRangeMm(prof, dist_m);

                    // Impact angle based on relative bearing (very simplified; no slope / elevation yet).
                    const glm::dvec2 dir = glm::length(toTarget) > 1e-6 ? (toTarget / glm::length(toTarget)) : glm::dvec2(1.0, 0.0);
                    const auto zone = damage::ClassifyZone(targetXf.hull_heading_rad, dir);
                    const double impactAngle = 0.0;

                    if (registry.all_of<damage::Armor, damage::DamageState>(bestTarget)) {
                        const auto& armor = registry.get<const damage::Armor>(bestTarget);
                        auto& dmg = registry.get<damage::DamageState>(bestTarget);
                        const auto res = damage::ApplyPenetration(armor, dmg, zone, pen_mm, impactAngle, baseSeed ^ 0xA5A5A5A5u);
                        ev.penetrated = res.penetrated;
                        ev.result_summary = res.summary;
                    } else {
                        ev.penetrated = false;
                        ev.result_summary = "Hit (no damage component)";
                    }
                } else {
                    // Miss: impact point is along the ray (purely for visualization/logging).
                    ev.impact_point_m = shooterXf.position_m + (toTarget / std::max(1e-6, dist_m)) * dist_m;
                    ev.penetrated = false;
                    ev.result_summary = "Miss";
                }

                spdlog::info("{} fires {} at {} (dist {:.0f} m, {}{}, ammo {})",
                             shooterName,
                             shooterGun.projectile_name.empty() ? "shot" : shooterGun.projectile_name,
                             targetName,
                             dist_m,
                             ev.hit ? "HIT" : "MISS",
                             ev.hit ? (ev.penetrated ? ", PEN" : ", NO PEN") : "",
                             shooterGun.ammo);

                if (combatLog) {
                    combatLog->Add(ev);
                }

                DebugLine line{};
                line.a_m = shooterXf.position_m;
                line.b_m = targetXf.position_m;
                line.ttl_s = 0.18;
                line.r = 255;
                line.g = 220;
                line.b = 60;
                line.a = 255;
                inOutDebugLines.push_back(line);
            }
        }

        registry.emplace_or_replace<EngagementInfo>(shooter, info);
    }
}

} // namespace clf::sim::weapon
