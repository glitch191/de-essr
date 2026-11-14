#include "TestSignals.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <cmath>

using deessr::DeEsserEngine;
using deessr::Mode;
using deessr::Params;

namespace
{
    constexpr double kSibLow = 4000.0;
    constexpr double kSibHigh = 12000.0;

    struct MaxReductionListener final : deessr::FrameListener
    {
        void frameProcessed (const deessr::FrameSnapshot& snapshot) noexcept override
        {
            maxReductionDb = std::max (maxReductionDb, snapshot.reductionDb);
            maxSibilance = std::max (maxSibilance, snapshot.sibilance);

            for (int k = 0; k < snapshot.numBins; ++k)
                maxGainDb = std::max (maxGainDb, snapshot.gainDb[k]);
        }

        float maxReductionDb = 0.0f;
        float maxSibilance = 0.0f;
        float maxGainDb = -1000.0f;
    };

    struct Result
    {
        double sibilantReductionDb = 0.0; // sibilant band, during the bursts
        double voicedReductionDb = 0.0;   // whole spectrum, during the voiced segments
    };

    Result measure (const testsig::Sequence& sequence, double sampleRate, const Params& params)
    {
        DeEsserEngine engine;
        engine.prepare (sampleRate, 1);
        engine.setParams (params);
        const auto out = testsig::run (engine, { sequence.samples }, 512)[0];
        const int latency = engine.latencySamples();

        // Skip the first cycles: the long-term references need a moment to settle.
        const int firstBurst = (int) sequence.sibilant.size() / 2;
        const int firstVoiced = (int) sequence.voiced.size() / 2;

        Result result;
        result.sibilantReductionDb = testsig::reductionDb (sequence.samples, out, latency, sequence.sibilant,
                                                           firstBurst, sampleRate, kSibLow, kSibHigh);
        result.voicedReductionDb = testsig::reductionDb (sequence.samples, out, latency, sequence.voiced,
                                                         firstVoiced, sampleRate, 80.0, 20000.0);
        return result;
    }
}

TEST_CASE ("Detection does not depend on the input level", "[detection][level]")
{
    const double sampleRate = GENERATE (44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0);
    CAPTURE (sampleRate);

    const Params params; // default settings
    const auto loud = measure (testsig::speechLike (sampleRate, 6, -10.0f), sampleRate, params);
    const auto quiet = measure (testsig::speechLike (sampleRate, 6, -30.0f), sampleRate, params);

    CAPTURE (loud.sibilantReductionDb, quiet.sibilantReductionDb, loud.voicedReductionDb, quiet.voicedReductionDb);

    // The sibilant bursts are clearly reduced at both levels...
    REQUIRE (loud.sibilantReductionDb > 3.0);
    REQUIRE (quiet.sibilantReductionDb > 3.0);

    // ...by the same amount, within 2 dB...
    REQUIRE (std::abs (loud.sibilantReductionDb - quiet.sibilantReductionDb) <= 2.0);

    // ...and the voiced segments are left alone.
    REQUIRE (std::abs (loud.voicedReductionDb) < 0.5);
    REQUIRE (std::abs (quiet.voicedReductionDb) < 0.5);
}

TEST_CASE ("A very quiet take is still de-essed", "[detection][level]")
{
    const double sampleRate = 48000.0;
    const auto result = measure (testsig::speechLike (sampleRate, 6, -45.0f), sampleRate, Params {});
    REQUIRE (result.sibilantReductionDb > 3.0);
    REQUIRE (std::abs (result.voicedReductionDb) < 0.5);
}

TEST_CASE ("The reduction never exceeds Range and never amplifies", "[reduction][range]")
{
    const double sampleRate = 48000.0;
    const float rangeDb = GENERATE (3.0f, 9.0f, 24.0f);
    const float focus = GENERATE (0.0f, 0.5f, 1.0f);
    CAPTURE (rangeDb, focus);

    const auto sequence = testsig::speechLike (sampleRate, 5, -6.0f, 0.0f);
    Params params;
    params.amount = 1.0f;
    params.rangeDb = rangeDb;
    params.focus = focus;

    DeEsserEngine engine;
    MaxReductionListener listener;
    engine.prepare (sampleRate, 1);
    engine.setParams (params);
    engine.setListener (&listener);
    const auto out = testsig::run (engine, { sequence.samples }, 512)[0];

    // Per-bin gains reported by the engine.
    REQUIRE (listener.maxSibilance > 0.9f);
    REQUIRE (listener.maxReductionDb > 0.5f * rangeDb);
    REQUIRE (listener.maxReductionDb <= rangeDb + 1.0e-3f);
    REQUIRE (listener.maxGainDb <= 0.0f);

    // Measured on the audio.
    const double measured = testsig::reductionDb (sequence.samples, out, engine.latencySamples(), sequence.sibilant,
                                                  2, sampleRate, kSibLow, kSibHigh);
    CAPTURE (measured);
    REQUIRE (measured > 0.5);
    REQUIRE (measured <= (double) rangeDb + 0.2);

    const double voiced = testsig::reductionDb (sequence.samples, out, engine.latencySamples(), sequence.voiced,
                                                4, sampleRate, 80.0, 20000.0);
    REQUIRE (voiced > -0.1); // no gain anywhere
}

