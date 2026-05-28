#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace clf::core {

struct Ray3D final {
    glm::vec3 origin{0.0f};
    glm::vec3 dir{0.0f, -1.0f, 0.0f}; // normalized
};

class Camera3D final {
public:
    void Reset();

    void SetAspect(float aspect) { m_aspect = aspect; }
    float Aspect() const { return m_aspect; }

    void SetTarget(const glm::vec3& t) { m_target = t; }
    const glm::vec3& Target() const { return m_target; }

    void SetYawPitchDistance(float yawRad, float pitchRad, float distanceMeters);

    float YawRad() const { return m_yawRad; }
    float PitchRad() const { return m_pitchRad; }
    float DistanceMeters() const { return m_distance; }

    void Orbit(float deltaYawRad, float deltaPitchRad);
    void Zoom(float deltaMeters);
    void PanTargetGround(float dxMeters, float dzMeters);

    glm::vec3 Position() const;

    glm::mat4 ViewMatrix() const;
    glm::mat4 ProjectionMatrix() const;

    Ray3D ScreenPointToRay(float xPx, float yPx, int viewportW, int viewportH) const;
    bool RaycastGroundPlaneY0(const Ray3D& ray, glm::vec2& outSimXY_m) const;

private:
    glm::vec3 m_target{0.0f, 0.0f, 0.0f};
    float m_yawRad = 0.7853982f;   // 45 deg
    float m_pitchRad = 0.7853982f; // 45 deg down
    float m_distance = 1500.0f;    // meters

    float m_fovYRad = 1.0471976f; // 60 deg
    float m_aspect = 16.0f / 9.0f;
    float m_near = 0.5f;
    float m_far = 50'000.0f;
};

} // namespace clf::core
