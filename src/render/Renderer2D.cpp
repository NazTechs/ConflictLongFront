#include "render/Renderer2D.hpp"

#include <SDL3/SDL.h>

#include "core/Camera2D.hpp"
#include "sim/Components.hpp"
#include "sim/World.hpp"

namespace clf::render {

Renderer2D::Renderer2D(SDL_Renderer* renderer)
    : m_renderer(renderer)
{
}

void Renderer2D::Render(const sim::World& world, const core::Camera2D& camera, int viewportW, int viewportH)
{
    const auto& registry = world.Registry();
    const auto view = registry.view<const sim::Tank>();

    const float ppm = static_cast<float>(camera.ZoomPixelsPerMeter());
    const float tankWm = 3.0f;
    const float tankHm = 2.0f;

    for (const auto entity : view) {
        const auto& tank = view.get<const sim::Tank>(entity);
        const glm::vec2 p = camera.WorldToScreenPx(tank.position_m, viewportW, viewportH);

        if (tank.team_id == 0) {
            SDL_SetRenderDrawColor(m_renderer, 80, 160, 255, 255);
        } else if (tank.team_id == 1) {
            SDL_SetRenderDrawColor(m_renderer, 255, 100, 100, 255);
        } else {
            SDL_SetRenderDrawColor(m_renderer, 200, 200, 200, 255);
        }

        const float w = tankWm * ppm;
        const float h = tankHm * ppm;
        SDL_FRect rect{p.x - w * 0.5f, p.y - h * 0.5f, w, h};
        SDL_RenderFillRect(m_renderer, &rect);
    }
}

} // namespace clf::render