TEST_CASE ("Focus goes from a wide cut to shaving only the peaks", "[reduction][focus]")
{
    const double sampleRate = 48000.0;
    const auto sequence = testsig::speechLike (sampleRate, 6, -6.0f, 0.0f);
    Params params;
    params.amount = 1.0f;
    params.rangeDb = 12.0f;

    params.focus = 0.0f;
    const auto wide = measure (sequence, sampleRate, params);
    params.focus = 1.0f;
    const auto surgical = measure (sequence, sampleRate, params);

    CAPTURE (wide.sibilantReductionDb, surgical.sibilantReductionDb);
    REQUIRE (wide.sibilantReductionDb > 10.0);    // close to Range over the whole band
    REQUIRE (surgical.sibilantReductionDb > 1.0); // still audible
    REQUIRE (surgical.sibilantReductionDb < wide.sibilantReductionDb - 1.0);
}

TEST_CASE ("Energy outside the sibilant band is preserved", "[reduction][band]")
{
    const double sampleRate = 48000.0;
    const auto sequence = testsig::overlappedSpeech (sampleRate, 8, -6.0f, 3.0f);
    Params params;
    params.amount = 0.8f;
    params.rangeDb = 12.0f;
    params.focus = 0.0f;

    DeEsserEngine engine;
    engine.prepare (sampleRate, 1);
    engine.setParams (params);
    const auto out = testsig::run (engine, { sequence.samples }, 512)[0];
    const int latency = engine.latencySamples();

    const double inBand = testsig::reductionDb (sequence.samples, out, latency, sequence.sibilant, 4,
                                                sampleRate, kSibLow, kSibHigh);
    const double below = testsig::reductionDb (sequence.samples, out, latency, sequence.sibilant, 4,
                                               sampleRate, 80.0, 3000.0);
    const double above = testsig::reductionDb (sequence.samples, out, latency, sequence.sibilant, 4,
                                               sampleRate, 14500.0, 20000.0);
    CAPTURE (inBand, below, above);
    REQUIRE (inBand > 3.0);
    REQUIRE (std::abs (below) < 0.5);
    REQUIRE (std::abs (above) < 0.5);
}

TEST_CASE ("The sibilant band setting moves the processing", "[reduction][band]")
{
    const double sampleRate = 48000.0;
    const auto sequence = testsig::speechLike (sampleRate, 6, -6.0f, 0.0f);
    Params params;
    params.amount = 1.0f;
    params.rangeDb = 12.0f;
    params.focus = 0.0f;
    params.bandLowHz = 10000.0f;  // the bursts sit between 5 and 9 kHz, below this band
    params.bandHighHz = 16000.0f;

    DeEsserEngine engine;
    engine.prepare (sampleRate, 1);
    engine.setParams (params);
    const auto out = testsig::run (engine, { sequence.samples }, 512)[0];

    const double burstBand = testsig::reductionDb (sequence.samples, out, engine.latencySamples(), sequence.sibilant, 3,
                                                   sampleRate, 5000.0, 8000.0);
    REQUIRE (std::abs (burstBand) < 0.5);
}

TEST_CASE ("Full stereo link applies the same gain to both channels", "[stereo]")
{
    const double sampleRate = 48000.0;
    const auto sequence = testsig::speechLike (sampleRate, 5, -6.0f);
    auto right = sequence.samples;
    testsig::scale (right, 0.4f);

    Params params;
    params.amount = 0.8f;
    params.rangeDb = 12.0f;
    params.stereoLink = 1.0f;

    DeEsserEngine engine;
    engine.prepare (sampleRate, 2);
    engine.setParams (params);
    const auto out = testsig::run (engine, { sequence.samples, right }, 512);

    // Right must stay an exact scaled copy of left: the stereo image does not move.
    auto expectedRight = out[0];
    testsig::scale (expectedRight, 0.4f);
    REQUIRE (testsig::residualDb (expectedRight, out[1], 0) < -90.0);

    const double reduction = testsig::reductionDb (sequence.samples, out[0], engine.latencySamples(), sequence.sibilant,
                                                   2, sampleRate, kSibLow, kSibHigh);
    REQUIRE (reduction > 3.0);
}

