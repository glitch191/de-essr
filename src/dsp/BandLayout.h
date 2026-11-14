#pragma once

#include <vector>

namespace deessr
{

/** Bin ranges and weights derived from the sample rate, the FFT size and the
    sibilant band settings. Shared by the detector and the reducer.
*/
struct BandLayout
{
    void prepare (double sampleRate, int fftSize);

    /** Recomputes the sibilant band. Cheap, no allocation; returns true if anything changed. */
    bool setBand (float lowHz, float highHz) noexcept;

    double sampleRate = 48000.0;
    int fftSize = 512;
    int numBins = 257;
    double binHz = 93.75;

    float lowHz = 0.0f;  // sanitised sibilant band edges
    float highHz = 0.0f;

    int totalLo = 0, totalHi = 0;       // bins used for the total energy
    int lowBandLo = 0, lowBandHi = 0;   // bins of the low reference band
    int sibLo = 0, sibHi = 0;           // bins where the sibilant weight is above zero

    std::vector<float> weight;          // sibilant band weight per bin, 0..1
};

} // namespace deessr
