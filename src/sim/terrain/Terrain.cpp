#include "sim/terrain/Terrain.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace clf::sim::terrain {

namespace {

constexpr double kHalfWorld = Terrain::kWorldSizeMeters * 0.5;

std::uint32_t Hash32(std::uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

double Hash2D01(int x, int y, std::uint32_t seed)
{
    const std::uint32_t ux = static_cast<std::uint32_t>(x);
    const std::uint32_t uy = static_cast<std::uint32_t>(y);
    std::uint32_t h = seed;
    h ^= Hash32(ux + 0x9e3779b9U);
    h ^= Hash32(uy + 0x85ebca6bU);
    h = Hash32(h);
    return static_cast<double>(h) / static_cast<double>(0xFFFFFFFFu);
}

double Smoothstep(double t)
{
    t = std::clamp(t, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

double ValueNoise2D(const glm::dvec2& p, double frequency, std::uint32_t seed)
{
    const double x = p.x * frequency;
    const double y = p.y * frequency;

    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = x0 + 1;
    const int y1 = y0 + 1;

    const double tx = Smoothstep(x - static_cast<double>(x0));
    const double ty = Smoothstep(y - static_cast<double>(y0));

    const double v00 = Hash2D01(x0, y0, seed);
    const double v10 = Hash2D01(x1, y0, seed);
    const double v01 = Hash2D01(x0, y1, seed);
    const double v11 = Hash2D01(x1, y1, seed);

    const double a = v00 + (v10 - v00) * tx;
    const double b = v01 + (v11 - v01) * tx;
    return a + (b - a) * ty; // [0..1]
}

double FractalValueNoise2D(const glm::dvec2& p, int octaves, double lacunarity, double gain, std::uint32_t seed)
{
    double amp = 1.0;
    double freq = 1.0;
    double sum = 0.0;
    double norm = 0.0;

    for (int i = 0; i < octaves; ++i) {
        sum += amp * ValueNoise2D(p, freq, seed + static_cast<std::uint32_t>(i) * 1013u);
        norm += amp;
        amp *= gain;
        freq *= lacunarity;
    }

    if (norm <= 0.0) {
        return 0.0;
    }

    // Map from [0..1] to [-1..1]
    return (sum / norm) * 2.0 - 1.0;
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
    // Deterministic procedural terrain using cheap value-noise FBM + a broad ridge so LOS has interesting cases.
    const double seedOffset = static_cast<double>(seed) * 0.001;

    // Precompute X-dependent terms (ridge + normalized X) to avoid expensive pow/exp per cell.
    std::vector<double> nxByX(static_cast<std::size_t>(m_samplesPerSide));
    std::vector<double> ridgeByX(static_cast<std::size_t>(m_samplesPerSide));

    for (int x = 0; x < m_samplesPerSide; ++x) {
        const double wx = -kHalfWorld + static_cast<double>(x) * m_cellSize_m;
        const double nx = (wx / Terrain::kWorldSizeMeters) + seedOffset;
        nxByX[static_cast<std::size_t>(x)] = nx;

        // Broad ridge centered at x=0 to create LOS-blocking opportunities.
        const double t = wx / 900.0;
        ridgeByX[static_cast<std::size_t>(x)] = std::exp(-(t * t));
    }

    for (int y = 0; y < m_samplesPerSide; ++y) {
        const double wy = -kHalfWorld + static_cast<double>(y) * m_cellSize_m;
        const double ny = (wy / Terrain::kWorldSizeMeters) - seedOffset;

        for (int x = 0; x < m_samplesPerSide; ++x) {
            const double nx = nxByX[static_cast<std::size_t>(x)];
            const double ridge = ridgeByX[static_cast<std::size_t>(x)];

            // Large scale undulation + smaller detail.
            // Keep octaves modest; this runs over ~1M samples.
            const glm::dvec2 p0(nx * 2.0, ny * 2.0);
            const glm::dvec2 p1(nx * 12.0, ny * 12.0);
            const double n0 = FractalValueNoise2D(p0, 4, 2.0, 0.5, seed);
            const double n1 = FractalValueNoise2D(p1, 2, 2.1, 0.55, seed ^ 0xB5297A4Du);

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
