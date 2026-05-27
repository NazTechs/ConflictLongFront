#include "sim/Systems.hpp"

#include <cmath>

#include <glm/geometric.hpp>

#include "sim/Components.hpp"

namespace clf::sim::systems {

void IntegrateTanks(entt::registry& registry, double dtSeconds)
{
    auto view = registry.view<Tank>();
    for (const auto entity : view) {
        auto& tank = view.get<Tank>(entity);
        tank.position_m += tank.velocity_mps * dtSeconds;

        const double speed2 = glm::dot(tank.velocity_mps, tank.velocity_mps);
        if (speed2 > 1e-12) {
            tank.heading_rad = std::atan2(tank.velocity_mps.y, tank.velocity_mps.x);
        }
    }
}

void BounceTanksInAabb(entt::registry& registry, const glm::dvec2& minMeters, const glm::dvec2& maxMeters)
{
    auto view = registry.view<Tank>();
    for (const auto entity : view) {
        auto& tank = view.get<Tank>(entity);

        if (tank.position_m.x < minMeters.x) {
            tank.position_m.x = minMeters.x;
            tank.velocity_mps.x = std::abs(tank.velocity_mps.x);
        } else if (tank.position_m.x > maxMeters.x) {
            tank.position_m.x = maxMeters.x;
            tank.velocity_mps.x = -std::abs(tank.velocity_mps.x);
        }

        if (tank.position_m.y < minMeters.y) {
            tank.position_m.y = minMeters.y;
            tank.velocity_mps.y = std::abs(tank.velocity_mps.y);
        } else if (tank.position_m.y > maxMeters.y) {
            tank.position_m.y = maxMeters.y;
            tank.velocity_mps.y = -std::abs(tank.velocity_mps.y);
        }
    }
}

} // namespace clf::sim::systems

