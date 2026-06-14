#include <Glad/glad.h>
#include <GLFW/glfw3.h>

#include "audio.hxx"
#include "shader.hxx"
#include "fft.hxx"
#include "modes.hxx"
#include "vis_state.hxx"

#include <algorithm>
#include <array>
#include <iostream>
#include <random>
#include <vector>

namespace {

constexpr int WINDOW_WIDTH  = 1280;
constexpr int WINDOW_HEIGHT = 720;

constexpr size_t RING_BUFFER_SIZE = 16384;
constexpr size_t FFT_SIZE         = 4096;

constexpr size_t MAX_LINE_VERTICES = 32768;
constexpr size_t MAX_FILL_VERTICES = 131072;
constexpr size_t MAX_GLOW_VERTICES = 262144;
constexpr size_t VERTEX_FLOATS     = 6; // x, y, r, g, b, pointSize

constexpr size_t MIN_BARS = 2;
constexpr size_t MAX_BARS = 4096;
constexpr size_t DENSE_BAR_CAP = 8192;

constexpr int MSAA_SAMPLES = 4;

enum class VisMode {
    TrueXY               = 0,
    Oscilloscope         = 1,
    SpectrumBars         = 2,
    MirroredWaveform     = 3,
    CircularOscilloscope = 4,
    CircularSpectrum     = 5,
    Lissajous            = 6,
    DenseSpectrum        = 7,
    CircularSpectrumFilled = 8,
    PulseRings           = 9
};

const char* modeName(VisMode mode) {
    switch (mode) {
        case VisMode::TrueXY:               return "0: True XY Oscilloscope";
        case VisMode::Oscilloscope:           return "1: Oscilloscope";
        case VisMode::SpectrumBars:           return "2: Spectrum Bars";
        case VisMode::MirroredWaveform:       return "3: Mirrored Waveform";
        case VisMode::CircularOscilloscope:   return "4: Circular Oscilloscope";
        case VisMode::CircularSpectrum:       return "5: Circular Spectrum";
        case VisMode::Lissajous:              return "6: Lissajous";
        case VisMode::DenseSpectrum:          return "7: Dense Spectrum";
        case VisMode::CircularSpectrumFilled: return "8: Circular Spectrum (Filled)";
        case VisMode::PulseRings:             return "9: Pulse Rings";
    }
    return "unknown";
}

bool keyEdge(GLFWwindow* window, int key, std::array<bool, GLFW_KEY_LAST + 1>& prevKeys) {
    const bool down = glfwGetKey(window, key) == GLFW_PRESS;
    const bool edge = down && !prevKeys[static_cast<size_t>(key)];
    prevKeys[static_cast<size_t>(key)] = down;
    return edge;
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

Color3 randomColor(std::mt19937& rng) {
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    float h = dist(rng);
    float s = 0.5f + 0.5f * dist(rng);
    float v = 0.6f + 0.4f * dist(rng);
    float r, g, b;
    hsvToRgb(h, s, v, r, g, b);
    return {r, g, b};
}

void randomizeGradient(VisState& state, std::mt19937& rng) {
    state.gradientColors.clear();
    for (size_t i = 0; i < state.gradientColorCount; ++i) {
        state.gradientColors.push_back(randomColor(rng));
    }
}

void resizeBarVectors(const VisState& state,
                       std::vector<float>& spectrumHeights,
                       std::vector<float>& circularHeights,
                       std::vector<float>& denseHeights,
                       std::vector<float>& filledHeights) {
    spectrumHeights.resize(state.numBars, 0.0f);
    circularHeights.resize(state.numBars, 0.0f);
    filledHeights.resize(state.numBars, 0.0f);

    const size_t denseCount = std::min<size_t>(state.numBars * 2, DENSE_BAR_CAP);
    denseHeights.resize(denseCount, 0.0f);
}

bool createMsaaFramebuffer(int width, int height, GLuint& fbo, GLuint& colorRBO) {
    glGenFramebuffers(1, &fbo);
    glGenRenderbuffers(1, &colorRBO);

    glBindRenderbuffer(GL_RENDERBUFFER, colorRBO);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, MSAA_SAMPLES, GL_RGBA8, width, height);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorRBO);

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return status == GL_FRAMEBUFFER_COMPLETE;
}

