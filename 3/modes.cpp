#include "modes.hxx"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
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

} // namespace

namespace modes {

namespace {
    std::vector<std::array<float, 3>> s_palette = {
        {{0.05f, 0.4f, 0.7f}},
        {{1.0f, 0.2f, 0.4f}},
        {{1.0f, 0.85f, 0.1f}},
        {{0.1f, 0.9f, 0.3f}}
    };
}

void randomizeGradient() {
    for (auto& c : s_palette) {
        float h = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
        float s = 0.5f + 0.5f * (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX));
        float v = 0.6f + 0.4f * (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX));
        hsvToRgb(h, s, v, c[0], c[1], c[2]);
    }
}

void randomizeFlat() {
    float h = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
    float s = 0.6f + 0.4f * (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX));
    float v = 0.7f + 0.3f * (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX));
    float r, g, b;
    hsvToRgb(h, s, v, r, g, b);
    std::array<float, 3> c = {r, g, b};
    s_palette.assign(s_palette.size(), c);
}

int paletteCount() { return static_cast<int>(s_palette.size()); }

void setPaletteCount(int n) {
    if (n < 1) n = 1;
    if (n > 40) n = 40;
    size_t old = s_palette.size();
    s_palette.resize(static_cast<size_t>(n));
    for (size_t i = old; i < static_cast<size_t>(n); ++i) {
        float h = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
        float s = 0.5f + 0.5f * (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX));
        float v = 0.6f + 0.4f * (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX));
        hsvToRgb(h, s, v, s_palette[i][0], s_palette[i][1], s_palette[i][2]);
    }
}

void gradientColor(float t, float& r, float& g, float& b) {
    if (s_palette.empty()) { r = 1.0f; g = 1.0f; b = 1.0f; return; }
    t = std::clamp(t, 0.0f, 1.0f);
    if (s_palette.size() == 1) {
        r = s_palette[0][0]; g = s_palette[0][1]; b = s_palette[0][2];
        return;
    }
    float ft = t * static_cast<float>(s_palette.size() - 1);
    size_t idx = static_cast<size_t>(ft);
    float frac = ft - static_cast<float>(idx);
    if (idx >= s_palette.size() - 1) {
        r = s_palette.back()[0]; g = s_palette.back()[1]; b = s_palette.back()[2];
        return;
    }
    r = s_palette[idx][0] + frac * (s_palette[idx+1][0] - s_palette[idx][0]);
    g = s_palette[idx][1] + frac * (s_palette[idx+1][1] - s_palette[idx][1]);
    b = s_palette[idx][2] + frac * (s_palette[idx+1][2] - s_palette[idx][2]);
}

static int s_bloomRings = 3;
static int s_ringCount = 3;
static int s_bloomSteps = 3;
static float s_bloomIntensity = 20.0f;

int bloomRings() { return s_bloomRings; }
void setBloomRings(int n) { if (n >= 1 && n <= 400) s_bloomRings = n; }
int ringCount() { return s_ringCount; }
void setRingCount(int n) { if (n >= 1 && n <= 20) s_ringCount = n; }
int bloomSteps() { return s_bloomSteps; }
void setBloomSteps(int n) { if (n >= 1 && n <= 20) s_bloomSteps = n; }
float bloomIntensity() { return s_bloomIntensity; }
void setBloomIntensity(float v) { if (v >= 1.0f && v <= 600.0f) s_bloomIntensity = v; }

