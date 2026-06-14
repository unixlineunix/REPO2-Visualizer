#include <Glad/glad.h>
#include <GLFW/glfw3.h>

#include "audio.hxx"
#include "shader.hxx"
#include "fft.hxx"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <vector>

namespace {

constexpr int WINDOW_WIDTH  = 1280;
constexpr int WINDOW_HEIGHT = 720;

constexpr size_t RING_BUFFER_SIZE   = 16384;
constexpr size_t OSCILLOSCOPE_POINTS = 1024;
constexpr size_t FFT_SIZE           = 2048;
constexpr size_t SPECTRUM_BARS      = 80;

constexpr float OSCILLOSCOPE_GAIN = 4.0f;

enum class VisMode { Oscilloscope, Spectrum };

void framebufferSizeCallback(GLFWwindow* /*window*/, int width, int height) {
    glViewport(0, 0, width, height);
}

// Builds an interleaved (x, y, r, g, b) vertex buffer for the oscilloscope.
// Applies a small moving-average smoothing filter across neighboring samples.
void buildOscilloscopeVertices(const std::vector<float>& samples, std::vector<float>& out) {
    const size_t n = samples.size();
    out.clear();
    out.reserve(n * 5);

    std::vector<float> smoothed(n);
    constexpr int radius = 2;
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
        smoothed[i] = sum / static_cast<float>(count);
    }

    for (size_t i = 0; i < n; ++i) {
        const float x = (static_cast<float>(i) / static_cast<float>(n - 1)) * 2.0f - 1.0f;
        const float y = std::clamp(smoothed[i] * OSCILLOSCOPE_GAIN, -1.0f, 1.0f);

        out.push_back(x);
        out.push_back(y);
        out.push_back(0.2f); // R
        out.push_back(1.0f); // G
        out.push_back(1.0f); // B
    }
}

