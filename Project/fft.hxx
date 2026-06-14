#pragma once

#include <vector>
#include <complex>
#include <cstddef>

namespace fft {

// In-place iterative radix-2 Cooley-Tukey FFT.
// a.size() MUST be a power of two.
void transform(std::vector<std::complex<float>>& a);

// Applies a Hann window to `samples[0..n-1]`, runs an FFT, and writes
// the normalized magnitude of the first n/2 bins into outMagnitudes.
// n MUST be a power of two.
void computeMagnitudeSpectrum(const float* samples, size_t n, std::vector<float>& outMagnitudes);

} // namespace fft