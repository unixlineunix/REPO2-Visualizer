#include <Glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "audio.hxx"
#include "fft.hxx"
#include "modes.hxx"
#include "shader.hxx"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

#include <webp/decode.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace {

int g_windowWidth = 1280;
int g_windowHeight = 720;

constexpr size_t RING_BUFFER_SIZE = 16384;
constexpr size_t FFT_SIZE = 2048;

constexpr size_t MAX_LINE_VERTICES = 65536;
constexpr size_t MAX_FILL_VERTICES = 65536;
constexpr size_t VERTEX_FLOATS = 6;

enum class VisMode {
  Oscilloscope = 1,
  SpectrumBars = 2,
  MirroredWaveform = 3,
  CircularOscilloscope = 4,
  CircularSpectrum = 5,
  Lissajous = 6,
  ParticleField = 7,
  LedBars = 8,
  PulseRings = 9
};

void framebufferSizeCallback(GLFWwindow *, int width, int height) {
  g_windowWidth = width;
  g_windowHeight = height;
  glViewport(0, 0, width, height);
}

const char *modeName(VisMode mode) {
  switch (mode) {
  case VisMode::Oscilloscope:
    return "1: Oscilloscope";
  case VisMode::SpectrumBars:
    return "2: Spectrum Bars";
  case VisMode::MirroredWaveform:
    return "3: Mirrored Waveform";
  case VisMode::CircularOscilloscope:
    return "4: Circular Oscilloscope";
  case VisMode::CircularSpectrum:
    return "5: Circular Spectrum";
  case VisMode::Lissajous:
    return "6: Lissajous";
  case VisMode::ParticleField:
    return "7: Particle Field";
  case VisMode::LedBars:
    return "8: LED Bars";
  case VisMode::PulseRings:
    return "9: Pulse Rings";
  }
  return "unknown";
}

GLuint loadTexture(const char *path, int &w, int &h) {
  int n;
  unsigned char *data = stbi_load(path, &w, &h, &n, 4);
  if (data) {
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    stbi_image_free(data);
    std::cout << "Loaded: " << path << " (" << w << "x" << h << ")\n";
    return tex;
  }

  const char *ext = std::strrchr(path, '.');
  if (ext &&
      (std::strcmp(ext, ".webp") == 0 || std::strcmp(ext, ".WEBP") == 0)) {
    FILE *f = fopen(path, "rb");
    if (!f) {
      std::cerr << "Failed to open: " << path << "\n";
      return 0;
    }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> buf(fsize);
    if (fread(buf.data(), 1, fsize, f) != static_cast<size_t>(fsize)) {
      fclose(f);
      std::cerr << "Failed to read: " << path << "\n";
      return 0;
    }
    fclose(f);
    WebPBitstreamFeatures features;
    if (WebPGetFeatures(buf.data(), buf.size(), &features) != VP8_STATUS_OK) {
      std::cerr << "Invalid WebP: " << path << "\n";
      return 0;
    }
    w = features.width;
    h = features.height;
    data = WebPDecodeRGBA(buf.data(), buf.size(), &w, &h);
    if (!data) {
      std::cerr << "Failed to decode WebP: " << path << "\n";
      return 0;
    }
    std::cout << "Loaded WebP: " << path << " (" << w << "x" << h << ")\n";
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    WebPFree(data);
    return tex;
  }

  std::cerr << "Failed to load: " << path << "\n";
  return 0;
}

struct ImageQuad {
  GLuint vao = 0, vbo = 0;
  bool valid = false;
};

ImageQuad makeImageQuad(float radius) {
  ImageQuad q;
  const float verts[] = {-radius, -radius, 0.0f,    0.0f,   radius, -radius,
                         1.0f,    0.0f,    radius,  radius, 1.0f,   1.0f,
                         -radius, -radius, 0.0f,    0.0f,   radius, radius,
                         1.0f,    1.0f,    -radius, radius, 0.0f,   1.0f};
  glGenVertexArrays(1, &q.vao);
  glGenBuffers(1, &q.vbo);
  glBindVertexArray(q.vao);
  glBindBuffer(GL_ARRAY_BUFFER, q.vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                        4 * static_cast<GLsizei>(sizeof(float)), (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
                        4 * static_cast<GLsizei>(sizeof(float)),
                        (void *)(2 * sizeof(float)));
  glEnableVertexAttribArray(1);
  glBindVertexArray(0);
  q.valid = true;
  return q;
}

} // namespace

