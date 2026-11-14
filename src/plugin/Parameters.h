#pragma once

#include "dsp/Params.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace ids
{
    inline constexpr const char* mode = "mode";
    inline constexpr const char* amount = "amount";
    inline constexpr const char* range = "range";
    inline constexpr const char* focus = "focus";
    inline constexpr const char* bandLow = "bandLow";
    inline constexpr const char* bandHigh = "bandHigh";
    inline constexpr const char* release = "release";
    inline constexpr const char* stereoLink = "stereoLink";
    inline constexpr const char* outputGain = "outputGain";
    inline constexpr const char* listen = "listen";
    inline constexpr const char* bypass = "bypass";
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

/** Lock-free access to the current parameter values from the audio thread. */
class ParameterReader
{
public:
    explicit ParameterReader (juce::AudioProcessorValueTreeState& state);

    deessr::Params read() const noexcept;

private:
    std::atomic<float>* mode = nullptr;
    std::atomic<float>* amount = nullptr;
    std::atomic<float>* range = nullptr;
    std::atomic<float>* focus = nullptr;
    std::atomic<float>* bandLow = nullptr;
    std::atomic<float>* bandHigh = nullptr;
    std::atomic<float>* release = nullptr;
    std::atomic<float>* stereoLink = nullptr;
    std::atomic<float>* outputGain = nullptr;
    std::atomic<float>* listen = nullptr;
    std::atomic<float>* bypass = nullptr;
};
