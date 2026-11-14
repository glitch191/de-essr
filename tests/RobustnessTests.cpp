#include "TestSignals.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <random>

using deessr::DeEsserEngine;
using deessr::Mode;
using deessr::Params;

namespace
{
    bool allFinite (const std::vector<float>& samples)
    {
        for (float s : samples)
            if (! std::isfinite (s))
                return false;

        return true;
    }

    Params randomParams (std::mt19937& generator)
    {
        std::uniform_real_distribution<float> unit (0.0f, 1.0f);
        Params p;
        p.mode = unit (generator) < 0.5f ? Mode::vocal : Mode::bus;
        p.amount = unit (generator);
        p.rangeDb = 24.0f * unit (generator);
        p.focus = unit (generator);
        p.bandLowHz = 2000.0f + 14000.0f * unit (generator);
        p.bandHighHz = 2000.0f + 14000.0f * unit (generator);
        p.releaseMs = 20.0f + 280.0f * unit (generator);
        p.stereoLink = unit (generator);
        p.outputGainDb = -12.0f + 24.0f * unit (generator);
        p.listen = unit (generator) < 0.3f;
        p.bypass = unit (generator) < 0.3f;
        return p;
    }
}

TEST_CASE ("Output stays finite on difficult input", "[robustness]")
{
    const double sampleRate = 48000.0;
    const int numSamples = 48000;
    std::mt19937 generator (7);
    std::uniform_real_distribution<float> noise (-1.0f, 1.0f);

    std::vector<std::vector<float>> inputs;
    inputs.emplace_back ((size_t) numSamples, 0.0f);                 // silence
    inputs.emplace_back ((size_t) numSamples, 1.0f);                 // DC at full scale
    inputs.emplace_back ((size_t) numSamples, 1.0e-38f);             // denormal range
    inputs.emplace_back ((size_t) numSamples);                       // full-scale white noise
    inputs.emplace_back ((size_t) numSamples);                       // Nyquist square
    inputs.emplace_back ((size_t) numSamples, 0.0f);                 // sparse impulses
    inputs.emplace_back ((size_t) numSamples);                       // noise with invalid samples

    for (int i = 0; i < numSamples; ++i)
    {
        inputs[3][(size_t) i] = noise (generator);
        inputs[4][(size_t) i] = (i & 1) ? 1.0f : -1.0f;
        inputs[6][(size_t) i] = 0.3f * noise (generator);

        if (i % 5000 == 0)
            inputs[5][(size_t) i] = 1.0f;
    }

    inputs[6][10000] = std::numeric_limits<float>::quiet_NaN();
    inputs[6][20000] = std::numeric_limits<float>::infinity();
    inputs[6][30000] = -std::numeric_limits<float>::infinity();

    for (const Mode mode : { Mode::vocal, Mode::bus })
    {
        for (size_t index = 0; index < inputs.size(); ++index)
        {
            CAPTURE (index);
            DeEsserEngine engine;
            engine.prepare (sampleRate, 2);
            Params params;
            params.mode = mode;
            params.amount = 1.0f;
            params.rangeDb = 24.0f;
            engine.setParams (params);

            const auto out = testsig::run (engine, { inputs[index], inputs[index] }, 256);
            REQUIRE (allFinite (out[0]));
            REQUIRE (allFinite (out[1]));
            REQUIRE (testsig::peak (out[0]) < 4.0f);

            // The engine keeps working after invalid input.
            const auto sequence = testsig::speechLike (sampleRate, 1, -6.0f);
            const auto after = testsig::run (engine, { sequence.samples, sequence.samples }, 256);
            REQUIRE (allFinite (after[0]));
            REQUIRE (testsig::peak (after[0]) > 0.05f);
        }
    }
}

TEST_CASE ("Random parameter changes never produce invalid or runaway output", "[robustness][parameters]")
{
    const double sampleRate = 48000.0;
    const auto sequence = testsig::speechLike (sampleRate, 6, -3.0f, 0.0f);
    const float inputPeak = testsig::peak (sequence.samples);
    std::mt19937 generator (123);
    std::uniform_int_distribution<int> blockSizes (1, 700);

    DeEsserEngine engine;
    engine.prepare (sampleRate, 2);

    auto left = sequence.samples;
    auto right = sequence.samples;
    int offset = 0;
    const int total = (int) left.size();

    while (offset < total)
    {
        engine.setParams (randomParams (generator));
        const int block = std::min (blockSizes (generator), total - offset);
        float* channels[2] = { left.data() + offset, right.data() + offset };
        engine.process (channels, 2, block);
        offset += block;
    }

    REQUIRE (allFinite (left));
    REQUIRE (allFinite (right));

    // Output gain is at most +12 dB (x4); allow a small margin for the overlap-add.
    REQUIRE (testsig::peak (left) < inputPeak * 4.5f);
}

TEST_CASE ("Output stage changes do not click", "[robustness][parameters]")
{
    // A slow sine has a small sample-to-sample step, so any discontinuity shows up
    // as a step far above it. Bypass, Listen and Output Gain are toggled at random.
    const double sampleRate = 48000.0;
    const int numSamples = 96000;
    const double frequency = 200.0;
    const float amplitude = 0.5f;
    std::vector<float> signal ((size_t) numSamples);

    for (int i = 0; i < numSamples; ++i)
        signal[(size_t) i] = amplitude * (float) std::sin (2.0 * 3.14159265358979323846 * frequency * (double) i / sampleRate);

    std::mt19937 generator (99);
    DeEsserEngine engine;
    engine.prepare (sampleRate, 1);
    engine.setParams (Params {});

    for (int offset = 0; offset < numSamples; offset += 1000)
    {
        engine.setParams (randomParams (generator));
        float* channels[1] = { signal.data() + offset };
        engine.process (channels, 1, std::min (1000, numSamples - offset));
    }

    // Largest natural step of the sine at +12 dB, plus the fastest cross-fade (10 ms).
    const float sineStep = 4.0f * amplitude * (float) (2.0 * 3.14159265358979323846 * frequency / sampleRate);
    const float fadeStep = 4.0f * amplitude / (0.010f * (float) sampleRate);
    float worst = 0.0f;

    for (int i = 1; i < numSamples; ++i)
        worst = std::max (worst, std::abs (signal[(size_t) i] - signal[(size_t) (i - 1)]));

    CAPTURE (worst, sineStep, fadeStep);
    REQUIRE (worst < 1.5f * (sineStep + 2.0f * fadeStep));
}

TEST_CASE ("Sibilant band edges are sanitised", "[robustness][parameters]")
{
    deessr::BandLayout layout;
    layout.prepare (44100.0, 512);

    layout.setBand (16000.0f, 2000.0f); // inverted
    REQUIRE (layout.lowHz < layout.highHz);
    REQUIRE (layout.highHz / layout.lowHz >= deessr::limits::bandMinRatio * 0.999f);
    REQUIRE (layout.sibLo <= layout.sibHi);

    layout.setBand (100.0f, 90000.0f); // out of range
    REQUIRE (layout.lowHz >= deessr::limits::bandMinHz);
    REQUIRE (layout.highHz <= deessr::limits::bandMaxHz);
}
