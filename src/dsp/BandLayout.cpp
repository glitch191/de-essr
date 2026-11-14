#include "dsp/BandLayout.h"

#include "dsp/Params.h"

#include <algorithm>
#include <cmath>

namespace deessr
{

namespace
{
    constexpr float kEdgeRatio = 1.122462f;      // band edges fade over a sixth of an octave
    constexpr double kLowBandStartHz = 100.0;
    constexpr double kLowBandEndHz = 3000.0;
    constexpr double kTotalStartHz = 80.0;
    constexpr double kTotalEndHz = 20000.0;
}

void BandLayout::prepare (double newSampleRate, int newFftSize)
{
    sampleRate = newSampleRate;
    fftSize = newFftSize;
    numBins = fftSize / 2 + 1;
    binHz = sampleRate / (double) fftSize;
    weight.assign ((size_t) numBins, 0.0f);

    const int lastBin = numBins - 2; // the Nyquist bin is never used

    totalLo = std::clamp ((int) std::ceil (kTotalStartHz / binHz), 1, lastBin);
    totalHi = std::clamp ((int) std::floor (kTotalEndHz / binHz), totalLo, lastBin);

    lowHz = 0.0f;
    highHz = 0.0f;
    setBand (4000.0f, 12000.0f);
}

bool BandLayout::setBand (float newLowHz, float newHighHz) noexcept
{
    // The std::max calls keep the clamp bounds ordered at unusually low sample rates.
    const float maxHigh = std::min (limits::bandMaxHz, (float) (sampleRate * 0.45));
    const float maxLow = std::max (limits::bandMinHz, maxHigh / limits::bandMinRatio);
    const float lo = std::clamp (newLowHz, limits::bandMinHz, maxLow);
    const float hi = std::clamp (newHighHz, lo * limits::bandMinRatio, std::max (lo * limits::bandMinRatio, maxHigh));

    if (lo == lowHz && hi == highHz)
        return false;

    lowHz = lo;
    highHz = hi;

    const int lastBin = numBins - 2;
    const float fadeLo = lo / kEdgeRatio;
    const float fadeHi = hi * kEdgeRatio;
    const float logRatio = std::log (kEdgeRatio);
    const float pi = 3.14159265358979f;

    sibLo = lastBin;
    sibHi = 1;

    for (int k = 0; k < numBins; ++k)
    {
        const float f = (float) ((double) k * binHz);
        float w = 0.0f;

        if (k >= 1 && k <= lastBin)
        {
            if (f >= lo && f <= hi)
                w = 1.0f;
            else if (f > fadeLo && f < lo)
                w = 0.5f - 0.5f * std::cos (pi * std::log (f / fadeLo) / logRatio);
            else if (f > hi && f < fadeHi)
                w = 0.5f - 0.5f * std::cos (pi * std::log (fadeHi / f) / logRatio);
        }

        weight[(size_t) k] = w;

        if (w > 0.0f)
        {
            sibLo = std::min (sibLo, k);
            sibHi = std::max (sibHi, k);
        }
    }

    if (sibLo > sibHi) // cannot happen with the limits above, kept as a safety net
    {
        sibLo = sibHi = std::clamp ((int) ((double) lo / binHz), 1, lastBin);
        weight[(size_t) sibLo] = 1.0f;
    }

    // Low reference band: below the sibilant band, never above 3 kHz.
    const double lowEnd = std::min (kLowBandEndHz, (double) lo * 0.75);
    lowBandLo = std::clamp ((int) std::ceil (kLowBandStartHz / binHz), 1, lastBin);
    lowBandHi = std::clamp ((int) std::floor (lowEnd / binHz), lowBandLo, lastBin);

    return true;
}

} // namespace deessr
