#pragma once

#include <cstdint>

struct SDL_Renderer;
struct SDL_Texture;

namespace clf::core {
class Camera2D;
}

namespace clf::sim {
class World;
}

namespace clf::sim::terrain {
class Terrain;
}

namespace clf::render {

struct TerrainRenderOptions final {
    bool showHeightOverlay = true;
    bool contourBands = true;
};

class TerrainRenderer final {
public:
    explicit TerrainRenderer(SDL_Renderer* renderer);
    ~TerrainRenderer();

    void Render(const sim::terrain::Terrain& terrain,
                const core::Camera2D& camera,
                int viewportW,
                int viewportH,
                const TerrainRenderOptions& options);

private:
    void EnsureTexture(const sim::terrain::Terrain& terrain, const TerrainRenderOptions& options);
    void RebuildTexture(const sim::terrain::Terrain& terrain, const TerrainRenderOptions& options);
    void DestroyTexture();

    SDL_Renderer* m_renderer = nullptr;
    SDL_Texture* m_texture = nullptr;

    int m_texW = 0;
    int m_texH = 0;
    bool m_lastContourBands = false;
};

} // namespace clf::render

