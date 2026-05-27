#pragma once

#include <entt/entity/registry.hpp>

namespace clf::sim::terrain {
class Terrain;
}

namespace clf::sim {

// Updates sensor detections and detection memory.
void UpdateDetection(entt::registry& registry, const terrain::Terrain& terrain, double dtSeconds);
void UpdateDetection(entt::registry& registry, const terrain::Terrain& terrain, double dtSeconds, double memoryTimeoutSeconds);

} // namespace clf::sim
