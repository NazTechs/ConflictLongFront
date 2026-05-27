#pragma once

#include <vector>

#include <entt/entity/registry.hpp>

namespace clf::sim {
struct DebugLine;
struct LosResult;
}

namespace clf::sim::terrain {
class Terrain;
}

namespace clf::sim::weapon {

void UpdateDirectFire(entt::registry& registry,
                      const terrain::Terrain& terrain,
                      double dtSeconds,
                      std::vector<DebugLine>& inOutDebugLines);

} // namespace clf::sim::weapon

