#include "sim/ai/TankAISystem.hpp"

#include <algorithm>
#include <cmath>
#include <random>

#include <glm/geometric.hpp>

#include "sim/Components.hpp"
#include "sim/components/DetectionComponent.hpp"
#include "sim/components/SensorComponent.hpp"
#include "sim/components/TransformComponent.hpp"
#include "sim/components/VehicleComponent.hpp"
#include "sim/terrain/Terrain.hpp"
#include "sim/ai/TankAIComponent.hpp"
#include "sim/damage/DamageComponent.hpp"

namespace clf::sim::ai {

namespace {

double WrapAngleRad(double a)
{
    while (a > 3.141592653589793) a -= 2.0 * 3.141592653589793;
    while (a < -3.141592653589793) a += 2.0 * 3.141592653589793;
    return a;
}

double AngleTo(const glm::dvec2& from, const glm::dvec2& to)
{
    const glm::dvec2 d = to - from;
    return std::atan2(d.y, d.x);
}

glm::dvec2 RandomWaypoint(std::mt19937& rng, double halfExtent_m)
{
    std::uniform_real_distribution<double> dist(-halfExtent_m + 200.0, +halfExtent_m - 200.0);
    return glm::dvec2(dist(rng), dist(rng));
}

} // namespace

void UpdateTankAI(entt::registry& registry, const terrain::Terrain& terrain, double dtSeconds, std::uint32_t seed)
{
    (void)terrain;
    std::mt19937 rng(seed);

    constexpr double halfExtent = terrain::Terrain::kWorldSizeMeters * 0.5;

    auto view = registry.view<const Tank, Transform, Vehicle, VehicleControl, const Sensor, Detection, TankAI>();
    for (const auto e : view) {
        const auto& tank = view.get<const Tank>(e);
        auto& xf = view.get<Transform>(e);
        auto& veh = view.get<Vehicle>(e);
        auto& ctrl = view.get<VehicleControl>(e);
        const auto& sensor = view.get<const Sensor>(e);
        auto& det = view.get<Detection>(e);
        auto& ai = view.get<TankAI>(e);

        if (const auto* dmg = registry.try_get<damage::DamageState>(e)) {
            if (dmg->destroyed) {
                ai.state = TankAIState::Destroyed;
                ctrl.desired_speed_mps = 0.0;
                continue;
            }
            if (dmg->mobility >= damage::SubsystemState::Disabled) {
                ai.state = TankAIState::Disabled;
                ctrl.desired_speed_mps = 0.0;
            }
        }

        ai.state_time_s += dtSeconds;

        // Turret scan: constant-rate sweep around the hull direction so detections can “integrate”.
        if (!(det.target_detected && det.current_target != entt::null)) {
            ai.scan_phase += dtSeconds;
            const double scanRate = 0.35; // rad/s
            ctrl.desired_turret_heading_rad = WrapAngleRad(xf.hull_heading_rad + ai.scan_phase * scanRate);
        }

        switch (ai.state) {
            case TankAIState::Idle:
            case TankAIState::Search: {
                ai.state = TankAIState::MoveToSearchPosition;
                ai.state_time_s = 0.0;
                ai.has_waypoint = false;
                break;
            }
            case TankAIState::MoveToSearchPosition: {
                if (!ai.has_waypoint || ai.state_time_s > 25.0) {
                    ai.search_waypoint_m = RandomWaypoint(rng, halfExtent);
                    ai.has_waypoint = true;
                    ai.state_time_s = 0.0;
                }

                const glm::dvec2 toWp = ai.search_waypoint_m - xf.position_m;
                const double dist = glm::length(toWp);

                if (dist < 40.0) {
                    ai.state = TankAIState::Scan;
                    ai.state_time_s = 0.0;
                    ctrl.desired_speed_mps = 0.0;
                } else {
                    ctrl.desired_hull_heading_rad = AngleTo(xf.position_m, ai.search_waypoint_m);
                    ctrl.desired_speed_mps = std::clamp(0.6 * tank.visual_range_m / 250.0, 6.0, veh.max_speed_mps);
                }

                if (det.target_detected) {
                    ai.state = TankAIState::AimAtTarget;
                    ai.state_time_s = 0.0;
                }
                break;
            }
            case TankAIState::Scan: {
                ctrl.desired_speed_mps = 0.0;
                if (ai.state_time_s > 7.0) {
                    ai.state = TankAIState::MoveToSearchPosition;
                    ai.state_time_s = 0.0;
                }
                if (det.target_detected) {
                    ai.state = TankAIState::AimAtTarget;
                    ai.state_time_s = 0.0;
                }
                break;
            }
            case TankAIState::AimAtTarget: {
                ctrl.desired_speed_mps = 0.0;
                if (det.current_target == entt::null || !registry.valid(det.current_target) || !registry.all_of<Transform>(det.current_target)) {
                    ai.state = TankAIState::MoveToSearchPosition;
                    ai.state_time_s = 0.0;
                    det.target_detected = false;
                    break;
                }

                const auto& txf = registry.get<const Transform>(det.current_target);
                const double bearing = AngleTo(xf.position_m, txf.position_m);
                ctrl.desired_turret_heading_rad = bearing;

                // Keep hull roughly oriented too (helps future movement).
                ctrl.desired_hull_heading_rad = bearing;

                // If we lost LOS, fall back to scan (memory can drive later).
                if (!det.target_has_los) {
                    ai.state = TankAIState::Reposition;
                    ai.state_time_s = 0.0;
                }
                break;
            }
            case TankAIState::Reposition: {
                // Simple behavior: move toward last known position for a short time.
                ctrl.desired_hull_heading_rad = AngleTo(xf.position_m, det.last_known_target_pos_m);
                ctrl.desired_speed_mps = std::min(veh.max_speed_mps, 10.0);
                if (ai.state_time_s > 4.0 || det.target_detected) {
                    ai.state = TankAIState::MoveToSearchPosition;
                    ai.state_time_s = 0.0;
                }
                break;
            }
            default:
                break;
        }

        // Ensure sensor range matches configured tank values for now (until tank-type JSON lands).
        // Keep this in AI system so it stays centralized during the prototype phase.
        (void)sensor;
        (void)veh;
    }
}

} // namespace clf::sim::ai
