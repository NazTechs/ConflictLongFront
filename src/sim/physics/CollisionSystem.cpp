#include "sim/physics/CollisionSystem.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include <glm/geometric.hpp>

#include "sim/components/CollisionComponent.hpp"
#include "sim/components/TransformComponent.hpp"

namespace clf::sim::physics {

void ResolveCollisions(entt::registry& registry)
{
    auto view = registry.view<Transform, Collision>();
    std::vector<entt::entity> entities;
    entities.reserve(view.size_hint());
    for (const auto e : view) {
        entities.push_back(e);
    }

    // Naive O(n^2) separation. Good enough for small prototype counts.
    for (std::size_t i = 0; i < entities.size(); ++i) {
        for (std::size_t j = i + 1; j < entities.size(); ++j) {
            auto& a = view.get<Transform>(entities[i]);
            auto& b = view.get<Transform>(entities[j]);
            const double ra = view.get<Collision>(entities[i]).radius_m;
            const double rb = view.get<Collision>(entities[j]).radius_m;

            const glm::dvec2 d = b.position_m - a.position_m;
            const double dist2 = d.x * d.x + d.y * d.y;
            const double minDist = std::max(0.1, ra + rb);

            if (dist2 >= minDist * minDist) {
                continue;
            }

            const double dist = std::sqrt(std::max(1e-12, dist2));
            const glm::dvec2 n = d / dist;
            const double penetration = minDist - dist;

            a.position_m -= n * (penetration * 0.5);
            b.position_m += n * (penetration * 0.5);
        }
    }
}

void ClampToMap(entt::registry& registry, double halfExtentMeters)
{
    auto view = registry.view<Transform>();
    for (const auto e : view) {
        auto& xf = view.get<Transform>(e);
        xf.position_m.x = std::clamp(xf.position_m.x, -halfExtentMeters, +halfExtentMeters);
        xf.position_m.y = std::clamp(xf.position_m.y, -halfExtentMeters, +halfExtentMeters);
    }
}

} // namespace clf::sim::physics
