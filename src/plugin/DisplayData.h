#pragma once

#include "dsp/DeEsserEngine.h"

#include <juce_core/juce_core.h>

#include <array>
#include <atomic>

/** Carries display data from the audio thread to the editor without locks or
    allocation.

    Spectrum and gain curve: one atomic float per bin. The editor may read a mix
    of two consecutive frames, which is invisible on screen.
    History: a single-producer, single-consumer queue, one point per STFT frame.
    When the editor is closed the queue fills up and new points are dropped.
*/
class DisplayData final : public deessr::FrameListener
{
public:
    static constexpr int maxBins = 1025;      // FFT of 2048 points at 176.4 and 192 kHz
    static constexpr int historyCapacity = 2048;

    struct HistoryPoint
    {
        float sibilance = 0.0f;
        float reductionDb = 0.0f;
    };

    // --- audio thread -------------------------------------------------------
    void frameProcessed (const deessr::FrameSnapshot& snapshot) noexcept override
    {
        const int count = juce::jmin (snapshot.numBins, maxBins);

        for (int k = 0; k < count; ++k)
        {
            envelope[(size_t) k].store (snapshot.envelopePower[k], std::memory_order_relaxed);
            gain[(size_t) k].store (snapshot.gainDb[k], std::memory_order_relaxed);
        }

        numBins.store (count, std::memory_order_relaxed);
        binHz.store ((float) (snapshot.sampleRate / (double) snapshot.fftSize), std::memory_order_relaxed);

        int start1, size1, start2, size2;
        fifo.prepareToWrite (1, start1, size1, start2, size2);

        if (size1 + size2 > 0)
        {
            history[(size_t) (size1 > 0 ? start1 : start2)] = { snapshot.sibilance, snapshot.reductionDb };
            fifo.finishedWrite (1);
        }
    }

    // --- message thread -----------------------------------------------------
    int readHistory (HistoryPoint* destination, int maxPoints) noexcept
    {
        int start1, size1, start2, size2;
        fifo.prepareToRead (maxPoints, start1, size1, start2, size2);

        for (int i = 0; i < size1; ++i)
            destination[i] = history[(size_t) (start1 + i)];

        for (int i = 0; i < size2; ++i)
            destination[size1 + i] = history[(size_t) (start2 + i)];

        fifo.finishedRead (size1 + size2);
        return size1 + size2;
    }

    int getNumBins() const noexcept { return numBins.load (std::memory_order_relaxed); }
    float getBinHz() const noexcept { return binHz.load (std::memory_order_relaxed); }
    float getEnvelope (int bin) const noexcept { return envelope[(size_t) bin].load (std::memory_order_relaxed); }
    float getGainDb (int bin) const noexcept { return gain[(size_t) bin].load (std::memory_order_relaxed); }

private:
    std::array<std::atomic<float>, maxBins> envelope {};
    std::array<std::atomic<float>, maxBins> gain {};
    std::atomic<int> numBins { 0 };
    std::atomic<float> binHz { 93.75f };

    juce::AbstractFifo fifo { historyCapacity };
    std::array<HistoryPoint, historyCapacity> history {};
};
