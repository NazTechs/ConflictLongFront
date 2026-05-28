#include "render/FogOfWarRenderer.hpp"

#include <algorithm>
#include <cmath>

#include <SDL3/SDL.h>

#include "core/Camera2D.hpp"
#include "sim/terrain/Terrain.hpp"
#include "sim/visibility/VisibilityMask.hpp"

namespace clf::render {

FogOfWarRenderer::FogOfWarRenderer(SDL_Renderer* renderer)
    : m_renderer(renderer)
{
}

FogOfWarRenderer::~FogOfWarRenderer()
{
    DestroyTexture();
}

void FogOfWarRenderer::DestroyTexture()
{
    if (m_texture) {
        SDL_DestroyTexture(m_texture);
        m_texture = nullptr;
    }
    m_texW = 0;
    m_texH = 0;
}

void FogOfWarRenderer::EnsureTexture(int w, int h)
{
    if (m_texture && w == m_texW && h == m_texH) {
        return;
    }
    DestroyTexture();
    if (!m_renderer || w <= 0 || h <= 0) {
        return;
    }

    // Use SDL_PIXELFORMAT_RGBA32 so we can write pixels as SDL_Color (r,g,b,a bytes).
    m_texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, w, h);
    if (m_texture) {
        SDL_SetTextureBlendMode(m_texture, SDL_BLENDMODE_BLEND);
        m_texW = w;
        m_texH = h;
    }
}

void FogOfWarRenderer::Render(sim::visibility::VisibilityMask& mask,
                             const core::Camera2D& camera,
                             int viewportW,
                             int viewportH,
                             const FogOfWarRenderOptions& options)
{
    if (!m_renderer || !options.enabled) {
        return;
    }
    if (mask.Width() <= 0 || mask.Height() <= 0) {
        return;
    }

    EnsureTexture(mask.Width(), mask.Height());
    if (!m_texture) {
        return;
    }

    if (mask.Dirty()) {
        void* pixels = nullptr;
        int pitch = 0;
        if (SDL_LockTexture(m_texture, nullptr, &pixels, &pitch) == 0) {
            auto* row = static_cast<std::uint8_t*>(pixels);
            const auto& cells = mask.Cells();

            const std::uint8_t aUnknown = static_cast<std::uint8_t>(std::clamp(options.unknown_alpha, 0.0f, 1.0f) * 255.0f);
            const std::uint8_t aKnown = static_cast<std::uint8_t>(std::clamp(options.known_alpha, 0.0f, 1.0f) * 255.0f);

            for (int y = 0; y < mask.Height(); ++y) {
                auto* dst = reinterpret_cast<std::uint8_t*>(row);
                for (int x = 0; x < mask.Width(); ++x) {
                    const auto v = cells[static_cast<std::size_t>(y) * static_cast<std::size_t>(mask.Width()) + static_cast<std::size_t>(x)];
                    std::uint8_t a = 0;
                    if (v == sim::visibility::CellVis::Unknown) {
                        a = aUnknown;
                    } else if (v == sim::visibility::CellVis::Known) {
                        a = aKnown;
                    } else {
                        a = 0;
                    }

                    // SDL_PIXELFORMAT_RGBA32: 4 bytes per pixel: r,g,b,a
                    const std::size_t off = static_cast<std::size_t>(x) * 4u;
                    dst[off + 0] = 0;
                    dst[off + 1] = 0;
                    dst[off + 2] = 0;
                    dst[off + 3] = a;
                }
                row += pitch;
            }
            SDL_UnlockTexture(m_texture);
            mask.ClearDirty();
        }
    }

    const double halfWorld = sim::terrain::Terrain::kWorldSizeMeters * 0.5;
    const glm::vec2 minPx = camera.WorldToScreenPx(glm::dvec2(-halfWorld, -halfWorld), viewportW, viewportH);
    const glm::vec2 maxPx = camera.WorldToScreenPx(glm::dvec2(+halfWorld, +halfWorld), viewportW, viewportH);

    SDL_FRect dst{};
    dst.x = std::min(minPx.x, maxPx.x);
    dst.y = std::min(minPx.y, maxPx.y);
    dst.w = std::abs(maxPx.x - minPx.x);
    dst.h = std::abs(maxPx.y - minPx.y);

    SDL_RenderTexture(m_renderer, m_texture, nullptr, &dst);
}

} // namespace clf::render
