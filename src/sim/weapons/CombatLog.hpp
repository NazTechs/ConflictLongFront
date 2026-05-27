#pragma once

#include <deque>

#include "sim/weapons/ShotEvent.hpp"

namespace clf::sim::weapons {

class CombatLog final {
public:
    void Add(const ShotEvent& ev);
    const std::deque<ShotEvent>& Events() const;
    void Clear();

private:
    std::deque<ShotEvent> m_events;
    std::size_t m_maxEvents = 200;
};

} // namespace clf::sim::weapons

