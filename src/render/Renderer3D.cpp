#include "render/Renderer3D.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include <SDL3/SDL.h>

#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "core/Camera3D.hpp"
#include "sim/Components.hpp"
#include "sim/World.hpp"
#include "sim/terrain/Terrain.hpp"
#include "sim/visibility/VisibilityMask.hpp"

namespace clf::render {

namespace {

struct Proj final {
    glm::vec2 pPx{0.0f, 0.0f};
    float depth = 0.0f;
    bool ok = false;
};

Proj ProjectPoint(const glm::mat4& vp, const glm::vec3& pWorld, int w, int h)
{
    const glm::vec4 clip = vp * glm::vec4(pWorld, 1.0f);
    if (std::abs(clip.w) < 1e-6f) {
        return {};
    }
    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    if (ndc.z < 0.0f || ndc.z > 1.0f) {
        // outside depth range (still can be drawn, but skip for now)
    }
    const float sx = (ndc.x * 0.5f + 0.5f) * static_cast<float>(w);
    const float sy = (1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(h);
    return Proj{glm::vec2(sx, sy), ndc.z, true};
}

void SetColor(SDL_Renderer* r, std::uint8_t R, std::uint8_t G, std::uint8_t B, std::uint8_t A)
{
    SDL_SetRenderDrawColor(r, R, G, B, A);
}

void DrawLine(SDL_Renderer* r, const glm::vec2& a, const glm::vec2& b)
{
    SDL_RenderLine(r, a.x, a.y, b.x, b.y);
}

glm::vec3 RotateYawY(float yawRad, const glm::vec3& v)
{
    const float c = std::cos(yawRad);
    const float s = std::sin(yawRad);
    return glm::vec3(c * v.x + s * v.z, v.y, -s * v.x + c * v.z);
}

} // namespace

Renderer3D::Renderer3D(SDL_Renderer* renderer)
    : m_renderer(renderer)
{
}

void Renderer3D::Render(const sim::World& world,
                        const core::Camera3D& camera,
                        int viewportW,
                        int viewportH,
                        entt::entity selectedEntity,
                        const Renderer3DOptions& options)
{
    if (!m_renderer || viewportW <= 0 || viewportH <= 0) {
        return;
    }

    SDL_BlendMode oldBlend = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode(m_renderer, &oldBlend);
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);

    const glm::mat4 vp = camera.ProjectionMatrix() * camera.ViewMatrix();

    const double half = sim::terrain::Terrain::kWorldSizeMeters * 0.5;

    // Terrain plane (two triangles via geometry API)
    {
        SDL_Vertex verts[4]{};
        const glm::vec3 p0(-static_cast<float>(half), 0.0f, -static_cast<float>(half));
        const glm::vec3 p1(+static_cast<float>(half), 0.0f, -static_cast<float>(half));
        const glm::vec3 p2(+static_cast<float>(half), 0.0f, +static_cast<float>(half));
        const glm::vec3 p3(-static_cast<float>(half), 0.0f, +static_cast<float>(half));

        const Proj a = ProjectPoint(vp, p0, viewportW, viewportH);
        const Proj b = ProjectPoint(vp, p1, viewportW, viewportH);
        const Proj c = ProjectPoint(vp, p2, viewportW, viewportH);
        const Proj d = ProjectPoint(vp, p3, viewportW, viewportH);
        if (a.ok && b.ok && c.ok && d.ok) {
            const SDL_FColor col{70.0f / 255.0f, 80.0f / 255.0f, 70.0f / 255.0f, 1.0f};
            verts[0].position = SDL_FPoint{a.pPx.x, a.pPx.y};
            verts[1].position = SDL_FPoint{b.pPx.x, b.pPx.y};
            verts[2].position = SDL_FPoint{c.pPx.x, c.pPx.y};
            verts[3].position = SDL_FPoint{d.pPx.x, d.pPx.y};
            for (auto& v : verts) {
                v.color = col;
                v.tex_coord = SDL_FPoint{0.0f, 0.0f};
            }
            const int indices[6]{0, 1, 2, 0, 2, 3};
            SDL_RenderGeometry(m_renderer, nullptr, verts, 4, indices, 6);
        }
    }

    // Grid lines
    if (options.showGrid) {
        const double step = 1000.0;
        SetColor(m_renderer, 110, 110, 110, 60);
        for (double x = -half; x <= half + 0.1; x += step) {
            const Proj a = ProjectPoint(vp, glm::vec3(static_cast<float>(x), 0.01f, -static_cast<float>(half)), viewportW, viewportH);
            const Proj b = ProjectPoint(vp, glm::vec3(static_cast<float>(x), 0.01f, +static_cast<float>(half)), viewportW, viewportH);
            if (a.ok && b.ok) DrawLine(m_renderer, a.pPx, b.pPx);
        }
        for (double z = -half; z <= half + 0.1; z += step) {
            const Proj a = ProjectPoint(vp, glm::vec3(-static_cast<float>(half), 0.01f, static_cast<float>(z)), viewportW, viewportH);
            const Proj b = ProjectPoint(vp, glm::vec3(+static_cast<float>(half), 0.01f, static_cast<float>(z)), viewportW, viewportH);
            if (a.ok && b.ok) DrawLine(m_renderer, a.pPx, b.pPx);
        }
    }

