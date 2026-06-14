#include "modes.hxx"

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

float magnitudeToNormalized(float mag) {
    const float db = 20.0f * std::log10(mag + 1e-6f);
    const float normalized = (db + 60.0f) / 60.0f;
    return std::clamp(normalized, 0.0f, 1.0f);
}

std::pair<size_t, size_t> logBinRange(size_t numBins, size_t numGroups, size_t groupIndex) {
    const double t0 = static_cast<double>(groupIndex) / static_cast<double>(numGroups);
    const double t1 = static_cast<double>(groupIndex + 1) / static_cast<double>(numGroups);

    size_t binStart = static_cast<size_t>(std::pow(static_cast<double>(numBins), t0));
    size_t binEnd   = static_cast<size_t>(std::pow(static_cast<double>(numBins), t1));

    binStart = std::clamp<size_t>(binStart, 1, numBins - 1);
    binEnd   = std::clamp<size_t>(binEnd, binStart + 1, numBins);

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

} // namespace

namespace modes {

ModeOutput buildOscilloscope(const std::vector<float>& samples) {
    ModeOutput out;
    const std::vector<float> smoothed = smoothSignal(samples, 2);
    const size_t n = smoothed.size();

    out.lineVertices.reserve(n * 6);
    for (size_t i = 0; i < n; ++i) {
        const float x = (static_cast<float>(i) / static_cast<float>(n - 1)) * 2.0f - 1.0f;
        const float y = std::clamp(smoothed[i] * 4.0f, -1.0f, 1.0f);
        pushVertex(out.lineVertices, x, y, 0.2f, 1.0f, 1.0f, 1.0f);
    }

    out.linePrimitive = GL_LINE_STRIP;
    out.lineSegments  = {{0, static_cast<GLsizei>(n)}};
    out.lineGlow      = true;
    return out;
}

ModeOutput buildSpectrumBars(const std::vector<float>& magnitudes, std::vector<float>& smoothedHeights) {
    ModeOutput out;
    const size_t numBins = magnitudes.size();
    const size_t numBars = smoothedHeights.size();

    constexpr float baseline = -0.95f, top = 0.95f, left = -0.98f, right = 0.98f;
    const float barWidth = (right - left) / static_cast<float>(numBars);
    const float gap = barWidth * 0.15f;

    out.fillVertices.reserve(numBars * 6 * 6);
    out.lineVertices.reserve(numBars * 6);

    for (size_t bar = 0; bar < numBars; ++bar) {
        const auto [binStart, binEnd] = logBinRange(numBins, numBars, bar);
        const float mag = groupMagnitude(magnitudes, binStart, binEnd);
        updateSmoothed(smoothedHeights[bar], magnitudeToNormalized(mag), 0.5f, 0.15f);
        const float h = smoothedHeights[bar];

        const float x0 = left + static_cast<float>(bar) * barWidth + gap * 0.5f;
        const float x1 = x0 + barWidth - gap;
        const float yTop = baseline + h * (top - baseline);

        const float rb = 0.0f, gb = 0.25f, bb = 0.35f;
        const float rt = 0.3f + 0.6f * h, gt = 1.0f, bt = 1.0f;

        pushVertex(out.fillVertices, x0, baseline, rb, gb, bb, 1.0f);
        pushVertex(out.fillVertices, x1, baseline, rb, gb, bb, 1.0f);
        pushVertex(out.fillVertices, x1, yTop,     rt, gt, bt, 1.0f);

        pushVertex(out.fillVertices, x0, baseline, rb, gb, bb, 1.0f);
        pushVertex(out.fillVertices, x1, yTop,     rt, gt, bt, 1.0f);
        pushVertex(out.fillVertices, x0, yTop,     rt, gt, bt, 1.0f);

        const float xc = (x0 + x1) * 0.5f;
        pushVertex(out.lineVertices, xc, yTop, rt, gt, bt, 1.0f);
    }

    out.linePrimitive = GL_LINE_STRIP;
    out.lineSegments  = {{0, static_cast<GLsizei>(numBars)}};
    out.lineGlow      = true;
    out.fillAlpha     = 1.0f;
    return out;
}

ModeOutput buildMirroredWaveform(const std::vector<float>& samples) {
    ModeOutput out;
    const std::vector<float> ds = downsample(samples, 512);
    const std::vector<float> smoothed = smoothSignal(ds, 2);
    const size_t n = smoothed.size();

    std::vector<float> xs(n), amps(n);
    for (size_t i = 0; i < n; ++i) {
        xs[i]   = (static_cast<float>(i) / static_cast<float>(n - 1)) * 2.0f - 1.0f;
        amps[i] = std::clamp(smoothed[i] * 4.0f, -1.0f, 1.0f) * 0.85f;
    }

    out.fillVertices.reserve((n - 1) * 6 * 6);
    for (size_t i = 0; i + 1 < n; ++i) {
        const float a0 = std::fabs(amps[i]);
        const float a1 = std::fabs(amps[i + 1]);

        const float r0 = 0.25f + 0.55f * a0, g0 = 0.05f, b0 = 0.35f + 0.5f * a0;
        const float r1 = 0.25f + 0.55f * a1, g1 = 0.05f, b1 = 0.35f + 0.5f * a1;

        pushVertex(out.fillVertices, xs[i],     amps[i],     r0, g0, b0, 1.0f);
        pushVertex(out.fillVertices, xs[i],    -amps[i],     r0, g0, b0, 1.0f);
        pushVertex(out.fillVertices, xs[i + 1], amps[i + 1], r1, g1, b1, 1.0f);

        pushVertex(out.fillVertices, xs[i],    -amps[i],     r0, g0, b0, 1.0f);
        pushVertex(out.fillVertices, xs[i + 1],-amps[i + 1], r1, g1, b1, 1.0f);
        pushVertex(out.fillVertices, xs[i + 1], amps[i + 1], r1, g1, b1, 1.0f);
    }

    out.lineVertices.reserve(n * 6);
    for (size_t i = 0; i < n; ++i) {
        pushVertex(out.lineVertices, xs[i], amps[i], 1.0f, 0.4f, 0.9f, 1.0f);
    }

    out.linePrimitive = GL_LINE_STRIP;
    out.lineSegments  = {{0, static_cast<GLsizei>(n)}};
    out.lineGlow      = true;
    out.fillAlpha     = 0.55f;
    return out;
}

ModeOutput buildCircularOscilloscope(const std::vector<float>& samples) {
    ModeOutput out;
    const std::vector<float> ds = downsample(samples, 512);
    const std::vector<float> smoothed = smoothSignal(ds, 2);
    const size_t n = smoothed.size();

    constexpr float baseRadius = 0.35f;
    constexpr float ampScale = 0.28f;

    out.lineVertices.reserve(n * 6);
    for (size_t i = 0; i < n; ++i) {
        const float angle = (static_cast<float>(i) / static_cast<float>(n)) * 2.0f * kPi;
        const float amp = std::clamp(smoothed[i] * 4.0f, -1.0f, 1.0f);
        const float radius = baseRadius + amp * ampScale;

        const float x = radius * std::cos(angle) * kAspect;
        const float y = radius * std::sin(angle);

        const float t = std::fabs(amp);
        pushVertex(out.lineVertices, x, y, 0.1f + 0.2f * t, 0.6f + 0.4f * t, 0.4f + 0.3f * t, 1.0f);
    }

    out.linePrimitive = GL_LINE_LOOP;
    out.lineSegments  = {{0, static_cast<GLsizei>(n)}};
    out.lineGlow      = true;
    return out;
}

ModeOutput buildCircularSpectrum(const std::vector<float>& magnitudes, std::vector<float>& smoothedHeights) {
    ModeOutput out;
    const size_t numBins = magnitudes.size();
    const size_t numBars = smoothedHeights.size();

    constexpr float innerRadius = 0.18f;
    constexpr float maxOuterRadius = 0.46f;
    const float halfAngle = (kPi / static_cast<float>(numBars)) * 0.35f;

    out.fillVertices.reserve(numBars * 6 * 6);
    out.lineVertices.reserve(numBars * 6);

    for (size_t bar = 0; bar < numBars; ++bar) {
        const auto [binStart, binEnd] = logBinRange(numBins, numBars, bar);
        const float mag = groupMagnitude(magnitudes, binStart, binEnd);
        updateSmoothed(smoothedHeights[bar], magnitudeToNormalized(mag), 0.5f, 0.15f);
        const float h = smoothedHeights[bar];

        const float outerRadius = innerRadius + h * (maxOuterRadius - innerRadius);
        const float angle = (static_cast<float>(bar) / static_cast<float>(numBars)) * 2.0f * kPi;
        const float a0 = angle - halfAngle;
        const float a1 = angle + halfAngle;

        const float ix0 = innerRadius * std::cos(a0) * kAspect, iy0 = innerRadius * std::sin(a0);
        const float ix1 = innerRadius * std::cos(a1) * kAspect, iy1 = innerRadius * std::sin(a1);
        const float ox0 = outerRadius * std::cos(a0) * kAspect, oy0 = outerRadius * std::sin(a0);
        const float ox1 = outerRadius * std::cos(a1) * kAspect, oy1 = outerRadius * std::sin(a1);

        constexpr float ri = 0.6f, gi = 0.15f, bi = 0.0f;
        const float ro = 1.0f, go = 0.55f + 0.45f * h, bo = 0.1f;

        pushVertex(out.fillVertices, ix0, iy0, ri, gi, bi, 1.0f);
        pushVertex(out.fillVertices, ix1, iy1, ri, gi, bi, 1.0f);
        pushVertex(out.fillVertices, ox1, oy1, ro, go, bo, 1.0f);

        pushVertex(out.fillVertices, ix0, iy0, ri, gi, bi, 1.0f);
        pushVertex(out.fillVertices, ox1, oy1, ro, go, bo, 1.0f);
        pushVertex(out.fillVertices, ox0, oy0, ro, go, bo, 1.0f);

        pushVertex(out.lineVertices, (ox0 + ox1) * 0.5f, (oy0 + oy1) * 0.5f, ro, go, bo, 1.0f);
    }

    out.linePrimitive = GL_LINE_LOOP;
    out.lineSegments  = {{0, static_cast<GLsizei>(numBars)}};
    out.lineGlow      = true;
    out.fillAlpha     = 1.0f;
    return out;
}

ModeOutput buildLissajous(const std::vector<float>& samples) {
    ModeOutput out;
    const size_t n = std::min<size_t>(1024, samples.size());
    const std::vector<float> ds = downsample(samples, n);
    const size_t delay = n / 6;

    constexpr float scale = 4.0f;

    out.lineVertices.reserve(n * 6);
    for (size_t i = 0; i < n; ++i) {
        const float x = std::clamp(ds[i] * scale, -1.0f, 1.0f) * 0.9f;
        const float y = std::clamp(ds[(i + delay) % n] * scale, -1.0f, 1.0f) * 0.9f;

        float r, g, b;
        hsvToRgb(static_cast<float>(i) / static_cast<float>(n), 0.85f, 1.0f, r, g, b);

        pushVertex(out.lineVertices, x, y, r, g, b, 1.0f);
    }

    out.linePrimitive = GL_LINE_STRIP;
    out.lineSegments  = {{0, static_cast<GLsizei>(n)}};
    out.lineGlow      = true;
    return out;
}

ModeOutput buildParticleField(const std::vector<float>& magnitudes, std::vector<float>& smoothedColumns) {
    ModeOutput out;
    const size_t numBins = magnitudes.size();
    const size_t cols = smoothedColumns.size();
    constexpr size_t rows = 18;

    out.lineVertices.reserve(cols * rows * 6);

    for (size_t col = 0; col < cols; ++col) {
        const auto [binStart, binEnd] = logBinRange(numBins, cols, col);
        const float mag = groupMagnitude(magnitudes, binStart, binEnd);
        updateSmoothed(smoothedColumns[col], magnitudeToNormalized(mag), 0.6f, 0.08f);
        const float level = smoothedColumns[col];

        const float x = -0.95f + (static_cast<float>(col) / static_cast<float>(cols - 1)) * 1.9f;

        for (size_t row = 0; row < rows; ++row) {
            const float normalizedRow = static_cast<float>(row) / static_cast<float>(rows - 1);
            const float y = -0.9f + normalizedRow * 1.8f;

            float r, g, b, size;
            if (normalizedRow <= level) {
                if (normalizedRow < 0.6f)       { r = 0.1f;  g = 1.0f;  b = 0.3f; }
                else if (normalizedRow < 0.85f) { r = 1.0f;  g = 0.9f;  b = 0.1f; }
                else                             { r = 1.0f;  g = 0.15f; b = 0.1f; }
                size = 11.0f;
            } else {
                r = 0.08f; g = 0.09f; b = 0.12f;
                size = 5.0f;
            }

            pushVertex(out.lineVertices, x, y, r, g, b, size);
        }
    }

    out.linePrimitive = GL_POINTS;
    out.linePoints    = true;
    out.lineGlow      = false;
    out.lineSegments  = {{0, static_cast<GLsizei>(cols * rows)}};
    return out;
}

ModeOutput buildLedBars(const std::vector<float>& magnitudes, std::vector<float>& smoothedHeights) {
    ModeOutput out;
    const size_t numBins = magnitudes.size();
    const size_t numBars = smoothedHeights.size();
    constexpr size_t segments = 14;

    constexpr float left = -0.97f, right = 0.97f, bottom = -0.92f, top = 0.92f;
    const float barWidth = (right - left) / static_cast<float>(numBars);
    const float barGap = barWidth * 0.2f;
    const float totalHeight = top - bottom;
    const float segHeight = totalHeight / static_cast<float>(segments);
    const float segGap = segHeight * 0.18f;

    out.fillVertices.reserve(numBars * segments * 6 * 6);

    for (size_t bar = 0; bar < numBars; ++bar) {
        const auto [binStart, binEnd] = logBinRange(numBins, numBars, bar);
        const float mag = groupMagnitude(magnitudes, binStart, binEnd);
        updateSmoothed(smoothedHeights[bar], magnitudeToNormalized(mag), 0.6f, 0.1f);
        const float h = smoothedHeights[bar];

        const float x0 = left + static_cast<float>(bar) * barWidth + barGap * 0.5f;
        const float x1 = x0 + barWidth - barGap;

        const size_t litSegments = static_cast<size_t>(h * static_cast<float>(segments) + 0.5f);

        for (size_t s = 0; s < litSegments && s < segments; ++s) {
            const float y0 = bottom + static_cast<float>(s) * segHeight + segGap * 0.5f;
            const float y1 = y0 + segHeight - segGap;

            const float frac = static_cast<float>(s) / static_cast<float>(segments);
            float r, g, b;
            if (frac < 0.6f)       { r = 0.1f; g = 0.9f;  b = 0.25f; }
            else if (frac < 0.85f) { r = 1.0f; g = 0.85f; b = 0.1f; }
            else                    { r = 1.0f; g = 0.15f; b = 0.1f; }

            pushVertex(out.fillVertices, x0, y0, r, g, b, 1.0f);
            pushVertex(out.fillVertices, x1, y0, r, g, b, 1.0f);
            pushVertex(out.fillVertices, x1, y1, r, g, b, 1.0f);

            pushVertex(out.fillVertices, x0, y0, r, g, b, 1.0f);
            pushVertex(out.fillVertices, x1, y1, r, g, b, 1.0f);
            pushVertex(out.fillVertices, x0, y1, r, g, b, 1.0f);
        }
    }

    out.lineVertices.clear();
    out.lineSegments.clear();
    out.lineGlow  = false;
    out.fillAlpha = 1.0f;
    return out;
}

ModeOutput buildPulseRings(const std::vector<float>& magnitudes, std::vector<float>& smoothedBands, float time) {
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
        updateSmoothed(smoothedBands[k], magnitudeToNormalized(mag), 0.4f, 0.06f);
    }

