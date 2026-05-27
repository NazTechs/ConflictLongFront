#pragma once

#include <cstdint>

struct SDL_Renderer;
struct SDL_Texture;

namespace clf::core {
class Camera2D;
}

namespace clf::sim::visibility {
class VisibilityMask;
}

namespace clf::render {

struct FogOfWarRenderOptions final {
    bool enabled = false;
    float unknown_alpha = 0.85f; // darken unknown
    float known_alpha = 0.55f;   // darken known-but-not-visible
};

class FogOfWarRenderer final {
public:
    explicit FogOfWarRenderer(SDL_Renderer* renderer);
    ~FogOfWarRenderer();

    FogOfWarRenderer(const FogOfWarRenderer&) = delete;
    FogOfWarRenderer& operator=(const FogOfWarRenderer&) = delete;

    void Render(sim::visibility::VisibilityMask& mask,
                const core::Camera2D& camera,
                int viewportW,
                int viewportH,
                const FogOfWarRenderOptions& options);

private:
    SDL_Renderer* m_renderer = nullptr;
    SDL_Texture* m_texture = nullptr;
    int m_texW = 0;
    int m_texH = 0;

    void EnsureTexture(int w, int h);
    void DestroyTexture();
};

} // namespace clf::render
