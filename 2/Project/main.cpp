#include <Glad/glad.h>
#include <GLFW/glfw3.h>

#include "audio.hxx"
#include "shader.hxx"
#include "fft.hxx"
#include "modes.hxx"

#include <iostream>
#include <vector>

namespace {

constexpr int WINDOW_WIDTH  = 1920;
constexpr int WINDOW_HEIGHT = 1080;

constexpr size_t RING_BUFFER_SIZE = 16384;
constexpr size_t FFT_SIZE         = 2048;

constexpr size_t MAX_LINE_VERTICES = 4096;
constexpr size_t MAX_FILL_VERTICES = 4096;
constexpr size_t VERTEX_FLOATS     = 6; // x, y, r, g, b, pointSize

enum class VisMode {
    Oscilloscope        = 1,
    SpectrumBars        = 2,
    MirroredWaveform    = 3,
    CircularOscilloscope = 4,
    CircularSpectrum    = 5,
    Lissajous           = 6,
    ParticleField       = 7,
    LedBars             = 8,
    PulseRings          = 9
};

void framebufferSizeCallback(GLFWwindow* /*window*/, int width, int height) {
    glViewport(0, 0, width, height);
}

const char* modeName(VisMode mode) {
    switch (mode) {
        case VisMode::Oscilloscope:         return "1: Oscilloscope";
        case VisMode::SpectrumBars:         return "2: Spectrum Bars";
        case VisMode::MirroredWaveform:     return "3: Mirrored Waveform";
        case VisMode::CircularOscilloscope: return "4: Circular Oscilloscope";
        case VisMode::CircularSpectrum:     return "5: Circular Spectrum";
        case VisMode::Lissajous:            return "6: Lissajous";
        case VisMode::ParticleField:        return "7: Particle Field";
        case VisMode::LedBars:              return "8: LED Bars";
        case VisMode::PulseRings:           return "9: Pulse Rings";
    }
    return "unknown";
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
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);

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

    // VAO/VBO for line/point primitives (oscilloscope, loops, particles, outlines).
    GLuint lineVAO = 0, lineVBO = 0;
    glGenVertexArrays(1, &lineVAO);
    glGenBuffers(1, &lineVBO);
    glBindVertexArray(lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
    glBufferData(GL_ARRAY_BUFFER,
                  static_cast<GLsizeiptr>(MAX_LINE_VERTICES * stride),
                  nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(stride), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(stride), reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(stride), reinterpret_cast<void*>(5 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // VAO/VBO for filled triangles (spectrum bars, mirrored waveform, LED bars, etc).
    GLuint fillVAO = 0, fillVBO = 0;
    glGenVertexArrays(1, &fillVAO);
    glGenBuffers(1, &fillVBO);
    glBindVertexArray(fillVAO);
    glBindBuffer(GL_ARRAY_BUFFER, fillVBO);
    glBufferData(GL_ARRAY_BUFFER,
                  static_cast<GLsizeiptr>(MAX_FILL_VERTICES * stride),
                  nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(stride), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(stride), reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(stride), reinterpret_cast<void*>(5 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    VisMode mode = VisMode::Oscilloscope;
    std::cout << "Mode: " << modeName(mode) << "\n";

    std::vector<float> sampleBuffer(FFT_SIZE);
    std::vector<float> magnitudes;

    std::vector<float> spectrumHeights(80, 0.0f);
    std::vector<float> circularSpectrumHeights(64, 0.0f);
    std::vector<float> ledHeights(40, 0.0f);
    std::vector<float> particleColumns(32, 0.0f);
    std::vector<float> pulseBands(3, 0.0f);

    while (!glfwWindowShouldClose(window)) {
        for (int k = 0; k < 9; ++k) {
            if (glfwGetKey(window, GLFW_KEY_1 + k) == GLFW_PRESS) {
                const VisMode requested = static_cast<VisMode>(k + 1);
                if (requested != mode) {
                    mode = requested;
                    std::cout << "Mode: " << modeName(mode) << "\n";
                }
            }
        }

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }

        audio.readLatest(sampleBuffer.data(), FFT_SIZE);
        fft::computeMagnitudeSpectrum(sampleBuffer.data(), FFT_SIZE, magnitudes);

        modes::ModeOutput output;
        switch (mode) {
            case VisMode::Oscilloscope:
                output = modes::buildOscilloscope(sampleBuffer);
                break;
            case VisMode::SpectrumBars:
                output = modes::buildSpectrumBars(magnitudes, spectrumHeights);
                break;
            case VisMode::MirroredWaveform:
                output = modes::buildMirroredWaveform(sampleBuffer);
                break;
            case VisMode::CircularOscilloscope:
                output = modes::buildCircularOscilloscope(sampleBuffer);
                break;
            case VisMode::CircularSpectrum:
                output = modes::buildCircularSpectrum(magnitudes, circularSpectrumHeights);
                break;
            case VisMode::Lissajous:
                output = modes::buildLissajous(sampleBuffer);
                break;
            case VisMode::ParticleField:
                output = modes::buildParticleField(magnitudes, particleColumns);
                break;
            case VisMode::LedBars:
                output = modes::buildLedBars(magnitudes, ledHeights);
                break;
            case VisMode::PulseRings:
                output = modes::buildPulseRings(magnitudes, pulseBands, static_cast<float>(glfwGetTime()));
                break;
        }

        glClearColor(0.02f, 0.02f, 0.04f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        shader.use();

        if (!output.fillVertices.empty()) {
            glBindBuffer(GL_ARRAY_BUFFER, fillVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0,
                            static_cast<GLsizeiptr>(output.fillVertices.size() * sizeof(float)),
                            output.fillVertices.data());

            glBindVertexArray(fillVAO);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            shader.setInt("uIsPoint", 0);
            shader.setFloat("uAlpha", output.fillAlpha);
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(output.fillVertices.size() / VERTEX_FLOATS));
        }

        if (!output.lineVertices.empty()) {
            glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0,
                            static_cast<GLsizeiptr>(output.lineVertices.size() * sizeof(float)),
                            output.lineVertices.data());

            glBindVertexArray(lineVAO);
            shader.setInt("uIsPoint", output.linePoints ? 1 : 0);

            if (output.linePoints) {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                shader.setFloat("uAlpha", 1.0f);
                for (const auto& seg : output.lineSegments) {
                    glDrawArrays(GL_POINTS, seg.first, seg.count);
                }
            } else {
                for (const auto& seg : output.lineSegments) {
                    if (output.lineGlow) {
                        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                        glLineWidth(6.0f);
                        shader.setFloat("uAlpha", 0.25f);
                        glDrawArrays(output.linePrimitive, seg.first, seg.count);
                    }

                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    glLineWidth(1.8f);
                    shader.setFloat("uAlpha", 1.0f);
                    glDrawArrays(output.linePrimitive, seg.first, seg.count);
                }
            }
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