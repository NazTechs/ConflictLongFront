#pragma once

#include <cstdint>
#include <vector>

#include <glm/vec2.hpp>

namespace clf::sim::terrain {

struct TerrainStats final {
    double minHeight_m = 0.0;
    double maxHeight_m = 0.0;
};

class Terrain final {
public:
    static constexpr double kWorldSizeMeters = 20'000.0;

    Terrain(int samplesPerSide, double cellSize_m);

    void GenerateProcedural(std::uint32_t seed);

    double CellSizeMeters() const;
    int SamplesPerSide() const;

    // World-space is centered at (0, 0) and spans [-kWorldSizeMeters/2, +kWorldSizeMeters/2].
    bool ContainsXY(const glm::dvec2& worldXY_m) const;
    glm::dvec2 ClampXY(const glm::dvec2& worldXY_m) const;

    // Bilinear interpolation of grid heights.
    double HeightAtXY(const glm::dvec2& worldXY_m) const;

    TerrainStats Stats() const;
    const std::vector<double>& HeightsMeters() const;

private:
    int m_samplesPerSide = 0;
    double m_cellSize_m = 1.0;
    std::vector<double> m_height_m;

    TerrainStats m_stats{};

    int Index(int x, int y) const;
    double HeightAtSample(int x, int y) const;
    void RecomputeStats();
};

} // namespace clf::sim::terrain

