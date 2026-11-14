#include "dsp/DeEsserEngine.h"

#include "dsp/DspMath.h"

namespace deessr
{

namespace
{
    constexpr double kMixSeconds = 0.010;       // bypass and listen cross-fades
    constexpr double kGainSeconds = 0.010;      // output gain smoothing
    constexpr float kAmountRamp = 0.05f;        // Amount below this fades the depth to zero
    constexpr float kUnityThresholdDb = -1.0e-3f;
}

int DeEsserEngine::fftOrderFor (double sampleRate) noexcept
{
    if (sampleRate <= 50000.0)
        return 9;  // 512

    if (sampleRate <= 100000.0)
        return 10; // 1024

    return 11;     // 2048
}

int DeEsserEngine::latencyFor (double sampleRate) noexcept
{
    const int size = 1 << fftOrderFor (sampleRate);
    return size + lookaheadFrames * (size / 4);
}

void DeEsserEngine::prepare (double sampleRate, int numChannels)
{
    rate = sampleRate;
    channelCount = std::clamp (numChannels, 1, maxChannels);
    pathCount = channelCount == 1 ? 1 : channelCount + 1;

    const int order = fftOrderFor (rate);
    frameSize = 1 << order;
    hop = frameSize / 4;
    bins = frameSize / 2 + 1;
    latency = latencyFor (rate);
    frameSeconds = (double) hop / rate;

    // Scales |X|^2 to mean-square units (a full-scale sine sums to about 0.5).
    powerScale = 4.0f / ((float) frameSize * (float) frameSize);
    mixStep = (float) (1.0 / (kMixSeconds * rate));
    gainCoef = onePoleCoef (kGainSeconds, 1.0 / rate);

    fft = std::make_unique<Fft> (order);

    const auto size = (size_t) frameSize;
    window.resize (size);
    const double pi = 3.14159265358979323846;

    // Periodic square-root Hann window, used for analysis and synthesis.
    // With 75 % overlap the squared windows sum to 2, hence the 0.5 at synthesis.
    for (int i = 0; i < frameSize; ++i)
        window[(size_t) i] = (float) std::sin (pi * (double) i / (double) frameSize);

    workRe.assign (size, 0.0f);
    workIm.assign (size, 0.0f);
    displayGain.assign ((size_t) bins, 0.0f);
    layout.prepare (rate, frameSize);

    const int ringFrames = lookaheadFrames + 1;

    for (int c = 0; c < channelCount; ++c)
    {
        auto& ch = chans[(size_t) c];
        ch.input.assign (size, 0.0f);
        ch.output.assign (size, 0.0f);
        ch.dry.assign ((size_t) latency, 0.0f);
        ch.spectraRe.assign ((size_t) bins * (size_t) ringFrames, 0.0f);
        ch.spectraIm.assign ((size_t) bins * (size_t) ringFrames, 0.0f);
    }

    for (int p = 0; p < pathCount; ++p)
    {
        auto& path = paths[(size_t) p];
        path.detector.prepare (frameSeconds);
        path.reducer.prepare (bins, ringFrames, frameSeconds);
        path.power.assign ((size_t) bins, 0.0f);
        path.gainDb.assign ((size_t) bins, 0.0f);
    }

    reset();
}

void DeEsserEngine::reset() noexcept
{
    for (int c = 0; c < channelCount; ++c)
    {
        auto& ch = chans[(size_t) c];
        std::fill (ch.input.begin(), ch.input.end(), 0.0f);
        std::fill (ch.output.begin(), ch.output.end(), 0.0f);
        std::fill (ch.dry.begin(), ch.dry.end(), 0.0f);
        std::fill (ch.spectraRe.begin(), ch.spectraRe.end(), 0.0f);
        std::fill (ch.spectraIm.begin(), ch.spectraIm.end(), 0.0f);
    }

    for (int p = 0; p < pathCount; ++p)
    {
        auto& path = paths[(size_t) p];
        path.detector.reset();
        path.reducer.reset();
        path.raw.fill (0.0f);
        path.rawIndex = 0;
        path.envelope = 0.0f;
        std::fill (path.gainDb.begin(), path.gainDb.end(), 0.0f);
    }

    position = 0;
    dryPosition = 0;
    hopCounter = 0;
    spectrumIndex = 0;
    smoothersSnapped = false;
}

void DeEsserEngine::setParams (const Params& newParams) noexcept
{
    params = newParams;
    params.amount = std::clamp (params.amount, 0.0f, 1.0f);
    params.rangeDb = std::clamp (params.rangeDb, 0.0f, limits::rangeMaxDb);
    params.focus = std::clamp (params.focus, 0.0f, 1.0f);
    params.releaseMs = std::clamp (params.releaseMs, limits::releaseMinMs, limits::releaseMaxMs);
    params.stereoLink = std::clamp (params.stereoLink, 0.0f, 1.0f);
    params.outputGainDb = std::clamp (params.outputGainDb, limits::outputGainMinDb, limits::outputGainMaxDb);

    activeTarget = params.bypass ? 0.0f : 1.0f;
    listenTarget = params.listen ? 1.0f : 0.0f;
    outputGainTarget = dbToGain (params.outputGainDb);

    if (! smoothersSnapped)
        snapSmoothers();
}

void DeEsserEngine::snapSmoothers() noexcept
{
    activeMix = activeTarget;
    listenMix = listenTarget;
    outputGain = outputGainTarget;
    smoothersSnapped = true;
}

void DeEsserEngine::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    const int active = std::min (numChannels, channelCount);
    const int mask = frameSize - 1;

