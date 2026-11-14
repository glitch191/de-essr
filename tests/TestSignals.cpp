#include "TestSignals.h"

#include "dsp/Fft.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace testsig
{

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    void normaliseRms (std::vector<float>& samples)
    {
        double sum = 0.0;

        for (float s : samples)
            sum += (double) s * (double) s;

        if (sum <= 0.0)
            return;

        const auto gain = (float) (1.0 / std::sqrt (sum / (double) samples.size()));

        for (float& s : samples)
            s *= gain;
    }

    /** Second-order filter (RBJ cookbook), run twice in series for a steeper slope. */
    void filter (std::vector<float>& samples, double sampleRate, double frequencyHz, bool highPass)
    {
        const double w0 = 2.0 * kPi * frequencyHz / sampleRate;
        const double alpha = std::sin (w0) / (2.0 * 0.7071);
        const double cosw = std::cos (w0);
        const double a0 = 1.0 + alpha;
        const double b1 = (highPass ? -(1.0 + cosw) : (1.0 - cosw)) / a0;
        const double b0 = (highPass ? (1.0 + cosw) : (1.0 - cosw)) / (2.0 * a0);
        const double a1 = -2.0 * cosw / a0;
        const double a2 = (1.0 - alpha) / a0;

        for (int pass = 0; pass < 2; ++pass)
        {
            double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;

            for (float& s : samples)
            {
                const double x = (double) s;
                const double y = b0 * x + b1 * x1 + b0 * x2 - a1 * y1 - a2 * y2;
                x2 = x1;
                x1 = x;
                y2 = y1;
                y1 = y;
                s = (float) y;
            }
        }
    }

    void fade (std::vector<float>& samples, int fadeSamples)
    {
        const int n = (int) samples.size();
        const int length = std::min (fadeSamples, n / 2);

        for (int i = 0; i < length; ++i)
        {
            const auto g = (float) (0.5 - 0.5 * std::cos (kPi * (double) i / (double) length));
            samples[(size_t) i] *= g;
            samples[(size_t) (n - 1 - i)] *= g;
        }
    }

    void addAt (std::vector<float>& destination, const std::vector<float>& source, int start, float gain)
    {
        for (size_t i = 0; i < source.size() && (size_t) start + i < destination.size(); ++i)
            destination[(size_t) start + i] += source[i] * gain;
    }

    int toSamples (double seconds, double sampleRate)
    {
        return (int) std::lround (seconds * sampleRate);
    }
}

std::vector<float> vowel (double sampleRate, int numSamples, double fundamentalHz)
{
    std::vector<float> out ((size_t) numSamples, 0.0f);

    for (int h = 1; (double) h * fundamentalHz < 3400.0; ++h)
    {
        const double amplitude = 1.0 / std::pow ((double) h, 1.2);
        const double phase = 0.37 * (double) (h * h);
        const double step = 2.0 * kPi * fundamentalHz * (double) h / sampleRate;

        for (int i = 0; i < numSamples; ++i)
            out[(size_t) i] += (float) (amplitude * std::sin (step * (double) i + phase));
    }

    normaliseRms (out);
    return out;
}

std::vector<float> bandNoise (double sampleRate, int numSamples, double lowHz, double highHz, std::uint32_t seed)
{
    std::mt19937 generator (seed);
    std::uniform_real_distribution<float> distribution (-1.0f, 1.0f);
    std::vector<float> out ((size_t) numSamples);

    for (float& s : out)
        s = distribution (generator);

    filter (out, sampleRate, lowHz, true);
    filter (out, sampleRate, highHz, false);
    normaliseRms (out);
    return out;
}

std::vector<float> accompaniment (double sampleRate, int numSamples, std::uint32_t seed)
{
    std::vector<float> out ((size_t) numSamples, 0.0f);
    const double notes[] = { 55.0, 82.41, 110.0 };

    for (double note : notes)
    {
        for (int h = 1; (double) h * note < 2000.0; ++h)
        {
            const double amplitude = 1.0 / (double) h;
            const double step = 2.0 * kPi * note * (double) h / sampleRate;

            for (int i = 0; i < numSamples; ++i)
                out[(size_t) i] += (float) (amplitude * std::sin (step * (double) i + 0.9 * (double) h));
        }
    }

    normaliseRms (out);

    // Steady high-frequency bed, 30 dB below the chord.
    const auto bed = bandNoise (sampleRate, numSamples, 3000.0, 12000.0, seed);
    addAt (out, bed, 0, 0.0316f);
    return out;
}

