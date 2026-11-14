#pragma once

#include "dsp/BandLayout.h"

#include <vector>

namespace deessr
{

/** Turns a sibilance depth into per-bin gains.

    For each frame it keeps a fractional-octave smoothed spectrum (the spectral
    envelope) and a long-term average of that envelope over non-sibilant frames.
    Gains only remove the excess:
      - wide part: the whole sibilant band goes down by the depth,
      - surgical part: only the part of the envelope above a ceiling (envelope
        peak minus depth) is brought down to that ceiling,
      - a bin is never pushed below the long-term non-sibilant average.
    Gains are smoothed across frequency and in time, and never exceed 0 dB.

    Frames are delayed by (ringFrames - 1) frames: pushFrame() stores the newest
    one, computeGains() works on the oldest one.
*/
class SpectralReducer
{
public:
    void prepare (int numBins, int ringFrames, double frameSeconds);
    void reset() noexcept;

    void pushFrame (const float* power, float rawSibilance, bool updateReference) noexcept;

    /** Writes numBins gains in dB (all <= 0) and returns the largest reduction in dB. */
    float computeGains (const BandLayout& layout, float depthDb, float focus, float* gainDbOut) noexcept;

    /** Smoothed spectrum (power) of the frame computeGains() works on. */
    const float* oldestEnvelope() const noexcept;

private:
    int numBins = 0;
    int ring = 1;
    int writeIndex = 0;

    std::vector<int> smoothLo, smoothHi;   // fractional-octave window per bin
    std::vector<double> prefix;            // running sum used by the smoothing
    std::vector<float> envelopes;          // ring * numBins smoothed spectra
    std::vector<float> longTerm;           // long-term envelope of non-sibilant frames
    bool longTermValid = false;
    std::vector<float> target, scratch;    // per-frame work buffers
    std::vector<float> gainDb;             // time-smoothed gains

    float longTermCoef = 0.0f;
    float releaseCoef = 0.0f;
    float lastMaxReduction = 0.0f;
};

} // namespace deessr
