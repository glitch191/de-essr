#pragma once

#include <algorithm>
#include <cmath>

namespace deessr
{

/** Lowest power value used before taking a logarithm (-140 dB). */
constexpr float kPowerFloor = 1.0e-14f;

inline float powerToDb (float power) noexcept
{
    return 10.0f * std::log10 (std::max (power, kPowerFloor));
}

inline float powerToDb (double power) noexcept
{
    return (float) (10.0 * std::log10 (std::max (power, (double) kPowerFloor)));
}

inline float dbToGain (float db) noexcept
{
    return std::exp (db * 0.11512925464970229f);
}

inline float sigmoid (float x) noexcept
{
    return 1.0f / (1.0f + std::exp (-x));
}

/** Coefficient of a one-pole smoother: state = target + (state - target) * coef. */
inline float onePoleCoef (double timeConstantSeconds, double stepSeconds) noexcept
{
    return (float) std::exp (-stepSeconds / std::max (timeConstantSeconds, 1.0e-6));
}

} // namespace deessr
