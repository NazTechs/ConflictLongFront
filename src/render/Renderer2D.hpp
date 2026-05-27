#pragma once

struct SDL_Renderer;

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

    void Render(const sim::World& world, const core::Camera2D& camera, int viewportW, int viewportH);

private:
    SDL_Renderer* m_renderer = nullptr;
};

} // namespace clf::render

