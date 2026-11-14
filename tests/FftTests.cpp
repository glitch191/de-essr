#include "dsp/Fft.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <random>
#include <vector>

TEST_CASE ("FFT forward then inverse is the identity", "[fft]")
{
    for (int order : { 6, 9, 10, 11 })
    {
        const deessr::Fft fft (order);
        const auto n = (size_t) fft.size();
        std::mt19937 generator (42);
        std::uniform_real_distribution<float> distribution (-1.0f, 1.0f);
        std::vector<float> re (n), im (n, 0.0f), original (n);

        for (size_t i = 0; i < n; ++i)
            original[i] = re[i] = distribution (generator);

        fft.forward (re.data(), im.data());
        fft.inverse (re.data(), im.data());

        float worst = 0.0f;

        for (size_t i = 0; i < n; ++i)
            worst = std::max ({ worst, std::abs (re[i] - original[i]), std::abs (im[i]) });

        REQUIRE (worst < 1.0e-5f);
    }
}

TEST_CASE ("FFT puts a sine in the expected bin", "[fft]")
{
    const deessr::Fft fft (6);
    const int n = fft.size();
    std::vector<float> re ((size_t) n), im ((size_t) n, 0.0f);

    for (int i = 0; i < n; ++i)
        re[(size_t) i] = (float) std::sin (2.0 * 3.14159265358979323846 * 8.0 * (double) i / (double) n);

    fft.forward (re.data(), im.data());

    for (int k = 0; k <= n / 2; ++k)
    {
        const float magnitude = std::hypot (re[(size_t) k], im[(size_t) k]);

        if (k == 8)
            REQUIRE (magnitude == Catch::Approx ((float) n / 2.0f).margin (1.0e-3));
        else
            REQUIRE (magnitude < 1.0e-3f);
    }
}