    // Debug cube at origin (bright yellow), size 50m.
    {
        const float s = 25.0f;
        const glm::vec3 o(0.0f, 0.0f, 0.0f);
        const glm::vec3 pts[8]{
            o + glm::vec3(-s, 0.0f, -s),
            o + glm::vec3(+s, 0.0f, -s),
            o + glm::vec3(+s, 0.0f, +s),
            o + glm::vec3(-s, 0.0f, +s),
            o + glm::vec3(-s, 50.0f, -s),
            o + glm::vec3(+s, 50.0f, -s),
            o + glm::vec3(+s, 50.0f, +s),
            o + glm::vec3(-s, 50.0f, +s),
        };

        const int edges[12][2]{
            {0,1},{1,2},{2,3},{3,0},
            {4,5},{5,6},{6,7},{7,4},
            {0,4},{1,5},{2,6},{3,7},
        };

        SetColor(m_renderer, 255, 255, 0, 220);
        for (const auto& e : edges) {
            const Proj a = ProjectPoint(vp, pts[e[0]], viewportW, viewportH);
            const Proj b = ProjectPoint(vp, pts[e[1]], viewportW, viewportH);
            if (a.ok && b.ok) {
                DrawLine(m_renderer, a.pPx, b.pPx);
            }
        }
    }

    // Fog overlay (sparse quads projected)
    if (options.showFog && options.fogMask && options.fogStrideCells > 0) {
        const auto& mask = *options.fogMask;
        const int mw = mask.Width();
        const int mh = mask.Height();
        if (mw > 0 && mh > 0) {
            const double cell = mask.CellSizeMeters();
            const double halfWorld = sim::terrain::Terrain::kWorldSizeMeters * 0.5;
            const glm::dvec2 worldMin{-halfWorld, -halfWorld};

            const std::uint8_t aUnknown = static_cast<std::uint8_t>(std::clamp(options.fogUnknownAlpha, 0.0f, 1.0f) * 255.0f);
            const std::uint8_t aKnown = static_cast<std::uint8_t>(std::clamp(options.fogKnownAlpha, 0.0f, 1.0f) * 255.0f);

            SDL_Vertex quad[4]{};
            for (int y = 0; y < mh; y += options.fogStrideCells) {
                for (int x = 0; x < mw; x += options.fogStrideCells) {
                    const auto v = mask.Get(x, y);
                    std::uint8_t a = 0;
                    if (v == sim::visibility::CellVis::Unknown) a = aUnknown;
                    else if (v == sim::visibility::CellVis::Known) a = aKnown;
                    else continue; // visible: skip

                    const double x0 = worldMin.x + static_cast<double>(x) * cell;
                    const double y0 = worldMin.y + static_cast<double>(y) * cell;
                    const double x1 = x0 + cell * options.fogStrideCells;
                    const double y1 = y0 + cell * options.fogStrideCells;

                    const glm::vec3 p0(static_cast<float>(x0), 0.03f, static_cast<float>(y0));
                    const glm::vec3 p1(static_cast<float>(x1), 0.03f, static_cast<float>(y0));
                    const glm::vec3 p2(static_cast<float>(x1), 0.03f, static_cast<float>(y1));
                    const glm::vec3 p3(static_cast<float>(x0), 0.03f, static_cast<float>(y1));

                    const Proj a0 = ProjectPoint(vp, p0, viewportW, viewportH);
                    const Proj a1 = ProjectPoint(vp, p1, viewportW, viewportH);
                    const Proj a2 = ProjectPoint(vp, p2, viewportW, viewportH);
                    const Proj a3 = ProjectPoint(vp, p3, viewportW, viewportH);
                    if (!(a0.ok && a1.ok && a2.ok && a3.ok)) {
                        continue;
                    }

                    const SDL_FColor col{0.0f, 0.0f, 0.0f, static_cast<float>(a) / 255.0f};
                    quad[0].position = SDL_FPoint{a0.pPx.x, a0.pPx.y};
                    quad[1].position = SDL_FPoint{a1.pPx.x, a1.pPx.y};
                    quad[2].position = SDL_FPoint{a2.pPx.x, a2.pPx.y};
                    quad[3].position = SDL_FPoint{a3.pPx.x, a3.pPx.y};
                    for (auto& q : quad) {
                        q.color = col;
                        q.tex_coord = SDL_FPoint{0.0f, 0.0f};
                    }
                    const int idx[6]{0, 1, 2, 0, 2, 3};
                    SDL_RenderGeometry(m_renderer, nullptr, quad, 4, idx, 6);
                }
            }
        }
    }

