#pragma once

namespace clf::core {

struct SimulationDebugSettings final {
    bool showAllUnitsInDebugView = true;
    bool showOnlyDetectedUnitsInUnitView = true;

    bool showWeaponRange = true;
    bool showVisualRange = true;
    bool showLosRay = true;
    bool showSensorCone = true;

    bool showTerrainHeightOverlay = true;
    bool terrainContourBands = true;

    bool showBlockedLosPoint = true;
    bool showShotTraces = true;
    bool highlightSelection = true;

    bool showDamageZones = false;

    bool showFogOfWar = true;
    bool showSearchWaypoints = true;
    bool showMovementVectors = true;
    bool showAiState = true;
    bool showDetectionDebug = false;
    bool showCollisionBounds = false;
};

} // namespace clf::core
