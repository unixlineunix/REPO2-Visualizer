#include "fft.hxx"

#include <cmath>
#include <utility>

namespace {
constexpr float kPi = 3.14159265358979323846f;
}

namespace fft {

static void bitReverseReorder(std::vector<std::complex<float>>& a) {
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(a[i], a[j]);
        }
    }
}

void transform(std::vector<std::complex<float>>& a) {
    const size_t n = a.size();
    if (n < 2) {
        return;
    }

    bitReverseReorder(a);

    for (size_t len = 2; len <= n; len <<= 1) {
        const float ang = -2.0f * kPi / static_cast<float>(len);
        const std::complex<float> wlen(std::cos(ang), std::sin(ang));

        for (size_t i = 0; i < n; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            const size_t half = len / 2;
            for (size_t j = 0; j < half; ++j) {
                const std::complex<float> u = a[i + j];
                const std::complex<float> v = a[i + j + half] * w;
                a[i + j]        = u + v;
                a[i + j + half] = u - v;
                w *= wlen;
            }
        }
    }
}

void computeMagnitudeSpectrum(const float* samples, size_t n, std::vector<float>& outMagnitudes) {
    std::vector<std::complex<float>> buffer(n);

    for (size_t i = 0; i < n; ++i) {
        const float window = 0.5f * (1.0f - std::cos(2.0f * kPi * static_cast<float>(i) /
                                                       static_cast<float>(n - 1)));
        buffer[i] = std::complex<float>(samples[i] * window, 0.0f);
    }

    transform(buffer);

    const size_t half = n / 2;
    outMagnitudes.resize(half);
    for (size_t i = 0; i < half; ++i) {
        outMagnitudes[i] = std::abs(buffer[i]) / static_cast<float>(n);
    }
}

} // namespace fft