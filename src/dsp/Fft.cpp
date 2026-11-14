#include "dsp/Fft.h"

#include <cmath>
#include <utility>

namespace deessr
{

Fft::Fft (int order)
    : n (1 << order)
{
    const auto count = (size_t) n;
    bitReversed.resize (count);

    for (int i = 0; i < n; ++i)
    {
        int reversed = 0;

        for (int bit = 0; bit < order; ++bit)
            if ((i >> bit) & 1)
                reversed |= 1 << (order - 1 - bit);

        bitReversed[(size_t) i] = reversed;
    }

    cosTable.resize (count / 2);
    sinTable.resize (count / 2);
    const double twoPi = 6.283185307179586476925286766559;

    for (int k = 0; k < n / 2; ++k)
    {
        const double angle = twoPi * (double) k / (double) n;
        cosTable[(size_t) k] = (float) std::cos (angle);
        sinTable[(size_t) k] = (float) std::sin (angle);
    }
}

void Fft::forward (float* re, float* im) const noexcept
{
    transform (re, im, false);
}

void Fft::inverse (float* re, float* im) const noexcept
{
    transform (re, im, true);

    const float scale = 1.0f / (float) n;

    for (int i = 0; i < n; ++i)
    {
        re[i] *= scale;
        im[i] *= scale;
    }
}

void Fft::transform (float* re, float* im, bool inverseTransform) const noexcept
{
    for (int i = 0; i < n; ++i)
    {
        const int j = bitReversed[(size_t) i];

        if (i < j)
        {
            std::swap (re[i], re[j]);
            std::swap (im[i], im[j]);
        }
    }

    const float sign = inverseTransform ? 1.0f : -1.0f;

    for (int length = 2; length <= n; length <<= 1)
    {
        const int half = length / 2;
        const int step = n / length;

        for (int start = 0; start < n; start += length)
        {
            for (int j = 0, k = 0; j < half; ++j, k += step)
            {
                const float wr = cosTable[(size_t) k];
                const float wi = sign * sinTable[(size_t) k];
                const int a = start + j;
                const int b = a + half;
                const float tr = re[b] * wr - im[b] * wi;
                const float ti = re[b] * wi + im[b] * wr;
                re[b] = re[a] - tr;
                im[b] = im[a] - ti;
                re[a] += tr;
                im[a] += ti;
            }
        }
    }
}

} // namespace deessr
