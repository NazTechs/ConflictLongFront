#include "sim/ai/TankAISystem.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

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

constexpr double kPi = 3.14159265358979323846;

double WrapAngleRad(double a)
{
    while (a > kPi) {
        a -= 2.0 * kPi;
    }
    while (a < -kPi) {
        a += 2.0 * kPi;
    }
    return a;
}

double AngleTo(const glm::dvec2& from, const glm::dvec2& to)
{
    const glm::dvec2 d = to - from;
    return std::atan2(d.y, d.x);
}

glm::dvec2 ClampToWorld(const glm::dvec2& p, double halfExtent_m)
{
    return {
        std::clamp(p.x, -halfExtent_m + 200.0, halfExtent_m - 200.0),
        std::clamp(p.y, -halfExtent_m + 200.0, halfExtent_m - 200.0)
    };
}

glm::dvec2 ForwardFromHeading(double heading_rad)
{
    return {std::cos(heading_rad), std::sin(heading_rad)};
}

glm::dvec2 NormalizeOr(const glm::dvec2& v, const glm::dvec2& fallback)
{
    const double len = glm::length(v);
    if (len < 1e-6) {
        return fallback;
    }
    return v / len;
}

// Human-like forward sweep pattern:
//
// generation 0: front center
// generation 1: front left
// generation 2: front right
// generation 3: deeper front center
// generation 4: deeper front left
// generation 5: deeper front right
//
// This avoids random front/back movement and makes the tank clear the map
// progressively like a search patrol.
glm::dvec2 TacticalSearchWaypoint(
    const glm::dvec2& position_m,
    double hull_heading_rad,
    std::uint32_t generation,
    double sectorSize_m,
    double halfExtent_m)
{
    const glm::dvec2 forward = ForwardFromHeading(hull_heading_rad);
    const glm::dvec2 side{-forward.y, forward.x};

    const std::uint32_t ring = generation / 3u;
    const std::uint32_t slot = generation % 3u;

    double sideOffset = 0.0;
    if (slot == 1u) {
        sideOffset = -1.0; // left
    } else if (slot == 2u) {
        sideOffset = 1.0;  // right
    }

    const double forwardDistance = sectorSize_m * static_cast<double>(ring + 1u);
    const double sideDistance = sectorSize_m * sideOffset;

    glm::dvec2 waypoint =
        position_m +
        forward * forwardDistance +
        side * sideDistance;

    return ClampToWorld(waypoint, halfExtent_m);
}

bool WaypointIsBehindTank(
    const glm::dvec2& position_m,
    double hull_heading_rad,
    const glm::dvec2& waypoint_m)
{
    const double desired = AngleTo(position_m, waypoint_m);
    const double error = std::abs(WrapAngleRad(desired - hull_heading_rad));

    // More than ~137 degrees means the point is mostly behind the tank.
    return error > 2.4;
}

} // namespace

