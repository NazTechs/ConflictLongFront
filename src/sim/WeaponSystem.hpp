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

namespace clf::sim::weapons {
class CombatLog;
}

namespace clf::sim::weapon {

void UpdateDirectFire(entt::registry& registry,
                      const terrain::Terrain& terrain,
                      double dtSeconds,
                      double simTimeSeconds,
                      weapons::CombatLog* combatLog,
                      std::vector<DebugLine>& inOutDebugLines);

} // namespace clf::sim::weapon
