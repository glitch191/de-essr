#include "dsp/SpectralReducer.h"

#include "dsp/DspMath.h"

namespace deessr
{

namespace
{
    constexpr double kSmoothingRatio = 1.0594630943592953; // a twelfth of an octave each side
    constexpr double kLongTermSeconds = 2.0;
    constexpr double kGainReleaseSeconds = 0.012;
    constexpr float kReferenceMaxSibilance = 0.2f;
    constexpr float kTiny = 1.0e-4f;
}

void SpectralReducer::prepare (int newNumBins, int ringFrames, double frameSeconds)
{
    numBins = newNumBins;
    ring = std::max (1, ringFrames);

    const auto bins = (size_t) numBins;
    smoothLo.resize (bins);
    smoothHi.resize (bins);

    for (int k = 0; k < numBins; ++k)
    {
        smoothLo[(size_t) k] = std::max (0, (int) std::floor ((double) k / kSmoothingRatio));
        smoothHi[(size_t) k] = std::min (numBins - 1, (int) std::ceil ((double) k * kSmoothingRatio));
    }

    prefix.assign (bins + 1, 0.0);
    envelopes.assign (bins * (size_t) ring, 0.0f);
    longTerm.assign (bins, 0.0f);
    target.assign (bins, 0.0f);
    scratch.assign (bins, 0.0f);
    gainDb.assign (bins, 0.0f);

    longTermCoef = onePoleCoef (kLongTermSeconds, frameSeconds);
    releaseCoef = onePoleCoef (kGainReleaseSeconds, frameSeconds);
    reset();
}

void SpectralReducer::reset() noexcept
{
    std::fill (envelopes.begin(), envelopes.end(), 0.0f);
    std::fill (longTerm.begin(), longTerm.end(), 0.0f);
    std::fill (gainDb.begin(), gainDb.end(), 0.0f);
    longTermValid = false;
    writeIndex = 0;
    lastMaxReduction = 0.0f;
}

void SpectralReducer::pushFrame (const float* power, float rawSibilance, bool updateReference) noexcept
{
    double sum = 0.0;
    prefix[0] = 0.0;

    for (int k = 0; k < numBins; ++k)
    {
        sum += (double) power[k];
        prefix[(size_t) k + 1] = sum;
    }

    float* envelope = envelopes.data() + (size_t) writeIndex * (size_t) numBins;

    for (int k = 0; k < numBins; ++k)
    {
        const int lo = smoothLo[(size_t) k];
        const int hi = smoothHi[(size_t) k];
        envelope[k] = (float) ((prefix[(size_t) hi + 1] - prefix[(size_t) lo]) / (double) (hi - lo + 1));
    }

    if (updateReference && rawSibilance < kReferenceMaxSibilance)
    {
        if (! longTermValid)
        {
            std::copy (envelope, envelope + numBins, longTerm.begin());
            longTermValid = true;
        }
        else
        {
            for (int k = 0; k < numBins; ++k)
                longTerm[(size_t) k] = envelope[k] + (longTerm[(size_t) k] - envelope[k]) * longTermCoef;
        }
    }

    writeIndex = (writeIndex + 1) % ring;
}

const float* SpectralReducer::oldestEnvelope() const noexcept
{
    // After pushFrame() the write index points at the oldest stored frame.
    return envelopes.data() + (size_t) writeIndex * (size_t) numBins;
}

float SpectralReducer::computeGains (const BandLayout& layout, float depthDb, float focus, float* gainDbOut) noexcept
{
    if (depthDb <= kTiny && lastMaxReduction <= kTiny)
    {
        // Nothing to do and nothing left to release: unity gain, exactly.
        std::fill (gainDb.begin(), gainDb.end(), 0.0f);
        std::fill (gainDbOut, gainDbOut + numBins, 0.0f);
        lastMaxReduction = 0.0f;
        return 0.0f;
    }

    std::fill (target.begin(), target.end(), 0.0f);

    if (depthDb > kTiny)
    {
        const float* envelope = oldestEnvelope();
        const int lo = layout.sibLo;
        const int hi = layout.sibHi;

        // Envelope in dB over the sibilant band and its peak.
        float peakDb = -200.0f;

        for (int k = lo; k <= hi; ++k)
        {
            const float db = powerToDb (envelope[k]);
            scratch[(size_t) k] = db;

            if (layout.weight[(size_t) k] > 0.5f)
                peakDb = std::max (peakDb, db);
        }

        const float ceilingDb = peakDb - depthDb;

        for (int k = lo; k <= hi; ++k)
        {
            const float w = layout.weight[(size_t) k];
            const float db = scratch[(size_t) k];
            const float wide = depthDb * w;
            const float surgical = std::clamp (db - ceilingDb, 0.0f, depthDb) * w;
            float reduction = wide + (surgical - wide) * focus;

            // Never dig below the long-term average of the non-sibilant source.
            if (longTermValid)
                reduction = std::min (reduction, std::max (0.0f, db - powerToDb (longTerm[(size_t) k])));

            target[(size_t) k] = -reduction;
        }

        // Light smoothing across frequency. Averaging values that are all within
        // [-depth, 0] keeps the result within that interval.
        for (int k = lo; k <= hi; ++k)
        {
            const float left = k > lo ? target[(size_t) k - 1] : target[(size_t) k];
            const float right = k < hi ? target[(size_t) k + 1] : target[(size_t) k];
            scratch[(size_t) k] = 0.25f * left + 0.5f * target[(size_t) k] + 0.25f * right;
        }

        for (int k = lo; k <= hi; ++k)
            target[(size_t) k] = scratch[(size_t) k];
    }

    // Time smoothing: instant attack (the lookahead already ramps the depth), short release.
    float maxReduction = 0.0f;

    for (int k = 0; k < numBins; ++k)
    {
        const float wanted = target[(size_t) k];
        float g = gainDb[(size_t) k];
        g = wanted < g ? wanted : wanted + (g - wanted) * releaseCoef;

        if (g > -kTiny)
            g = 0.0f;

        gainDb[(size_t) k] = g;
        gainDbOut[k] = g;
        maxReduction = std::max (maxReduction, -g);
    }

    lastMaxReduction = maxReduction;
    return maxReduction;
}

} // namespace deessr
