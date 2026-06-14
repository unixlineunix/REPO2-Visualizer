#pragma once

#include <Glad/glad.h>
#include <vector>

#include "vis_state.hxx"

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
    float lineWidth = 1.8f;

    // Drawn with GL_TRIANGLES.
    std::vector<float> fillVertices; // interleaved: x, y, r, g, b, pointSize
    float fillAlpha = 1.0f;

    // Additive bloom overlay, drawn with GL_TRIANGLES after fillVertices.
    std::vector<float> glowVertices; // interleaved: x, y, r, g, b, pointSize
};

ModeOutput buildOscilloscope(const std::vector<float>& samples, const VisState& state);
ModeOutput buildSpectrumBars(const std::vector<float>& magnitudes, std::vector<float>& smoothedHeights, const VisState& state);
ModeOutput buildMirroredWaveform(const std::vector<float>& samples, const VisState& state);
ModeOutput buildCircularOscilloscope(const std::vector<float>& samples, const VisState& state);
ModeOutput buildCircularSpectrum(const std::vector<float>& magnitudes, std::vector<float>& smoothedHeights, const VisState& state);
ModeOutput buildLissajous(const std::vector<float>& samples, const VisState& state);
ModeOutput buildDenseSpectrum(const std::vector<float>& magnitudes, std::vector<float>& smoothedHeights, const VisState& state);
ModeOutput buildCircularSpectrumFilled(const std::vector<float>& magnitudes, std::vector<float>& smoothedHeights, const VisState& state, float time);
ModeOutput buildPulseRings(const std::vector<float>& magnitudes, std::vector<float>& smoothedBands, const VisState& state, float time);
ModeOutput buildTrueXY(const std::vector<float>& left, const std::vector<float>& right, const VisState& state);
ModeOutput buildAnalogScope(const std::vector<float>& left, const std::vector<float>& right, const VisState& state);

} // namespace modes