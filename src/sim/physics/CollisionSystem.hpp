#pragma once

#include <entt/entity/registry.hpp>

namespace clf::sim::physics {

void ResolveCollisions(entt::registry& registry);
void ClampToMap(entt::registry& registry, double halfExtentMeters);

} // namespace clf::sim::physics

