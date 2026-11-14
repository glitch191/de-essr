#pragma once

#include "dsp/BandLayout.h"
#include "dsp/Params.h"

namespace deessr
{

/** Level-independent sibilance detector.

    Works on one power spectrum per STFT frame and returns a continuous value
    between 0 (no sibilance) and 1 (certain sibilance). Every criterion is
    relative: band energy ratios, the level of the sibilant band compared to a
    slow reference of the source level, and spectral shape. The same sound is
    therefore detected the same way whatever its absolute level.

    Vocal mode (single source):
      - ratio of the sibilant band energy to the energy below 3 kHz,
      - spectral centroid (the spectral counterpart of the zero-crossing rate),
      - sibilant band level relative to the long-term level of the source,
      - a guard against isolated pure tones.

    Bus mode (bus or full mix, where voiced / unvoiced criteria mean nothing):
      - excess of the sibilant band energy over its own long-term average,
      - excess of the band ratio over its long-term average,
      the smaller of the two wins, so a louder chorus or a missing kick drum
      does not trigger the detector.
*/
class SibilanceDetector
{
public:
    void prepare (double frameSeconds);
    void reset() noexcept;

    /** Analyses one frame. power holds layout.numBins values in mean-square units. */
    float analyse (const float* power, const BandLayout& layout, Mode mode, float amount) noexcept;

    /** True when the last frame was loud enough to update long-term references. */
    bool lastFrameUsable() const noexcept { return usable; }

    float longTermLevelDb() const noexcept { return levelDb; }

private:
    // one-pole coefficients, per frame
    float levelUpCoef = 0.0f, levelDownCoef = 0.0f, levelSlowCoef = 0.0f;
    float busCoef = 0.0f, busSlowCoef = 0.0f, busWarmCoef = 0.0f;
    int busWarmupFrames = 0;

    // state
    bool levelValid = false;
    float levelDb = -100.0f;      // long-term level of the source
    bool busValid = false;
    int busWarmCount = 0;
    float busSibDb = -100.0f;     // long-term sibilant band energy
    float busRatioDb = 0.0f;      // long-term band ratio
    float previous = 0.0f;        // previous result, used for hysteresis
    bool usable = false;
};

} // namespace deessr
