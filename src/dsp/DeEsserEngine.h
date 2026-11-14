#pragma once

#include "dsp/BandLayout.h"
#include "dsp/Fft.h"
#include "dsp/Params.h"
#include "dsp/SibilanceDetector.h"
#include "dsp/SpectralReducer.h"

#include <array>
#include <memory>
#include <vector>

namespace deessr
{

/** What the engine reports after each STFT frame (used by the display and the tests).
    Pointers are only valid during the callback.
*/
struct FrameSnapshot
{
    const float* envelopePower = nullptr; // smoothed input spectrum, mean-square units
    const float* gainDb = nullptr;        // applied gain per bin, always <= 0
    int numBins = 0;
    int fftSize = 0;
    double sampleRate = 0.0;
    float sibilance = 0.0f;               // 0..1 after lookahead and release
    float reductionDb = 0.0f;             // largest per-bin reduction of the frame
};

class FrameListener
{
public:
    virtual ~FrameListener() = default;

    /** Called on the audio thread: must not allocate, lock or block. */
    virtual void frameProcessed (const FrameSnapshot& snapshot) noexcept = 0;
};

/** Complete de-esser: STFT with weighted overlap-add, level-independent
    detection, spectral reduction, lookahead, aligned bypass and listen output.

    process() is real-time safe: all memory is allocated in prepare().
    The latency is constant for a given sample rate and never depends on settings.
*/
class DeEsserEngine
{
public:
    static constexpr int maxChannels = 2;
    static constexpr int lookaheadFrames = 2;

    /** FFT order used at a sample rate (window of about 10.7 ms at every rate). */
    static int fftOrderFor (double sampleRate) noexcept;

    /** Latency in samples at a sample rate: one window plus the lookahead. */
    static int latencyFor (double sampleRate) noexcept;

    void prepare (double sampleRate, int numChannels);
    void reset() noexcept;

    void setParams (const Params& newParams) noexcept;
    void setListener (FrameListener* newListener) noexcept { listener = newListener; }

    /** Processes numSamples in place. Any block size is accepted. */
    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    int latencySamples() const noexcept { return latency; }
    int fftSize() const noexcept { return frameSize; }
    int hopSize() const noexcept { return hop; }
    double sampleRate() const noexcept { return rate; }

private:
    struct Path
    {
        SibilanceDetector detector;
        SpectralReducer reducer;
        std::array<float, lookaheadFrames + 1> raw {};
        int rawIndex = 0;
        float envelope = 0.0f;
        std::vector<float> power;
        std::vector<float> gainDb;
    };

    struct Channel
    {
        std::vector<float> input;     // last frameSize input samples (ring)
        std::vector<float> output;    // overlap-add accumulator (ring)
        std::vector<float> dry;       // delay line of latency samples
        std::vector<float> spectraRe; // ring of half spectra waiting for the lookahead
        std::vector<float> spectraIm;
    };

    void processFrame() noexcept;
    void snapSmoothers() noexcept;

    // configuration
    double rate = 48000.0;
    int channelCount = 0;
    int pathCount = 0;
    int frameSize = 512;
    int hop = 128;
    int bins = 257;
    int latency = 768;
    float powerScale = 1.0f;
    float mixStep = 0.0f;
    float gainCoef = 0.0f;
    double frameSeconds = 0.0;

    std::unique_ptr<Fft> fft;
    std::vector<float> window;
    std::vector<float> workRe, workIm;
    std::vector<float> displayGain;
    BandLayout layout;

    std::array<Channel, maxChannels> chans;
    std::array<Path, maxChannels + 1> paths; // one per channel plus the linked path

    // run-time state
    Params params;
    int position = 0;      // write position in the input / output rings
    int dryPosition = 0;
    int hopCounter = 0;
    int spectrumIndex = 0; // next slot to write in the spectra rings

    float activeMix = 1.0f, activeTarget = 1.0f; // 1 = processing, 0 = bypassed
    float listenMix = 0.0f, listenTarget = 0.0f;
    float outputGain = 1.0f, outputGainTarget = 1.0f;
    bool smoothersSnapped = false;

    FrameListener* listener = nullptr;
};

} // namespace deessr
