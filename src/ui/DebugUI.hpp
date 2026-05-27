#pragma once

namespace clf::ui {

struct DebugUIState final {
    float fps = 0.0f;
    int unitCount = 0;
    double simTimeSeconds = 0.0;
    double zoomPixelsPerMeter = 0.0;
};

class DebugUI final {
public:
    void Render(const DebugUIState& state);
};

} // namespace clf::ui

