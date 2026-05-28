#pragma once

#include "core/SimulationDebugSettings.hpp"
#include "render/RenderMode.hpp"
#include "render/ViewMode.hpp"

namespace clf::core {
class Camera3D;
}

namespace clf::ui {

struct ViewControlState final {
    clf::render::ViewMode viewMode = clf::render::ViewMode::Spectator;
    clf::render::RenderMode renderMode = clf::render::RenderMode::TopDown2D;
    bool followSelected = false;
    bool requestFocusSelected = false;
};

class ViewControlPanel final {
public:
    void Render(ViewControlState& inOutState,
                clf::core::SimulationDebugSettings& inOutDebugSettings,
                clf::core::Camera3D& camera3D,
                bool hasSelection);
};

} // namespace clf::ui
