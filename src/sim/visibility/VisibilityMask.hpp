#pragma once

#include <cstdint>
#include <vector>

namespace clf::sim::visibility {

enum class CellVis : std::uint8_t {
    Unknown = 0,
    Known = 1,
    Visible = 2,
};

class VisibilityMask final {
public:
    VisibilityMask() = default;
    VisibilityMask(int width, int height, double cellSize_m);

    void Resize(int width, int height, double cellSize_m);

    int Width() const;
    int Height() const;
    double CellSizeMeters() const;

    void ClearToUnknown();
    void DecayVisibleToKnown();

    CellVis Get(int x, int y) const;
    void Set(int x, int y, CellVis v);

    const std::vector<CellVis>& Cells() const;
    bool Dirty() const;
    void ClearDirty();

private:
    int m_w = 0;
    int m_h = 0;
    double m_cell_m = 100.0;
    std::vector<CellVis> m_cells;
    bool m_dirty = true;

    int Index(int x, int y) const;
};

} // namespace clf::sim::visibility