int main(int argc, char **argv) {
  const char *bgImagePath = nullptr;
  const char *img5Path = nullptr;
  bool experimental4 = false;
  bool bgBackground = false;

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--help") == 0 ||
        std::strcmp(argv[i], "-h") == 0) {
      std::cout
          << "Usage: visualizer [options]\n"
          << "  --img5 <path>        Image overlay for circular modes 4/5\n"
          << "  --bg-image <path>    Background image\n"
          << "  --bg-background      Enable background image\n"
          << "  --experimental 4     Enable experimental mode 4 image\n"
          << "  --help, -h           Show this help\n";
      return 0;
    } else if (std::strcmp(argv[i], "--bg-image") == 0 && i + 1 < argc)
      bgImagePath = argv[++i];
    else if (std::strcmp(argv[i], "--img5") == 0 && i + 1 < argc)
      img5Path = argv[++i];
    else if (std::strcmp(argv[i], "--experimental") == 0 && i + 1 < argc)
      experimental4 = (std::strcmp(argv[++i], "4") == 0);
    else if (std::strcmp(argv[i], "--bg-background") == 0)
      bgBackground = true;
    else {
      std::cerr << "Unknown argument: " << argv[i] << "\n";
      std::cerr
          << "Usage: visualizer [options]\n"
          << "  --img5 <path>        Image overlay for circular modes 4/5\n"
          << "  --bg-image <path>    Background image\n"
          << "  --bg-background      Enable background image\n"
          << "  --experimental 4     Enable experimental mode 4 image\n"
          << "  --help, -h           Show this help\n";
      return 1;
    }
  }

  if (!glfwInit()) {
    std::cerr << "Failed to initialize GLFW\n";
    return -1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_SAMPLES, 16);
  glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);

  GLFWmonitor *primary = glfwGetPrimaryMonitor();
  const GLFWvidmode *vm = glfwGetVideoMode(primary);
  int winW = vm->width * 3 / 4;
  int winH = vm->height * 3 / 4;

  GLFWwindow *window =
      glfwCreateWindow(winW, winH, "Audio Visualizer", nullptr, nullptr);
  if (!window) {
    std::cerr << "Failed to create GLFW window\n";
    glfwTerminate();
    return -1;
  }

  glfwMakeContextCurrent(window);
  glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
  glfwSwapInterval(1);

  int fbW, fbH;
  glfwGetFramebufferSize(window, &fbW, &fbH);
  g_windowWidth = fbW;
  g_windowHeight = fbH;

  if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
    std::cerr << "Failed to initialize GLAD\n";
    glfwDestroyWindow(window);
    glfwTerminate();
    return -1;
  }

  glViewport(0, 0, fbW, fbH);
  glEnable(GL_BLEND);
  glEnable(GL_PROGRAM_POINT_SIZE);

  AudioCapture captureMic(RING_BUFFER_SIZE, false);
  AudioCapture captureSys(RING_BUFFER_SIZE, true);

  bool micRunning = captureMic.start();
  bool sysRunning = false;
  if (!micRunning)
    std::cerr << "Failed to start mic\n";

  Shader shader("vertex_shader.glsl", "fragment_shader.glsl");
  Shader imgShader("image.vert", "image.frag");

  const size_t stride = VERTEX_FLOATS * sizeof(float);

  GLuint lineVAO = 0, lineVBO = 0;
  glGenVertexArrays(1, &lineVAO);
  glGenBuffers(1, &lineVBO);
  glBindVertexArray(lineVAO);
  glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(MAX_LINE_VERTICES * stride), nullptr,
               GL_DYNAMIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(stride),
                        (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(stride),
                        (void *)(2 * sizeof(float)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(stride),
                        (void *)(5 * sizeof(float)));
  glEnableVertexAttribArray(2);

  GLuint fillVAO = 0, fillVBO = 0;
  glGenVertexArrays(1, &fillVAO);
  glGenBuffers(1, &fillVBO);
  glBindVertexArray(fillVAO);
  glBindBuffer(GL_ARRAY_BUFFER, fillVBO);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(MAX_FILL_VERTICES * stride), nullptr,
               GL_DYNAMIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(stride),
                        (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(stride),
                        (void *)(2 * sizeof(float)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(stride),
                        (void *)(5 * sizeof(float)));
  glEnableVertexAttribArray(2);
  glBindVertexArray(0);

  // Image textures and quads
  constexpr float IMG_RADIUS = 0.46f;
  ImageQuad imgQuad = makeImageQuad(IMG_RADIUS);
  ImageQuad bgQuad = makeImageQuad(1.05f);

  GLuint img5Tex = 0;
  int img5W = 0, img5H = 0;
  if (img5Path)
    img5Tex = loadTexture(img5Path, img5W, img5H);

  GLuint bgTex = 0;
  int bgW = 0, bgH = 0;
  if (bgImagePath)
    bgTex = loadTexture(bgImagePath, bgW, bgH);

  bool colorFill = true;
  bool bloomEnabled = true;

  // Audio source state
  enum class AudioSrc { Mic, Sys, Both };
  AudioSrc audioSrc = AudioSrc::Mic;

  VisMode mode = VisMode::Oscilloscope;
  std::cout << "Mode: " << modeName(mode) << "\n";

  float barMult = 1.0f;
  float sensitivity = 1.0f;
  float zoom = 1.0f;
  bool antialiasing = false;
  bool prevLeft = false, prevRight = false, prevUp = false, prevDown = false;
  bool prevNumKeys[9] = {false};
  bool prevA = false, prevS = false, prevZ = false, prevG = false,
       prevR = false;
  bool prevM = false, prevTick = false, prevL = false, prevI = false,
       prevV = false;
  bool prevMinus = false, prevEqual = false, prevComma = false,
       prevPeriod = false;
  bool prevApostrophe = false;
  bool prevSemicolon = false;
  bool prevLeftBracket = false, prevRightBracket = false;
  bool prevSlash = false, prevBackslash = false;

  std::vector<float> sampleBuffer(FFT_SIZE);
  std::vector<float> tempBuffer(FFT_SIZE);
  std::vector<float> magnitudes;

  auto spectrumHeights =
      std::vector<float>(static_cast<size_t>(80 * barMult), 0.0f);
  auto circularSpectrumHeights =
      std::vector<float>(static_cast<size_t>(64 * barMult), 0.0f);
  auto ledHeights = std::vector<float>(static_cast<size_t>(40 * barMult), 0.0f);
  auto particleColumns =
      std::vector<float>(static_cast<size_t>(32 * barMult), 0.0f);
  auto pulseBands = std::vector<float>(
      std::max<size_t>(3, static_cast<size_t>(3 * barMult)), 0.0f);



  auto hasBarBloom = [](VisMode m) {
    return m == VisMode::SpectrumBars || m == VisMode::CircularSpectrum ||
           m == VisMode::LedBars;
  };

  auto resizePreserve = [](std::vector<float>& vec, size_t newSize) {
    if (newSize == vec.size()) return;
    std::vector<float> old = std::move(vec);
    vec.assign(newSize, 0.0f);
    for (size_t i = 0; i < newSize; ++i) {
      size_t src = old.size() > 0
        ? static_cast<size_t>((static_cast<float>(i) / newSize) * old.size())
        : 0;
      if (src < old.size()) vec[i] = old[src];
    }
  };

  auto adjustBarCount = [&](int delta) {
    int actualDelta = delta;
    if (delta == 1 || delta == -1) {
      if (mode == VisMode::SpectrumBars) actualDelta = delta * 2;
      else if (mode == VisMode::CircularSpectrum) actualDelta = delta * 5;
      else if (mode == VisMode::ParticleField) actualDelta = delta * 7;
      else if (mode == VisMode::LedBars) actualDelta = delta * 8;
    }

    switch (mode) {
    case VisMode::SpectrumBars: {
      const size_t n =
          std::clamp(spectrumHeights.size() + actualDelta, size_t{8}, size_t{400});
      resizePreserve(spectrumHeights, n);
      barMult = static_cast<float>(n) / 80.0f;
      std::cout << "Bars: " << n << "\n";
      break;
    }
    case VisMode::CircularSpectrum: {
      const size_t n = std::clamp(circularSpectrumHeights.size() + actualDelta,
                                  size_t{8}, size_t{300});
      resizePreserve(circularSpectrumHeights, n);
      barMult = static_cast<float>(n) / 64.0f;
      std::cout << "Bars: " << n << "\n";
      break;
    }
    case VisMode::ParticleField: {
      const size_t n =
          std::clamp(particleColumns.size() + actualDelta, size_t{4}, size_t{200});
      resizePreserve(particleColumns, n);
      barMult = static_cast<float>(n) / 32.0f;
      std::cout << "Columns: " << n << "\n";
      break;
    }
    case VisMode::LedBars: {
      const size_t n =
          std::clamp(ledHeights.size() + actualDelta, size_t{4}, size_t{200});
      resizePreserve(ledHeights, n);
      barMult = static_cast<float>(n) / 40.0f;
      std::cout << "Bars: " << n << "\n";
      break;
    }
    case VisMode::PulseRings:
      modes::setRingCount(modes::ringCount() + actualDelta);
      std::cout << "Pulse rings: " << modes::ringCount() << "\n";
      break;
    default:
      break;
    }
  };

  while (!glfwWindowShouldClose(window)) {
    for (int k = 0; k < 9; ++k) {
      bool down = glfwGetKey(window, GLFW_KEY_1 + k) == GLFW_PRESS;
      if (down && !prevNumKeys[k]) {
        VisMode req = static_cast<VisMode>(k + 1);
        if (req != mode) {
          mode = req;
          std::cout << "Mode: " << modeName(mode) << "\n";
        } else {
          adjustBarCount(1);
        }
      }
      prevNumKeys[k] = down;
    }

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
      glfwSetWindowShouldClose(window, true);

    const bool aDown = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
    if (aDown && !prevA) {
      antialiasing = !antialiasing;
      if (antialiasing) {
        glEnable(GL_MULTISAMPLE);
        std::cout << "AA on\n";
      } else {
        glDisable(GL_MULTISAMPLE);
        std::cout << "AA off\n";
      }
    }
    prevA = aDown;

    const bool sDown = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
    if (sDown && !prevS) {
      if (audioSrc == AudioSrc::Sys) {
        audioSrc = AudioSrc::Mic;
        sysRunning = false;
        captureSys.stop();
        if (!micRunning)
          micRunning = captureMic.start();
        std::cout << "Mic\n";
      } else {
        audioSrc = AudioSrc::Sys;
        micRunning = false;
        captureMic.stop();
        if (!sysRunning)
          sysRunning = captureSys.start();
        std::cout << "System sound\n";
      }
    }
    prevS = sDown;

    const bool zDown = glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS;
    if (zDown && !prevZ) {
      if (audioSrc == AudioSrc::Both) {
        audioSrc = AudioSrc::Mic;
        sysRunning = false;
        captureSys.stop();
        if (!micRunning)
          micRunning = captureMic.start();
        std::cout << "Mic\n";
      } else {
        audioSrc = AudioSrc::Both;
        if (!micRunning)
          micRunning = captureMic.start();
        if (!sysRunning)
          sysRunning = captureSys.start();
        std::cout << "Mic + System\n";
      }
    }
    prevZ = zDown;

    const bool tickDown =
        glfwGetKey(window, GLFW_KEY_GRAVE_ACCENT) == GLFW_PRESS;
    if (tickDown && !prevTick && hasBarBloom(mode)) {
      modes::setBloomSteps(modes::bloomSteps() - 1);
      std::cout << "Bloom balls: " << modes::bloomSteps() << "\n";
    }
    prevTick = tickDown;

    const bool lDown = glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS;
    if (lDown && !prevL) {
      bloomEnabled = !bloomEnabled;
      std::cout << "Bloom: " << (bloomEnabled ? "on" : "off") << "\n";
    }
    prevL = lDown;

    const bool iDown = glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS;
    if (iDown && !prevI) {
      bloomEnabled = !bloomEnabled;
      std::cout << "Bloom: " << (bloomEnabled ? "on" : "off") << "\n";
    }
    prevI = iDown;

    const bool mDown = glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS;
    if (mDown && !prevM) {
      if (micRunning) {
        micRunning = false;
        captureMic.stop();
        std::cout << "Mic muted\n";
      } else {
        micRunning = captureMic.start();
        std::cout << "Mic unmuted\n";
      }
    }
    prevM = mDown;

    const bool minusDown = glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS;
    const bool equalDown = glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS;
    if (minusDown && !prevMinus) {
      zoom = std::min(10.0f, zoom * 1.15f);
      std::cout << "Zoom in: " << zoom << "\n";
    }
    if (equalDown && !prevEqual) {
      zoom = std::max(0.2f, zoom / 1.15f);
      std::cout << "Zoom out: " << zoom << "\n";
    }
    prevEqual = equalDown;
    prevMinus = minusDown;

    const bool gDown = glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS;
    if (gDown && !prevG) {
      modes::randomizeGradient();
      std::cout << "Gradient randomized\n";
    }
    prevG = gDown;

    const bool rDown = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
    if (rDown && !prevR) {
      modes::randomizeFlat();
      std::cout << "Color randomized\n";
    }
    prevR = rDown;

    const bool vDown = glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS;
    if (vDown && !prevV) {
      colorFill = !colorFill;
      std::cout << "Color fill: " << (colorFill ? "on" : "off") << "\n";
    }
    prevV = vDown;

    const bool commaDown = glfwGetKey(window, GLFW_KEY_COMMA) == GLFW_PRESS;
    const bool periodDown = glfwGetKey(window, GLFW_KEY_PERIOD) == GLFW_PRESS;
    if (commaDown && !prevComma) {
      modes::setPaletteCount(modes::paletteCount() - 1);
      std::cout << "Gradient stops: " << modes::paletteCount() << "\n";
    }
    if (periodDown && !prevPeriod) {
      modes::setPaletteCount(modes::paletteCount() + 1);
      std::cout << "Gradient stops: " << modes::paletteCount() << "\n";
    }
    prevComma = commaDown;
    prevPeriod = periodDown;

    const bool left = glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS;
    const bool right = glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS;
    const bool up = glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS;
    const bool down = glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS;

    if (left && !prevLeft)
      adjustBarCount(-1);
    if (right && !prevRight)
      adjustBarCount(+1);
    if (up && !prevUp) {
      sensitivity = std::min(10000.0f, sensitivity * 1.2f);
      std::cout << "Sensitivity: " << sensitivity << "\n";
    }
    if (down && !prevDown) {
      sensitivity = std::max(0.1f, sensitivity / 1.2f);
      std::cout << "Sensitivity: " << sensitivity << "\n";
    }
    prevLeft = left;
    prevRight = right;
    prevUp = up;
    prevDown = down;

    const bool apostropheDown = glfwGetKey(window, GLFW_KEY_APOSTROPHE) == GLFW_PRESS;
    if (apostropheDown && !prevApostrophe && hasBarBloom(mode)) {
      modes::setBloomSteps(modes::bloomSteps() + 1);
      std::cout << "Bloom balls: " << modes::bloomSteps() << "\n";
    }
    prevApostrophe = apostropheDown;

    const bool semicolonDown = glfwGetKey(window, GLFW_KEY_SEMICOLON) == GLFW_PRESS;
    if (semicolonDown && !prevSemicolon && hasBarBloom(mode)) {
      modes::setBloomSteps(modes::bloomSteps() + 1);
      std::cout << "Bloom balls: " << modes::bloomSteps() << "\n";
    }
    prevSemicolon = semicolonDown;

    const bool leftBracketDown = glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS;
    const bool rightBracketDown = glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS;
    if (leftBracketDown && !prevLeftBracket) {
      modes::setBloomIntensity(modes::bloomIntensity() - 2.0f);
      std::cout << "Bloom intensity: " << modes::bloomIntensity() << "\n";
    }
    if (rightBracketDown && !prevRightBracket) {
      modes::setBloomIntensity(modes::bloomIntensity() + 2.0f);
      std::cout << "Bloom intensity: " << modes::bloomIntensity() << "\n";
    }
    prevLeftBracket = leftBracketDown;
    prevRightBracket = rightBracketDown;

    const bool slashDown = glfwGetKey(window, GLFW_KEY_SLASH) == GLFW_PRESS;
    const bool backslashDown = glfwGetKey(window, GLFW_KEY_BACKSLASH) == GLFW_PRESS;
    if (slashDown && !prevSlash) {
      modes::setBloomRings(modes::bloomRings() - 1);
      std::cout << "Bloom rings: " << modes::bloomRings() << "\n";
    }
    if (backslashDown && !prevBackslash) {
      modes::setBloomRings(modes::bloomRings() + 1);
      std::cout << "Bloom rings: " << modes::bloomRings() << "\n";
    }
    prevSlash = slashDown;
    prevBackslash = backslashDown;

    // Read audio
    if (micRunning && sysRunning) {
      captureMic.readLatest(sampleBuffer.data(), FFT_SIZE);
      captureSys.readLatest(tempBuffer.data(), FFT_SIZE);
      for (size_t i = 0; i < FFT_SIZE; ++i)
        sampleBuffer[i] = (sampleBuffer[i] + tempBuffer[i]) * 0.5f;
    } else if (micRunning) {
      captureMic.readLatest(sampleBuffer.data(), FFT_SIZE);
    } else if (sysRunning) {
      captureSys.readLatest(sampleBuffer.data(), FFT_SIZE);
    } else {
      std::fill(sampleBuffer.begin(), sampleBuffer.end(), 0.0f);
    }

    for (auto &s : sampleBuffer)
      s *= sensitivity;
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
      output =
          modes::buildCircularSpectrum(magnitudes, circularSpectrumHeights);
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
      output = modes::buildPulseRings(magnitudes, pulseBands,
                                      static_cast<float>(glfwGetTime()));
      break;
    }

    glClearColor(0.02f, 0.02f, 0.04f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    const float aspect =
        static_cast<float>(g_windowWidth) / static_cast<float>(g_windowHeight);
    const float z = zoom;
    glm::mat4 proj;
    switch (mode) {
    case VisMode::Oscilloscope:
    case VisMode::SpectrumBars:
    case VisMode::MirroredWaveform:
    case VisMode::ParticleField:
    case VisMode::LedBars:
      proj = glm::ortho(-z, z, -z, z, -1.0f, 1.0f);
      break;
    default:
      proj = glm::ortho(-z, z, -z / aspect, z / aspect, -1.0f, 1.0f);
      break;
    }

    shader.use();
    shader.setMat4("uProj", &proj[0][0]);

    // Background image (static, not affected by zoom/projection)
    if (bgTex && bgBackground && bgQuad.valid) {
      glBindVertexArray(bgQuad.vao);
      imgShader.use();
      glm::mat4 identity(1.0f);
      imgShader.setMat4("uProj", &identity[0][0]);
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, bgTex);
      imgShader.setInt("uTexture", 0);
      imgShader.setFloat("uAlpha", 0.3f);
      imgShader.setInt("uCircular", 0);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glDrawArrays(GL_TRIANGLES, 0, 6);
      glBindVertexArray(0);
      shader.use();
      shader.setMat4("uProj", &proj[0][0]);
    }

    // Radial fill image for modes 4 and 5
    bool isCircular = (mode == VisMode::CircularOscilloscope ||
                       mode == VisMode::CircularSpectrum);
    bool useImg5 = isCircular && img5Tex && imgQuad.valid;
    bool useExp4 = (mode == VisMode::CircularOscilloscope) && experimental4 &&
                   img5Tex && imgQuad.valid;
    if (useImg5 || useExp4) {
      glBindVertexArray(imgQuad.vao);
      imgShader.use();
      imgShader.setMat4("uProj", &proj[0][0]);
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, img5Tex);
      imgShader.setInt("uTexture", 0);
      imgShader.setFloat("uAlpha", 1.0f);
      imgShader.setInt("uCircular", 1);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glDrawArrays(GL_TRIANGLES, 0, 6);
      glBindVertexArray(0);
      shader.use();
      shader.setMat4("uProj", &proj[0][0]);
    }

    if (colorFill && !output.fillVertices.empty()) {
      glBindBuffer(GL_ARRAY_BUFFER, fillVBO);
      glBufferSubData(
          GL_ARRAY_BUFFER, 0,
          static_cast<GLsizeiptr>(output.fillVertices.size() * sizeof(float)),
          output.fillVertices.data());
      glBindVertexArray(fillVAO);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE);
      shader.setInt("uIsPoint", 0);
      shader.setFloat("uAlpha", output.fillAlpha);
      glDrawArrays(
          GL_TRIANGLES, 0,
          static_cast<GLsizei>(output.fillVertices.size() / VERTEX_FLOATS));
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    if (!output.lineVertices.empty()) {
      glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
      glBufferSubData(
          GL_ARRAY_BUFFER, 0,
          static_cast<GLsizeiptr>(output.lineVertices.size() * sizeof(float)),
          output.lineVertices.data());
      glBindVertexArray(lineVAO);
      shader.setInt("uIsPoint", output.linePoints ? 1 : 0);

      if (output.linePoints) {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        shader.setFloat("uAlpha", 1.0f);
        for (const auto &seg : output.lineSegments)
          glDrawArrays(GL_POINTS, seg.first, seg.count);
      } else {
        for (const auto &seg : output.lineSegments) {
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

    // Bloom tips
    if (bloomEnabled && !output.tipVertices.empty()) {
      glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
      glBufferSubData(
          GL_ARRAY_BUFFER, 0,
          static_cast<GLsizeiptr>(output.tipVertices.size() * sizeof(float)),
          output.tipVertices.data());
      glBindVertexArray(lineVAO);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE);
      shader.setInt("uIsPoint", 1);
      shader.setFloat("uAlpha", 1.0f);
      shader.setInt("uBloomRings", modes::bloomRings());
      glDrawArrays(GL_POINTS, 0,
                   static_cast<GLsizei>(output.tipVertices.size() / VERTEX_FLOATS));
      shader.setInt("uBloomRings", 0);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    glBindVertexArray(0);
    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  captureMic.stop();
  captureSys.stop();

  if (img5Tex)
    glDeleteTextures(1, &img5Tex);
  if (bgTex)
    glDeleteTextures(1, &bgTex);
  if (imgQuad.valid) {
    glDeleteBuffers(1, &imgQuad.vbo);
    glDeleteVertexArrays(1, &imgQuad.vao);
  }
  if (bgQuad.valid) {
    glDeleteBuffers(1, &bgQuad.vbo);
    glDeleteVertexArrays(1, &bgQuad.vao);
  }
  glDeleteBuffers(1, &lineVBO);
  glDeleteVertexArrays(1, &lineVAO);
  glDeleteBuffers(1, &fillVBO);
  glDeleteVertexArrays(1, &fillVAO);
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