ModeOutput buildOscilloscope(const std::vector<float>& samples) {
    ModeOutput out;
    const std::vector<float> smoothed = smoothSignal(samples, 2);
    const size_t n = smoothed.size();

    out.lineVertices.reserve(n * 6);
    for (size_t i = 0; i < n; ++i) {
        const float x = (static_cast<float>(i) / static_cast<float>(n - 1)) * 2.0f - 1.0f;
        const float y = std::clamp(smoothed[i] * 2.0f, -1.0f, 1.0f);
        float r, g, b;
        gradientColor(std::abs(y) * 0.6f + 0.2f, r, g, b);
        pushVertex(out.lineVertices, x, y, r, g, b, 1.0f);
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
    out.tipVertices.reserve(numBars * static_cast<size_t>(s_bloomSteps) * 6);

    for (size_t bar = 0; bar < numBars; ++bar) {
        const auto [binStart, binEnd] = linearBinRange(numBins, numBars, bar);
        const float mag = groupMagnitude(magnitudes, binStart, binEnd);
        updateSmoothed(smoothedHeights[bar], magnitudeToNormalized(mag), 0.5f, 0.15f);
        const float h = smoothedHeights[bar];

        const float x0 = left + static_cast<float>(bar) * barWidth + gap * 0.5f;
        const float x1 = x0 + barWidth - gap;
        const float yTop = baseline + h * (top - baseline);

        float rb, gb, bb, rt, gt, bt;
        gradientColor(0.0f, rb, gb, bb);
        gradientColor(h, rt, gt, bt);

        pushVertex(out.fillVertices, x0, baseline, rb, gb, bb, 1.0f);
        pushVertex(out.fillVertices, x1, baseline, rb, gb, bb, 1.0f);
        pushVertex(out.fillVertices, x1, yTop,     rt, gt, bt, 1.0f);

        pushVertex(out.fillVertices, x0, baseline, rb, gb, bb, 1.0f);
        pushVertex(out.fillVertices, x1, yTop,     rt, gt, bt, 1.0f);
        pushVertex(out.fillVertices, x0, yTop,     rt, gt, bt, 1.0f);

        const float xc = (x0 + x1) * 0.5f;
        pushVertex(out.lineVertices, xc, yTop, rt, gt, bt, 1.0f);
        for (int si = 0; si < s_bloomSteps; ++si) {
            const float stepT = static_cast<float>(si + 1) / static_cast<float>(s_bloomSteps);
            const float yStep = baseline + stepT * (yTop - baseline);
            float sr, sg, sb;
            gradientColor(stepT * h, sr, sg, sb);
            out.tipVertices.insert(out.tipVertices.end(), {xc, yStep, sr, sg, sb, s_bloomIntensity * h});
        }
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
        amps[i] = std::clamp(smoothed[i] * 2.0f, -1.0f, 1.0f) * 0.85f;
    }

    out.fillVertices.reserve((n - 1) * 6 * 6);
    for (size_t i = 0; i + 1 < n; ++i) {
        const float a0 = std::fabs(amps[i]);
        const float a1 = std::fabs(amps[i + 1]);

        float r0, g0, b0, r1, g1, b1;
        gradientColor(a0 * 0.5f, r0, g0, b0);
        gradientColor(a1 * 0.5f, r1, g1, b1);

        pushVertex(out.fillVertices, xs[i],     amps[i],     r0, g0, b0, 1.0f);
        pushVertex(out.fillVertices, xs[i],    -amps[i],     r0, g0, b0, 1.0f);
        pushVertex(out.fillVertices, xs[i + 1], amps[i + 1], r1, g1, b1, 1.0f);

        pushVertex(out.fillVertices, xs[i],    -amps[i],     r0, g0, b0, 1.0f);
        pushVertex(out.fillVertices, xs[i + 1],-amps[i + 1], r1, g1, b1, 1.0f);
        pushVertex(out.fillVertices, xs[i + 1], amps[i + 1], r1, g1, b1, 1.0f);
    }

    out.lineVertices.reserve(n * 6);
    for (size_t i = 0; i < n; ++i) {
        float lr, lg, lb;
        gradientColor(0.5f, lr, lg, lb);
        pushVertex(out.lineVertices, xs[i], amps[i], lr, lg, lb, 1.0f);
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

        const float x = radius * std::cos(angle);
        const float y = radius * std::sin(angle);

        const float t = std::fabs(amp);
        float r, g, b;
        gradientColor(t, r, g, b);
        pushVertex(out.lineVertices, x, y, r, g, b, 1.0f);
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
    out.tipVertices.reserve(numBars * static_cast<size_t>(s_bloomSteps) * 6);

    for (size_t bar = 0; bar < numBars; ++bar) {
        const auto [binStart, binEnd] = linearBinRange(numBins, numBars, bar);
        const float mag = groupMagnitude(magnitudes, binStart, binEnd);
        updateSmoothed(smoothedHeights[bar], magnitudeToNormalized(mag), 0.5f, 0.15f);
        const float h = smoothedHeights[bar];

        const float outerRadius = innerRadius + h * (maxOuterRadius - innerRadius);
        const float angle = (static_cast<float>(bar) / static_cast<float>(numBars)) * 2.0f * kPi;
        const float a0 = angle - halfAngle;
        const float a1 = angle + halfAngle;

        const float ix0 = innerRadius * std::cos(a0), iy0 = innerRadius * std::sin(a0);
        const float ix1 = innerRadius * std::cos(a1), iy1 = innerRadius * std::sin(a1);
        const float ox0 = outerRadius * std::cos(a0), oy0 = outerRadius * std::sin(a0);
        const float ox1 = outerRadius * std::cos(a1), oy1 = outerRadius * std::sin(a1);

        float ri, gi, bi, ro, go, bo;
        gradientColor(0.0f, ri, gi, bi);
        gradientColor(h, ro, go, bo);

        pushVertex(out.fillVertices, ix0, iy0, ri, gi, bi, 1.0f);
        pushVertex(out.fillVertices, ix1, iy1, ri, gi, bi, 1.0f);
        pushVertex(out.fillVertices, ox1, oy1, ro, go, bo, 1.0f);

        pushVertex(out.fillVertices, ix0, iy0, ri, gi, bi, 1.0f);
        pushVertex(out.fillVertices, ox1, oy1, ro, go, bo, 1.0f);
        pushVertex(out.fillVertices, ox0, oy0, ro, go, bo, 1.0f);

        const float tx = (ox0 + ox1) * 0.5f, ty = (oy0 + oy1) * 0.5f;
        pushVertex(out.lineVertices, tx, ty, ro, go, bo, 1.0f);
        for (int si = 0; si < s_bloomSteps; ++si) {
            const float stepT = static_cast<float>(si + 1) / static_cast<float>(s_bloomSteps);
            const float rStep = innerRadius + stepT * (outerRadius - innerRadius);
            const float sx = rStep * std::cos(angle);
            const float sy = rStep * std::sin(angle);
            float sr, sg, sb;
            gradientColor(stepT * h, sr, sg, sb);
            out.tipVertices.insert(out.tipVertices.end(), {sx, sy, sr, sg, sb, s_bloomIntensity * h});
        }
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
        gradientColor(static_cast<float>(i) / static_cast<float>(n), r, g, b);

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
    out.tipVertices.reserve(cols * rows * 6);

    for (size_t col = 0; col < cols; ++col) {
        const auto [binStart, binEnd] = linearBinRange(numBins, cols, col);
        const float mag = groupMagnitude(magnitudes, binStart, binEnd);
        updateSmoothed(smoothedColumns[col], magnitudeToNormalized(mag), 0.6f, 0.08f);
        const float level = std::pow(smoothedColumns[col], 0.5f);

        const float x = -0.95f + (static_cast<float>(col) / static_cast<float>(cols - 1)) * 1.9f;

        for (size_t row = 0; row < rows; ++row) {
            const float normalizedRow = static_cast<float>(row) / static_cast<float>(rows - 1);
            const float y = -0.9f + normalizedRow * 1.8f;

            float r, g, b, size;
            if (normalizedRow <= level) {
                gradientColor(normalizedRow, r, g, b);
                size = 11.0f;
            } else {
                float dr = 0.08f, dg = 0.09f, db = 0.12f;
                r = dr; g = dg; b = db;
                size = 5.0f;
            }

            pushVertex(out.lineVertices, x, y, r, g, b, size);
            out.tipVertices.insert(out.tipVertices.end(), {x, y, r, g, b, s_bloomIntensity});
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
    out.tipVertices.reserve(numBars * static_cast<size_t>(s_bloomSteps) * 6);

    for (size_t bar = 0; bar < numBars; ++bar) {
        const auto [binStart, binEnd] = linearBinRange(numBins, numBars, bar);
        const float mag = groupMagnitude(magnitudes, binStart, binEnd);
        updateSmoothed(smoothedHeights[bar], magnitudeToNormalized(mag), 0.6f, 0.1f);
        const float h = std::pow(smoothedHeights[bar], 0.5f);

        const float x0 = left + static_cast<float>(bar) * barWidth + barGap * 0.5f;
        const float x1 = x0 + barWidth - barGap;

        const size_t litSegments = static_cast<size_t>(h * static_cast<float>(segments) + 0.5f);

        for (size_t s = 0; s < litSegments && s < segments; ++s) {
            const float y0 = bottom + static_cast<float>(s) * segHeight + segGap * 0.5f;
            const float y1 = y0 + segHeight - segGap;

            const float frac = static_cast<float>(s) / static_cast<float>(segments);
            float r, g, b;
            gradientColor(frac, r, g, b);

            pushVertex(out.fillVertices, x0, y0, r, g, b, 1.0f);
            pushVertex(out.fillVertices, x1, y0, r, g, b, 1.0f);
            pushVertex(out.fillVertices, x1, y1, r, g, b, 1.0f);

            pushVertex(out.fillVertices, x0, y0, r, g, b, 1.0f);
            pushVertex(out.fillVertices, x1, y1, r, g, b, 1.0f);
            pushVertex(out.fillVertices, x0, y1, r, g, b, 1.0f);
        }

        if (litSegments > 0) {
            const float xc = (x0 + x1) * 0.5f;
            const float tipY1 = bottom + static_cast<float>(litSegments - 1) * segHeight + segHeight - segGap;
            const float gradPos = static_cast<float>(litSegments) / static_cast<float>(segments);
            float tr, tg, tb;
            gradientColor(gradPos, tr, tg, tb);
            for (int si = 0; si < s_bloomSteps; ++si) {
                const float stepT = static_cast<float>(si + 1) / static_cast<float>(s_bloomSteps);
                const float yStep = bottom + stepT * (tipY1 - bottom);
                float sr, sg, sb;
                gradientColor(stepT * gradPos, sr, sg, sb);
                out.tipVertices.insert(out.tipVertices.end(), {xc, yStep, sr, sg, sb, s_bloomIntensity * h});
            }
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
    const size_t numRings = static_cast<size_t>(s_ringCount);

    if (smoothedBands.size() != numRings)
        smoothedBands.assign(numRings, 0.0f);

    out.lineVertices.reserve(numRings * ringSegments * 6);

    const float totalFreq = static_cast<float>(numBins);
    for (size_t k = 0; k < numRings; ++k) {
        const float t = static_cast<float>(k) / static_cast<float>(numRings);
        const size_t binStart = std::max<size_t>(1, static_cast<size_t>(t * totalFreq));
        const size_t binEnd = std::min(numBins, static_cast<size_t>((t + 1.0f / numRings) * totalFreq));

        const float mag = groupMagnitude(magnitudes, binStart, binEnd);
        if (k < smoothedBands.size())
            updateSmoothed(smoothedBands[k], magnitudeToNormalized(mag), 0.4f, 0.06f);
        const float bandH = k < smoothedBands.size() ? smoothedBands[k] : 0.0f;

        const float baseRadius = 0.12f + t * 0.38f;
        const float scale = 0.30f - t * 0.14f;
        const float radius = baseRadius + bandH * scale;
        const float phase = time * (k % 2 == 0 ? 0.3f : -0.2f);

        out.lineSegments.push_back({static_cast<GLint>(out.lineVertices.size() / 6),
                                     static_cast<GLsizei>(ringSegments)});

        for (size_t j = 0; j < ringSegments; ++j) {
            const float angle = phase + (static_cast<float>(j) / static_cast<float>(ringSegments)) * 2.0f * kPi;
            const float x = radius * std::cos(angle);
            const float y = radius * std::sin(angle);
            float pr, pg, pb;
            gradientColor(t, pr, pg, pb);
            pushVertex(out.lineVertices, x, y, pr, pg, pb, 1.0f);
        }
    }

    out.linePrimitive = GL_LINE_LOOP;
    out.lineGlow      = true;
    return out;
}

} // namespace modes