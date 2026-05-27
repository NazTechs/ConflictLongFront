#include "sim/LineOfSight.hpp"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

#include "sim/terrain/Terrain.hpp"

namespace clf::sim {

bool hasLineOfSight(const terrain::Terrain& terrain,
                    glm::dvec2 observerXY_m,
                    double observerHeightAboveGround_m,
                    glm::dvec2 targetXY_m,
                    double targetHeightAboveGround_m,
                    LosResult* outResult)
{
    LosResult local{};

    const glm::dvec2 delta = targetXY_m - observerXY_m;
    const double dist_m = glm::length(delta);
    if (dist_m <= 1e-6) {
        if (outResult) {
            *outResult = local;
        }
        return true;
    }

    const double z0_m = terrain.HeightAtXY(observerXY_m) + observerHeightAboveGround_m;
    const double z1_m = terrain.HeightAtXY(targetXY_m) + targetHeightAboveGround_m;

    // Sampling step in meters. Tied to terrain resolution, but clamped so short rays still get enough samples.
    const double step_m = std::clamp(terrain.CellSizeMeters() * 0.5, 5.0, 50.0);
    const int steps = std::max(2, static_cast<int>(std::ceil(dist_m / step_m)));

    constexpr double epsilon_m = 0.05;

    for (int i = 1; i < steps; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(steps);
        const glm::dvec2 p = observerXY_m + delta * t;

        const double terrainZ_m = terrain.HeightAtXY(p);
        const double rayZ_m = z0_m + (z1_m - z0_m) * t;

        local.samples_tested++;

        if (terrainZ_m > rayZ_m - epsilon_m) {
            local.visible = false;
            local.blocked_point_m = p;
            local.blocked_terrain_height_m = terrainZ_m;
            local.blocked_ray_height_m = rayZ_m;
            break;
        }
    }

    if (outResult) {
        *outResult = local;
    }
    return local.visible;
}

} // namespace clf::sim

