#pragma once

#include "core/SimulationDebugSettings.hpp"
#include "render/ViewMode.hpp"

namespace clf::ui {

struct ViewControlState final {
    clf::render::ViewMode viewMode = clf::render::ViewMode::Spectator;
};

class ViewControlPanel final {
public:
    void Render(ViewControlState& inOutState, clf::core::SimulationDebugSettings& inOutDebugSettings);
};

} // namespace clf::ui

