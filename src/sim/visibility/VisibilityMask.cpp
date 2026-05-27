#include "sim/visibility/VisibilityMask.hpp"

#include <algorithm>

namespace clf::sim::visibility {

VisibilityMask::VisibilityMask(int width, int height, double cellSize_m)
{
    Resize(width, height, cellSize_m);
}

void VisibilityMask::Resize(int width, int height, double cellSize_m)
{
    m_w = std::max(0, width);
    m_h = std::max(0, height);
    m_cell_m = std::max(1.0, cellSize_m);
    m_cells.assign(static_cast<std::size_t>(m_w) * static_cast<std::size_t>(m_h), CellVis::Unknown);
    m_dirty = true;
}

int VisibilityMask::Width() const { return m_w; }
int VisibilityMask::Height() const { return m_h; }
double VisibilityMask::CellSizeMeters() const { return m_cell_m; }

void VisibilityMask::ClearToUnknown()
{
    std::fill(m_cells.begin(), m_cells.end(), CellVis::Unknown);
    m_dirty = true;
}

void VisibilityMask::DecayVisibleToKnown()
{
    for (auto& c : m_cells) {
        if (c == CellVis::Visible) {
            c = CellVis::Known;
        }
    }
    m_dirty = true;
}

CellVis VisibilityMask::Get(int x, int y) const
{
    if (x < 0 || y < 0 || x >= m_w || y >= m_h) {
        return CellVis::Unknown;
    }
    return m_cells[static_cast<std::size_t>(Index(x, y))];
}

void VisibilityMask::Set(int x, int y, CellVis v)
{
    if (x < 0 || y < 0 || x >= m_w || y >= m_h) {
        return;
    }
    const int idx = Index(x, y);
    if (m_cells[static_cast<std::size_t>(idx)] != v) {
        m_cells[static_cast<std::size_t>(idx)] = v;
        m_dirty = true;
    }
}

const std::vector<CellVis>& VisibilityMask::Cells() const { return m_cells; }
bool VisibilityMask::Dirty() const { return m_dirty; }
void VisibilityMask::ClearDirty() { m_dirty = false; }

int VisibilityMask::Index(int x, int y) const
{
    return y * m_w + x;
}

} // namespace clf::sim::visibility

