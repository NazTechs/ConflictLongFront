#include "sim/ai/TankAISystem.hpp"

#include <algorithm>
#include <cmath>
#include <random>

#include <glm/geometric.hpp>
#include <entt/entity/entity.hpp>

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

glm::dvec2 RandomWaypointAround(std::mt19937& rng, const glm::dvec2& origin, double halfExtent_m, double minDist_m, double maxDist_m)
{
    std::uniform_real_distribution<double> ang(0.0, 2.0 * 3.141592653589793);
    std::uniform_real_distribution<double> rad(minDist_m, maxDist_m);

    for (int attempt = 0; attempt < 32; ++attempt) {
        const double a = ang(rng);
        const double r = rad(rng);
        glm::dvec2 p = origin + glm::dvec2(std::cos(a), std::sin(a)) * r;
        p.x = std::clamp(p.x, -halfExtent_m + 200.0, +halfExtent_m - 200.0);
        p.y = std::clamp(p.y, -halfExtent_m + 200.0, +halfExtent_m - 200.0);
        return p;
    }
    return RandomWaypoint(rng, halfExtent_m);
}

std::mt19937 MakeEntityRng(std::uint32_t seed, entt::entity e, std::uint32_t generation)
{
    const std::uint32_t eid = static_cast<std::uint32_t>(entt::to_integral(e));
    std::seed_seq seq{seed, eid, generation, seed ^ (eid * 0x9E3779B9u)};
    return std::mt19937(seq);
}

} // namespace

void UpdateTankAI(entt::registry& registry, const terrain::Terrain& terrain, double dtSeconds, std::uint32_t seed, const AiSettings& settings)
{
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

        // Stuck detection (movement is handled by MovementSystem; here we just observe).
        if (ai.state_time_s <= dtSeconds * 1.5) {
            ai.last_pos_m = xf.position_m;
        }
        const double moved_m = glm::length(xf.position_m - ai.last_pos_m);
        ai.last_pos_m = xf.position_m;
        if (moved_m < 0.25 && veh.speed_mps > 1.0) {
            ai.stuck_time_s += dtSeconds;
        } else {
            ai.stuck_time_s = std::max(0.0, ai.stuck_time_s - dtSeconds * 0.5);
        }

        // Turret scan: constant-rate sweep around the hull direction so detections can integrate.
        if (!(det.target_detected && det.current_target != entt::null)) {
            const double scanSpeedRad_s = settings.turret.scan_speed_deg_s * (3.141592653589793 / 180.0);
            const double arcRad = settings.turret.scan_arc_deg * (3.141592653589793 / 180.0);
            ai.scan_phase += dtSeconds * scanSpeedRad_s;
            const double offset = std::sin(ai.scan_phase) * (arcRad * 0.5);
            ctrl.desired_turret_heading_rad = WrapAngleRad(xf.hull_heading_rad + offset);
        }

        switch (ai.state) {
            case TankAIState::Idle:
            case TankAIState::Search: {
                ai.state = TankAIState::SearchSelectWaypoint;
                ai.state_time_s = 0.0;
                ai.has_waypoint = false;
                break;
            }
            case TankAIState::SearchSelectWaypoint: {
                auto rng = MakeEntityRng(seed, e, ai.waypoint_generation++);

                // If we have a recent last-known contact, bias toward it first.
                if (det.memory_remaining_s > 0.0) {
                    ai.search_waypoint_m = det.last_known_target_pos_m;
                    ai.has_waypoint = true;
                } else {
                    ai.search_waypoint_m = RandomWaypointAround(
                        rng,
                        xf.position_m,
                        halfExtent,
                        settings.search.min_waypoint_distance_m,
                        settings.search.max_waypoint_distance_m);
                    ai.has_waypoint = true;
                }
                ai.state = TankAIState::MoveToWaypoint;
                ai.state_time_s = 0.0;
                break;
            }
            case TankAIState::MoveToWaypoint:
            case TankAIState::ScanWhileMoving: {
                if (!ai.has_waypoint) {
                    ai.state = TankAIState::SearchSelectWaypoint;
                    ai.state_time_s = 0.0;
                    break;
                }

                const glm::dvec2 toWp = ai.search_waypoint_m - xf.position_m;
                const double dist = glm::length(toWp);

                const double reachedRadius_m = settings.search.waypoint_reached_radius_m;
                if (dist < reachedRadius_m) {
                    ai.has_waypoint = false;
                    ai.state = TankAIState::SearchSelectWaypoint;
                    ai.state_time_s = 0.0;
                    ctrl.desired_speed_mps = 0.0;
                    break;
                }

                // If stuck, pick a new waypoint.
                if (ai.stuck_time_s > settings.search.stuck_timeout_s) {
                    ai.has_waypoint = false;
                    ai.stuck_time_s = 0.0;
                    ai.state = TankAIState::SearchSelectWaypoint;
                    ai.state_time_s = 0.0;
                    ctrl.desired_speed_mps = 0.0;
                    break;
                }

                // Slow down as we approach the waypoint.
                const double slowDist = 700.0;
                const double speedFrac = std::clamp(dist / slowDist, 0.25, 1.0);
                const double cruise = std::min(veh.max_speed_mps, settings.movement.default_max_speed_mps);

                ctrl.desired_hull_heading_rad = AngleTo(xf.position_m, ai.search_waypoint_m);
                ctrl.desired_speed_mps = cruise * speedFrac;
                ai.state = TankAIState::ScanWhileMoving;

                if (det.target_detected) {
                    ai.state = TankAIState::AimAtTarget;
                    ai.state_time_s = 0.0;
                }
                break;
            }
            case TankAIState::Scan: {
                ctrl.desired_speed_mps = 0.0;
                if (ai.state_time_s > 7.0) {
                    ai.state = TankAIState::SearchSelectWaypoint;
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
                    ai.state = TankAIState::SearchSelectWaypoint;
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
                if (ai.state_time_s > 8.0 || det.target_detected) {
                    ai.state = TankAIState::SearchSelectWaypoint;
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
        (void)tank;
    }
}

} // namespace clf::sim::ai
