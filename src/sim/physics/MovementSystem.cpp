#include "sim/physics/MovementSystem.hpp"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

#include "sim/components/TransformComponent.hpp"
#include "sim/components/VehicleComponent.hpp"
#include "sim/damage/DamageComponent.hpp"

namespace clf::sim::physics {

namespace {

double WrapAngleRad(double a)
{
    while (a > 3.141592653589793) a -= 2.0 * 3.141592653589793;
    while (a < -3.141592653589793) a += 2.0 * 3.141592653589793;
    return a;
}

double ApproachAngle(double current, double target, double maxStep)
{
    const double delta = WrapAngleRad(target - current);
    const double step = std::clamp(delta, -maxStep, +maxStep);
    return WrapAngleRad(current + step);
}

double Approach(double current, double target, double maxDelta)
{
    const double d = target - current;
    if (std::abs(d) <= maxDelta) return target;
    return current + (d > 0.0 ? maxDelta : -maxDelta);
}

} // namespace

void UpdateMovement(entt::registry& registry, double dtSeconds)
{
    auto view = registry.view<Transform, Vehicle, VehicleControl>();
    for (const auto e : view) {
        auto& xf = view.get<Transform>(e);
        auto& veh = view.get<Vehicle>(e);
        const auto& ctrl = view.get<VehicleControl>(e);

        if (const auto* dmg = registry.try_get<damage::DamageState>(e)) {
            if (dmg->destroyed || dmg->mobility >= damage::SubsystemState::Disabled) {
                veh.speed_mps = 0.0;
                // Turret still allowed for now (unless future firepower disables it).
                continue;
            }
        }

        // Clamp desired speed.
        const double desiredSpeed = std::clamp(ctrl.desired_speed_mps, 0.0, veh.max_speed_mps);

        // Speed control (accel/brake).
        const double maxUp = veh.accel_mps2 * dtSeconds;
        const double maxDown = veh.brake_mps2 * dtSeconds;
        if (desiredSpeed >= veh.speed_mps) {
            veh.speed_mps = Approach(veh.speed_mps, desiredSpeed, maxUp);
        } else {
            veh.speed_mps = Approach(veh.speed_mps, desiredSpeed, maxDown);
        }

        // Hull heading control.
        const double maxHullStep = veh.hull_turn_rate_radps * dtSeconds;
        xf.hull_heading_rad = ApproachAngle(xf.hull_heading_rad, ctrl.desired_hull_heading_rad, maxHullStep);

        // Turret heading control (relative to world).
        const double maxTurretStep = veh.turret_turn_rate_radps * dtSeconds;
        veh.turret_heading_rad = ApproachAngle(veh.turret_heading_rad, ctrl.desired_turret_heading_rad, maxTurretStep);

        // Integrate position along hull heading.
        const glm::dvec2 forward{std::cos(xf.hull_heading_rad), std::sin(xf.hull_heading_rad)};
        xf.position_m += forward * veh.speed_mps * dtSeconds;
    }
}

} // namespace clf::sim::physics
