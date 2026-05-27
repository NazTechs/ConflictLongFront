#include "sim/terrain/Terrain.hpp"

#include <algorithm>
#include <cmath>

#include <glm/gtc/noise.hpp>

namespace clf::sim::terrain {

namespace {

constexpr double kHalfWorld = Terrain::kWorldSizeMeters * 0.5;

double FractalNoise2D(const glm::dvec2& p, int octaves, double lacunarity, double gain)
{
    double amp = 1.0;
    double freq = 1.0;
    double sum = 0.0;
    double norm = 0.0;

    for (int i = 0; i < octaves; ++i) {
        sum += amp * glm::simplex(p * freq);
        norm += amp;
        amp *= gain;
        freq *= lacunarity;
    }

    if (norm <= 0.0) {
        return 0.0;
    }
    return sum / norm; // roughly [-1, +1]
}

} // namespace

Terrain::Terrain(int samplesPerSide, double cellSize_m)
    : m_samplesPerSide(samplesPerSide),
      m_cellSize_m(cellSize_m),
      m_height_m(static_cast<std::size_t>(samplesPerSide) * static_cast<std::size_t>(samplesPerSide), 0.0)
{
    RecomputeStats();
}

void Terrain::GenerateProcedural(std::uint32_t seed)
{
    // Deterministic procedural terrain using GLM simplex noise + a broad ridge so LOS has interesting cases.
    const double seedOffset = static_cast<double>(seed) * 0.001;

    for (int y = 0; y < m_samplesPerSide; ++y) {
        for (int x = 0; x < m_samplesPerSide; ++x) {
            const double wx = -kHalfWorld + static_cast<double>(x) * m_cellSize_m;
            const double wy = -kHalfWorld + static_cast<double>(y) * m_cellSize_m;

            // Normalize to ~[0..1] range for stable noise inputs.
            const double nx = (wx / Terrain::kWorldSizeMeters) + seedOffset;
            const double ny = (wy / Terrain::kWorldSizeMeters) - seedOffset;

            // Large scale undulation + smaller detail.
            const double n0 = FractalNoise2D(glm::dvec2(nx * 2.0, ny * 2.0), 5, 2.0, 0.5);
            const double n1 = FractalNoise2D(glm::dvec2(nx * 14.0, ny * 14.0), 3, 2.1, 0.55);

            // Broad ridge centered at x=0 to create guaranteed LOS-blocking opportunities.
            const double ridge = std::exp(-std::pow(wx / 900.0, 2.0)); // ~900m half-width

            // Heights in meters. Keep it plausible (tens to a few hundred meters of relief).
            const double height_m = (n0 * 220.0) + (n1 * 35.0) + (ridge * 140.0) - 40.0;

            m_height_m[static_cast<std::size_t>(Index(x, y))] = height_m;
        }
    }

    RecomputeStats();
}

double Terrain::CellSizeMeters() const
{
    return m_cellSize_m;
}

int Terrain::SamplesPerSide() const
{
    return m_samplesPerSide;
}

bool Terrain::ContainsXY(const glm::dvec2& worldXY_m) const
{
    return worldXY_m.x >= -kHalfWorld && worldXY_m.x <= kHalfWorld &&
           worldXY_m.y >= -kHalfWorld && worldXY_m.y <= kHalfWorld;
}

glm::dvec2 Terrain::ClampXY(const glm::dvec2& worldXY_m) const
{
    return glm::dvec2(
        std::clamp(worldXY_m.x, -kHalfWorld, kHalfWorld),
        std::clamp(worldXY_m.y, -kHalfWorld, kHalfWorld));
}

double Terrain::HeightAtXY(const glm::dvec2& worldXY_m) const
{
    const glm::dvec2 clamped = ClampXY(worldXY_m);

    // Convert to grid space where (0,0) is at world (-half,-half).
    const double gx = (clamped.x + kHalfWorld) / m_cellSize_m;
    const double gy = (clamped.y + kHalfWorld) / m_cellSize_m;

    const int x0 = std::clamp(static_cast<int>(std::floor(gx)), 0, m_samplesPerSide - 1);
    const int y0 = std::clamp(static_cast<int>(std::floor(gy)), 0, m_samplesPerSide - 1);
    const int x1 = std::min(x0 + 1, m_samplesPerSide - 1);
    const int y1 = std::min(y0 + 1, m_samplesPerSide - 1);

    const double tx = gx - static_cast<double>(x0);
    const double ty = gy - static_cast<double>(y0);

    const double h00 = HeightAtSample(x0, y0);
    const double h10 = HeightAtSample(x1, y0);
    const double h01 = HeightAtSample(x0, y1);
    const double h11 = HeightAtSample(x1, y1);

    const double hx0 = h00 + (h10 - h00) * tx;
    const double hx1 = h01 + (h11 - h01) * tx;
    return hx0 + (hx1 - hx0) * ty;
}

TerrainStats Terrain::Stats() const
{
    return m_stats;
}

const std::vector<double>& Terrain::HeightsMeters() const
{
    return m_height_m;
}

int Terrain::Index(int x, int y) const
{
    return y * m_samplesPerSide + x;
}

double Terrain::HeightAtSample(int x, int y) const
{
    x = std::clamp(x, 0, m_samplesPerSide - 1);
    y = std::clamp(y, 0, m_samplesPerSide - 1);
    return m_height_m[static_cast<std::size_t>(Index(x, y))];
}

void Terrain::RecomputeStats()
{
    if (m_height_m.empty()) {
        m_stats = {};
        return;
    }

    auto [minIt, maxIt] = std::minmax_element(m_height_m.begin(), m_height_m.end());
    m_stats.minHeight_m = *minIt;
    m_stats.maxHeight_m = *maxIt;
}

} // namespace clf::sim::terrain

