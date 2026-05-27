#pragma once

#include <glm/vec2.hpp>
#include <entt/entity/registry.hpp>

namespace clf::sim::systems {

void IntegrateTanks(entt::registry& registry, double dtSeconds);
void BounceTanksInAabb(entt::registry& registry, const glm::dvec2& minMeters, const glm::dvec2& maxMeters);

} // namespace clf::sim::systems

