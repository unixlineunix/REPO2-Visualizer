#pragma once

#include <Glad/glad.h>
#include <vector>

namespace modes {

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

} // namespace modes