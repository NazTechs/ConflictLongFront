#pragma once

#include <entt/entity/entity.hpp>

namespace clf::sim {
class World;
}

namespace clf::ui {

class SelectedUnitPanel final {
public:
    void Render(entt::entity selectedEntity, sim::World& world);
};

} // namespace clf::ui
