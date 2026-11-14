#pragma once

namespace deessr
{

enum class Mode
{
    vocal = 0, // single source: uses voiced / unvoiced criteria
    bus = 1    // bus or full mix: uses the excess over the long-term average
};

/** User-facing settings of the engine, in real units. */
struct Params
{
    Mode mode = Mode::vocal;
    float amount = 0.5f;        // 0..1, relative sensitivity (not an absolute threshold)
    float rangeDb = 9.0f;       // maximum reduction, 0..24 dB
    float focus = 0.5f;         // 0 = wide (whole band moves), 1 = surgical (peaks only)
    float bandLowHz = 4000.0f;  // sibilant band, low edge
    float bandHighHz = 12000.0f; // sibilant band, high edge
    float releaseMs = 60.0f;    // release of the sibilance envelope
    float stereoLink = 1.0f;    // 0 = independent channels, 1 = fully linked
    float outputGainDb = 0.0f;
    bool listen = false;        // output the removed signal instead of the processed one
    bool bypass = false;        // latency-aligned bypass
};

namespace limits
{
    constexpr float bandMinHz = 2000.0f;
    constexpr float bandMaxHz = 16000.0f;
    constexpr float bandMinRatio = 1.26f; // high edge is at least a third of an octave above low edge
    constexpr float rangeMaxDb = 24.0f;
    constexpr float releaseMinMs = 20.0f;
    constexpr float releaseMaxMs = 300.0f;
    constexpr float outputGainMinDb = -12.0f;
    constexpr float outputGainMaxDb = 12.0f;
}

} // namespace deessr
