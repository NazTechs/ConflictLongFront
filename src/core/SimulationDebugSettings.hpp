#pragma once

namespace clf::core {

struct SimulationDebugSettings final {
    bool showWeaponRange = true;
    bool showVisualRange = true;
    bool showLosRay = true;

    bool showTerrainHeightOverlay = true;
    bool terrainContourBands = true;

    bool showBlockedLosPoint = true;
    bool showShotTraces = true;
    bool highlightSelection = true;
};

} // namespace clf::core

