#pragma once

#include <chrono>

namespace clf::core {

class Time final {
public:
    Time();

    void Tick();
    double DeltaSeconds() const;
    double ElapsedSeconds() const;
    double FPS() const;

private:
    using clock = std::chrono::steady_clock;

    clock::time_point m_start;
    clock::time_point m_last;

    double m_deltaSeconds = 0.0;
    double m_elapsedSeconds = 0.0;
    double m_fps = 0.0;
};

} // namespace clf::core

