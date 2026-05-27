#include "sim/weapons/CombatLog.hpp"

namespace clf::sim::weapons {

void CombatLog::Add(const ShotEvent& ev)
{
    m_events.push_back(ev);
    while (m_events.size() > m_maxEvents) {
        m_events.pop_front();
    }
}

const std::deque<ShotEvent>& CombatLog::Events() const
{
    return m_events;
}

void CombatLog::Clear()
{
    m_events.clear();
}

} // namespace clf::sim::weapons

