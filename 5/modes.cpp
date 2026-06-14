#include "modes.hxx"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <utility>

namespace {

constexpr float kPi = 3.14159265358979323846f;

std::vector<float> smoothSignal(const std::vector<float>& samples, int radius) {
    const size_t n = samples.size();
    std::vector<float> out(n);
    for (size_t i = 0; i < n; ++i) {
        float sum = 0.0f;
        int count = 0;
        for (int k = -radius; k <= radius; ++k) {
            const long idx = static_cast<long>(i) + k;
            if (idx >= 0 && idx < static_cast<long>(n)) {
                sum += samples[static_cast<size_t>(idx)];
                ++count;
            }
        }
        out[i] = sum / static_cast<float>(count);
    }
    return out;
}

std::vector<float> downsample(const std::vector<float>& in, size_t targetCount) {
    std::vector<float> out(targetCount);
    const float ratio = static_cast<float>(in.size()) / static_cast<float>(targetCount);
    for (size_t i = 0; i < targetCount; ++i) {
        size_t srcIdx = static_cast<size_t>(static_cast<float>(i) * ratio);
        srcIdx = std::min(srcIdx, in.size() - 1);
        out[i] = in[srcIdx];
    }
    return out;
}

void hsvToRgb(float h, float s, float v, float& r, float& g, float& b) {
    const float c = v * s;
    const float hh = h * 6.0f;
    const float x = c * (1.0f - std::fabs(std::fmod(hh, 2.0f) - 1.0f));
    const float m = v - c;

    float rr, gg, bb;
    if (hh < 1.0f)      { rr = c; gg = x; bb = 0.0f; }
    else if (hh < 2.0f) { rr = x; gg = c; bb = 0.0f; }
    else if (hh < 3.0f) { rr = 0.0f; gg = c; bb = x; }
    else if (hh < 4.0f) { rr = 0.0f; gg = x; bb = c; }
    else if (hh < 5.0f) { rr = x; gg = 0.0f; bb = c; }
    else                { rr = c; gg = 0.0f; bb = x; }

    r = rr + m;
    g = gg + m;
    b = bb + m;
}

float magnitudeToNormalized(float mag, float sensitivity) {
    const float db = 20.0f * std::log10(mag * sensitivity + 1e-6f);
    const float normalized = (db + 60.0f) / 60.0f;
    return std::clamp(normalized, 0.0f, 1.0f);
}

// Linearly interpolate between FFT bins for sub-bin resolution
float sampleInterpolated(const std::vector<float>& magnitudes, float binIndex) {
    if (magnitudes.empty()) return 0.0f;
    const float idx = std::clamp(binIndex, 0.0f, static_cast<float>(magnitudes.size() - 1));
    const size_t i0 = static_cast<size_t>(idx);
    const size_t i1 = std::min(i0 + 1, magnitudes.size() - 1);
    const float frac = idx - static_cast<float>(i0);
    return magnitudes[i0] * (1.0f - frac) + magnitudes[i1] * frac;
}

std::pair<size_t, size_t> linearBinRange(size_t numBins, size_t numGroups, size_t groupIndex) {
    size_t binStart =  groupIndex       * numBins / numGroups;
    size_t binEnd   = (groupIndex + 1) * numBins / numGroups;
    if (binStart >= numBins) binStart = numBins - 1;
    if (binEnd > numBins) binEnd = numBins;
    if (binEnd <= binStart) binEnd = binStart + 1;
    return {binStart, binEnd};
}

float groupMagnitude(const std::vector<float>& magnitudes, size_t binStart, size_t binEnd) {
    float sum = 0.0f;
    size_t count = 0;
    for (size_t b = binStart; b < binEnd; ++b) {
        sum += magnitudes[b];
        ++count;
    }
    return count > 0 ? sum / static_cast<float>(count) : 0.0f;
}

void updateSmoothed(float& smoothed, float target, float attack, float decay) {
    if (target > smoothed) {
        smoothed = smoothed * (1.0f - attack) + target * attack;
    } else {
        smoothed = smoothed * (1.0f - decay) + target * decay;
    }
}

void pushVertex(std::vector<float>& buf, float x, float y, float r, float g, float b, float size) {
    buf.push_back(x);
    buf.push_back(y);
    buf.push_back(r);
    buf.push_back(g);
    buf.push_back(b);
    buf.push_back(size);
}

// Additive glow overlay for a rectangular bar: fades from black at the
// baseline to `topColor` at the bar's top, plus a soft halo above the bar.
void appendBarGlow(std::vector<float>& glow, float x0, float x1, float baseline, float yTop,
                    Color3 topColor, float bloomSize) {
    const float barWidth = x1 - x0;
    const float haloX = barWidth * bloomSize * 0.5f;
    const float gx0 = x0 - haloX;
    const float gx1 = x1 + haloX;

    pushVertex(glow, gx0, baseline, 0.0f, 0.0f, 0.0f, 1.0f);
    pushVertex(glow, gx1, baseline, 0.0f, 0.0f, 0.0f, 1.0f);
    pushVertex(glow, gx1, yTop,     topColor.r, topColor.g, topColor.b, 1.0f);

    pushVertex(glow, gx0, baseline, 0.0f, 0.0f, 0.0f, 1.0f);
    pushVertex(glow, gx1, yTop,     topColor.r, topColor.g, topColor.b, 1.0f);
    pushVertex(glow, gx0, yTop,     topColor.r, topColor.g, topColor.b, 1.0f);

    const float overshoot = (yTop - baseline) * bloomSize * 0.6f;
    const float gyTop2 = yTop + overshoot;

    pushVertex(glow, gx0, yTop, topColor.r, topColor.g, topColor.b, 1.0f);
    pushVertex(glow, gx1, yTop, topColor.r, topColor.g, topColor.b, 1.0f);
    pushVertex(glow, gx1, gyTop2, 0.0f, 0.0f, 0.0f, 1.0f);

    pushVertex(glow, gx0, yTop, topColor.r, topColor.g, topColor.b, 1.0f);
    pushVertex(glow, gx1, gyTop2, 0.0f, 0.0f, 0.0f, 1.0f);
    pushVertex(glow, gx0, gyTop2, 0.0f, 0.0f, 0.0f, 1.0f);
}

// Additive radial glow extending outward beyond a radial bar segment.
void appendRadialGlow(std::vector<float>& glow, float a0, float a1, float innerRadius, float outerRadius,
                       Color3 topColor, float bloomSize, float aspect) {
    const float haloR = (outerRadius - innerRadius) * bloomSize * 0.8f;
    const float outerHaloRadius = outerRadius + haloR;

    const float ox0 = outerRadius * std::cos(a0) * aspect, oy0 = outerRadius * std::sin(a0);
    const float ox1 = outerRadius * std::cos(a1) * aspect, oy1 = outerRadius * std::sin(a1);
    const float hx0 = outerHaloRadius * std::cos(a0) * aspect, hy0 = outerHaloRadius * std::sin(a0);
    const float hx1 = outerHaloRadius * std::cos(a1) * aspect, hy1 = outerHaloRadius * std::sin(a1);

    pushVertex(glow, ox0, oy0, topColor.r, topColor.g, topColor.b, 1.0f);
    pushVertex(glow, ox1, oy1, topColor.r, topColor.g, topColor.b, 1.0f);
    pushVertex(glow, hx1, hy1, 0.0f, 0.0f, 0.0f, 1.0f);

    pushVertex(glow, ox0, oy0, topColor.r, topColor.g, topColor.b, 1.0f);
    pushVertex(glow, hx1, hy1, 0.0f, 0.0f, 0.0f, 1.0f);
    pushVertex(glow, hx0, hy0, 0.0f, 0.0f, 0.0f, 1.0f);
}

// Shared implementation for bar-style spectra; `gapFraction` of 0 produces
// contiguous bars with no spacing.
modes::ModeOutput buildBarSpectrum(const std::vector<float>& magnitudes, std::vector<float>& smoothedHeights,
                                    const VisState& state, float gapFraction) {
    modes::ModeOutput out;
    const size_t numBins = magnitudes.size();
    const size_t numBars = smoothedHeights.size();

    constexpr float baseline = -0.95f, top = 0.95f, left = -0.98f, right = 0.98f;
    const float barWidth = (right - left) / static_cast<float>(numBars);
    const float gap = barWidth * gapFraction;

    out.fillVertices.reserve(numBars * 6 * 6);
    out.lineVertices.reserve(numBars * 6);
    if (state.bloom) {
        out.glowVertices.reserve(numBars * 12 * 6);
    }

    for (size_t bar = 0; bar < numBars; ++bar) {
        const float t = static_cast<float>(bar) / static_cast<float>(numBars);
        
        const auto [binStart, binEnd] = linearBinRange(numBins, numBars, bar);
        const float mag = groupMagnitude(magnitudes, binStart, binEnd);
        
        updateSmoothed(smoothedHeights[bar], magnitudeToNormalized(mag, state.sensitivity), 0.5f, 0.15f);
        const float h = smoothedHeights[bar];

        const float x0 = left + static_cast<float>(bar) * barWidth + gap * 0.5f;
        const float x1 = x0 + barWidth - gap;
        const float yTop = baseline + h * (top - baseline);

        const Color3 bottomColor = resolveColor(state, Color3{0.0f, 0.25f, 0.35f}, t);
        const Color3 topColor    = resolveColor(state, Color3{0.3f + 0.6f * h, 1.0f, 1.0f}, t);

        pushVertex(out.fillVertices, x0, baseline, bottomColor.r, bottomColor.g, bottomColor.b, 1.0f);
        pushVertex(out.fillVertices, x1, baseline, bottomColor.r, bottomColor.g, bottomColor.b, 1.0f);
        pushVertex(out.fillVertices, x1, yTop,     topColor.r, topColor.g, topColor.b, 1.0f);

        pushVertex(out.fillVertices, x0, baseline, bottomColor.r, bottomColor.g, bottomColor.b, 1.0f);
        pushVertex(out.fillVertices, x1, yTop,     topColor.r, topColor.g, topColor.b, 1.0f);
        pushVertex(out.fillVertices, x0, yTop,     topColor.r, topColor.g, topColor.b, 1.0f);

        const float xc = (x0 + x1) * 0.5f;
        pushVertex(out.lineVertices, xc, yTop, topColor.r, topColor.g, topColor.b, 1.0f);

        if (state.bloom) {
            appendBarGlow(out.glowVertices, x0, x1, baseline, yTop, topColor, state.bloomSize);
        }
    }

    out.linePrimitive = GL_LINE_STRIP;
    out.lineSegments  = {{0, static_cast<GLsizei>(numBars)}};
    out.lineGlow      = true;
    out.fillAlpha     = 1.0f;
    return out;
}

// Shared implementation for radial spectra.
modes::ModeOutput buildCircularBars(const std::vector<float>& magnitudes, std::vector<float>& smoothedHeights,
                                     const VisState& state) {
    modes::ModeOutput out;
    const size_t numBins = magnitudes.size();
    const size_t numBars = smoothedHeights.size();

    constexpr float innerRadius = 0.18f;
    constexpr float maxOuterRadius = 0.46f;
    const float halfAngle = (kPi / static_cast<float>(numBars)) * 0.35f;

    out.fillVertices.reserve(numBars * 6 * 6);
    out.lineVertices.reserve(numBars * 6);
    if (state.bloom) {
        out.glowVertices.reserve(numBars * 6 * 6);
    }

    for (size_t bar = 0; bar < numBars; ++bar) {
        const float t = static_cast<float>(bar) / static_cast<float>(numBars);
        
        const auto [binStart, binEnd] = linearBinRange(numBins, numBars, bar);
        const float mag = groupMagnitude(magnitudes, binStart, binEnd);
        
        updateSmoothed(smoothedHeights[bar], magnitudeToNormalized(mag, state.sensitivity), 0.5f, 0.15f);
        const float h = smoothedHeights[bar];

        const float outerRadius = innerRadius + h * (maxOuterRadius - innerRadius);
        const float angle = t * 2.0f * kPi;
        const float a0 = angle - halfAngle;
        const float a1 = angle + halfAngle;

        const float ix0 = innerRadius * std::cos(a0) * state.aspect, iy0 = innerRadius * std::sin(a0);
        const float ix1 = innerRadius * std::cos(a1) * state.aspect, iy1 = innerRadius * std::sin(a1);
        const float ox0 = outerRadius * std::cos(a0) * state.aspect, oy0 = outerRadius * std::sin(a0);
        const float ox1 = outerRadius * std::cos(a1) * state.aspect, oy1 = outerRadius * std::sin(a1);

        const Color3 innerColor = resolveColor(state, Color3{0.6f, 0.15f, 0.0f}, t);
        const Color3 outerColor = resolveColor(state, Color3{1.0f, 0.55f + 0.45f * h, 0.1f}, t);

        pushVertex(out.fillVertices, ix0, iy0, innerColor.r, innerColor.g, innerColor.b, 1.0f);
        pushVertex(out.fillVertices, ix1, iy1, innerColor.r, innerColor.g, innerColor.b, 1.0f);
        pushVertex(out.fillVertices, ox1, oy1, outerColor.r, outerColor.g, outerColor.b, 1.0f);

        pushVertex(out.fillVertices, ix0, iy0, innerColor.r, innerColor.g, innerColor.b, 1.0f);
        pushVertex(out.fillVertices, ox1, oy1, outerColor.r, outerColor.g, outerColor.b, 1.0f);
        pushVertex(out.fillVertices, ox0, oy0, outerColor.r, outerColor.g, outerColor.b, 1.0f);

        pushVertex(out.lineVertices, (ox0 + ox1) * 0.5f, (oy0 + oy1) * 0.5f,
                   outerColor.r, outerColor.g, outerColor.b, 1.0f);

        if (state.bloom) {
            appendRadialGlow(out.glowVertices, a0, a1, innerRadius, outerRadius, outerColor, state.bloomSize, state.aspect);
        }
    }

    out.linePrimitive = GL_LINE_LOOP;
    out.lineSegments  = {{0, static_cast<GLsizei>(numBars)}};
    out.lineGlow      = true;
    out.fillAlpha     = 1.0f;
    return out;
}

} // namespace

