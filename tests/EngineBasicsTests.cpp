#include "TestSignals.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cmath>

using deessr::DeEsserEngine;
using deessr::Params;

namespace
{
    int peakIndex (const std::vector<float>& samples)
    {
        int index = 0;

        for (int i = 1; i < (int) samples.size(); ++i)
            if (std::abs (samples[(size_t) i]) > std::abs (samples[(size_t) index]))
                index = i;

        return index;
    }
}

TEST_CASE ("Latency is constant, reported correctly and below 20 ms", "[engine][latency]")
{
    const double sampleRate = GENERATE (44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0);
    CAPTURE (sampleRate);

    DeEsserEngine engine;
    engine.prepare (sampleRate, 1);

    const int latency = engine.latencySamples();
    REQUIRE (latency == DeEsserEngine::latencyFor (sampleRate));
    REQUIRE (latency == engine.fftSize() + DeEsserEngine::lookaheadFrames * engine.hopSize());
    REQUIRE ((double) latency / sampleRate <= 0.020);

    std::vector<float> impulse ((size_t) (latency * 4), 0.0f);
    const int at = 1000;
    impulse[(size_t) at] = 0.5f;

    SECTION ("with Amount at zero the impulse comes out intact, exactly latency samples later")
    {
        Params params;
        params.amount = 0.0f;
        engine.setParams (params);

        const auto out = testsig::run (engine, { impulse }, 512)[0];
        REQUIRE (peakIndex (out) == at + latency);
        REQUIRE (std::abs (out[(size_t) (at + latency)] - 0.5f) < 1.0e-5f);
    }

    SECTION ("settings do not move the latency")
    {
        Params params;
        params.amount = 1.0f;
        params.rangeDb = 24.0f;
        params.focus = 1.0f;
        params.bandLowHz = 2000.0f;
        params.bandHighHz = 16000.0f;
        params.releaseMs = 300.0f;
        engine.setParams (params);

        REQUIRE (engine.latencySamples() == latency);
        const auto out = testsig::run (engine, { impulse }, 512)[0];
        REQUIRE (peakIndex (out) == at + latency);
    }
}

TEST_CASE ("Amount at zero leaves the signal untouched", "[engine][null]")
{
    const double sampleRate = GENERATE (44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0);
    CAPTURE (sampleRate);

    const auto sequence = testsig::speechLike (sampleRate, 2, -6.0f);
    DeEsserEngine engine;
    engine.prepare (sampleRate, 1);

    Params params;
    params.amount = 0.0f;
    params.rangeDb = 24.0f;
    engine.setParams (params);

    const auto out = testsig::run (engine, { sequence.samples }, 512)[0];
    REQUIRE (testsig::residualDb (sequence.samples, out, engine.latencySamples()) < -90.0);
}

TEST_CASE ("A voiced signal passes unchanged even at full Amount", "[engine][null]")
{
    const double sampleRate = 48000.0;
    const auto tone = testsig::vowel (sampleRate, 96000);
    auto input = tone;
    testsig::scale (input, 0.2f);

    DeEsserEngine engine;
    engine.prepare (sampleRate, 1);

    Params params;
    params.amount = 1.0f;
    params.rangeDb = 24.0f;
    engine.setParams (params);

    const auto out = testsig::run (engine, { input }, 512)[0];
    REQUIRE (testsig::residualDb (input, out, engine.latencySamples()) < -90.0);
}

TEST_CASE ("Bypass outputs the input delayed by the latency", "[engine][bypass]")
{
    const double sampleRate = 48000.0;
    const auto sequence = testsig::speechLike (sampleRate, 2, -6.0f);
    DeEsserEngine engine;
    engine.prepare (sampleRate, 1);

    Params params;
    params.amount = 1.0f;
    params.bypass = true;
    engine.setParams (params);

    const auto out = testsig::run (engine, { sequence.samples }, 512)[0];
    const auto latency = (size_t) engine.latencySamples();
    bool identical = true;

    for (size_t i = 0; i + latency < out.size(); ++i)
        identical = identical && out[i + latency] == sequence.samples[i];

    REQUIRE (identical);
}

TEST_CASE ("Processed signal plus Listen signal equals the delayed input", "[engine][listen]")
{
    const double sampleRate = 48000.0;
    const auto sequence = testsig::speechLike (sampleRate, 4, -6.0f);

    Params params;
    params.amount = 0.8f;
    params.rangeDb = 12.0f;

    DeEsserEngine processedEngine;
    processedEngine.prepare (sampleRate, 1);
    processedEngine.setParams (params);
    const auto processed = testsig::run (processedEngine, { sequence.samples }, 512)[0];

    params.listen = true;
    DeEsserEngine listenEngine;
    listenEngine.prepare (sampleRate, 1);
    listenEngine.setParams (params);
    const auto removed = testsig::run (listenEngine, { sequence.samples }, 512)[0];

    // The removed signal must not be empty, otherwise the identity proves nothing.
    REQUIRE (testsig::peak (removed) > 1.0e-3f);

    auto sum = processed;

    for (size_t i = 0; i < sum.size(); ++i)
        sum[i] += removed[i];

    REQUIRE (testsig::residualDb (sequence.samples, sum, processedEngine.latencySamples()) < -90.0);
}

TEST_CASE ("The block size does not change the result", "[engine][blocks]")
{
    const double sampleRate = 48000.0;
    const auto sequence = testsig::speechLike (sampleRate, 2, -6.0f);
    Params params;
    params.amount = 0.8f;

    DeEsserEngine reference;
    reference.prepare (sampleRate, 1);
    reference.setParams (params);
    const auto expected = testsig::run (reference, { sequence.samples }, 512)[0];

    for (int blockSize : { 1, 7, 64, 127, 480, 4096 })
    {
        CAPTURE (blockSize);
        DeEsserEngine engine;
        engine.prepare (sampleRate, 1);
        engine.setParams (params);
        const auto out = testsig::run (engine, { sequence.samples }, blockSize)[0];
        REQUIRE (out == expected);
    }
}

TEST_CASE ("reset() returns the engine to its initial state", "[engine]")
{
    const double sampleRate = 48000.0;
    const auto sequence = testsig::speechLike (sampleRate, 2, -6.0f);
    DeEsserEngine engine;
    engine.prepare (sampleRate, 1);
    engine.setParams (Params {});

    const auto first = testsig::run (engine, { sequence.samples }, 512)[0];
    engine.reset();
    const auto second = testsig::run (engine, { sequence.samples }, 512)[0];
    REQUIRE (first == second);
}
