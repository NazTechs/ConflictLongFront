#include "render/Renderer2D.hpp"

#include <SDL3/SDL.h>

#include "core/Camera2D.hpp"
#include "sim/Components.hpp"
#include "sim/components/DetectionComponent.hpp"
#include "sim/components/TransformComponent.hpp"
#include "sim/World.hpp"

namespace clf::render {

Renderer2D::Renderer2D(SDL_Renderer* renderer)
    : m_renderer(renderer),
      m_terrainRenderer(renderer),
      m_debugOverlayRenderer(renderer)
{
}

void Renderer2D::Render(const sim::World& world, const core::Camera2D& camera, int viewportW, int viewportH, const Options& options)
{
    m_terrainRenderer.Render(world.GetTerrain(), camera, viewportW, viewportH, options.terrain);

    const auto& registry = world.Registry();
    const auto view = registry.view<const sim::Tank, const sim::Transform>();

    const float ppm = static_cast<float>(camera.ZoomPixelsPerMeter());

    auto isVisibleInMode = [&](entt::entity entity) -> bool {
        if (options.viewMode == ViewMode::Spectator) {
            return true;
        }
        if (options.viewMode == ViewMode::DebugTactical) {
            if (options.showAllUnitsInDebugView) {
                return true;
            }
            // If not omniscient, fall through to detection-based filter using viewerEntity.
        }

        if (options.viewerEntity == entt::null || !registry.valid(options.viewerEntity)) {
            return true;
        }
        if (entity == options.viewerEntity) {
            return true;
        }
        if (!options.showOnlyDetectedUnits) {
            return true;
        }

        const auto* det = registry.try_get<sim::Detection>(options.viewerEntity);
        if (!det) {
            return true;
        }
        return det->target_detected && det->current_target == entity;
    };

    for (const auto entity : view) {
        if (!isVisibleInMode(entity)) {
            continue;
        }
        const auto& tank = view.get<const sim::Tank>(entity);
        const auto& xf = view.get<const sim::Transform>(entity);
        const glm::vec2 p = camera.WorldToScreenPx(xf.position_m, viewportW, viewportH);

        if (tank.team_id == 0) {
            SDL_SetRenderDrawColor(m_renderer, 80, 160, 255, 255);
        } else if (tank.team_id == 1) {
            SDL_SetRenderDrawColor(m_renderer, 255, 100, 100, 255);
        } else {
            SDL_SetRenderDrawColor(m_renderer, 200, 200, 200, 255);
        }

        const float rPx = std::max(2.0f, static_cast<float>(tank.radius_m) * ppm);
        const float w = std::max(4.0f, rPx * 1.6f);
        const float h = std::max(3.0f, rPx * 1.0f);
        SDL_FRect rect{p.x - w * 0.5f, p.y - h * 0.5f, w, h};
        SDL_RenderFillRect(m_renderer, &rect);
    }

    m_debugOverlayRenderer.Render(world, camera, viewportW, viewportH, options.selectedEntity, options.overlay);
}

} // namespace clf::render