void framebufferSizeCallback(GLFWwindow* /*window*/, int width, int height) {
    glViewport(0, 0, width, height);
}

} // namespace

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT,
                                           "Audio Visualizer", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::cerr << "Failed to initialize GLAD\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    int fbWidth = WINDOW_WIDTH;
    int fbHeight = WINDOW_HEIGHT;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);

    glViewport(0, 0, fbWidth, fbHeight);
    glEnable(GL_BLEND);
    glEnable(GL_PROGRAM_POINT_SIZE);

    AudioCapture audio(RING_BUFFER_SIZE);
    if (!audio.start()) {
        std::cerr << "Failed to start audio capture\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    Shader shader("vertex_shader.glsl", "fragment_shader.glsl");

    const size_t stride = VERTEX_FLOATS * sizeof(float);

    auto setupVAO = [&](GLuint& vao, GLuint& vbo, size_t maxVertices) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(maxVertices * stride), nullptr, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(stride), reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(stride), reinterpret_cast<void*>(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(stride), reinterpret_cast<void*>(5 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glBindVertexArray(0);
    };

    GLuint lineVAO = 0, lineVBO = 0;
    GLuint fillVAO = 0, fillVBO = 0;
    GLuint glowVAO = 0, glowVBO = 0;
    setupVAO(lineVAO, lineVBO, MAX_LINE_VERTICES);
    setupVAO(fillVAO, fillVBO, MAX_FILL_VERTICES);
    setupVAO(glowVAO, glowVBO, MAX_GLOW_VERTICES);


    glfwSwapInterval(0);

    GLuint msaaFBO = 0, msaaColorRBO = 0;
    bool msaaReady = createMsaaFramebuffer(fbWidth, fbHeight, msaaFBO, msaaColorRBO);
    if (!msaaReady) {
        std::cerr << "Warning: MSAA framebuffer creation failed; anti-aliasing unavailable\n";
    }

    VisState state;
    state.aspect = static_cast<float>(fbHeight) / static_cast<float>(fbWidth);

    std::mt19937 rng(std::random_device{}());

    std::array<bool, GLFW_KEY_LAST + 1> prevKeys{};

    VisMode mode = VisMode::Oscilloscope;
    std::cout << "Mode: " << modeName(mode) << "\n";

    std::vector<float> sampleBuffer(FFT_SIZE);
    std::vector<float> leftBuffer(FFT_SIZE), rightBuffer(FFT_SIZE);
    std::vector<float> magnitudes;

    std::vector<float> spectrumHeights;
    std::vector<float> circularSpectrumHeights;
    std::vector<float> denseHeights;
    std::vector<float> circularFilledHeights;
    std::vector<float> pulseBands(3, 0.0f);
    resizeBarVectors(state, spectrumHeights, circularSpectrumHeights, denseHeights, circularFilledHeights);

    constexpr float kBloomIntensityLevels[] = {0.5f, 1.0f, 1.5f, 2.0f, 2.5f};
    size_t bloomIntensityIndex = 1;
    state.bloomIntensity = kBloomIntensityLevels[bloomIntensityIndex];

    while (!glfwWindowShouldClose(window)) {
        // --- Resize handling ---
        int currentFbWidth, currentFbHeight;
        glfwGetFramebufferSize(window, &currentFbWidth, &currentFbHeight);
        if (currentFbWidth != fbWidth || currentFbHeight != fbHeight) {
            fbWidth = currentFbWidth;
            fbHeight = currentFbHeight;
            state.aspect = static_cast<float>(fbHeight) / static_cast<float>(fbWidth);
            
            if (msaaReady) {
                glDeleteRenderbuffers(1, &msaaColorRBO);
                glDeleteFramebuffers(1, &msaaFBO);
                msaaReady = createMsaaFramebuffer(fbWidth, fbHeight, msaaFBO, msaaColorRBO);
            }
        }

        // --- Mode switching ---
        for (int k = 0; k <= 9; ++k) {
            int key = (k == 0) ? GLFW_KEY_0 : (GLFW_KEY_1 + k - 1);
            if (glfwGetKey(window, key) == GLFW_PRESS) {
                const VisMode requested = static_cast<VisMode>(k);
                if (requested != mode) {
                    mode = requested;
                    if (state.analogScope) {
                        state.analogScope = false;
                        std::cout << "Analog Scope: off\n";
                    }
                    std::cout << "Mode: " << modeName(mode) << "\n";
                }
            }
        }

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }

        // --- Bar count or Wave Zoom (Left / Right) ---
        if (keyEdge(window, GLFW_KEY_LEFT, prevKeys)) {
            if (mode == VisMode::Oscilloscope || mode == VisMode::MirroredWaveform || mode == VisMode::TrueXY) {
                if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
                    state.waveSpeed -= 0.1f;
                    std::cout << "Wave Speed: " << state.waveSpeed << "\n";
                } else {
                    state.waveZoom = std::max(0.1f, state.waveZoom - 0.1f);
                    std::cout << "Wave Zoom: " << state.waveZoom << "\n";
                }
            } else {
                if (state.numBars > MIN_BARS) {
                    state.numBars = (state.numBars - MIN_BARS >= 4) ? state.numBars - 4 : MIN_BARS;
                    resizeBarVectors(state, spectrumHeights, circularSpectrumHeights, denseHeights, circularFilledHeights);
                    std::cout << "Bars: " << state.numBars << "\n";
                }
            }
        }
        if (keyEdge(window, GLFW_KEY_RIGHT, prevKeys)) {
            if (mode == VisMode::Oscilloscope || mode == VisMode::MirroredWaveform || mode == VisMode::TrueXY) {
                if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
                    state.waveSpeed += 0.1f;
                    std::cout << "Wave Speed: " << state.waveSpeed << "\n";
                } else {
                    state.waveZoom = std::min(10.0f, state.waveZoom + 0.1f);
                    std::cout << "Wave Zoom: " << state.waveZoom << "\n";
                }
            } else {
                if (state.numBars < state.maxBarsLimit) {
                    state.numBars = std::min<size_t>(state.maxBarsLimit, state.numBars + 4);
                    resizeBarVectors(state, spectrumHeights, circularSpectrumHeights, denseHeights, circularFilledHeights);
                    std::cout << "Bars: " << state.numBars << "\n";
                }
            }
        }

        // --- Break Limits (B) ---
        if (keyEdge(window, GLFW_KEY_B, prevKeys)) {
            state.maxBarsLimit = std::min<size_t>(MAX_BARS, state.maxBarsLimit * 2);
            state.maxSensitivityLimit *= 2.0f;
            std::cout << "--- LIMITS DOUBLED ---\n";
            std::cout << "Max Bars: " << state.maxBarsLimit << "\n";
            std::cout << "Max Sensitivity: " << state.maxSensitivityLimit << "\n";
        }

        // --- Sensitivity (Up / Down) ---
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
            state.sensitivity = std::min(state.maxSensitivityLimit, state.sensitivity + 0.02f);
        }
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
            state.sensitivity = std::max(0.001f, state.sensitivity - 0.02f);
        }

        // --- Zoom (- and =) ---
        if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS) {
            state.zoom = std::max(0.3f, state.zoom - 0.01f);
        }
        if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS) {
            state.zoom = std::min(3.0f, state.zoom + 0.01f);
        }

        // --- Color modes ---
        if (keyEdge(window, GLFW_KEY_R, prevKeys)) {
            state.colorMode = ColorMode::RandomSolid;
            state.randomSolid = randomColor(rng);
        }
        if (keyEdge(window, GLFW_KEY_G, prevKeys)) {
            state.colorMode = ColorMode::RandomGradient;
            randomizeGradient(state, rng);
            std::cout << "Gradient randomized (" << state.gradientColorCount << " colors)\n";
        }
        if (keyEdge(window, GLFW_KEY_PERIOD, prevKeys)) {
            const bool shiftHeld = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS
                                || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
            if (shiftHeld) {
                state.waveSpeed = std::min(20.0f, state.waveSpeed + 0.5f);
                std::cout << "Wave Speed: " << state.waveSpeed << "\n";
            } else if (state.gradientColorCount < 64) {
                state.gradientColorCount++;
                if (state.colorMode == ColorMode::RandomGradient) {
                    state.gradientColors.push_back(randomColor(rng));
                }
                std::cout << "Gradient colors: " << state.gradientColorCount << "\n";
            }
        }
        if (keyEdge(window, GLFW_KEY_COMMA, prevKeys)) {
            const bool shiftHeld = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS
                                || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
            if (shiftHeld) {
                state.waveSpeed = std::max(0.0f, state.waveSpeed - 0.5f);
                std::cout << "Wave Speed: " << state.waveSpeed << "\n";
            } else if (state.gradientColorCount > 1) {
                state.gradientColorCount--;
                if (state.colorMode == ColorMode::RandomGradient && !state.gradientColors.empty()) {
                    state.gradientColors.pop_back();
                }
                std::cout << "Gradient colors: " << state.gradientColorCount << "\n";
            }
        }

        // --- Audio input source ---
        if (keyEdge(window, GLFW_KEY_M, prevKeys)) {
            if (audio.switchSource(CaptureSource::Microphone)) {
                state.inputMode = InputMode::Microphone;
                std::cout << "Input: Microphone\n";
            } else {
                std::cerr << "Failed to switch to microphone input\n";
            }
        }
        if (keyEdge(window, GLFW_KEY_S, prevKeys)) {
            if (audio.switchSource(CaptureSource::SystemAudio)) {
                state.inputMode = InputMode::SystemAudio;
                std::cout << "Input: System audio\n";
            } else {
                std::cerr << "Failed to switch to system audio input\n";
            }
        }

        // --- Anti-aliasing ---
        if (keyEdge(window, GLFW_KEY_A, prevKeys)) {
            state.antiAliasing = !state.antiAliasing;
            std::cout << "Anti-aliasing: " << (state.antiAliasing ? "on" : "off") << "\n";
        }

        // --- Bloom ---
        if (keyEdge(window, GLFW_KEY_L, prevKeys)) {
            state.bloom = !state.bloom;
            std::cout << "Bloom: " << (state.bloom ? "on" : "off") << "\n";
        }
        if (keyEdge(window, GLFW_KEY_APOSTROPHE, prevKeys)) {
            const bool shiftHeld = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS
                                || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
            if (shiftHeld && state.analogScope) {
                state.analogLineCount = std::min(size_t(4096), state.analogLineCount * 2);
                std::cout << "AnalogScope line count: " << state.analogLineCount << "\n";
            } else {
                bloomIntensityIndex = (bloomIntensityIndex + 1) % 5;
                state.bloomIntensity = kBloomIntensityLevels[bloomIntensityIndex];
                std::cout << "Bloom intensity: " << state.bloomIntensity << "\n";
            }
        }
        if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS) {
            state.bloomSize = std::max(0.05f, state.bloomSize - 0.01f);
        }
        if (glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS) {
            state.bloomSize = std::min(1.0f, state.bloomSize + 0.01f);
        }

        // --- TrueXY / AnalogScope sub-modes (Q/W/E/I/O/P) ---
        if (keyEdge(window, GLFW_KEY_Q, prevKeys)) {
            if (state.analogScope) {
                state.analogScopeMode = AnalogScopeMode::Scatter;
                std::cout << "AnalogScope: Scatter\n";
            } else {
                state.trueXYMode = TrueXYMode::Scatter;
                std::cout << "TrueXY: Scatter\n";
            }
        }
        if (keyEdge(window, GLFW_KEY_W, prevKeys)) {
            if (state.analogScope) {
                state.analogScopeMode = AnalogScopeMode::Both;
                std::cout << "AnalogScope: Both\n";
            } else {
                state.trueXYMode = TrueXYMode::LineStrip;
                std::cout << "TrueXY: Line Strip\n";
            }
        }
        if (keyEdge(window, GLFW_KEY_E, prevKeys) && !state.analogScope) {
            state.trueXYMode = TrueXYMode::Both;
            std::cout << "TrueXY: Both\n";
        }
        if (keyEdge(window, GLFW_KEY_I, prevKeys) && !state.analogScope) {
            state.trueXYMode = TrueXYMode::FilledTrail;
            std::cout << "TrueXY: Filled Trail\n";
        }
        if (keyEdge(window, GLFW_KEY_O, prevKeys) && !state.analogScope) {
            state.trueXYMode = TrueXYMode::GlowScatter;
            std::cout << "TrueXY: Glow Scatter\n";
        }
        if (keyEdge(window, GLFW_KEY_J, prevKeys) && !state.analogScope) {
            state.trueXYMode = TrueXYMode::PhosphorTrail;
            std::cout << "TrueXY: Phosphor Trail\n";
        }
        if (keyEdge(window, GLFW_KEY_H, prevKeys)) {
            if (state.analogScope) {
                state.analogScopeMode = AnalogScopeMode::Trace;
                std::cout << "AnalogScope: Trace\n";
            } else {
                state.analogScope = true;
                std::cout << "Analog Scope (P31): on\n";
            }
        }
        if (keyEdge(window, GLFW_KEY_P, prevKeys)) {
            if (state.analogScope) {
                constexpr float rates[] = {18.0f, 1.0f, 2.0f, 3.0f};
                static size_t pIdx = 0;
                pIdx = (pIdx + 1) % 4;
                state.analogScopeDecay = rates[pIdx];
                std::cout << "AnalogScope decay rate: " << state.analogScopeDecay << "\n";
            } else {
                state.trueXYLines = !state.trueXYLines;
                std::cout << "TrueXY Lines: " << (state.trueXYLines ? "on" : "off") << "\n";
            }
        }
        if (keyEdge(window, GLFW_KEY_Z, prevKeys) && state.analogScope) {
            state.analogResolution = std::max(size_t(64), state.analogResolution / 2);
            std::cout << "AnalogScope resolution: " << state.analogResolution << "\n";
        }
        if (keyEdge(window, GLFW_KEY_X, prevKeys) && state.analogScope) {
            state.analogResolution = std::min(size_t(4096), state.analogResolution * 2);
            std::cout << "AnalogScope resolution: " << state.analogResolution << "\n";
        }
        if (keyEdge(window, GLFW_KEY_C, prevKeys) && state.analogScope) {
            state.analogScopeDecay = std::max(0.5f, state.analogScopeDecay * 0.75f);
            std::cout << "AnalogScope decay: " << state.analogScopeDecay << "\n";
        }
        if (keyEdge(window, GLFW_KEY_V, prevKeys) && state.analogScope) {
            state.analogScopeDecay = std::min(40.0f, state.analogScopeDecay * 1.5f);
            std::cout << "AnalogScope decay: " << state.analogScopeDecay << "\n";
        }
        if (keyEdge(window, GLFW_KEY_SEMICOLON, prevKeys) && state.analogScope) {
            state.analogLineCount = std::max(size_t(64), state.analogLineCount / 2);
            std::cout << "AnalogScope line count: " << state.analogLineCount << "\n";
        }

        // --- Audio + FFT ---
        audio.readLatestStereo(leftBuffer.data(), rightBuffer.data(), FFT_SIZE);
        for (size_t i = 0; i < FFT_SIZE; ++i) {
            sampleBuffer[i] = (leftBuffer[i] + rightBuffer[i]) * 0.5f;
        }
        fft::computeMagnitudeSpectrum(sampleBuffer.data(), FFT_SIZE, magnitudes);

        modes::ModeOutput output;
        const float time = static_cast<float>(glfwGetTime());

        if (state.analogScope) {
            output = modes::buildAnalogScope(leftBuffer, rightBuffer, state);
        } else switch (mode) {
            case VisMode::TrueXY:
                output = modes::buildTrueXY(leftBuffer, rightBuffer, state);
                break;
            case VisMode::Oscilloscope:
                output = modes::buildOscilloscope(sampleBuffer, state);
                break;
            case VisMode::SpectrumBars:
                output = modes::buildSpectrumBars(magnitudes, spectrumHeights, state);
                break;
            case VisMode::MirroredWaveform:
                output = modes::buildMirroredWaveform(sampleBuffer, state);
                break;
            case VisMode::CircularOscilloscope:
                output = modes::buildCircularOscilloscope(sampleBuffer, state);
                break;
            case VisMode::CircularSpectrum:
                output = modes::buildCircularSpectrum(magnitudes, circularSpectrumHeights, state);
                break;
            case VisMode::Lissajous:
                output = modes::buildLissajous(sampleBuffer, state);
                break;
            case VisMode::DenseSpectrum:
                output = modes::buildDenseSpectrum(magnitudes, denseHeights, state);
                break;
            case VisMode::CircularSpectrumFilled:
                output = modes::buildCircularSpectrumFilled(magnitudes, circularFilledHeights, state, time);
                break;
            case VisMode::PulseRings:
                output = modes::buildPulseRings(magnitudes, pulseBands, state, time);
                break;
        }

        // --- Render ---
        const bool useMsaa = state.antiAliasing && msaaReady;
        glBindFramebuffer(GL_FRAMEBUFFER, useMsaa ? msaaFBO : 0);
        glViewport(0, 0, fbWidth, fbHeight);

        glClearColor(0.02f, 0.02f, 0.04f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        shader.use();
        shader.setFloat("uZoom", state.zoom);

        if (!output.fillVertices.empty()) {
            glBindBuffer(GL_ARRAY_BUFFER, fillVBO);
            glBufferData(GL_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(output.fillVertices.size() * sizeof(float)),
                         output.fillVertices.data(), GL_DYNAMIC_DRAW);

            glBindVertexArray(fillVAO);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            shader.setInt("uIsPoint", 0);
            shader.setFloat("uAlpha", output.fillAlpha);
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(output.fillVertices.size() / VERTEX_FLOATS));
        }

        if (!output.glowVertices.empty()) {
            glBindBuffer(GL_ARRAY_BUFFER, glowVBO);
            glBufferData(GL_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(output.glowVertices.size() * sizeof(float)),
                         output.glowVertices.data(), GL_DYNAMIC_DRAW);

            glBindVertexArray(glowVAO);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            shader.setInt("uIsPoint", 0);
            shader.setFloat("uAlpha", std::clamp(state.bloomIntensity, 0.0f, 3.0f));
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(output.glowVertices.size() / VERTEX_FLOATS));
        }

        if (!output.lineVertices.empty()) {
            glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
            glBufferData(GL_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(output.lineVertices.size() * sizeof(float)),
                         output.lineVertices.data(), GL_DYNAMIC_DRAW);

            glBindVertexArray(lineVAO);
            shader.setInt("uIsPoint", output.linePoints ? 1 : 0);

            if (output.linePoints) {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                shader.setFloat("uAlpha", 1.0f);
                for (const auto& seg : output.lineSegments) {
                    glDrawArrays(GL_POINTS, seg.first, seg.count);
                }
            } else {
                float glowWidth = 6.0f;
                float glowAlpha = 0.25f;
                if (state.bloom) {
                    glowWidth *= (1.0f + state.bloomSize * 2.0f);
                    glowAlpha *= state.bloomIntensity;
                }

                for (const auto& seg : output.lineSegments) {
                    if (output.lineGlow) {
                        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                        glLineWidth(glowWidth);
                        shader.setFloat("uAlpha", glowAlpha);
                        glDrawArrays(output.linePrimitive, seg.first, seg.count);
                    }

                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    glLineWidth(output.lineWidth);
                    shader.setFloat("uAlpha", 1.0f);
                    glDrawArrays(output.linePrimitive, seg.first, seg.count);
                }
            }
        }

        glBindVertexArray(0);

        if (useMsaa) {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, msaaFBO);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glBlitFramebuffer(0, 0, fbWidth, fbHeight, 0, 0, fbWidth, fbHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    audio.stop();

    glDeleteBuffers(1, &lineVBO);
    glDeleteVertexArrays(1, &lineVAO);
    glDeleteBuffers(1, &fillVBO);
    glDeleteVertexArrays(1, &fillVAO);
    glDeleteBuffers(1, &glowVBO);
    glDeleteVertexArrays(1, &glowVAO);

    if (msaaReady) {
        glDeleteRenderbuffers(1, &msaaColorRBO);
        glDeleteFramebuffers(1, &msaaFBO);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}