#pragma once

#include <cstdint>

#include <entt/entity/registry.hpp>

namespace clf::sim::terrain {
class Terrain;
}

namespace clf::sim::ai {

void UpdateTankAI(entt::registry& registry, const terrain::Terrain& terrain, double dtSeconds, std::uint32_t seed);

} // namespace clf::sim::ai

