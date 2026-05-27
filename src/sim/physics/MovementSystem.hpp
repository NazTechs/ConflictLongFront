#pragma once

#include <entt/entity/registry.hpp>

namespace clf::sim::physics {

void UpdateMovement(entt::registry& registry, double dtSeconds);

} // namespace clf::sim::physics

