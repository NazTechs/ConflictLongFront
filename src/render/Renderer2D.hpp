#pragma once

struct SDL_Renderer;

#include <entt/entity/entity.hpp>

#include "render/DebugOverlayRenderer.hpp"
#include "render/TerrainRenderer.hpp"
#include "render/ViewMode.hpp"

namespace clf::core {
class Camera2D;
}

namespace clf::sim {
class World;
}

namespace clf::render {

class Renderer2D final {
public:
    explicit Renderer2D(SDL_Renderer* renderer);

    struct Options final {
        entt::entity selectedEntity = entt::null;
        ViewMode viewMode = ViewMode::Spectator;
        entt::entity viewerEntity = entt::null; // for realistic visibility filtering
        bool showOnlyDetectedUnits = true;
        bool showAllUnitsInDebugView = true;
        TerrainRenderOptions terrain;
        DebugOverlayOptions overlay;
    };

    void Render(const sim::World& world, const core::Camera2D& camera, int viewportW, int viewportH, const Options& options);

private:
    SDL_Renderer* m_renderer = nullptr;
    TerrainRenderer m_terrainRenderer;
    DebugOverlayRenderer m_debugOverlayRenderer;
};

} // namespace clf::render