TEST_CASE ("Stereo link decides whether one channel drives the other", "[stereo]")
{
    const double sampleRate = 48000.0;
    const auto left = testsig::speechLike (sampleRate, 6, -6.0f, 3.0f);
    const int numSamples = (int) left.samples.size();

    // Right: a steady voiced tone with a faint high-frequency bed, and no sibilance of its own.
    auto right = testsig::vowel (sampleRate, numSamples, 120.0);
    const auto bed = testsig::bandNoise (sampleRate, numSamples, 5000.0, 9000.0, 77u);

    for (int i = 0; i < numSamples; ++i)
        right[(size_t) i] = 0.2f * right[(size_t) i] + 0.004f * bed[(size_t) i];

    Params params;
    params.amount = 1.0f;
    params.rangeDb = 12.0f;
    params.focus = 0.0f;

    const auto rightBandReduction = [&] (float link)
    {
        params.stereoLink = link;
        DeEsserEngine engine;
        engine.prepare (sampleRate, 2);
        engine.setParams (params);
        const auto out = testsig::run (engine, { left.samples, right }, 512);
        return testsig::reductionDb (right, out[1], engine.latencySamples(), left.sibilant, 3,
                                     sampleRate, 5000.0, 9000.0);
    };

    const double linked = rightBandReduction (1.0f);
    const double independent = rightBandReduction (0.0f);
    CAPTURE (linked, independent);
    REQUIRE (linked > 3.0);                 // left's sibilance pulls right down with it
    REQUIRE (std::abs (independent) < 0.5); // right is left alone
}

TEST_CASE ("Bus mode handles a voice inside a mix, Vocal mode does not", "[detection][bus]")
{
    const double sampleRate = 48000.0;
    auto sequence = testsig::speechLike (sampleRate, 8, -12.0f);
    const int numSamples = (int) sequence.samples.size();

    // Backing track: 6 dB above the voice, with its own steady high-frequency content.
    double voiceEnergy = 0.0;

    for (const auto& segment : sequence.voiced)
        for (int i = 0; i < segment.length; ++i)
            voiceEnergy += std::pow ((double) sequence.samples[(size_t) (segment.start + i)], 2.0);

    int voicedSamples = 0;

    for (const auto& segment : sequence.voiced)
        voicedSamples += segment.length;

    const auto voiceRms = (float) std::sqrt (voiceEnergy / (double) voicedSamples);
    const auto backing = testsig::accompaniment (sampleRate, numSamples, 5u);

    for (int i = 0; i < numSamples; ++i)
        sequence.samples[(size_t) i] += 2.0f * voiceRms * backing[(size_t) i];

    Params params;
    params.amount = 0.6f;
    params.rangeDb = 9.0f;

    params.mode = Mode::bus;
    const auto bus = measure (sequence, sampleRate, params);
    params.mode = Mode::vocal;
    const auto vocal = measure (sequence, sampleRate, params);

    CAPTURE (bus.sibilantReductionDb, bus.voicedReductionDb, vocal.sibilantReductionDb);

    // Bus mode finds the sibilance in the mix and leaves the rest alone.
    REQUIRE (bus.sibilantReductionDb > 2.0);
    REQUIRE (std::abs (bus.voicedReductionDb) < 0.5);

    // The steady high-frequency bed of the backing track is not reduced between bursts.
    {
        DeEsserEngine engine;
        engine.prepare (sampleRate, 1);
        params.mode = Mode::bus;
        engine.setParams (params);
        const auto out = testsig::run (engine, { sequence.samples }, 512)[0];
        const double bed = testsig::reductionDb (sequence.samples, out, engine.latencySamples(), sequence.voiced, 8,
                                                 sampleRate, kSibLow, kSibHigh, 0.08);
        CAPTURE (bed);
        REQUIRE (std::abs (bed) < 0.5);
    }

    // Vocal mode relies on voiced / unvoiced criteria that a mix does not satisfy.
    REQUIRE (vocal.sibilantReductionDb < bus.sibilantReductionDb - 1.0);
}
