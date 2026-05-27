#pragma once

#include <entt/entity/registry.hpp>

namespace clf::sim::manual {

// Processes manual waypoint driving:
// - Only entities in ControlMode::Manual with a waypoint path are driven.
// - Writes desired speed/heading into VehicleControl; MovementSystem performs integration.
void UpdateManualMovement(entt::registry& registry, double dtSeconds);

} // namespace clf::sim::manual