// Groups FFT magnitude bins into log-scaled bars and builds:
//  - fillOut:    triangle list for filled bars (x, y, r, g, b)
//  - outlineOut: line-strip vertices tracing the bar tops (x, y, r, g, b)
// smoothedHeights persists across frames for temporal smoothing/decay.
void buildSpectrumVertices(const std::vector<float>& magnitudes,
                            std::vector<float>& smoothedHeights,
                            std::vector<float>& fillOut,
                            std::vector<float>& outlineOut) {
    const size_t numBins = magnitudes.size();
    const size_t numBars = smoothedHeights.size();

    fillOut.clear();
    outlineOut.clear();
    fillOut.reserve(numBars * 6 * 5);
    outlineOut.reserve(numBars * 5);

    constexpr float baseline = -0.95f;
    constexpr float top      =  0.95f;
    constexpr float left     = -0.98f;
    constexpr float right    =  0.98f;

    const float totalWidth = right - left;
    const float barWidth   = totalWidth / static_cast<float>(numBars);
    const float gap        = barWidth * 0.15f;

    for (size_t bar = 0; bar < numBars; ++bar) {
        const double t0 = static_cast<double>(bar) / static_cast<double>(numBars);
        const double t1 = static_cast<double>(bar + 1) / static_cast<double>(numBars);

        size_t binStart = static_cast<size_t>(std::pow(static_cast<double>(numBins), t0));
        size_t binEnd   = static_cast<size_t>(std::pow(static_cast<double>(numBins), t1));

        binStart = std::clamp<size_t>(binStart, 1, numBins - 1);
        binEnd   = std::clamp<size_t>(binEnd, binStart + 1, numBins);

        float magSum = 0.0f;
        size_t magCount = 0;
        for (size_t b = binStart; b < binEnd; ++b) {
            magSum += magnitudes[b];
            ++magCount;
        }
        const float mag = magCount > 0 ? magSum / static_cast<float>(magCount) : 0.0f;

        const float db = 20.0f * std::log10(mag + 1e-6f);
        float normalized = (db + 60.0f) / 60.0f;
        normalized = std::clamp(normalized, 0.0f, 1.0f);

        float& smoothedHeight = smoothedHeights[bar];
        if (normalized > smoothedHeight) {
            smoothedHeight = smoothedHeight * 0.5f + normalized * 0.5f; // fast attack
        } else {
            smoothedHeight = smoothedHeight * 0.85f + normalized * 0.15f; // slow decay
        }

        const float x0 = left + static_cast<float>(bar) * barWidth + gap * 0.5f;
        const float x1 = x0 + barWidth - gap;
        const float yTop = baseline + smoothedHeight * (top - baseline);

        const float rBottom = 0.0f, gBottom = 0.25f, bBottom = 0.35f;
        const float rTop = 0.3f + 0.6f * smoothedHeight, gTop = 1.0f, bTop = 1.0f;

        auto pushVertex = [&](float x, float y, float r, float g, float b) {
            fillOut.push_back(x);
            fillOut.push_back(y);
            fillOut.push_back(r);
            fillOut.push_back(g);
            fillOut.push_back(b);
        };

        // Two triangles forming the bar rectangle.
        pushVertex(x0, baseline, rBottom, gBottom, bBottom);
        pushVertex(x1, baseline, rBottom, gBottom, bBottom);
        pushVertex(x1, yTop,     rTop,    gTop,    bTop);

        pushVertex(x0, baseline, rBottom, gBottom, bBottom);
        pushVertex(x1, yTop,     rTop,    gTop,    bTop);
        pushVertex(x0, yTop,     rTop,    gTop,    bTop);

        const float xCenter = (x0 + x1) * 0.5f;
        outlineOut.push_back(xCenter);
        outlineOut.push_back(yTop);
        outlineOut.push_back(rTop);
        outlineOut.push_back(gTop);
        outlineOut.push_back(bTop);
    }
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

    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    glEnable(GL_BLEND);

    AudioCapture audio(RING_BUFFER_SIZE);
    if (!audio.start()) {
        std::cerr << "Failed to start audio capture\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    Shader shader("vertex_shader.glsl", "fragment_shader.glsl");

    // --- VAO/VBO for the line strip (oscilloscope waveform OR spectrum outline) ---
    GLuint lineVAO = 0, lineVBO = 0;
    glGenVertexArrays(1, &lineVAO);
    glGenBuffers(1, &lineVBO);
    glBindVertexArray(lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
    glBufferData(GL_ARRAY_BUFFER,
                  static_cast<GLsizeiptr>(OSCILLOSCOPE_POINTS * 5 * sizeof(float)),
                  nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // --- VAO/VBO for filled spectrum bars (triangles) ---
    GLuint fillVAO = 0, fillVBO = 0;
    glGenVertexArrays(1, &fillVAO);
    glGenBuffers(1, &fillVBO);
    glBindVertexArray(fillVAO);
    glBindBuffer(GL_ARRAY_BUFFER, fillVBO);
    glBufferData(GL_ARRAY_BUFFER,
                  static_cast<GLsizeiptr>(SPECTRUM_BARS * 6 * 5 * sizeof(float)),
                  nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    VisMode mode = VisMode::Oscilloscope;
    bool tabPressedLast = false;

    std::vector<float> sampleBuffer(FFT_SIZE);
    std::vector<float> vertexBuffer;
    std::vector<float> fillBuffer;
    std::vector<float> outlineBuffer;
    std::vector<float> magnitudes;
    std::vector<float> smoothedHeights(SPECTRUM_BARS, 0.0f);

    while (!glfwWindowShouldClose(window)) {
        const bool tabPressed = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
        if (tabPressed && !tabPressedLast) {
            mode = (mode == VisMode::Oscilloscope) ? VisMode::Spectrum : VisMode::Oscilloscope;
        }
        tabPressedLast = tabPressed;

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }

        glClearColor(0.02f, 0.02f, 0.04f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        shader.use();

        if (mode == VisMode::Oscilloscope) {
            sampleBuffer.assign(OSCILLOSCOPE_POINTS, 0.0f);
            audio.readLatest(sampleBuffer.data(), OSCILLOSCOPE_POINTS);

            buildOscilloscopeVertices(sampleBuffer, vertexBuffer);

            glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0,
                            static_cast<GLsizeiptr>(vertexBuffer.size() * sizeof(float)),
                            vertexBuffer.data());

            glBindVertexArray(lineVAO);
            const GLsizei pointCount = static_cast<GLsizei>(vertexBuffer.size() / 5);

            // Glow pass: thick, low-alpha, additive blending.
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glLineWidth(6.0f);
            shader.setFloat("uAlpha", 0.20f);
            glDrawArrays(GL_LINE_STRIP, 0, pointCount);

            // Main pass: thin, full-brightness, normal blending.
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glLineWidth(1.8f);
            shader.setFloat("uAlpha", 1.0f);
            glDrawArrays(GL_LINE_STRIP, 0, pointCount);

        } else { // Spectrum
            sampleBuffer.assign(FFT_SIZE, 0.0f);
            audio.readLatest(sampleBuffer.data(), FFT_SIZE);

            fft::computeMagnitudeSpectrum(sampleBuffer.data(), FFT_SIZE, magnitudes);
            buildSpectrumVertices(magnitudes, smoothedHeights, fillBuffer, outlineBuffer);

            // Filled bars.
            glBindBuffer(GL_ARRAY_BUFFER, fillVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0,
                            static_cast<GLsizeiptr>(fillBuffer.size() * sizeof(float)),
                            fillBuffer.data());

            glBindVertexArray(fillVAO);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            shader.setFloat("uAlpha", 1.0f);
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(fillBuffer.size() / 5));

            // Glow outline along bar tops.
            glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0,
                            static_cast<GLsizeiptr>(outlineBuffer.size() * sizeof(float)),
                            outlineBuffer.data());

            glBindVertexArray(lineVAO);
            const GLsizei outlinePoints = static_cast<GLsizei>(outlineBuffer.size() / 5);

            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glLineWidth(6.0f);
            shader.setFloat("uAlpha", 0.30f);
            glDrawArrays(GL_LINE_STRIP, 0, outlinePoints);

            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glLineWidth(1.8f);
            shader.setFloat("uAlpha", 1.0f);
            glDrawArrays(GL_LINE_STRIP, 0, outlinePoints);
        }

        glBindVertexArray(0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    audio.stop();

    glDeleteBuffers(1, &lineVBO);
    glDeleteVertexArrays(1, &lineVAO);
    glDeleteBuffers(1, &fillVBO);
    glDeleteVertexArrays(1, &fillVAO);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}