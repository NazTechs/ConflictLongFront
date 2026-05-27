#pragma once

#include <chrono>
#include <string_view>

#include <spdlog/spdlog.h>

namespace clf::core {

// Simple RAII timer for startup/load profiling.
// Usage:
//   { ScopedTimer t("[Startup] Load weapons"); ... }
class ScopedTimer final {
public:
    explicit ScopedTimer(std::string_view name)
        : m_name(name),
          m_start(std::chrono::steady_clock::now())
    {
    }

    ~ScopedTimer()
    {
        const auto end = std::chrono::steady_clock::now();
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - m_start).count();
        spdlog::info("{}: {} ms", m_name, ms);
    }

    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;

private:
    std::string_view m_name;
    std::chrono::steady_clock::time_point m_start;
};

} // namespace clf::core