void UpdateTankAI(
    entt::registry& registry,
    const terrain::Terrain& terrain,
    double dtSeconds,
    std::uint32_t seed,
    const AiSettings& settings)
{
    (void)seed;

    constexpr double halfExtent = terrain::Terrain::kWorldSizeMeters * 0.5;

    auto view = registry.view<
        const Tank,
        Transform,
        Vehicle,
        VehicleControl,
        const Sensor,
        Detection,
        TankAI>();

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
                continue;
            }
        }

        ai.state_time_s += dtSeconds;

        // Stuck detection.
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

        // Turret scans while no target is detected.
        if (!(det.target_detected && det.current_target != entt::null)) {
            const double scanSpeedRad_s =
                settings.turret.scan_speed_deg_s * (kPi / 180.0);

            const double arcRad =
                settings.turret.scan_arc_deg * (kPi / 180.0);

            ai.scan_phase += dtSeconds * scanSpeedRad_s;

            const double offset = std::sin(ai.scan_phase) * (arcRad * 0.5);

            ctrl.desired_turret_heading_rad =
                WrapAngleRad(xf.hull_heading_rad + offset);
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
                // Priority 1: investigate last known enemy position.
                if (det.memory_remaining_s > 0.0) {
                    ai.search_waypoint_m = det.last_known_target_pos_m;
                }
                // Priority 2: tactical forward sweep.
                else {
                    const double sectorSize_m =
                        std::clamp(
                            settings.search.max_waypoint_distance_m * 0.45,
                            800.0,
                            1800.0);

                    ai.search_waypoint_m = TacticalSearchWaypoint(
                        xf.position_m,
                        xf.hull_heading_rad,
                        ai.waypoint_generation++,
                        sectorSize_m,
                        halfExtent);
                }

                ai.search_waypoint_m = ClampToWorld(ai.search_waypoint_m, halfExtent);
                ai.has_waypoint = true;
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

                // Do not waste time turning around to a bad waypoint.
                // Human-like search should continue mostly forward.
                if (WaypointIsBehindTank(
                        xf.position_m,
                        xf.hull_heading_rad,
                        ai.search_waypoint_m)) {
                    ai.has_waypoint = false;
                    ai.state = TankAIState::SearchSelectWaypoint;
                    ai.state_time_s = 0.0;
                    break;
                }

                const glm::dvec2 toWp = ai.search_waypoint_m - xf.position_m;
                const double dist = glm::length(toWp);

                if (dist < settings.search.waypoint_reached_radius_m) {
                    ai.has_waypoint = false;
                    ai.state = TankAIState::SearchSelectWaypoint;
                    ai.state_time_s = 0.0;
                    ctrl.desired_speed_mps = 0.0;
                    break;
                }

                if (ai.stuck_time_s > settings.search.stuck_timeout_s) {
                    ai.has_waypoint = false;
                    ai.stuck_time_s = 0.0;
                    ai.state = TankAIState::SearchSelectWaypoint;
                    ai.state_time_s = 0.0;
                    ctrl.desired_speed_mps = 0.0;
                    break;
                }

                const double slowDist_m = 700.0;
                const double speedFrac =
                    std::clamp(dist / slowDist_m, 0.25, 1.0);

                const double cruise =
                    std::min(veh.max_speed_mps, settings.movement.default_max_speed_mps);

                ctrl.desired_hull_heading_rad =
                    AngleTo(xf.position_m, ai.search_waypoint_m);

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

                if (det.current_target == entt::null ||
                    !registry.valid(det.current_target) ||
                    !registry.all_of<Transform>(det.current_target)) {
                    ai.state = TankAIState::SearchSelectWaypoint;
                    ai.state_time_s = 0.0;
                    det.target_detected = false;
                    break;
                }

                const auto& txf = registry.get<const Transform>(det.current_target);
                const double bearing = AngleTo(xf.position_m, txf.position_m);

                ctrl.desired_turret_heading_rad = bearing;
                ctrl.desired_hull_heading_rad = bearing;

                if (!det.target_has_los) {
                    ai.state = TankAIState::Reposition;
                    ai.state_time_s = 0.0;
                }

                break;
            }

            case TankAIState::Reposition: {
                const glm::dvec2 lastKnown =
                    ClampToWorld(det.last_known_target_pos_m, halfExtent);

                ctrl.desired_hull_heading_rad =
                    AngleTo(xf.position_m, lastKnown);

                ctrl.desired_speed_mps =
                    std::min(veh.max_speed_mps, 10.0);

                if (ai.state_time_s > 8.0 || det.target_detected) {
                    ai.state = TankAIState::SearchSelectWaypoint;
                    ai.state_time_s = 0.0;
                }

                break;
            }

            case TankAIState::Disabled:
            case TankAIState::Destroyed: {
                ctrl.desired_speed_mps = 0.0;
                break;
            }

            default:
                break;
        }

        (void)sensor;
        (void)tank;
        (void)terrain;
    }
}

} // namespace clf::sim::ai