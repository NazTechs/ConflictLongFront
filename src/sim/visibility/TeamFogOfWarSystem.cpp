#include "sim/visibility/TeamFogOfWarSystem.hpp"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

#include "sim/LineOfSight.hpp"
#include "sim/components/SensorComponent.hpp"
#include "sim/components/TankComponent.hpp"
#include "sim/components/TransformComponent.hpp"
#include "sim/components/VehicleComponent.hpp"
#include "sim/terrain/Terrain.hpp"

namespace clf::sim::visibility {

namespace {

double WrapAngleRad(double a)
{
    while (a > 3.141592653589793) a -= 2.0 * 3.141592653589793;
    while (a < -3.141592653589793) a += 2.0 * 3.141592653589793;
    return a;
}

} // namespace

void TeamFogOfWarSystem::Configure(const TeamFogParams& p)
{
    m_params = p;
    if (m_params.grid_resolution_m < 10.0) m_params.grid_resolution_m = 10.0;
    if (m_params.update_rate_hz < 0.5) m_params.update_rate_hz = 0.5;
    if (m_params.max_teams < 1) m_params.max_teams = 1;
}

void TeamFogOfWarSystem::Reset()
{
    m_accum_s = 0.0;
    for (auto& m : m_teamMasks) {
        m.ClearToUnknown();
    }
}

void TeamFogOfWarSystem::EnsureMasksSized(double worldSizeMeters)
{
    const double cell = std::max(10.0, m_params.grid_resolution_m);
    const int w = std::max(1, static_cast<int>(std::ceil(worldSizeMeters / cell)));
    const int h = w;

    if (static_cast<int>(m_teamMasks.size()) != m_params.max_teams) {
        m_teamMasks.assign(static_cast<std::size_t>(m_params.max_teams), VisibilityMask{});
    }

    for (auto& mask : m_teamMasks) {
        if (mask.Width() != w || mask.Height() != h || std::abs(mask.CellSizeMeters() - cell) > 1e-6) {
            mask.Resize(w, h, cell);
            mask.ClearToUnknown();
        }
    }
}

VisibilityMask& TeamFogOfWarSystem::MaskForTeam(int teamId)
{
    teamId = std::clamp(teamId, 0, std::max(0, m_params.max_teams - 1));
    if (m_teamMasks.empty()) {
        m_teamMasks.assign(static_cast<std::size_t>(std::max(1, m_params.max_teams)), VisibilityMask{});
    }
    return m_teamMasks[static_cast<std::size_t>(teamId)];
}

const VisibilityMask& TeamFogOfWarSystem::MaskForTeam(int teamId) const
{
    teamId = std::clamp(teamId, 0, std::max(0, m_params.max_teams - 1));
    return m_teamMasks[static_cast<std::size_t>(teamId)];
}

void TeamFogOfWarSystem::Update(entt::registry& registry, const terrain::Terrain& terrain, double dtSeconds)
{
    EnsureMasksSized(terrain::Terrain::kWorldSizeMeters);

    const double updatePeriod_s = 1.0 / std::max(0.5, m_params.update_rate_hz);
    m_accum_s += dtSeconds;
    if (m_accum_s < updatePeriod_s) {
        return;
    }
    m_accum_s = 0.0;

    for (auto& mask : m_teamMasks) {
        mask.DecayVisibleToKnown();
    }

    const double halfWorld = terrain::Terrain::kWorldSizeMeters * 0.5;
    const glm::dvec2 worldMin{-halfWorld, -halfWorld};

    const int w = m_teamMasks[0].Width();
    const int h = m_teamMasks[0].Height();
    const double cell = m_teamMasks[0].CellSizeMeters();

    auto toIndex = [&](const glm::dvec2& p) -> glm::ivec2 {
        const glm::dvec2 rel = p - worldMin;
        return glm::ivec2(static_cast<int>(std::floor(rel.x / cell)), static_cast<int>(std::floor(rel.y / cell)));
    };

    auto tanks = registry.view<const Tank, const Transform, const Sensor>();
    for (const auto e : tanks) {
        const auto& tank = tanks.get<const Tank>(e);
        const auto& xf = tanks.get<const Transform>(e);
        const auto& sensor = tanks.get<const Sensor>(e);

        if (tank.team_id < 0 || tank.team_id >= m_params.max_teams) {
            continue;
        }

        double heading_rad = xf.hull_heading_rad;
        if (registry.all_of<Vehicle>(e)) {
            heading_rad = registry.get<const Vehicle>(e).turret_heading_rad;
        }

        const double sensorRange_m = std::max(0.0, sensor.range_m);
        if (sensorRange_m <= 1.0) {
            continue;
        }

        const double halfFov = sensor.fov_rad * 0.5;

        const glm::ivec2 minI = glm::clamp(toIndex(xf.position_m - glm::dvec2(sensorRange_m, sensorRange_m)), glm::ivec2(0, 0), glm::ivec2(w - 1, h - 1));
        const glm::ivec2 maxI = glm::clamp(toIndex(xf.position_m + glm::dvec2(sensorRange_m, sensorRange_m)), glm::ivec2(0, 0), glm::ivec2(w - 1, h - 1));

        auto& mask = MaskForTeam(tank.team_id);
        for (int y = minI.y; y <= maxI.y; ++y) {
            for (int x = minI.x; x <= maxI.x; ++x) {
                const glm::dvec2 cellCenter = worldMin + glm::dvec2((static_cast<double>(x) + 0.5) * cell, (static_cast<double>(y) + 0.5) * cell);
                const glm::dvec2 d = cellCenter - xf.position_m;
                const double dist = glm::length(d);
                if (dist > sensorRange_m) {
                    continue;
                }
                const double ang = std::atan2(d.y, d.x);
                const double da = WrapAngleRad(ang - heading_rad);
                if (std::abs(da) > halfFov) {
                    continue;
                }
                if (hasLineOfSight(terrain, xf.position_m, tank.sensor_height_m, cellCenter, 0.0, nullptr)) {
                    mask.Set(x, y, CellVis::Visible);
                }
            }
        }
    }
}

} // namespace clf::sim::visibility

