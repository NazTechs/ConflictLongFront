#include "sim/visibility/FogOfWarSystem.hpp"

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

// Faster LOS approximation for fog-of-war: fewer samples than the precise LOS used for targeting.
bool HasLosFast(const terrain::Terrain& terrain,
                const glm::dvec2& aXY,
                double aHeightAGL_m,
                const glm::dvec2& bXY,
                double bHeightAGL_m)
{
    const glm::dvec2 delta = bXY - aXY;
    const double dist_m = glm::length(delta);
    if (dist_m <= 1e-6) {
        return true;
    }

    const double z0_m = terrain.HeightAtXY(aXY) + aHeightAGL_m;
    const double z1_m = terrain.HeightAtXY(bXY) + bHeightAGL_m;

    const double step_m = std::clamp(terrain.CellSizeMeters() * 2.0, 50.0, 200.0);
    const int steps = std::min(96, std::max(2, static_cast<int>(std::ceil(dist_m / step_m))));

    constexpr double epsilon_m = 0.05;
    for (int i = 1; i < steps; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(steps);
        const glm::dvec2 p = aXY + delta * t;
        const double terrainZ_m = terrain.HeightAtXY(p);
        const double rayZ_m = z0_m + (z1_m - z0_m) * t;
        if (terrainZ_m > rayZ_m - epsilon_m) {
            return false;
        }
    }

    return true;
}

} // namespace

void FogOfWarSystem::Configure(const FogOfWarParams& p)
{
    m_params = p;
    if (m_params.grid_resolution_m < 10.0) {
        m_params.grid_resolution_m = 10.0;
    }
    if (m_params.update_rate_hz < 0.5) {
        m_params.update_rate_hz = 0.5;
    }
}

void FogOfWarSystem::Reset()
{
    m_accum_s = 0.0;
    m_lastViewer = entt::null;
    m_stats = FogOfWarStats{};
    m_mask.ClearToUnknown();
}

void FogOfWarSystem::EnsureMaskSized(double worldSizeMeters)
{
    const double cell = std::max(10.0, m_params.grid_resolution_m);
    const int w = std::max(1, static_cast<int>(std::ceil(worldSizeMeters / cell)));
    const int h = w;
    if (m_mask.Width() != w || m_mask.Height() != h || std::abs(m_mask.CellSizeMeters() - cell) > 1e-6) {
        m_mask.Resize(w, h, cell);
        m_mask.ClearToUnknown();
    }
}

void FogOfWarSystem::Update(entt::registry& registry, const terrain::Terrain& terrain, entt::entity viewer, double dtSeconds)
{
    m_stats = FogOfWarStats{};

    if (viewer == entt::null || !registry.valid(viewer)) {
        m_lastViewer = entt::null;
        return;
    }

    if (!registry.all_of<Tank, Transform, Sensor>(viewer)) {
        m_lastViewer = entt::null;
        return;
    }

    EnsureMaskSized(terrain::Terrain::kWorldSizeMeters);

    // If the viewer changes, refresh sooner.
    if (viewer != m_lastViewer) {
        m_accum_s = 1e9;
        m_lastViewer = viewer;
    }

    const double updatePeriod_s = 1.0 / std::max(0.5, m_params.update_rate_hz);
    m_accum_s += dtSeconds;
    if (m_accum_s < updatePeriod_s) {
        return;
    }
    m_accum_s = 0.0;

    const auto& tank = registry.get<const Tank>(viewer);
    const auto& xf = registry.get<const Transform>(viewer);
    const auto& sensor = registry.get<const Sensor>(viewer);

    const double sensorRange_m = std::max(0.0, sensor.range_m);
    if (sensorRange_m <= 1.0) {
        return;
    }

    double sensorHeading_rad = xf.hull_heading_rad;
    if (registry.all_of<Vehicle>(viewer)) {
        sensorHeading_rad = registry.get<const Vehicle>(viewer).turret_heading_rad;
    }

    const double halfFov = sensor.fov_rad * 0.5;

    const double cell = m_mask.CellSizeMeters();
    const int w = m_mask.Width();
    const int h = m_mask.Height();

    const double halfWorld = terrain::Terrain::kWorldSizeMeters * 0.5;
    const glm::dvec2 worldMin{-halfWorld, -halfWorld};

    // Convert world XY to fog cell index.
    auto toIndex = [&](const glm::dvec2& p) -> glm::ivec2 {
        const glm::dvec2 rel = p - worldMin;
        return glm::ivec2(static_cast<int>(std::floor(rel.x / cell)), static_cast<int>(std::floor(rel.y / cell)));
    };

    const glm::ivec2 minI = glm::clamp(toIndex(xf.position_m - glm::dvec2(sensorRange_m, sensorRange_m)), glm::ivec2(0, 0), glm::ivec2(w - 1, h - 1));
    const glm::ivec2 maxI = glm::clamp(toIndex(xf.position_m + glm::dvec2(sensorRange_m, sensorRange_m)), glm::ivec2(0, 0), glm::ivec2(w - 1, h - 1));

    m_mask.DecayVisibleToKnown();

    for (int y = minI.y; y <= maxI.y; ++y) {
        for (int x = minI.x; x <= maxI.x; ++x) {
            const glm::dvec2 cellCenter = worldMin + glm::dvec2((static_cast<double>(x) + 0.5) * cell, (static_cast<double>(y) + 0.5) * cell);
            const glm::dvec2 d = cellCenter - xf.position_m;
            const double dist = glm::length(d);
            if (dist > sensorRange_m) {
                continue;
            }

            const double ang = std::atan2(d.y, d.x);
            const double da = WrapAngleRad(ang - sensorHeading_rad);
            if (std::abs(da) > halfFov) {
                continue;
            }

            ++m_stats.cells_tested;
            if (HasLosFast(terrain, xf.position_m, tank.sensor_height_m, cellCenter, 0.0)) {
                m_mask.Set(x, y, CellVis::Visible);
                ++m_stats.cells_visible;
            }
        }
    }
}

} // namespace clf::sim::visibility
