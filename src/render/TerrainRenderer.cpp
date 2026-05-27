#include "render/TerrainRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include <SDL3/SDL.h>

#include "core/Camera2D.hpp"
#include "sim/terrain/Terrain.hpp"

namespace clf::render {

namespace {

std::uint8_t LerpU8(std::uint8_t a, std::uint8_t b, double t)
{
    const double v = static_cast<double>(a) + (static_cast<double>(b) - static_cast<double>(a)) * t;
    return static_cast<std::uint8_t>(std::clamp(v, 0.0, 255.0));
}

struct Rgba final {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;
};

Rgba LerpColor(const Rgba& a, const Rgba& b, double t)
{
    return Rgba{
        .r = LerpU8(a.r, b.r, t),
        .g = LerpU8(a.g, b.g, t),
        .b = LerpU8(a.b, b.b, t),
        .a = 255,
    };
}

Rgba HeightToColor(double height_m, const clf::sim::terrain::TerrainStats& stats, bool contourBands)
{
    const double span = std::max(1.0, stats.maxHeight_m - stats.minHeight_m);
    double t = (height_m - stats.minHeight_m) / span;
    t = std::clamp(t, 0.0, 1.0);

    if (contourBands) {
        // Quantize to produce contour-style bands.
        constexpr double bands = 14.0;
        t = std::floor(t * bands) / bands;
    }

    // 4-stop gradient: deep lowlands -> green -> brown -> snow.
    const Rgba c0{20, 40, 90, 255};
    const Rgba c1{40, 120, 60, 255};
    const Rgba c2{140, 110, 70, 255};
    const Rgba c3{235, 235, 235, 255};

    if (t < 0.40) {
        return LerpColor(c0, c1, t / 0.40);
    }
    if (t < 0.75) {
        return LerpColor(c1, c2, (t - 0.40) / 0.35);
    }
    return LerpColor(c2, c3, (t - 0.75) / 0.25);
}

} // namespace

TerrainRenderer::TerrainRenderer(SDL_Renderer* renderer)
    : m_renderer(renderer)
{
}

TerrainRenderer::~TerrainRenderer()
{
    DestroyTexture();
}

void TerrainRenderer::Render(const sim::terrain::Terrain& terrain,
                             const core::Camera2D& camera,
                             int viewportW,
                             int viewportH,
                             const TerrainRenderOptions& options)
{
    if (!options.showHeightOverlay) {
        return;
    }

    EnsureTexture(terrain, options);
    if (!m_texture) {
        return;
    }

    const glm::vec2 mapCenterPx = camera.WorldToScreenPx(glm::dvec2(0.0, 0.0), viewportW, viewportH);
    const float ppm = static_cast<float>(camera.ZoomPixelsPerMeter());
    const float worldSizePx = static_cast<float>(sim::terrain::Terrain::kWorldSizeMeters) * ppm;

    SDL_FRect dst{
        mapCenterPx.x - worldSizePx * 0.5f,
        mapCenterPx.y - worldSizePx * 0.5f,
        worldSizePx,
        worldSizePx,
    };

    SDL_RenderTexture(m_renderer, m_texture, nullptr, &dst);
}

void TerrainRenderer::EnsureTexture(const sim::terrain::Terrain& terrain, const TerrainRenderOptions& options)
{
    const int desired = terrain.SamplesPerSide();

    const bool needNew = (m_texture == nullptr) ||
                         (m_texW != desired) ||
                         (m_texH != desired) ||
                         (m_lastContourBands != options.contourBands);

    if (needNew) {
        RebuildTexture(terrain, options);
    }
}

void TerrainRenderer::RebuildTexture(const sim::terrain::Terrain& terrain, const TerrainRenderOptions& options)
{
    DestroyTexture();

    m_texW = terrain.SamplesPerSide();
    m_texH = terrain.SamplesPerSide();
    m_lastContourBands = options.contourBands;

    SDL_Surface* surface = SDL_CreateSurface(m_texW, m_texH, SDL_PIXELFORMAT_RGBA32);
    if (!surface) {
        return;
    }

    const auto stats = terrain.Stats();
    const auto& h = terrain.HeightsMeters();

    std::uint8_t* pixels = static_cast<std::uint8_t*>(surface->pixels);
    const int pitch = surface->pitch;

    for (int y = 0; y < m_texH; ++y) {
        std::uint8_t* row = pixels + y * pitch;
        for (int x = 0; x < m_texW; ++x) {
            const std::size_t idx = static_cast<std::size_t>(y) * static_cast<std::size_t>(m_texW) + static_cast<std::size_t>(x);
            const double height_m = (idx < h.size()) ? h[idx] : 0.0;

            const Rgba c = HeightToColor(height_m, stats, options.contourBands);

            const int o = x * 4;
            row[o + 0] = c.r;
            row[o + 1] = c.g;
            row[o + 2] = c.b;
            row[o + 3] = c.a;
        }
    }

    m_texture = SDL_CreateTextureFromSurface(m_renderer, surface);
    SDL_DestroySurface(surface);

    if (m_texture) {
        SDL_SetTextureScaleMode(m_texture, SDL_SCALEMODE_LINEAR);
    }
}

void TerrainRenderer::DestroyTexture()
{
    if (m_texture) {
        SDL_DestroyTexture(m_texture);
        m_texture = nullptr;
    }
    m_texW = 0;
    m_texH = 0;
}

} // namespace clf::render
