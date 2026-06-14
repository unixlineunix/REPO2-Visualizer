#pragma once

#include <Glad/glad.h>
#include <vector>

namespace modes {

constexpr float kAspect = 720.0f / 1280.0f;

struct Segment {
    GLint first;
    GLsizei count;
};

struct ModeOutput {
    // Drawn with GL_LINE_STRIP / GL_LINE_LOOP / GL_POINTS depending on linePrimitive/linePoints.
    std::vector<float> lineVertices; // interleaved: x, y, r, g, b, pointSize
    std::vector<Segment> lineSegments;
    GLenum linePrimitive = GL_LINE_STRIP;
    bool linePoints = false;
    bool lineGlow = true;

    // Drawn with GL_TRIANGLES.
    std::vector<float> fillVertices; // interleaved: x, y, r, g, b, pointSize
    float fillAlpha = 1.0f;

    // Bloom points at tips, drawn with GL_POINTS (additive blend). Format: x, y, r, g, b, size.
    std::vector<float> tipVertices;
};

ModeOutput buildOscilloscope(const std::vector<float>& samples);
ModeOutput buildSpectrumBars(const std::vector<float>& magnitudes, std::vector<float>& smoothedHeights);
ModeOutput buildMirroredWaveform(const std::vector<float>& samples);
ModeOutput buildCircularOscilloscope(const std::vector<float>& samples);
ModeOutput buildCircularSpectrum(const std::vector<float>& magnitudes, std::vector<float>& smoothedHeights);
ModeOutput buildLissajous(const std::vector<float>& samples);
ModeOutput buildParticleField(const std::vector<float>& magnitudes, std::vector<float>& smoothedColumns);
ModeOutput buildLedBars(const std::vector<float>& magnitudes, std::vector<float>& smoothedHeights);
ModeOutput buildPulseRings(const std::vector<float>& magnitudes, std::vector<float>& smoothedBands, float time);

// Palette API
void randomizeGradient();
void randomizeFlat();
int paletteCount();
void setPaletteCount(int n);
float bloomIntensity();
void setBloomIntensity(float v);
int bloomRings();
void setBloomRings(int n);
int ringCount();
void setRingCount(int n);
int bloomSteps();
void setBloomSteps(int n);
void gradientColor(float t, float& r, float& g, float& b);

} // namespace modes