#include "sim/manual/ManualMovementSystem.hpp"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

#include "sim/Components.hpp"
#include "sim/components/TransformComponent.hpp"
#include "sim/components/VehicleComponent.hpp"

namespace clf::sim::manual {

namespace {

double AngleTo(const glm::dvec2& from, const glm::dvec2& to)
{
    const glm::dvec2 d = to - from;
    return std::atan2(d.y, d.x);
}

} // namespace

void UpdateManualMovement(entt::registry& registry, double dtSeconds)
{
    (void)dtSeconds;

    auto view = registry.view<ControlModeComponent, WaypointPathComponent, Transform, Vehicle, VehicleControl>();
    for (const auto e : view) {
        auto& mode = view.get<ControlModeComponent>(e);
        if (mode.mode != ControlMode::Manual) {
            continue;
        }

        auto& path = view.get<WaypointPathComponent>(e);
        auto& xf = view.get<Transform>(e);
        auto& veh = view.get<Vehicle>(e);
        auto& ctrl = view.get<VehicleControl>(e);

        if (path.waypoints.empty()) {
            ctrl.desired_speed_mps = 0.0;
            continue;
        }

        const glm::dvec2 target = path.waypoints.front();
        const glm::dvec2 to = target - xf.position_m;
        const double dist = glm::length(to);

        const double arrive = std::max(1.0, path.arrival_radius_m);
        if (dist <= arrive) {
            path.waypoints.erase(path.waypoints.begin());
            if (path.waypoints.empty()) {
                ctrl.desired_speed_mps = 0.0;
            }
            continue;
        }

        ctrl.desired_hull_heading_rad = AngleTo(xf.position_m, target);

        // Slow down as we approach to reduce overshoot.
        const double slowDist = std::max(50.0, arrive * 6.0);
        const double speedFrac = std::clamp(dist / slowDist, 0.20, 1.0);
        ctrl.desired_speed_mps = std::min(veh.max_speed_mps, 10.0) * speedFrac;
    }
}

} // namespace clf::sim::manual