Sequence speechLike (double sampleRate, int cycles, float peakDb, float burstRelativeDb)
{
    const int vowelA = toSamples (0.35, sampleRate);
    const int burst = toSamples (0.14, sampleRate);
    const int vowelB = toSamples (0.30, sampleRate);
    const int silence = toSamples (0.11, sampleRate);
    const int cycle = vowelA + burst + vowelB + silence;
    const int fadeLength = toSamples (0.008, sampleRate);
    const float burstGain = std::pow (10.0f, burstRelativeDb / 20.0f);

    Sequence sequence;
    sequence.samples.assign ((size_t) (cycle * cycles), 0.0f);

    for (int c = 0; c < cycles; ++c)
    {
        const int base = c * cycle;

        auto a = vowel (sampleRate, vowelA, 140.0);
        fade (a, fadeLength);
        addAt (sequence.samples, a, base, 1.0f);
        sequence.voiced.push_back ({ base, vowelA });

        auto s = bandNoise (sampleRate, burst, 5000.0, 9000.0, 1000u + (std::uint32_t) c);
        fade (s, fadeLength);
        addAt (sequence.samples, s, base + vowelA, burstGain);
        sequence.sibilant.push_back ({ base + vowelA, burst });

        auto b = vowel (sampleRate, vowelB, 165.0);
        fade (b, fadeLength);
        addAt (sequence.samples, b, base + vowelA + burst, 1.0f);
        sequence.voiced.push_back ({ base + vowelA + burst, vowelB });
    }

    scale (sequence.samples, std::pow (10.0f, peakDb / 20.0f) / peak (sequence.samples));
    return sequence;
}

Sequence overlappedSpeech (double sampleRate, int cycles, float peakDb, float burstRelativeDb)
{
    const int gap = toSamples (0.45, sampleRate);
    const int burst = toSamples (0.14, sampleRate);
    const int cycle = gap + burst;
    const int fadeLength = toSamples (0.008, sampleRate);
    const float burstGain = std::pow (10.0f, burstRelativeDb / 20.0f);

    Sequence sequence;
    sequence.samples = vowel (sampleRate, cycle * cycles, 150.0);

    for (int c = 0; c < cycles; ++c)
    {
        const int base = c * cycle;
        sequence.voiced.push_back ({ base, gap });

        auto s = bandNoise (sampleRate, burst, 5000.0, 9000.0, 2000u + (std::uint32_t) c);
        fade (s, fadeLength);
        addAt (sequence.samples, s, base + gap, burstGain);
        sequence.sibilant.push_back ({ base + gap, burst });
    }

    scale (sequence.samples, std::pow (10.0f, peakDb / 20.0f) / peak (sequence.samples));
    return sequence;
}

void scale (std::vector<float>& samples, float gain)
{
    for (float& s : samples)
        s *= gain;
}

float peak (const std::vector<float>& samples)
{
    float result = 0.0f;

    for (float s : samples)
        result = std::max (result, std::abs (s));

    return result;
}

double bandEnergy (const std::vector<float>& samples, int start, int length,
                   double sampleRate, double lowHz, double highHz)
{
    constexpr int order = 10;
    constexpr int size = 1 << order;
    static const deessr::Fft fft (order);

    std::vector<float> re ((size_t) size), im ((size_t) size);
    const int firstBin = std::max (1, (int) std::ceil (lowHz * size / sampleRate));
    const int lastBin = std::min (size / 2 - 1, (int) std::floor (highHz * size / sampleRate));
    double energy = 0.0;

    for (int offset = start; offset + size <= start + length && offset + size <= (int) samples.size(); offset += size / 2)
    {
        for (int i = 0; i < size; ++i)
        {
            const double w = 0.5 - 0.5 * std::cos (2.0 * kPi * (double) i / (double) size);
            re[(size_t) i] = samples[(size_t) (offset + i)] * (float) w;
            im[(size_t) i] = 0.0f;
        }

        fft.forward (re.data(), im.data());

        for (int k = firstBin; k <= lastBin; ++k)
            energy += (double) re[(size_t) k] * (double) re[(size_t) k] + (double) im[(size_t) k] * (double) im[(size_t) k];
    }

    return energy;
}

double reductionDb (const std::vector<float>& input, const std::vector<float>& output, int latency,
                    const std::vector<Segment>& segments, int firstSegment,
                    double sampleRate, double lowHz, double highHz, double marginSeconds)
{
    const int margin = toSamples (marginSeconds, sampleRate);
    double before = 0.0, after = 0.0;

    for (size_t i = (size_t) firstSegment; i < segments.size(); ++i)
    {
        const int start = segments[i].start + margin;
        const int length = segments[i].length - 2 * margin;
        before += bandEnergy (input, start, length, sampleRate, lowHz, highHz);
        after += bandEnergy (output, start + latency, length, sampleRate, lowHz, highHz);
    }

    return 10.0 * std::log10 (std::max (before, 1.0e-30) / std::max (after, 1.0e-30));
}

std::vector<std::vector<float>> run (deessr::DeEsserEngine& engine,
                                     const std::vector<std::vector<float>>& input, int blockSize)
{
    auto output = input;
    const int numChannels = (int) output.size();
    const int numSamples = (int) output[0].size();
    float* pointers[deessr::DeEsserEngine::maxChannels] = {};

    for (int offset = 0; offset < numSamples; offset += blockSize)
    {
        for (int c = 0; c < numChannels; ++c)
            pointers[c] = output[(size_t) c].data() + offset;

        engine.process (pointers, numChannels, std::min (blockSize, numSamples - offset));
    }

    return output;
}

double residualDb (const std::vector<float>& input, const std::vector<float>& output, int latency)
{
    double difference = 0.0, reference = 0.0;

    for (size_t i = 0; i + (size_t) latency < output.size(); ++i)
    {
        const double d = (double) output[i + (size_t) latency] - (double) input[i];
        difference += d * d;
        reference += (double) input[i] * (double) input[i];
    }

    return 10.0 * std::log10 (std::max (difference, 1.0e-30) / std::max (reference, 1.0e-30));
}

} // namespace testsig
