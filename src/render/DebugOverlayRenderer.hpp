#pragma once

#include <cstdint>

#include <entt/entity/entity.hpp>

struct SDL_Renderer;

namespace clf::core {
class Camera2D;
}

namespace clf::sim {
class World;
}

namespace clf::render {

struct DebugOverlayOptions final {
    bool showWeaponRange = true;
    bool showVisualRange = true;
    bool showLosRay = true;
    bool showBlockedPoint = true;
    bool showShotTraces = true;
    bool highlightSelection = true;
};

class DebugOverlayRenderer final {
public:
    explicit DebugOverlayRenderer(SDL_Renderer* renderer);

    void Render(const sim::World& world,
                const core::Camera2D& camera,
                int viewportW,
                int viewportH,
                entt::entity selectedEntity,
                const DebugOverlayOptions& options);

private:
    SDL_Renderer* m_renderer = nullptr;
};

} // namespace clf::render