    // Tanks (simple top face box) + selection ring
    {
        const auto& registry = world.Registry();
        const auto view = registry.view<const sim::Tank, const sim::Transform>();

        for (const auto e : view) {
            const auto& tank = view.get<const sim::Tank>(e);
            const auto& xf = view.get<const sim::Transform>(e);

            const float yaw = static_cast<float>(xf.hull_heading_rad);
            const glm::vec3 center(
                static_cast<float>(xf.position_m.x),
                0.0f,
                static_cast<float>(xf.position_m.y));

            const glm::vec3 halfSize(3.5f, 1.2f, 1.8f);
            const glm::vec3 cornersLocal[4]{
                {-halfSize.x, 0.05f, -halfSize.z},
                {+halfSize.x, 0.05f, -halfSize.z},
                {+halfSize.x, 0.05f, +halfSize.z},
                {-halfSize.x, 0.05f, +halfSize.z},
            };

            SDL_Vertex verts[4]{};
            bool ok = true;
            for (int i = 0; i < 4; ++i) {
                const glm::vec3 w = center + RotateYawY(yaw, cornersLocal[i]);
                const Proj p = ProjectPoint(vp, w, viewportW, viewportH);
                ok = ok && p.ok;
                verts[i].position = SDL_FPoint{p.pPx.x, p.pPx.y};
                verts[i].tex_coord = SDL_FPoint{0.0f, 0.0f};
            }

            if (!ok) {
                continue;
            }

            SDL_FColor col{};
            if (tank.team_id == 0) col = SDL_FColor{80.0f / 255.0f, 160.0f / 255.0f, 255.0f / 255.0f, 220.0f / 255.0f};
            else if (tank.team_id == 1) col = SDL_FColor{255.0f / 255.0f, 100.0f / 255.0f, 100.0f / 255.0f, 220.0f / 255.0f};
            else col = SDL_FColor{220.0f / 255.0f, 220.0f / 255.0f, 220.0f / 255.0f, 220.0f / 255.0f};

            for (auto& v : verts) v.color = col;
            const int idx[6]{0, 1, 2, 0, 2, 3};
            SDL_RenderGeometry(m_renderer, nullptr, verts, 4, idx, 6);

            if (e == selectedEntity) {
                // simple ring
                SetColor(m_renderer, 255, 255, 255, 180);
                const int segments = 48;
                glm::vec2 prev{};
                for (int i = 0; i <= segments; ++i) {
                    const float a = (2.0f * 3.1415926f) * (static_cast<float>(i) / segments);
                    const glm::vec3 wp = center + glm::vec3(std::cos(a) * 8.0f, 0.06f, std::sin(a) * 8.0f);
                    const Proj pp = ProjectPoint(vp, wp, viewportW, viewportH);
                    if (i > 0 && pp.ok) {
                        DrawLine(m_renderer, prev, pp.pPx);
                    }
                    prev = pp.pPx;
                }
            }

            if (options.showWaypoints && e == selectedEntity) {
                if (const auto* path = registry.try_get<sim::WaypointPathComponent>(e)) {
                    glm::vec3 prev = center + glm::vec3(0.0f, 0.08f, 0.0f);
                    SetColor(m_renderer, 120, 255, 120, 220);
                    for (const auto& wp : path->waypoints) {
                        const glm::vec3 wpp(static_cast<float>(wp.x), 0.08f, static_cast<float>(wp.y));
                        const Proj a = ProjectPoint(vp, prev, viewportW, viewportH);
                        const Proj b = ProjectPoint(vp, wpp, viewportW, viewportH);
                        if (a.ok && b.ok) {
                            DrawLine(m_renderer, a.pPx, b.pPx);
                            // marker
                            const Proj m0 = ProjectPoint(vp, wpp + glm::vec3(-3.0f, 0.0f, -3.0f), viewportW, viewportH);
                            const Proj m1 = ProjectPoint(vp, wpp + glm::vec3(+3.0f, 0.0f, +3.0f), viewportW, viewportH);
                            const Proj m2 = ProjectPoint(vp, wpp + glm::vec3(-3.0f, 0.0f, +3.0f), viewportW, viewportH);
                            const Proj m3 = ProjectPoint(vp, wpp + glm::vec3(+3.0f, 0.0f, -3.0f), viewportW, viewportH);
                            if (m0.ok && m1.ok) DrawLine(m_renderer, m0.pPx, m1.pPx);
                            if (m2.ok && m3.ok) DrawLine(m_renderer, m2.pPx, m3.pPx);
                        }
                        prev = wpp;
                    }
                }
            }
        }
    }

    SDL_SetRenderDrawBlendMode(m_renderer, oldBlend);
}

} // namespace clf::render
