#include "dsp/FactoryPresets.h"

#include <array>

namespace deessr
{

namespace
{
    constexpr const char* sung = "Sung vocals";
    constexpr const char* spoken = "Spoken voice";
    constexpr const char* sends = "Effect sends";
    constexpr const char* instruments = "Instruments";
    constexpr const char* buses = "Buses and master";

    /** Amount, Focus and Stereo Link are given in percent, as shown in the editor. */
    constexpr FactoryPreset make (const char* category, const char* name, Mode mode,
                                  float amountPercent, float rangeDb, float focusPercent,
                                  float bandLowHz, float bandHighHz, float releaseMs,
                                  float stereoLinkPercent = 100.0f)
    {
        Params p;
        p.mode = mode;
        p.amount = amountPercent * 0.01f;
        p.rangeDb = rangeDb;
        p.focus = focusPercent * 0.01f;
        p.bandLowHz = bandLowHz;
        p.bandHighHz = bandHighHz;
        p.releaseMs = releaseMs;
        p.stereoLink = stereoLinkPercent * 0.01f;
        p.outputGainDb = 0.0f;
        return { category, name, p };
    }

    constexpr Mode V = Mode::vocal;
    constexpr Mode B = Mode::bus;

    // Starting points drawn from common mixing practice: where sibilance usually
    // sits for each kind of source, and how much reduction stays natural.
    //                                               Amount Range Focus  Low     High   Release Link
    constexpr std::array presets {
        // --- Sung vocals ------------------------------------------------------
        make (sung, "Default",                    V,  50,   9.0f,  50,  4000,  12000,   60),
        make (sung, "Gentle",                     V,  40,   6.0f,  55,  4500,  11000,   70),
        make (sung, "Firm",                       V,  65,  12.0f,  40,  4000,  12000,   50),
        make (sung, "Heavy",                      V,  80,  18.0f,  30,  3500,  13000,   50),
        make (sung, "Transparent Peaks Only",     V,  50,   9.0f, 100,  4000,  12000,   60),
        make (sung, "Female Lead",                V,  55,   9.0f,  55,  6000,  12000,   50),
        make (sung, "Male Lead",                  V,  55,   9.0f,  50,  3500,   9000,   70),
        make (sung, "Bright Pop Lead",            V,  60,  10.0f,  45,  5000,  11000,   45),
        make (sung, "Rap, Fast Delivery",         V,  60,  10.0f,  40,  4500,  11000,   30),
        make (sung, "Breathy and Airy",           V,  45,   6.0f,  75,  5000,  13000,   90),
        make (sung, "Whispered",                  V,  35,   6.0f,  80,  5000,  14000,  120),
        make (sung, "Sh and Ch Sounds",           V,  55,   9.0f,  40,  2500,   6000,   70),
        make (sung, "Whistling S",                V,  60,  12.0f, 100,  6000,  10000,   40),
        make (sung, "After Heavy Compression",    V,  65,  12.0f,  50,  4500,  12000,   40),
        make (sung, "Backing Vocals",             V,  60,  12.0f,  25,  4000,  12000,   60),
        make (sung, "Doubles and Harmonies",      V,  70,  15.0f,  15,  4000,  13000,   70),

        // --- Spoken voice -----------------------------------------------------
        make (spoken, "Podcast, Natural",         V,  45,   7.0f,  60,  4500,  10000,   70),
        make (spoken, "Podcast, Firm",            V,  60,  10.0f,  50,  4500,  11000,   60),
        make (spoken, "Voice-over, Male",         V,  50,   8.0f,  55,  3500,   9000,   70),
        make (spoken, "Voice-over, Female",       V,  50,   8.0f,  55,  5500,  11500,   60),
        make (spoken, "Audiobook Narration",      V,  40,   6.0f,  70,  4500,  10000,  100),
        make (spoken, "Dialogue, Lavalier",       V,  50,   8.0f,  50,  4000,   9000,   60),
        make (spoken, "Stream, Headset Mic",      V,  60,  10.0f,  40,  5000,  12000,   50),
        make (spoken, "Phone or Conference Call", V,  60,  10.0f,  30,  3000,   7000,   60),
        make (spoken, "Sharp T and D",            V,  50,   8.0f,  60,  7000,  14000,   30),

        // --- Effect sends -----------------------------------------------------
        make (sends, "Before Reverb",             V,  75,  18.0f,  10,  4000,  14000,   80),
        make (sends, "Before Delay",              V,  70,  15.0f,  15,  4000,  13000,   60),
        make (sends, "Before Saturation",         V,  65,  12.0f,  20,  4500,  12000,   50),

        // --- Instruments ------------------------------------------------------
        make (instruments, "Hi-Hat",              B,  55,   8.0f,  50,  7000,  15000,   40),
        make (instruments, "Drum Overheads",      B,  50,   6.0f,  60,  6000,  14000,  100),
        make (instruments, "Acoustic Guitar, String Squeak", B, 55, 9.0f, 70, 2500, 7000, 60),
        make (instruments, "Electric Guitar, Pick Attack",   B, 50, 6.0f, 60, 3000, 8000, 40),
        make (instruments, "Strings, Bow Harshness",         B, 45, 6.0f, 80, 2500, 6000, 100),
        make (instruments, "Brass Edge",          B,  45,   6.0f,  70,  2500,   7000,   80),
        make (instruments, "Harsh Synth",         B,  50,   8.0f,  70,  5000,  14000,   80),

        // --- Buses and master -------------------------------------------------
        make (buses, "Vocal Bus",                 B,  50,   6.0f,  40,  4500,  12000,   70),
        make (buses, "Drum Bus",                  B,  45,   5.0f,  60,  6000,  14000,   80),
        make (buses, "Mix Bus, Light",            B,  35,   3.0f,  60,  5000,  13000,  100),
        make (buses, "Mix Bus, Standard",         B,  45,   4.5f,  50,  5000,  13000,   90),
        make (buses, "Master, Transparent",       B,  30,   2.0f,  80,  6000,  14000,  150),
        make (buses, "Master, Vinyl Prep",        B,  55,   6.0f,  40,  5000,  15000,   80),
        make (buses, "Wide Stereo, Unlinked",     B,  45,   4.5f,  50,  5000,  13000,   90,  0),
    };
}

std::span<const FactoryPreset> factoryPresets() noexcept
{
    return presets;
}

} // namespace deessr
