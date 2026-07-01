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
    // Global / compile-time constants now configurable at runtime
    size_t minBars = 2;
    size_t maxBars = 4096;
    size_t denseBarCap = 8192;
    int    msaaSamples = 4;

    size_t numBars = 64;
    float sensitivity = 1.0f;
    float zoom = 1.0f;
    float aspect = 1.0f;

    size_t maxBarsLimit = 200;
    float maxSensitivityLimit = 5.0f;
    float limitMultiplier = 2.0f;

    float waveZoom = 1.0f;
    float waveSpeed = 0.0f;

    ColorMode colorMode = ColorMode::Normal;
    Color3 randomSolid{0.2f, 1.0f, 1.0f};
    std::vector<Color3> gradientColors{{1.0f, 0.0f, 0.3f}};
    size_t gradientColorCount = 1;

    bool bloom = false;
    float bloomIntensity = 1.0f;
    float bloomSize = 0.3f;
    int bloomSteps = 3;
    int bloomRings = 3;
    float bloomBallIntensity = 20.0f;

    bool antiAliasing = false;
    bool glow = true;
    bool jumpColor = false;
    bool jumpOnGradient = false;
    float colorBrightness = 0.5f;
    float jumpSensitivity = 1.0f;
    bool diffColor = false;
    float diffSensitivity = 1.0f;
    bool lineSmooth = false;
    float lineSharpness = 1.0f;
    size_t particleCount = 256;
    float particleSize = 11.0f;
    InputMode inputMode = InputMode::Microphone;
    TrueXYMode trueXYMode = TrueXYMode::Scatter;
    bool trueXYLines = false;
    bool analogScope = false;
    AnalogScopeMode analogScopeMode = AnalogScopeMode::Trace;
    float analogScopeDecay = 4.0f;
    size_t analogResolution = 4096;
    size_t analogLineCount = 4096;

    float windowOpacity = 1.0f;
};

Color3 resolveColor(const VisState& state, Color3 base, float t);