namespace modes {

ModeOutput buildOscilloscope(const std::vector<float>& samples, const VisState& state) {
    ModeOutput out;
    const std::vector<float> smoothed = smoothSignal(samples, 2);
    const size_t n = smoothed.size();

    // waveZoom > 1.0 means fewer samples (zoomed in, longer wavelength)
    // waveZoom < 1.0 means more samples (zoomed out, shorter wavelength)
    // Here we interpret "shorter wavelength" as more cycles in the view.
    // So waveZoom will scale the window of samples.
    size_t displayCount = static_cast<size_t>(static_cast<float>(n) / state.waveZoom);
    displayCount = std::clamp<size_t>(displayCount, 16, n);

    // Add a time-based offset for "speed"
    float timeOffset = static_cast<float>(glfwGetTime()) * state.waveSpeed * static_cast<float>(n);
    long baseIdx = static_cast<long>(timeOffset) % static_cast<long>(n);

    out.lineVertices.reserve(displayCount * 6);
    for (size_t i = 0; i < displayCount; ++i) {
        const float x = (static_cast<float>(i) / static_cast<float>(displayCount - 1)) * 2.0f - 1.0f;
        
        size_t sampleIdx = (static_cast<size_t>(baseIdx) + i) % n;
        const float y = std::clamp(smoothed[sampleIdx] * 4.0f * state.sensitivity, -1.0f, 1.0f);

        const float t = static_cast<float>(i) / static_cast<float>(displayCount - 1);
        const Color3 color = resolveColor(state, Color3{0.2f, 1.0f, 1.0f}, t);

        pushVertex(out.lineVertices, x, y, color.r, color.g, color.b, 1.0f);
    }

    out.linePrimitive = GL_LINE_STRIP;
    out.lineSegments  = {{0, static_cast<GLsizei>(displayCount)}};
    out.lineGlow      = true;
    return out;
}

ModeOutput buildSpectrumBars(const std::vector<float>& magnitudes, std::vector<float>& smoothedHeights, const VisState& state) {
    return buildBarSpectrum(magnitudes, smoothedHeights, state, 0.15f);
}

ModeOutput buildDenseSpectrum(const std::vector<float>& magnitudes, std::vector<float>& smoothedHeights, const VisState& state) {
    return buildBarSpectrum(magnitudes, smoothedHeights, state, 0.0f);
}

ModeOutput buildMirroredWaveform(const std::vector<float>& samples, const VisState& state) {
    ModeOutput out;
    const std::vector<float> ds = downsample(samples, 1024); // Use higher res for better zoom
    const std::vector<float> smoothed = smoothSignal(ds, 2);
    const size_t n = smoothed.size();

    size_t displayCount = static_cast<size_t>(static_cast<float>(n) / state.waveZoom);
    displayCount = std::clamp<size_t>(displayCount, 16, n);

    float timeOffset = static_cast<float>(glfwGetTime()) * state.waveSpeed * static_cast<float>(n);
    long baseIdx = static_cast<long>(timeOffset) % static_cast<long>(n);

    std::vector<float> xs(displayCount), amps(displayCount);
    for (size_t i = 0; i < displayCount; ++i) {
        xs[i]   = (static_cast<float>(i) / static_cast<float>(displayCount - 1)) * 2.0f - 1.0f;
        size_t sampleIdx = (static_cast<size_t>(baseIdx) + i) % n;
        amps[i] = std::clamp(smoothed[sampleIdx] * 4.0f * state.sensitivity, -1.0f, 1.0f) * 0.85f;
    }

    out.fillVertices.reserve((displayCount - 1) * 6 * 6);
    for (size_t i = 0; i + 1 < displayCount; ++i) {
        const float a0 = std::fabs(amps[i]);
        const float a1 = std::fabs(amps[i + 1]);

        const float t0 = static_cast<float>(i) / static_cast<float>(displayCount - 1);
        const float t1 = static_cast<float>(i + 1) / static_cast<float>(displayCount - 1);

        const Color3 c0 = resolveColor(state, Color3{0.25f + 0.55f * a0, 0.05f, 0.35f + 0.5f * a0}, t0);
        const Color3 c1 = resolveColor(state, Color3{0.25f + 0.55f * a1, 0.05f, 0.35f + 0.5f * a1}, t1);

        pushVertex(out.fillVertices, xs[i],     amps[i],     c0.r, c0.g, c0.b, 1.0f);
        pushVertex(out.fillVertices, xs[i],    -amps[i],     c0.r, c0.g, c0.b, 1.0f);
        pushVertex(out.fillVertices, xs[i + 1], amps[i + 1], c1.r, c1.g, c1.b, 1.0f);

        pushVertex(out.fillVertices, xs[i],    -amps[i],     c0.r, c0.g, c0.b, 1.0f);
        pushVertex(out.fillVertices, xs[i + 1],-amps[i + 1], c1.r, c1.g, c1.b, 1.0f);
        pushVertex(out.fillVertices, xs[i + 1], amps[i + 1], c1.r, c1.g, c1.b, 1.0f);
    }

    out.lineVertices.reserve(displayCount * 6);
    for (size_t i = 0; i < displayCount; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(displayCount - 1);
        const Color3 c = resolveColor(state, Color3{1.0f, 0.4f, 0.9f}, t);
        pushVertex(out.lineVertices, xs[i], amps[i], c.r, c.g, c.b, 1.0f);
    }

    out.linePrimitive = GL_LINE_STRIP;
    out.lineSegments  = {{0, static_cast<GLsizei>(displayCount)}};
    out.lineGlow      = true;
    out.fillAlpha     = 0.55f;
    return out;
}

ModeOutput buildCircularOscilloscope(const std::vector<float>& samples, const VisState& state) {
    ModeOutput out;
    const std::vector<float> ds = downsample(samples, 512);
    const std::vector<float> smoothed = smoothSignal(ds, 2);
    const size_t n = smoothed.size();

    constexpr float baseRadius = 0.35f;
    constexpr float ampScale = 0.28f;

    out.lineVertices.reserve(n * 6);
    for (size_t i = 0; i < n; ++i) {
        const float angle = (static_cast<float>(i) / static_cast<float>(n)) * 2.0f * kPi;
        const float amp = std::clamp(smoothed[i] * 4.0f * state.sensitivity, -1.0f, 1.0f);
        const float radius = baseRadius + amp * ampScale;

        const float x = radius * std::cos(angle) * state.aspect;
        const float y = radius * std::sin(angle);

        const float t = static_cast<float>(i) / static_cast<float>(n);
        const float a = std::fabs(amp);
        const Color3 color = resolveColor(state, Color3{0.1f + 0.2f * a, 0.6f + 0.4f * a, 0.4f + 0.3f * a}, t);

        pushVertex(out.lineVertices, x, y, color.r, color.g, color.b, 1.0f);
    }

    out.linePrimitive = GL_LINE_LOOP;
    out.lineSegments  = {{0, static_cast<GLsizei>(n)}};
    out.lineGlow      = true;
    return out;
}

ModeOutput buildCircularSpectrum(const std::vector<float>& magnitudes, std::vector<float>& smoothedHeights, const VisState& state) {
    return buildCircularBars(magnitudes, smoothedHeights, state);
}

ModeOutput buildCircularSpectrumFilled(const std::vector<float>& magnitudes, std::vector<float>& smoothedHeights,
                                        const VisState& state, float time) {
    ModeOutput out = buildCircularBars(magnitudes, smoothedHeights, state);

    float avg = 0.0f;
    for (float h : smoothedHeights) {
        avg += h;
    }
    avg /= static_cast<float>(std::max<size_t>(smoothedHeights.size(), 1));

    constexpr float innerRadius = 0.18f;
    constexpr size_t discSegments = 48;
    const float pulse = (0.4f + 0.6f * avg) * (1.0f + 0.05f * std::sin(time * 2.0f));
    const float discRadius = innerRadius * pulse;

    const Color3 fillColor = resolveColor(state, Color3{0.55f * pulse, 0.2f * pulse, 0.85f * pulse}, 0.5f);

    out.fillVertices.reserve(out.fillVertices.size() + discSegments * 3 * 6);
    for (size_t j = 0; j < discSegments; ++j) {
        const float a0 = (static_cast<float>(j) / static_cast<float>(discSegments)) * 2.0f * kPi;
        const float a1 = (static_cast<float>(j + 1) / static_cast<float>(discSegments)) * 2.0f * kPi;

        pushVertex(out.fillVertices, 0.0f, 0.0f, fillColor.r, fillColor.g, fillColor.b, 1.0f);
        pushVertex(out.fillVertices, discRadius * std::cos(a0) * state.aspect, discRadius * std::sin(a0),
                   fillColor.r, fillColor.g, fillColor.b, 1.0f);
        pushVertex(out.fillVertices, discRadius * std::cos(a1) * state.aspect, discRadius * std::sin(a1),
                   fillColor.r, fillColor.g, fillColor.b, 1.0f);
    }

    return out;
}

ModeOutput buildLissajous(const std::vector<float>& samples, const VisState& state) {
    ModeOutput out;
    const size_t n = std::min<size_t>(1024, samples.size());
    const std::vector<float> ds = downsample(samples, n);
    const size_t delay = n / 6;

    const float scale = 4.0f * state.sensitivity;

    out.lineVertices.reserve(n * 6);
    for (size_t i = 0; i < n; ++i) {
        const float x = std::clamp(ds[i] * scale, -1.0f, 1.0f) * 0.9f;
        const float y = std::clamp(ds[(i + delay) % n] * scale, -1.0f, 1.0f) * 0.9f;

        const float t = static_cast<float>(i) / static_cast<float>(n);
        float rr, gg, bb;
        hsvToRgb(t, 0.85f, 1.0f, rr, gg, bb);
        const Color3 color = resolveColor(state, Color3{rr, gg, bb}, t);

        pushVertex(out.lineVertices, x, y, color.r, color.g, color.b, 1.0f);
    }

    out.linePrimitive = GL_LINE_STRIP;
    out.lineSegments  = {{0, static_cast<GLsizei>(n)}};
    out.lineGlow      = true;
    return out;
}

ModeOutput buildPulseRings(const std::vector<float>& magnitudes, std::vector<float>& smoothedBands, const VisState& state, float time) {
    ModeOutput out;
    const size_t numBins = magnitudes.size();
    constexpr size_t ringSegments = 96;

    const std::pair<size_t, size_t> ranges[3] = {
        {1, std::min<size_t>(8, numBins)},
        {std::min<size_t>(8, numBins), std::min<size_t>(100, numBins)},
        {std::min<size_t>(100, numBins), std::min<size_t>(400, numBins)}
    };

    for (size_t k = 0; k < 3; ++k) {
        const float mag = groupMagnitude(magnitudes, ranges[k].first, ranges[k].second);
        updateSmoothed(smoothedBands[k], magnitudeToNormalized(mag, state.sensitivity), 0.4f, 0.06f);
    }

    constexpr float baseRadii[3]  = {0.15f, 0.27f, 0.39f};
    constexpr float ringScale[3]  = {0.30f, 0.22f, 0.16f};
    constexpr float phaseSpeed[3] = {0.3f, -0.2f, 0.15f};
    const Color3 baseColors[3] = {
        {1.0f, 0.35f, 0.2f},
        {0.25f, 1.0f, 0.4f},
        {0.3f, 0.55f, 1.0f}
    };

    out.lineVertices.reserve(3 * ringSegments * 6);
    for (size_t k = 0; k < 3; ++k) {
        const float radius = baseRadii[k] + smoothedBands[k] * ringScale[k];
        const float phase = time * phaseSpeed[k];

        const float t = static_cast<float>(k) / 2.0f;
        const Color3 color = resolveColor(state, baseColors[k], t);

        out.lineSegments.push_back({static_cast<GLint>(out.lineVertices.size() / 6),
                                     static_cast<GLsizei>(ringSegments)});

        for (size_t j = 0; j < ringSegments; ++j) {
            const float angle = phase + (static_cast<float>(j) / static_cast<float>(ringSegments)) * 2.0f * kPi;
            const float x = radius * std::cos(angle) * state.aspect;
            const float y = radius * std::sin(angle);
            pushVertex(out.lineVertices, x, y, color.r, color.g, color.b, 1.0f);
        }
    }

    out.linePrimitive = GL_LINE_LOOP;
    out.lineGlow      = true;
    return out;
}

ModeOutput buildTrueXY(const std::vector<float>& left, const std::vector<float>& right, const VisState& state) {
    ModeOutput out;
    const size_t n = std::min(left.size(), right.size());
    if (n == 0) return out;

    constexpr size_t targetCount = 256;
    const size_t count = std::min(n, targetCount);
    const std::vector<float> dsLeft = downsample(left, count);
    const std::vector<float> dsRight = downsample(right, count);

    size_t displayCount = static_cast<size_t>(static_cast<float>(count) / state.waveZoom);
    displayCount = std::clamp<size_t>(displayCount, 64u, count);

    const Color3 baseColor = {0.2f, 1.0f, 0.3f};

    // Pre-compute positions and colors
    struct Pt { float x, y; Color3 c; };
    std::vector<Pt> pts(displayCount);
    for (size_t i = 0; i < displayCount; ++i) {
        pts[i].x = std::clamp(dsLeft[i]  * 2.0f * state.sensitivity * state.aspect, -1.0f, 1.0f);
        pts[i].y = std::clamp(dsRight[i] * 2.0f * state.sensitivity, -1.0f, 1.0f);
        const float t = static_cast<float>(i) / static_cast<float>(displayCount - 1);
        pts[i].c = resolveColor(state, baseColor, t);
    }

    switch (state.trueXYMode) {
        case TrueXYMode::Scatter:
            out.lineVertices.reserve(displayCount * 6);
            for (const auto& p : pts)
                pushVertex(out.lineVertices, p.x, p.y, p.c.r, p.c.g, p.c.b, 3.0f);
            out.linePrimitive = GL_POINTS;
            out.linePoints = true;
            out.lineSegments = {{0, static_cast<GLsizei>(displayCount)}};
            break;

        case TrueXYMode::LineStrip:
            out.lineVertices.reserve(displayCount * 6);
            for (const auto& p : pts)
                pushVertex(out.lineVertices, p.x, p.y, p.c.r, p.c.g, p.c.b, 1.0f);
            out.linePrimitive = GL_LINE_STRIP;
            out.lineSegments = {{0, static_cast<GLsizei>(displayCount)}};
            break;

        case TrueXYMode::Both:
            out.lineVertices.reserve(displayCount * 6);
            for (const auto& p : pts)
                pushVertex(out.lineVertices, p.x, p.y, p.c.r, p.c.g, p.c.b, 1.0f);
            out.linePrimitive = GL_LINE_STRIP;
            out.lineSegments = {{0, static_cast<GLsizei>(displayCount)}};
            out.fillVertices.reserve(displayCount * 6 * 6);
            for (const auto& p : pts) {
                const float s = 0.004f;
                pushVertex(out.fillVertices, p.x - s, p.y - s, p.c.r, p.c.g, p.c.b, 1.0f);
                pushVertex(out.fillVertices, p.x + s, p.y - s, p.c.r, p.c.g, p.c.b, 1.0f);
                pushVertex(out.fillVertices, p.x + s, p.y + s, p.c.r, p.c.g, p.c.b, 1.0f);
                pushVertex(out.fillVertices, p.x - s, p.y - s, p.c.r, p.c.g, p.c.b, 1.0f);
                pushVertex(out.fillVertices, p.x + s, p.y + s, p.c.r, p.c.g, p.c.b, 1.0f);
                pushVertex(out.fillVertices, p.x - s, p.y + s, p.c.r, p.c.g, p.c.b, 1.0f);
            }
            out.fillAlpha = 1.0f;
            break;

        case TrueXYMode::FilledTrail: {
            constexpr float hw = 0.003f;
            out.fillVertices.reserve((displayCount - 1) * 6 * 6);
            for (size_t i = 0; i + 1 < displayCount; ++i) {
                const auto& a = pts[i];
                const auto& b = pts[i + 1];
                const float dx = b.y - a.y;
                const float dy = -(b.x - a.x);
                const float len = std::sqrt(dx * dx + dy * dy);
                const float nx = len > 0.001f ? dx / len * hw : hw;
                const float ny = len > 0.001f ? dy / len * hw : hw;
                pushVertex(out.fillVertices, a.x - nx, a.y - ny, a.c.r, a.c.g, a.c.b, 1.0f);
                pushVertex(out.fillVertices, a.x + nx, a.y + ny, a.c.r, a.c.g, a.c.b, 1.0f);
                pushVertex(out.fillVertices, b.x + nx, b.y + ny, b.c.r, b.c.g, b.c.b, 1.0f);
                pushVertex(out.fillVertices, a.x - nx, a.y - ny, a.c.r, a.c.g, a.c.b, 1.0f);
                pushVertex(out.fillVertices, b.x + nx, b.y + ny, b.c.r, b.c.g, b.c.b, 1.0f);
                pushVertex(out.fillVertices, b.x - nx, b.y - ny, b.c.r, b.c.g, b.c.b, 1.0f);
            }
            out.fillAlpha = 1.0f;
            break;
        }

        case TrueXYMode::GlowScatter:
            out.lineVertices.reserve(displayCount * 6);
            out.glowVertices.reserve(displayCount * 6 * 6);
            for (const auto& p : pts) {
                pushVertex(out.lineVertices, p.x, p.y, p.c.r, p.c.g, p.c.b, 4.0f);
                const float s = 0.006f;
                pushVertex(out.glowVertices, p.x - s, p.y - s, p.c.r, p.c.g, p.c.b, 1.0f);
                pushVertex(out.glowVertices, p.x + s, p.y - s, p.c.r, p.c.g, p.c.b, 1.0f);
                pushVertex(out.glowVertices, p.x + s, p.y + s, p.c.r, p.c.g, p.c.b, 1.0f);
                pushVertex(out.glowVertices, p.x - s, p.y - s, p.c.r, p.c.g, p.c.b, 1.0f);
                pushVertex(out.glowVertices, p.x + s, p.y + s, p.c.r, p.c.g, p.c.b, 1.0f);
                pushVertex(out.glowVertices, p.x - s, p.y + s, p.c.r, p.c.g, p.c.b, 1.0f);
            }
            out.linePrimitive = GL_POINTS;
            out.linePoints = true;
            out.lineSegments = {{0, static_cast<GLsizei>(displayCount)}};
            break;

        case TrueXYMode::PhosphorTrail: {
            const float time = static_cast<float>(glfwGetTime());
            const float phase = time * state.waveSpeed;
            const long rawIdx = static_cast<long>(phase * static_cast<float>(displayCount));
            const long modIdx = rawIdx % static_cast<long>(displayCount);
            const size_t baseIdx = static_cast<size_t>(modIdx < 0 ? modIdx + static_cast<long>(displayCount) : modIdx);
            const size_t trailLen = std::min<size_t>(displayCount / 2, 64);

            out.lineVertices.reserve(trailLen * 6);
            out.glowVertices.reserve(trailLen * 6 * 6);
            for (size_t i = 0; i < trailLen; ++i) {
                const size_t idx = (static_cast<size_t>(baseIdx) + i) % displayCount;
                const float fade = static_cast<float>(i) / static_cast<float>(trailLen - 1);
                const float brightness = 0.2f + 0.8f * fade;
                const auto& p = pts[idx];
                const float r = p.c.r * brightness;
                const float g = p.c.g * brightness;
                const float b = p.c.b * brightness;
                pushVertex(out.lineVertices, p.x, p.y, r, g, b, 1.0f + 3.0f * fade);
                const float s = 0.003f + 0.004f * fade;
                pushVertex(out.glowVertices, p.x - s, p.y - s, r, g, b, 1.0f);
                pushVertex(out.glowVertices, p.x + s, p.y - s, r, g, b, 1.0f);
                pushVertex(out.glowVertices, p.x + s, p.y + s, r, g, b, 1.0f);
                pushVertex(out.glowVertices, p.x - s, p.y - s, r, g, b, 1.0f);
                pushVertex(out.glowVertices, p.x + s, p.y + s, r, g, b, 1.0f);
                pushVertex(out.glowVertices, p.x - s, p.y + s, r, g, b, 1.0f);
            }
            out.linePrimitive = GL_POINTS;
            out.linePoints = true;
            out.lineSegments = {{0, static_cast<GLsizei>(trailLen)}};
            break;
        }


    }

    out.lineGlow = true;
    return out;
}

ModeOutput buildAnalogScope(const std::vector<float>& left, const std::vector<float>& right, const VisState& state) {
    ModeOutput out;

    // P31 phosphor: subdued green (overridable by R/G color modes)
    const Color3 baseColor = {0.08f, 0.5f, 0.06f};

    struct Pt { float x, y; };
    auto makePt = [&](float l, float r) -> Pt {
        return {
            std::clamp(l * 1.2f * state.sensitivity * state.aspect, -1.0f, 1.0f),
            std::clamp(r * 1.2f * state.sensitivity, -1.0f, 1.0f)
        };
    };

    out.lineWidth = 2.0f;
    out.lineGlow  = true;
    out.fillAlpha = 1.0f;

    const bool isTrace   = state.analogScopeMode == AnalogScopeMode::Trace;
    const bool isScatter = state.analogScopeMode == AnalogScopeMode::Scatter;
    const bool isBoth    = state.analogScopeMode == AnalogScopeMode::Both;

    // Use buffer at user-controlled resolution
    const size_t bufLen = std::min({left.size(), right.size(), state.analogResolution});
    const size_t traceSamples = (isTrace || isBoth)
        ? std::min(bufLen, state.analogLineCount)
        : 0;
    const size_t particleSamples = (isScatter || isBoth) ? bufLen : 0;

    // --- XY trace (full buffer, 4x interpolation, phosphor gradient) ---
    if (traceSamples > 1) {
        const size_t interpLen = traceSamples * 4 - 3;
        struct InterpPt { float x, y; };
        std::vector<InterpPt> interpPts(interpLen);

        for (size_t i = 0; i < traceSamples; ++i) {
            const float l = left[i], r = right[i];
            interpPts[i * 4] = {makePt(l, r).x, makePt(l, r).y};
            if (i + 1 < traceSamples) {
                for (size_t k = 1; k <= 3; ++k) {
                    const float t = k / 4.0f;
                    const float lm = l + (left[i + 1] - l) * t;
                    const float rm = r + (right[i + 1] - r) * t;
                    interpPts[i * 4 + k] = {makePt(lm, rm).x, makePt(lm, rm).y};
                }
            }
        }

        out.lineVertices.reserve(interpLen * 6);
        out.linePrimitive = GL_LINE_STRIP;
        out.linePoints    = false;

        // Z-axis blanking threshold for teleport suppression
        constexpr float zBlankThresh = 0.35f;
        const float decayRate = state.analogScopeDecay;
        GLsizei segStart = 0;

        for (size_t i = 0; i < interpLen; ++i) {
            // Check blanking at raw sample boundaries (every 4th point)
            if (i > 0 && i % 4 == 0) {
                const size_t rawIdx = i / 4;
                const float dl = std::abs(left[rawIdx] - left[rawIdx - 1]);
                const float dr = std::abs(right[rawIdx] - right[rawIdx - 1]);
                if (dl + dr >= zBlankThresh) {
                    const GLsizei vertCount = static_cast<GLsizei>(out.lineVertices.size() / 6);
                    if (vertCount > segStart) {
                        out.lineSegments.push_back({segStart, vertCount - segStart});
                    }
                    segStart = vertCount;
                }
            }
            const auto& p = interpPts[i];
            const float age = static_cast<float>(interpLen - i) / static_cast<float>(interpLen);
            const float brightness = std::exp(-age * decayRate) * 0.95f + 0.05f;
            const Color3 c = resolveColor(state, baseColor, 1.0f - age);
            pushVertex(out.lineVertices, p.x, p.y, c.r * brightness, c.g * brightness, c.b * brightness, 1.0f);
        }
        const GLsizei vertCount = static_cast<GLsizei>(out.lineVertices.size() / 6);
        if (vertCount > segStart) {
            out.lineSegments.push_back({segStart, vertCount - segStart});
        }
    }

    // --- Exponential particles (Scatter and Both) ---
    if (particleSamples > 1) {
        const std::vector<float> dsLeft = downsample(left, particleSamples);
        const std::vector<float> dsRight = downsample(right, particleSamples);

        constexpr float pointSize = 0.0025f;
        const float s = pointSize;
        out.fillVertices.reserve(particleSamples * 6 * 6);

        for (size_t i = 0; i < particleSamples; ++i) {
            const Pt p = makePt(dsLeft[i], dsRight[i]);
            const Color3 pc = resolveColor(state, baseColor, static_cast<float>(i) / static_cast<float>(particleSamples - 1));
            pushVertex(out.fillVertices, p.x - s, p.y - s, pc.r, pc.g, pc.b, 1.0f);
            pushVertex(out.fillVertices, p.x + s, p.y - s, pc.r, pc.g, pc.b, 1.0f);
            pushVertex(out.fillVertices, p.x + s, p.y + s, pc.r, pc.g, pc.b, 1.0f);
            pushVertex(out.fillVertices, p.x - s, p.y - s, pc.r, pc.g, pc.b, 1.0f);
            pushVertex(out.fillVertices, p.x + s, p.y + s, pc.r, pc.g, pc.b, 1.0f);
            pushVertex(out.fillVertices, p.x - s, p.y + s, pc.r, pc.g, pc.b, 1.0f);
        }
    }

    return out;
}

} // namespace modes