    if (! smoothersSnapped)
        snapSmoothers();

    for (int i = 0; i < numSamples; ++i)
    {
        // Per-sample smoothing of the output stage, shared by all channels.
        if (activeMix != activeTarget)
            activeMix = activeMix < activeTarget ? std::min (activeTarget, activeMix + mixStep)
                                                 : std::max (activeTarget, activeMix - mixStep);

        if (listenMix != listenTarget)
            listenMix = listenMix < listenTarget ? std::min (listenTarget, listenMix + mixStep)
                                                 : std::max (listenTarget, listenMix - mixStep);

        if (outputGain != outputGainTarget)
        {
            outputGain = outputGainTarget + (outputGain - outputGainTarget) * gainCoef;

            if (std::abs (outputGain - outputGainTarget) < 1.0e-6f)
                outputGain = outputGainTarget;
        }

        for (int c = 0; c < active; ++c)
        {
            auto& ch = chans[(size_t) c];
            const float in = channels[c][i];

            const float dry = ch.dry[(size_t) dryPosition];
            ch.dry[(size_t) dryPosition] = in;

            // Invalid input (NaN, infinity) must not poison the analysis.
            ch.input[(size_t) position] = std::abs (in) < 1.0e10f ? in : 0.0f;

            const float processed = ch.output[(size_t) position];
            ch.output[(size_t) position] = 0.0f;

            float signal = processed * outputGain;

            if (listenMix > 0.0f)
                signal += listenMix * ((dry - processed) - signal);

            float out;

            if (activeMix >= 1.0f)
                out = signal;
            else if (activeMix <= 0.0f)
                out = dry;
            else
                out = dry + activeMix * (signal - dry);

            channels[c][i] = out;
        }

        position = (position + 1) & mask;
        dryPosition = dryPosition + 1 == latency ? 0 : dryPosition + 1;

        if (++hopCounter == hop)
        {
            hopCounter = 0;
            processFrame();
        }
    }
}

