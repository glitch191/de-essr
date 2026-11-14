#include "TestSignals.h"

#include "dsp/FactoryPresets.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <set>
#include <string>

using deessr::DeEsserEngine;
using deessr::Mode;

TEST_CASE ("Factory presets are well formed", "[presets]")
{
    using namespace deessr;
    const auto presets = factoryPresets();
    REQUIRE (presets.size() >= 30);

    std::set<std::string> names;
    std::string lastCategory;
    std::set<std::string> closedCategories;

    for (const auto& preset : presets)
    {
        CAPTURE (preset.name);
        const auto& p = preset.params;

        REQUIRE (std::string (preset.name).size() > 0);
        REQUIRE (std::string (preset.category).size() > 0);
        REQUIRE (names.insert (std::string (preset.category) + "/" + preset.name).second); // unique

        REQUIRE (p.amount > 0.0f);
        REQUIRE (p.amount <= 1.0f);
        REQUIRE (p.rangeDb > 0.0f);
        REQUIRE (p.rangeDb <= limits::rangeMaxDb);
        REQUIRE (p.focus >= 0.0f);
        REQUIRE (p.focus <= 1.0f);
        REQUIRE (p.bandLowHz >= limits::bandMinHz);
        REQUIRE (p.bandHighHz <= limits::bandMaxHz);
        REQUIRE (p.bandHighHz >= p.bandLowHz * limits::bandMinRatio);
        REQUIRE (p.releaseMs >= limits::releaseMinMs);
        REQUIRE (p.releaseMs <= limits::releaseMaxMs);
        REQUIRE (p.stereoLink >= 0.0f);
        REQUIRE (p.stereoLink <= 1.0f);
        REQUIRE (p.outputGainDb == 0.0f);
        REQUIRE_FALSE (p.listen);
        REQUIRE_FALSE (p.bypass);

        // Categories are contiguous, so the menu can be built in one pass.
        if (preset.category != lastCategory)
        {
            REQUIRE (closedCategories.insert (lastCategory).second);
            lastCategory = preset.category;
        }
    }

    // The first preset is the plugin default.
    const Params defaults;
    const auto& first = presets.front().params;
    REQUIRE (first.mode == defaults.mode);
    REQUIRE (first.amount == defaults.amount);
    REQUIRE (first.rangeDb == defaults.rangeDb);
    REQUIRE (first.focus == defaults.focus);
    REQUIRE (first.bandLowHz == defaults.bandLowHz);
    REQUIRE (first.bandHighHz == defaults.bandHighHz);
    REQUIRE (first.releaseMs == defaults.releaseMs);
    REQUIRE (first.stereoLink == defaults.stereoLink);
}

TEST_CASE ("Every factory preset behaves on a speech-like signal", "[presets]")
{
    const double sampleRate = 48000.0;
    const auto sequence = testsig::speechLike (sampleRate, 6, -10.0f, 0.0f);

    for (const auto& preset : deessr::factoryPresets())
    {
        CAPTURE (preset.category, preset.name);

        DeEsserEngine engine;
        engine.prepare (sampleRate, 2);
        engine.setParams (preset.params);
        const auto out = testsig::run (engine, { sequence.samples, sequence.samples }, 512);
        const int latency = engine.latencySamples();

        bool finite = true;

        for (float s : out[0])
            finite = finite && std::isfinite (s);

        REQUIRE (finite);

        // Voiced segments stay intact and nothing is amplified.
        const double voiced = testsig::reductionDb (sequence.samples, out[0], latency, sequence.voiced, 6,
                                                   sampleRate, 80.0, 20000.0);
        REQUIRE (std::abs (voiced) < 0.5);

        // The bursts sit between 5 and 9 kHz. Measure where the preset's band overlaps them.
        const double low = std::max (5000.0, (double) preset.params.bandLowHz);
        const double high = std::min (9000.0, (double) preset.params.bandHighHz);
        const double reduction = testsig::reductionDb (sequence.samples, out[0], latency, sequence.sibilant, 3,
                                                       sampleRate, low, high);
        CAPTURE (reduction);
        REQUIRE (reduction > -0.1);
        REQUIRE (reduction <= (double) preset.params.rangeDb + 0.2);

        // Vocal presets must clearly act on a lone voice when their band covers the bursts.
        if (preset.params.mode == Mode::vocal && high - low >= 2000.0)
            REQUIRE (reduction > 1.0);
    }
}
