#pragma once

#include <glm/vec2.hpp>

namespace clf::sim::terrain {
class Terrain;
}

namespace clf::sim {

struct LosResult final {
    bool visible = true;

    // If blocked, the approximate world-space point where the terrain rises above the LOS ray.
    glm::dvec2 blocked_point_m{0.0, 0.0};
    double blocked_terrain_height_m = 0.0;
    double blocked_ray_height_m = 0.0;

    int samples_tested = 0;
};

// Terrain-aware line-of-sight test between two XY points.
//
// Math:
// - Let z0 = terrain(observer) + observerHeight, z1 = terrain(target) + targetHeight
// - For t in (0,1), sample terrain at p(t) = lerp(observer, target, t)
// - Compare terrainHeight(p(t)) to zRay(t) = z0 + (z1 - z0) * t
// - If terrainHeight > zRay (within epsilon), LOS is blocked at that sample.
bool hasLineOfSight(const terrain::Terrain& terrain,
                    glm::dvec2 observerXY_m,
                    double observerHeightAboveGround_m,
                    glm::dvec2 targetXY_m,
                    double targetHeightAboveGround_m,
                    LosResult* outResult);

} // namespace clf::sim

