#include "sim/DetectionSystem.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/geometric.hpp>

#include "sim/Components.hpp"
#include "sim/LineOfSight.hpp"
#include "sim/components/DetectionComponent.hpp"
#include "sim/components/SensorComponent.hpp"
#include "sim/components/TransformComponent.hpp"
#include "sim/components/VehicleComponent.hpp"
#include "sim/damage/DamageComponent.hpp"
#include "sim/terrain/Terrain.hpp"

namespace clf::sim {

namespace {

double WrapAngleRad(double a)
{
    while (a > 3.141592653589793) a -= 2.0 * 3.141592653589793;
    while (a < -3.141592653589793) a += 2.0 * 3.141592653589793;
    return a;
}

double AngleToTarget(const glm::dvec2& from, const glm::dvec2& to)
{
    const glm::dvec2 d = to - from;
    return std::atan2(d.y, d.x);
}

double RequiredDetectTime(const Sensor& s, double dist_m)
{
    const double near = std::max(1.0, s.detect_near_m);
    const double far = std::max(near + 1.0, s.range_m);
    const double t = std::clamp((dist_m - near) / (far - near), 0.0, 1.0);
    return s.detect_time_near_s + (s.detect_time_far_s - s.detect_time_near_s) * t;
}

} // namespace

void UpdateDetection(entt::registry& registry, const terrain::Terrain& terrain, double dtSeconds)
{
    auto sensors = registry.view<const Tank, const Transform, const Vehicle, const Sensor, Detection>();
    auto targets = registry.view<const Tank, const Transform>();

    for (const auto e : sensors) {
        const auto& tank = sensors.get<const Tank>(e);
        const auto& xf = sensors.get<const Transform>(e);
        const auto& veh = sensors.get<const Vehicle>(e);
        const auto& sensor = sensors.get<const Sensor>(e);
        auto& det = sensors.get<Detection>(e);

        entt::entity best = entt::null;
        double bestDist2 = std::numeric_limits<double>::infinity();

        for (const auto cand : targets) {
            if (cand == e) continue;
            const auto& t2 = targets.get<const Tank>(cand);
            if (t2.team_id == tank.team_id) continue;

            const auto& xf2 = targets.get<const Transform>(cand);
            const glm::dvec2 d = xf2.position_m - xf.position_m;
            const double dist2 = glm::dot(d, d);
            if (dist2 < bestDist2) {
                best = cand;
                bestDist2 = dist2;
            }
        }

        // Defaults.
        det.current_target = best;
        det.target_detected = false;
        det.target_distance_m = 0.0;
        det.target_in_sensor_range = false;
        det.target_in_fov = false;
        det.target_has_los = false;

        bool canSenseNow = false;

        if (best != entt::null) {
            const auto& targetXf = targets.get<const Transform>(best);
            const double dist_m = std::sqrt(std::max(0.0, bestDist2));
            det.target_distance_m = dist_m;

            double range_m = sensor.range_m;
            if (const auto* dmg = registry.try_get<damage::DamageState>(e)) {
                if (dmg->optics >= damage::SubsystemState::Damaged) {
                    range_m *= 0.75;
                }
                if (dmg->optics >= damage::SubsystemState::Disabled) {
                    range_m *= 0.35;
                }
            }

            det.target_in_sensor_range = dist_m <= range_m;

            const double bearing = AngleToTarget(xf.position_m, targetXf.position_m);
            const double turret = veh.turret_heading_rad;
            const double delta = std::abs(WrapAngleRad(bearing - turret));
            det.target_in_fov = delta <= (sensor.fov_rad * 0.5);

            LosResult los{};
            const bool hasLos = hasLineOfSight(
                terrain,
                xf.position_m,
                tank.sensor_height_m,
                targetXf.position_m,
                // Use target sensor height as a proxy for target “visible height” for now.
                targets.get<const Tank>(best).sensor_height_m,
                &los);
            det.target_has_los = hasLos;

            canSenseNow = det.target_in_sensor_range && det.target_in_fov && det.target_has_los;
            det.required_detect_s = RequiredDetectTime(sensor, dist_m);
            if (const auto* dmg = registry.try_get<damage::DamageState>(e)) {
                if (dmg->optics >= damage::SubsystemState::Damaged) {
                    det.required_detect_s *= 1.5;
                }
                if (dmg->optics >= damage::SubsystemState::Disabled) {
                    det.required_detect_s *= 2.5;
                }
            }
        } else {
            det.required_detect_s = 0.0;
        }

        if (canSenseNow) {
            det.detect_progress_s = std::min(det.required_detect_s, det.detect_progress_s + dtSeconds);
            if (det.detect_progress_s >= det.required_detect_s && det.current_target != entt::null) {
                det.target_detected = true;
                det.memory_remaining_s = 6.0;
                det.last_known_target_pos_m = registry.get<const Transform>(det.current_target).position_m;
            }
        } else {
            det.detect_progress_s = std::max(0.0, det.detect_progress_s - dtSeconds * 0.8);
            det.memory_remaining_s = std::max(0.0, det.memory_remaining_s - dtSeconds);
        }
    }
}

} // namespace clf::sim
