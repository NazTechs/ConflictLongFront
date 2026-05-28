#include "core/Camera3D.hpp"

#include <algorithm>
#include <cmath>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

namespace clf::core {

namespace {

constexpr float kPi = 3.14159265358979323846f;

glm::vec3 ForwardFromYawPitch(float yawRad, float pitchRad)
{
    // Y is up, XZ is ground. Pitch is down from horizon.
    const float cy = std::cos(yawRad);
    const float sy = std::sin(yawRad);
    const float cp = std::cos(pitchRad);
    const float sp = std::sin(pitchRad);
    // Looking direction (from camera towards target) in world.
    return glm::normalize(glm::vec3(cy * cp, -sp, sy * cp));
}

} // namespace

void Camera3D::Reset()
{
    m_target = glm::vec3(0.0f, 0.0f, 0.0f);
    m_yawRad = 0.7853982f;
    m_pitchRad = 0.7853982f;
    m_distance = 1500.0f;
    m_fovYRad = 1.0471976f;
    m_near = 0.5f;
    m_far = 50'000.0f;
}

void Camera3D::SetYawPitchDistance(float yawRad, float pitchRad, float distanceMeters)
{
    m_yawRad = yawRad;
    m_pitchRad = std::clamp(pitchRad, 0.17f, 1.48f);
    m_distance = std::clamp(distanceMeters, 20.0f, 10'000.0f);
}

void Camera3D::Orbit(float deltaYawRad, float deltaPitchRad)
{
    m_yawRad += deltaYawRad;
    m_pitchRad = std::clamp(m_pitchRad + deltaPitchRad, 0.17f, 1.48f); // ~10..85 deg
}

void Camera3D::Zoom(float deltaMeters)
{
    m_distance = std::clamp(m_distance + deltaMeters, 20.0f, 10'000.0f);
}

void Camera3D::PanTargetGround(float dxMeters, float dzMeters)
{
    // Pan in world XZ, independent of yaw. (Good enough for now.)
    m_target.x += dxMeters;
    m_target.z += dzMeters;
}

glm::vec3 Camera3D::Position() const
{
    const glm::vec3 forward = ForwardFromYawPitch(m_yawRad, m_pitchRad);
    return m_target - forward * m_distance;
}

glm::mat4 Camera3D::ViewMatrix() const
{
    const glm::vec3 pos = Position();
    return glm::lookAtRH(pos, m_target, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 Camera3D::ProjectionMatrix() const
{
    return glm::perspectiveRH_ZO(m_fovYRad, std::max(0.01f, m_aspect), m_near, m_far);
}

Ray3D Camera3D::ScreenPointToRay(float xPx, float yPx, int viewportW, int viewportH) const
{
    // NDC: [-1,1]
    const float nx = (2.0f * (xPx / std::max(1, viewportW))) - 1.0f;
    const float ny = 1.0f - (2.0f * (yPx / std::max(1, viewportH)));

    const glm::mat4 invVP = glm::inverse(ProjectionMatrix() * ViewMatrix());

    const glm::vec4 pNear = invVP * glm::vec4(nx, ny, 0.0f, 1.0f);
    const glm::vec4 pFar = invVP * glm::vec4(nx, ny, 1.0f, 1.0f);

    const glm::vec3 wNear = glm::vec3(pNear) / pNear.w;
    const glm::vec3 wFar = glm::vec3(pFar) / pFar.w;

    Ray3D ray{};
    ray.origin = wNear;
    ray.dir = glm::normalize(wFar - wNear);
    return ray;
}

bool Camera3D::RaycastGroundPlaneY0(const Ray3D& ray, glm::vec2& outSimXY_m) const
{
    // Plane y=0. Solve origin.y + t*dir.y = 0
    const float dy = ray.dir.y;
    if (std::abs(dy) < 1e-6f) {
        return false;
    }
    const float t = -ray.origin.y / dy;
    if (t < 0.0f) {
        return false;
    }
    const glm::vec3 p = ray.origin + ray.dir * t;
    outSimXY_m = glm::vec2(p.x, p.z); // sim x/y in meters
    return true;
}

} // namespace clf::core
