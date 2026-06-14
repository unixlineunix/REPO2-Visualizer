#pragma once

#include <vector>
#include <cstddef>

struct Color3 {
    float r;
    float g;
    float b;
};

enum class ColorMode { Normal, RandomSolid, RandomGradient };
enum class InputMode { Microphone, SystemAudio };
enum class TrueXYMode { Scatter, LineStrip, Both, FilledTrail, GlowScatter, PhosphorTrail };
enum class AnalogScopeMode { Trace, Scatter, Both };

struct VisState {
    size_t numBars = 64;
    float sensitivity = 1.0f;
    float zoom = 1.0f;
    float aspect = 1.0f;

    size_t maxBarsLimit = 200;
    float maxSensitivityLimit = 5.0f;

    float waveZoom = 1.0f;
    float waveSpeed = 0.0f;

    ColorMode colorMode = ColorMode::Normal;
    Color3 randomSolid{0.2f, 1.0f, 1.0f};
    std::vector<Color3> gradientColors{{1.0f, 0.0f, 0.3f}};
    size_t gradientColorCount = 1;

    bool bloom = false;
    float bloomIntensity = 1.0f;
    float bloomSize = 0.3f;

    bool antiAliasing = false;
    InputMode inputMode = InputMode::Microphone;
    TrueXYMode trueXYMode = TrueXYMode::Scatter;
    bool trueXYLines = false;
    bool analogScope = false;
    AnalogScopeMode analogScopeMode = AnalogScopeMode::Trace;
};

Color3 resolveColor(const VisState& state, Color3 base, float t);