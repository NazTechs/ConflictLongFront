#include "render/CameraController.hpp"

#include "core/Camera2D.hpp"
#include "sim/Components.hpp"
#include "sim/components/TransformComponent.hpp"
#include "sim/World.hpp"

namespace clf::render {

void CameraController::SetViewMode(ViewMode mode)
{
    m_mode = mode;
}

ViewMode CameraController::GetViewMode() const
{
    return m_mode;
}

void CameraController::ApplyViewMode(core::Camera2D& camera,
                                    const sim::World& world,
                                    entt::entity selectedEntity)
{
    const auto& registry = world.Registry();

    auto followEntity = [&](entt::entity e) {
        if (e == entt::null || !registry.valid(e) || !registry.all_of<sim::Transform>(e)) {
            return false;
        }
        const auto& xf = registry.get<const sim::Transform>(e);
        const glm::dvec2 pos = xf.position_m;
        const glm::dvec2 delta = pos - camera.PositionMeters();
        camera.PanMeters(delta.x, delta.y);
        return true;
    };

    switch (m_mode) {
        case ViewMode::Spectator:
        case ViewMode::DebugTactical:
            return;
        case ViewMode::SelectedTank:
            followEntity(selectedEntity);
            return;
        case ViewMode::BlueTank: {
            auto view = registry.view<const sim::Tank>();
            for (const auto e : view) {
                const auto& t = view.get<const sim::Tank>(e);
                if (t.team_id == 0) {
                    followEntity(e);
                    break;
                }
            }
            return;
        }
        case ViewMode::RedTank: {
            auto view = registry.view<const sim::Tank>();
            for (const auto e : view) {
                const auto& t = view.get<const sim::Tank>(e);
                if (t.team_id == 1) {
                    followEntity(e);
                    break;
                }
            }
            return;
        }
    }
}

} // namespace clf::render
