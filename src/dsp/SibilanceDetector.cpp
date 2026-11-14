#include "dsp/SibilanceDetector.h"

#include "dsp/DspMath.h"

namespace deessr
{

namespace
{
    constexpr float kSilenceDb = -70.0f;          // absolute gate, only freezes the references
    constexpr float kRelativeGateDb = 25.0f;      // frames this far below the source level are ignored
    constexpr float kHysteresisDb = 2.0f;

    // Vocal mode
    constexpr float kRatioCentreDb = -6.0f;       // sibilant band vs low band
    constexpr float kRatioWidthDb = 2.0f;
    constexpr float kCentroidCentreHz = 2200.0f;
    constexpr float kCentroidWidthHz = 350.0f;
    constexpr float kLevelWidthDb = 3.0f;
    constexpr float kLevelAtZeroAmountDb = 6.0f;  // sibilant band vs source level, Amount = 0
    constexpr float kLevelAtFullAmountDb = -30.0f; // same, Amount = 1
    constexpr float kTonalCentre = 0.8f;          // share of band energy held by one spectral line
    constexpr float kTonalWidth = 0.05f;

    // Bus mode
    constexpr float kBusExcessAtZeroAmountDb = 12.0f;
    constexpr float kBusExcessAtFullAmountDb = 1.0f;
    constexpr float kBusWidthDb = 1.5f;

    constexpr float kFloor = 0.02f;               // results below this are exactly zero
}

void SibilanceDetector::prepare (double frameSeconds)
{
    levelUpCoef = onePoleCoef (0.3, frameSeconds);
    levelDownCoef = onePoleCoef (3.0, frameSeconds);
    levelSlowCoef = onePoleCoef (12.0, frameSeconds);
    busCoef = onePoleCoef (3.0, frameSeconds);
    busSlowCoef = onePoleCoef (18.0, frameSeconds);
    busWarmCoef = onePoleCoef (0.1, frameSeconds);
    busWarmupFrames = std::max (1, (int) (0.4 / frameSeconds));
    reset();
}

void SibilanceDetector::reset() noexcept
{
    levelValid = false;
    levelDb = -100.0f;
    busValid = false;
    busWarmCount = 0;
    busSibDb = -100.0f;
    busRatioDb = 0.0f;
    previous = 0.0f;
    usable = false;
}

float SibilanceDetector::analyse (const float* power, const BandLayout& layout, Mode mode, float amount) noexcept
{
    // --- energies -----------------------------------------------------------
    double total = 0.0, centroidSum = 0.0;

    for (int k = layout.totalLo; k <= layout.totalHi; ++k)
    {
        total += (double) power[k];
        centroidSum += (double) power[k] * (double) k;
    }

    double low = 0.0;

    for (int k = layout.lowBandLo; k <= layout.lowBandHi; ++k)
        low += (double) power[k];

    double sib = 0.0;
    float peak = 0.0f;
    int peakBin = layout.sibLo;

    for (int k = layout.sibLo; k <= layout.sibHi; ++k)
    {
        const float weighted = power[k] * layout.weight[(size_t) k];
        sib += (double) weighted;

        if (weighted > peak)
        {
            peak = weighted;
            peakBin = k;
        }
    }

    // Also rejects NaN and infinity: the comparison is false for both.
    const bool finite = total < 1.0e12;
    const float totalDb = finite ? powerToDb (total) : -200.0f;

    if (! finite || totalDb < kSilenceDb)
    {
        previous = 0.0f;
        usable = false;
        return 0.0f;
    }

    const float sibDb = powerToDb (sib);
    const float ratioDb = sibDb - powerToDb (low);
    const float hysteresis = previous > 0.5f ? kHysteresisDb : 0.0f;

    // --- long-term level of the source (vocal reference) ---------------------
    if (! levelValid)
    {
        levelDb = totalDb;
        levelValid = true;
    }
    else if (totalDb > levelDb - kRelativeGateDb)
    {
        const float coef = totalDb > levelDb ? levelUpCoef
                                             : (previous > 0.5f ? levelSlowCoef : levelDownCoef);
        levelDb = totalDb + (levelDb - totalDb) * coef;
    }

    usable = totalDb > levelDb - kRelativeGateDb;

    // --- vocal criteria -----------------------------------------------------
    float vocal = 0.0f;
    {
        const float centroidHz = (float) (centroidSum / std::max (total, 1.0e-30) * layout.binHz);

        float line = peak;

        if (peakBin > layout.sibLo)
            line += power[peakBin - 1] * layout.weight[(size_t) (peakBin - 1)];

        if (peakBin < layout.sibHi)
            line += power[peakBin + 1] * layout.weight[(size_t) (peakBin + 1)];

        const float tonality = (float) ((double) line / std::max (sib, 1.0e-30));
        const float levelCentre = kLevelAtZeroAmountDb + (kLevelAtFullAmountDb - kLevelAtZeroAmountDb) * amount;

        const float ratioScore = sigmoid ((ratioDb + hysteresis - kRatioCentreDb) / kRatioWidthDb);
        const float centroidScore = sigmoid ((centroidHz - kCentroidCentreHz) / kCentroidWidthHz);
        const float levelScore = sigmoid ((sibDb - levelDb + hysteresis - levelCentre) / kLevelWidthDb);
        const float tonalScore = 1.0f - sigmoid ((tonality - kTonalCentre) / kTonalWidth);

        vocal = ratioScore * centroidScore * levelScore * tonalScore;
    }

    // --- bus criteria -------------------------------------------------------
    float bus = 0.0f;
    const bool warmingUp = busWarmCount < busWarmupFrames;

    if (! busValid)
    {
        busSibDb = sibDb;
        busRatioDb = ratioDb;
        busValid = true;
    }

    if (! warmingUp)
    {
        const float excess = std::min (sibDb - busSibDb, ratioDb - busRatioDb) + hysteresis;
        const float centre = kBusExcessAtZeroAmountDb + (kBusExcessAtFullAmountDb - kBusExcessAtZeroAmountDb) * amount;
        bus = sigmoid ((excess - centre) / kBusWidthDb);
    }

    {
        const float coef = warmingUp ? busWarmCoef : (bus > 0.5f ? busSlowCoef : busCoef);
        busSibDb = sibDb + (busSibDb - sibDb) * coef;
        busRatioDb = ratioDb + (busRatioDb - ratioDb) * coef;

        if (warmingUp)
            ++busWarmCount;
    }

    // --- result -------------------------------------------------------------
    float result = mode == Mode::vocal ? vocal : bus;
    result = std::clamp ((result - kFloor) / (1.0f - 2.0f * kFloor), 0.0f, 1.0f);
    previous = result;
    return result;
}

} // namespace deessr
