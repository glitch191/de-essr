// CPU benchmark of the DSP core: processes a synthetic stereo signal and reports
// the processing time as a percentage of real time (the share of one core).

#include "TestSignals.h"

#include <algorithm>
#include <chrono>
#include <cstdio>

int main()
{
    const double sampleRates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };
    const int blockSize = 512;

    std::printf ("de-essr DSP benchmark, one stereo instance, block of %d samples\n", blockSize);

    for (const double sampleRate : sampleRates)
    {
        const auto sequence = testsig::speechLike (sampleRate, 22, -6.0f, 0.0f); // about 20 seconds
        const double seconds = (double) sequence.samples.size() / sampleRate;

        deessr::DeEsserEngine engine;
        engine.prepare (sampleRate, 2);
        deessr::Params params;
        params.amount = 0.8f;
        params.stereoLink = 0.5f; // worst case: all three analysis paths contribute
        engine.setParams (params);

        double best = 1.0e9;

        for (int pass = 0; pass < 3; ++pass)
        {
            engine.reset();
            const auto start = std::chrono::steady_clock::now();
            const auto out = testsig::run (engine, { sequence.samples, sequence.samples }, blockSize);
            const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
            best = std::min (best, elapsed.count());

            if (out[0].empty())
                return 1;
        }

        std::printf ("  %6.1f kHz: %.2f %% of one core (%.1f s of audio in %.0f ms)\n",
                     sampleRate / 1000.0, 100.0 * best / seconds, seconds, best * 1000.0);
    }

    return 0;
}
