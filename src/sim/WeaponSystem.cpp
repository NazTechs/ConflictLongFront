#include "sim/WeaponSystem.hpp"

#include <limits>
#include <string>

#include <glm/geometric.hpp>

#include <spdlog/spdlog.h>

#include "sim/Components.hpp"
#include "sim/DebugDraw.hpp"
#include "sim/LineOfSight.hpp"
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

} // namespace

void UpdateDirectFire(entt::registry& registry,
                      const terrain::Terrain& terrain,
                      double dtSeconds,
                      std::vector<DebugLine>& inOutDebugLines)
{
    auto view = registry.view<Tank, DirectFireGun>();

    // Update reload timers first.
    for (const auto e : view) {
        auto& gun = view.get<DirectFireGun>(e);
        gun.reload_remaining_s = std::max(0.0, gun.reload_remaining_s - dtSeconds);
    }

    // Direct fire selection: nearest enemy tank.
    for (const auto shooter : view) {
        const auto& shooterTank = view.get<Tank>(shooter);
        auto& shooterGun = view.get<DirectFireGun>(shooter);

        entt::entity bestTarget = entt::null;
        double bestDist2 = std::numeric_limits<double>::infinity();

        auto tankView = registry.view<const Tank>();
        for (const auto candidate : tankView) {
            if (candidate == shooter) {
                continue;
            }
            const auto& candTank = tankView.get<const Tank>(candidate);
            if (candTank.team_id == shooterTank.team_id) {
                continue;
            }

            const glm::dvec2 d = candTank.position_m - shooterTank.position_m;
            const double dist2 = glm::dot(d, d);
            if (dist2 < bestDist2) {
                bestDist2 = dist2;
                bestTarget = candidate;
            }
        }

        EngagementInfo info{};
        info.target = bestTarget;

        if (bestTarget != entt::null) {
            const auto& targetTank = registry.get<const Tank>(bestTarget);
            const glm::dvec2 toTarget = targetTank.position_m - shooterTank.position_m;
            const double dist_m = glm::length(toTarget);

            LosResult los{};
            const bool losClear = hasLineOfSight(
                terrain,
                shooterTank.position_m,
                shooterTank.sensor_height_m,
                targetTank.position_m,
                targetTank.sensor_height_m,
                &los);

            info.target_distance_m = dist_m;
            info.target_in_visual_range = dist_m <= shooterTank.visual_range_m;
            info.target_in_weapon_range = dist_m <= shooterGun.max_range_m;
            info.has_line_of_sight = losClear;
            info.has_blocked_point = !losClear;
            info.blocked_point_m = los.blocked_point_m;

            const bool canFire =
                info.target_in_weapon_range &&
                info.has_line_of_sight &&
                (shooterGun.ammo > 0) &&
                (shooterGun.reload_remaining_s <= 0.0);

            if (canFire) {
                shooterGun.ammo -= 1;
                shooterGun.reload_remaining_s = shooterGun.reload_time_s;

                const std::string shooterName = GetUnitName(registry, shooter, shooterTank);
                const std::string targetName = GetUnitName(registry, bestTarget, targetTank);

                spdlog::info("{} fires {} at {} (dist {:.0f} m, ammo {})",
                             shooterName,
                             shooterGun.projectile_name.empty() ? "shot" : shooterGun.projectile_name,
                             targetName,
                             dist_m,
                             shooterGun.ammo);

                DebugLine line{};
                line.a_m = shooterTank.position_m;
                line.b_m = targetTank.position_m;
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

