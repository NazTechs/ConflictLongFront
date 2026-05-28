#pragma once

#include <entt/entity/entity.hpp>

struct SDL_Renderer;

namespace clf::core {
class Camera3D;
}

namespace clf::sim {
class World;
namespace visibility {
class VisibilityMask;
}
}

namespace clf::render {

struct Renderer3DOptions final {
    bool showGrid = true;
    bool showWaypoints = true;
    bool showFog = true;
    int fogStrideCells = 4; // draw every Nth fog cell in each axis (performance)
    sim::visibility::VisibilityMask* fogMask = nullptr;
    float fogUnknownAlpha = 0.85f;
    float fogKnownAlpha = 0.55f;
};

class Renderer3D final {
public:
    explicit Renderer3D(SDL_Renderer* renderer);

    void Render(const sim::World& world,
                const core::Camera3D& camera,
                int viewportW,
                int viewportH,
                entt::entity selectedEntity,
                const Renderer3DOptions& options);

private:
    SDL_Renderer* m_renderer = nullptr;
};

} // namespace clf::render
