#pragma once

#include "dsp/Params.h"

#include <span>

namespace deessr
{

/** A factory preset: a starting point for a kind of source.

    A preset sets Mode, Amount, Range, Focus, the sibilant band, Release,
    Stereo Link and Output Gain. It never touches Listen or Bypass.
*/
struct FactoryPreset
{
    const char* category;
    const char* name;
    Params params;
};

/** All factory presets, grouped by category in menu order. */
std::span<const FactoryPreset> factoryPresets() noexcept;

} // namespace deessr
