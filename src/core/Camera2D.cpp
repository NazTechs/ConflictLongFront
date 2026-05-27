#include "core/Camera2D.hpp"

#include <algorithm>

namespace clf::core {

Camera2D::Camera2D() = default;

glm::vec2 Camera2D::WorldToScreenPx(const glm::dvec2& worldPosMeters, int viewportW, int viewportH) const
{
    const glm::dvec2 rel = worldPosMeters - m_positionMeters;
    const double x = rel.x * m_zoomPixelsPerMeter + static_cast<double>(viewportW) * 0.5;
    const double y = rel.y * m_zoomPixelsPerMeter + static_cast<double>(viewportH) * 0.5;
    return glm::vec2(static_cast<float>(x), static_cast<float>(y));
}

glm::dvec2 Camera2D::ScreenToWorldMeters(const glm::vec2& screenPx, int viewportW, int viewportH) const
{
    if (m_zoomPixelsPerMeter <= 0.0) {
        return m_positionMeters;
    }

    const double x = (static_cast<double>(screenPx.x) - static_cast<double>(viewportW) * 0.5) / m_zoomPixelsPerMeter + m_positionMeters.x;
    const double y = (static_cast<double>(screenPx.y) - static_cast<double>(viewportH) * 0.5) / m_zoomPixelsPerMeter + m_positionMeters.y;
    return glm::dvec2(x, y);
}

void Camera2D::PanMeters(double dxMeters, double dyMeters)
{
    m_positionMeters += glm::dvec2(dxMeters, dyMeters);
}

void Camera2D::PanPixels(double dxPixels, double dyPixels)
{
    if (m_zoomPixelsPerMeter <= 0.0) {
        return;
    }
    m_positionMeters += glm::dvec2(dxPixels / m_zoomPixelsPerMeter, dyPixels / m_zoomPixelsPerMeter);
}

void Camera2D::MultiplyZoom(double factor)
{
    if (factor <= 0.0) {
        return;
    }

    m_zoomPixelsPerMeter *= factor;
    m_zoomPixelsPerMeter = std::clamp(m_zoomPixelsPerMeter, 0.02, 800.0);
}

double Camera2D::ZoomPixelsPerMeter() const
{
    return m_zoomPixelsPerMeter;
}

const glm::dvec2& Camera2D::PositionMeters() const
{
    return m_positionMeters;
}

} // namespace clf::core
