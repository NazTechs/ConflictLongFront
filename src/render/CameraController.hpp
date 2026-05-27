#pragma once

#include <entt/entity/entity.hpp>

#include "render/ViewMode.hpp"

namespace clf::core {
class Camera2D;
}

namespace clf::sim {
class World;
}

namespace clf::render {

class CameraController final {
public:
    void SetViewMode(ViewMode mode);
    ViewMode GetViewMode() const;

    // Updates camera position based on selected view mode (unit-follow, debug, etc.).
    void ApplyViewMode(core::Camera2D& camera,
                       const sim::World& world,
                       entt::entity selectedEntity);

private:
    ViewMode m_mode = ViewMode::Spectator;
};

} // namespace clf::render

