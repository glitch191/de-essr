#pragma once

#include <vector>

namespace deessr
{

/** Small in-place radix-2 complex FFT.

    All tables are built in the constructor. The transform methods do not
    allocate, lock or touch shared state, so they are safe on the audio thread.
*/
class Fft
{
public:
    explicit Fft (int order);

    int size() const noexcept { return n; }

    /** Forward transform, in place, unscaled. */
    void forward (float* re, float* im) const noexcept;

    /** Inverse transform, in place, scaled by 1/size (forward then inverse is the identity). */
    void inverse (float* re, float* im) const noexcept;

private:
    void transform (float* re, float* im, bool inverseTransform) const noexcept;

    int n = 0;
    std::vector<int> bitReversed;
    std::vector<float> cosTable;
    std::vector<float> sinTable;
};

} // namespace deessr
