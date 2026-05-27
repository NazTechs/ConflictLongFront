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
    bool showSensorCone = true;
    bool showBlockedPoint = true;
    bool showShotTraces = true;
    bool highlightSelection = true;
    bool showDamageZones = false;
    bool showSearchWaypoints = true;
    bool showMovementVectors = true;
    bool showAiState = true;
    bool showDetectionDebug = false;
    bool showCollisionBounds = false;
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
