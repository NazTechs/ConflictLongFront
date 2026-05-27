#include "render/DebugOverlayRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include <SDL3/SDL.h>

#include "core/Camera2D.hpp"
#include "sim/Components.hpp"
#include "sim/World.hpp"

namespace clf::render {

namespace {

void SetDrawColor(SDL_Renderer* renderer, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
}

void DrawCircle(SDL_Renderer* renderer, const glm::vec2& centerPx, float radiusPx, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    if (radiusPx <= 0.5f) {
        SDL_RenderPoint(renderer, centerPx.x, centerPx.y);
        return;
    }

    const int segments = std::clamp(static_cast<int>(radiusPx * 0.25f), 24, 120);
    std::vector<SDL_FPoint> pts;
    pts.reserve(static_cast<std::size_t>(segments) + 1u);

    const float step = (2.0f * 3.14159265358979323846f) / static_cast<float>(segments);
    for (int i = 0; i <= segments; ++i) {
        const float ang = step * static_cast<float>(i);
        pts.push_back(SDL_FPoint{
            centerPx.x + std::cos(ang) * radiusPx,
            centerPx.y + std::sin(ang) * radiusPx,
        });
    }

    SetDrawColor(renderer, r, g, b, a);
    SDL_RenderLines(renderer, pts.data(), static_cast<int>(pts.size()));
}

void DrawCross(SDL_Renderer* renderer, const glm::vec2& pPx, float sizePx, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    sizePx = std::max(2.0f, sizePx);
    SetDrawColor(renderer, r, g, b, a);
    SDL_RenderLine(renderer, pPx.x - sizePx, pPx.y - sizePx, pPx.x + sizePx, pPx.y + sizePx);
    SDL_RenderLine(renderer, pPx.x - sizePx, pPx.y + sizePx, pPx.x + sizePx, pPx.y - sizePx);
}

} // namespace

DebugOverlayRenderer::DebugOverlayRenderer(SDL_Renderer* renderer)
    : m_renderer(renderer)
{
}

void DebugOverlayRenderer::Render(const sim::World& world,
                                 const core::Camera2D& camera,
                                 int viewportW,
                                 int viewportH,
                                 entt::entity selectedEntity,
                                 const DebugOverlayOptions& options)
{
    if (!m_renderer) {
        return;
    }

    SDL_BlendMode oldBlend = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode(m_renderer, &oldBlend);
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);

    std::uint8_t oldR = 255, oldG = 255, oldB = 255, oldA = 255;
    SDL_GetRenderDrawColor(m_renderer, &oldR, &oldG, &oldB, &oldA);

    if (options.showShotTraces) {
        for (const auto& line : world.DebugLines()) {
            const glm::vec2 a = camera.WorldToScreenPx(line.a_m, viewportW, viewportH);
            const glm::vec2 b = camera.WorldToScreenPx(line.b_m, viewportW, viewportH);
            SetDrawColor(m_renderer, line.r, line.g, line.b, line.a);
            SDL_RenderLine(m_renderer, a.x, a.y, b.x, b.y);
        }
    }

    const auto& registry = world.Registry();
    if (selectedEntity != entt::null && registry.valid(selectedEntity) && registry.all_of<sim::Tank>(selectedEntity)) {
        const auto& tank = registry.get<const sim::Tank>(selectedEntity);
        const glm::vec2 centerPx = camera.WorldToScreenPx(tank.position_m, viewportW, viewportH);
        const float ppm = static_cast<float>(camera.ZoomPixelsPerMeter());

        if (options.highlightSelection) {
            const float selR = std::max(8.0f, static_cast<float>(tank.radius_m) * ppm * 1.4f);
            DrawCircle(m_renderer, centerPx, selR, 255, 255, 255, 200);
        }

        if (const auto* gun = registry.try_get<sim::DirectFireGun>(selectedEntity)) {
            if (options.showWeaponRange && gun->max_range_m > 0.0) {
                const float rangePx = static_cast<float>(gun->max_range_m) * ppm;
                DrawCircle(m_renderer, centerPx, rangePx, 255, 220, 60, 120);
            }
        }

        if (options.showVisualRange && tank.visual_range_m > 0.0) {
            const float rangePx = static_cast<float>(tank.visual_range_m) * ppm;
            DrawCircle(m_renderer, centerPx, rangePx, 80, 220, 255, 90);
        }

        if (options.showLosRay) {
            if (const auto* engagement = registry.try_get<sim::EngagementInfo>(selectedEntity)) {
                if (engagement->target != entt::null && registry.valid(engagement->target) && registry.all_of<sim::Tank>(engagement->target)) {
                    const auto& targetTank = registry.get<const sim::Tank>(engagement->target);
                    const glm::vec2 targetPx = camera.WorldToScreenPx(targetTank.position_m, viewportW, viewportH);

                    if (engagement->has_line_of_sight) {
                        SetDrawColor(m_renderer, 60, 255, 120, 220);
                        SDL_RenderLine(m_renderer, centerPx.x, centerPx.y, targetPx.x, targetPx.y);
                    } else {
                        SetDrawColor(m_renderer, 255, 80, 80, 220);
                        SDL_RenderLine(m_renderer, centerPx.x, centerPx.y, targetPx.x, targetPx.y);

                        if (options.showBlockedPoint && engagement->has_blocked_point) {
                            const glm::vec2 blockedPx = camera.WorldToScreenPx(engagement->blocked_point_m, viewportW, viewportH);
                            DrawCross(m_renderer, blockedPx, 6.0f, 255, 120, 120, 255);
                        }
                    }
                }
            }
        }
    }

    SDL_SetRenderDrawColor(m_renderer, oldR, oldG, oldB, oldA);
    SDL_SetRenderDrawBlendMode(m_renderer, oldBlend);
}

} // namespace clf::render

