#include "Timer.hpp"

Timer::Timer()
{
    m_start = std::chrono::high_resolution_clock::now();
}

Timer::~Timer()
{
    auto duration = std::chrono::high_resolution_clock::now() - m_start;
    auto time_in_ms = (float)std::chrono::duration_cast<std::chrono::microseconds>(duration).count() * 0.001f;
}

float Timer::stop()
{
    auto duration = std::chrono::high_resolution_clock::now() - m_start;
    auto time_in_ms = (float)std::chrono::duration_cast<std::chrono::microseconds>(duration).count() * 0.001f;
    return  time_in_ms;
}