void DeEsserEngine::processFrame() noexcept
{
    const int mask = frameSize - 1;
    const int half = frameSize / 2;
    const int ringFrames = lookaheadFrames + 1;
    const auto slotOffset = (size_t) spectrumIndex * (size_t) bins;

    layout.setBand (params.bandLowHz, params.bandHighHz);

    // --- analysis: newest frame of each channel ------------------------------
    for (int c = 0; c < channelCount; ++c)
    {
        auto& ch = chans[(size_t) c];
        float* re = workRe.data();
        float* im = workIm.data();

        // position is the next write index, i.e. the oldest sample of the ring.
        for (int j = 0; j < frameSize; ++j)
        {
            re[j] = ch.input[(size_t) ((position + j) & mask)] * window[(size_t) j];
            im[j] = 0.0f;
        }

        fft->forward (re, im);

        float* storedRe = ch.spectraRe.data() + slotOffset;
        float* storedIm = ch.spectraIm.data() + slotOffset;
        float* power = paths[(size_t) c].power.data();

        for (int k = 0; k <= half; ++k)
        {
            storedRe[k] = re[k];
            storedIm[k] = im[k];
            power[k] = (re[k] * re[k] + im[k] * im[k]) * powerScale;
        }
    }

    spectrumIndex = (spectrumIndex + 1) % ringFrames;
    const auto oldestOffset = (size_t) spectrumIndex * (size_t) bins; // frame to synthesise

    if (channelCount == 2)
    {
        const float* a = paths[0].power.data();
        const float* b = paths[1].power.data();
        float* linked = paths[2].power.data();

        for (int k = 0; k < bins; ++k)
            linked[k] = 0.5f * (a[k] + b[k]);
    }

    // --- detection and gains, per path ---------------------------------------
    const float releaseCoef = onePoleCoef ((double) params.releaseMs * 0.001, frameSeconds);
    const float amountScale = std::clamp (params.amount / kAmountRamp, 0.0f, 1.0f);
    float sibilance = 0.0f;

    for (int p = 0; p < pathCount; ++p)
    {
        auto& path = paths[(size_t) p];
        const float raw = path.detector.analyse (path.power.data(), layout, params.mode, params.amount);
        path.reducer.pushFrame (path.power.data(), raw, path.detector.lastFrameUsable());

        path.raw[(size_t) path.rawIndex] = raw;
        path.rawIndex = (path.rawIndex + 1) % ringFrames;

        // Lookahead: the frame being synthesised is the oldest one. Later frames
        // open the gain early, with a ramp, so the onset is never let through.
        float ahead = 0.0f;

        for (int i = 0; i < ringFrames; ++i)
        {
            const float weight = 1.0f - (float) i / (float) ringFrames;
            ahead = std::max (ahead, path.raw[(size_t) ((path.rawIndex + i) % ringFrames)] * weight);
        }

        path.envelope = std::max (ahead, path.envelope * releaseCoef);

        if (path.envelope < 1.0e-4f)
            path.envelope = 0.0f;

        const float depthDb = params.rangeDb * path.envelope * amountScale;
        path.reducer.computeGains (layout, depthDb, params.focus, path.gainDb.data());
        sibilance = std::max (sibilance, path.envelope);
    }

    // --- synthesis of the delayed frame --------------------------------------
    const bool stereo = channelCount == 2;
    const float link = params.stereoLink;
    float maxReduction = 0.0f;
    std::fill (displayGain.begin(), displayGain.end(), 0.0f);

    for (int c = 0; c < channelCount; ++c)
    {
        auto& ch = chans[(size_t) c];
        float* re = workRe.data();
        float* im = workIm.data();
        const float* storedRe = ch.spectraRe.data() + oldestOffset;
        const float* storedIm = ch.spectraIm.data() + oldestOffset;
        const float* own = paths[(size_t) c].gainDb.data();
        const float* linked = stereo ? paths[2].gainDb.data() : own;

        for (int k = 0; k <= half; ++k)
        {
            const float g = stereo ? own[k] + (linked[k] - own[k]) * link : own[k];
            float r = storedRe[k];
            float x = storedIm[k];

            if (g < kUnityThresholdDb)
            {
                const float gain = dbToGain (g);
                r *= gain;
                x *= gain;
                displayGain[(size_t) k] = std::min (displayGain[(size_t) k], g);
                maxReduction = std::max (maxReduction, -g);
            }

            re[k] = r;
            im[k] = x;
        }

        // Rebuild the conjugate-symmetric half so the inverse transform is real.
        for (int k = 1; k < half; ++k)
        {
            re[frameSize - k] = re[k];
            im[frameSize - k] = -im[k];
        }

        fft->inverse (re, im);

        for (int j = 0; j < frameSize; ++j)
            ch.output[(size_t) ((position + j) & mask)] += re[j] * window[(size_t) j] * 0.5f;
    }

    if (listener != nullptr)
    {
        FrameSnapshot snapshot;
        snapshot.envelopePower = paths[(size_t) (pathCount - 1)].reducer.oldestEnvelope();
        snapshot.gainDb = displayGain.data();
        snapshot.numBins = bins;
        snapshot.fftSize = frameSize;
        snapshot.sampleRate = rate;
        snapshot.sibilance = sibilance;
        snapshot.reductionDb = maxReduction;
        listener->frameProcessed (snapshot);
    }
}

} // namespace deessr