    constexpr float baseRadii[3]  = {0.15f, 0.27f, 0.39f};
    constexpr float ringScale[3]  = {0.30f, 0.22f, 0.16f};
    constexpr float phaseSpeed[3] = {0.3f, -0.2f, 0.15f};
    constexpr float colors[3][3]  = {
        {1.0f, 0.35f, 0.2f},
        {0.25f, 1.0f, 0.4f},
        {0.3f, 0.55f, 1.0f}
    };

    out.lineVertices.reserve(3 * ringSegments * 6);
    for (size_t k = 0; k < 3; ++k) {
        const float radius = baseRadii[k] + smoothedBands[k] * ringScale[k];
        const float phase = time * phaseSpeed[k];

        out.lineSegments.push_back({static_cast<GLint>(out.lineVertices.size() / 6),
                                     static_cast<GLsizei>(ringSegments)});

        for (size_t j = 0; j < ringSegments; ++j) {
            const float angle = phase + (static_cast<float>(j) / static_cast<float>(ringSegments)) * 2.0f * kPi;
            const float x = radius * std::cos(angle) * kAspect;
            const float y = radius * std::sin(angle);
            pushVertex(out.lineVertices, x, y, colors[k][0], colors[k][1], colors[k][2], 1.0f);
        }
    }

    out.linePrimitive = GL_LINE_LOOP;
    out.lineGlow      = true;
    return out;
}

} // namespace modes