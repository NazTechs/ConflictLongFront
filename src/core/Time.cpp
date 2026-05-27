#include "core/Time.hpp"

namespace clf::core {

Time::Time()
{
    m_start = clock::now();
    m_last = m_start;
}

void Time::Tick()
{
    const auto now = clock::now();
    const std::chrono::duration<double> dt = now - m_last;

    m_last = now;
    m_deltaSeconds = dt.count();
    m_elapsedSeconds = std::chrono::duration<double>(now - m_start).count();

    if (m_deltaSeconds > 0.0) {
        const double instantFps = 1.0 / m_deltaSeconds;
        m_fps = (m_fps <= 0.0) ? instantFps : (m_fps * 0.9 + instantFps * 0.1);
    }
}

double Time::DeltaSeconds() const
{
    return m_deltaSeconds;
}

double Time::ElapsedSeconds() const
{
    return m_elapsedSeconds;
}

double Time::FPS() const
{
    return m_fps;
}

} // namespace clf::core

