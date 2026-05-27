#pragma once

#include <glm/vec2.hpp>

namespace clf::core {

class Camera2D final {
public:
    Camera2D();

    glm::vec2 WorldToScreenPx(const glm::dvec2& worldPosMeters, int viewportW, int viewportH) const;

    void PanMeters(double dxMeters, double dyMeters);
    void PanPixels(double dxPixels, double dyPixels);

    void MultiplyZoom(double factor);

    double ZoomPixelsPerMeter() const;
    const glm::dvec2& PositionMeters() const;

private:
    glm::dvec2 m_positionMeters{0.0, 0.0}; // world-space center
    double m_zoomPixelsPerMeter = 20.0;    // pixels per meter
};

} // namespace clf::core

