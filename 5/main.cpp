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
#include <sstream>
#include <string>
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

constexpr const char* kDetailText = R"DOC(
================================================================================
  AUDIO VISUALIZER — COMPLETE TECHNICAL REFERENCE
================================================================================

  Author:  uki
  Lang:    C++20
  Deps:    GLFW 3, OpenGL 4.6 Core, miniaudio (ALSA), Glad
  Binary:  visualizer

================================================================================
  1.  WHAT THIS IS
================================================================================

  A real-time, GPU-accelerated audio visualization application. It captures
  live stereo audio from either your microphone or system output (PipeWire/
  ALSA via miniaudio), processes it with a custom real FFT implementation,
  and renders ten visualization modes plus an independent analog oscilloscope
  emulator. Every mode uses OpenGL 4.6 Core with VAOs, VBOs, and a single
  GLSL shader that supports both point and line rendering with per-vertex
  coloring and additive glow passes.

================================================================================
  2.  BUILDING & RUNNING
================================================================================

  Prerequisites:  g++ with C++20 support, GLFW, OpenGL dev headers,
                  ALSA dev libraries (libasound2-dev), libgl1-mesa-dev
                  or equivalent.

  Build:          make clean && make

  Flags:          visualizer [--help] [--summary] [--detail] [--detail-summary]
                  --help           usage and keybind list
                  --summary        mode list and keybinds, condensed
                  --detail         full line-by-line code walkthrough
                  --detail-summary this document

  Run:            ./visualizer
                  Press 0-9 to pick a visualization mode (0 is TrueXY).
                  Press H to enter the analog scope (one-way; 0-9 exits).
                  Press ESC to exit.

  Audio:          Defaults to microphone input. Press M to toggle between
                  microphone and system audio capture at any time.

================================================================================
  3.  FILE-BY-FILE BREAKDOWN
================================================================================

  3.1  main.cpp
  ~~~~~~~~~~~~~~
  Entry point and render loop: GLFW window creation, OpenGL 4.6 Core context
  setup, VBO/VAO initialisation, audio capture scheduling, keyboard input
  handling, buffer uploads, and draw dispatch.

  keyEdge()
    Press-transition detector. Stores each frame's key states in a
    std::array<bool, GLFW_KEY_LAST + 1> and returns true only when a key
    transitions from GLFW_RELEASE to GLFW_PRESS. Used for all toggle-style
    actions so holding a key doesn't repeat-fire it.

  Render loop (per frame, driven by vsync via glfwSwapBuffers)
    1. glfwPollEvents()
    2. Apply keyboard state changes to VisState
    3. audio.readLatestStereo() -> stereo float buffers (FFT_SIZE = 4096)
    4. Mix mono -> FFT -> magnitude spectrum (2048 bins)
    5. Dispatch to the active mode's builder (modes::build*)
    6. For each non-empty vertex/fill/glow buffer:
         a) glBufferData(GL_ARRAY_BUFFER, ...) to upload (orphan + alloc,
            not sub-data, to avoid implicit GPU sync stalls)
         b) set shader uniforms
         c) glDrawArrays() with the appropriate primitive
    7. glfwSwapBuffers()

  Line rendering (per segment in output.lineSegments)
    1. If lineGlow is set: additive blend (GL_SRC_ALPHA, GL_ONE), a wide
       line (6px + bloom scaling), low alpha (~0.25) -> soft glow halo.
    2. Normal blend (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA), thin line
       (output.lineWidth), full alpha -> crisp trace drawn on top.
    This two-pass approach fakes CRT phosphor glow without post-processing.

  3.2  modes.cpp / modes.hxx
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~
  All visualization mode builders. Each is a pure function: takes audio data
  and VisState, returns a ModeOutput. No GPU state is touched here.

  ModeOutput fields:
    lineVertices   x,y,r,g,b,point_size per vertex (std::vector<float>)
    fillVertices   triangles for filled areas
    glowVertices   additive glow quads (optional)
    lineSegments   {GLsizei first, GLsizei count} per drawable segment
    linePrimitive  GL_LINE_STRIP or GL_POINTS
    linePoints     true if lineVertices should be drawn as GL_POINTS
    lineGlow       enable the additive glow pass before the main line
    lineWidth      thickness of the crisp line
    fillAlpha      alpha for fill triangles

  Builders, in mode-key order:

    0  buildTrueXY()
       Stereo L/R Lissajous XY display. Down-samples 4096 -> 256 points.
       Sub-modes (E/I/O/J):
         Scatter       GL_POINTS at each sample (default)
         LineStrip     GL_LINE_STRIP connecting samples
         Both          line strip + scatter points
         FilledTrail   thick filled ribbon (computed perpendicular normals)
         GlowScatter   points + additive glow quads at each point
         PhosphorTrail rotating 64-point window with brightness fade
       Color resolved per vertex via resolveColor(state, baseColor, t).

    1  buildOscilloscope()
       Down-samples to 512 points, 2-tap moving-average smoothing. Scrolling
       line strip; horizontal scroll offset driven by waveSpeed. Color
       resolved per vertex.

    2  buildSpectrumBars()
       FFT magnitudes -> vertical bars. Bar heights smoothed with a two-pole
       IIR filter (state preserved across frames in spectrumHeights). Each
       bar is two triangles forming a rectangle, with optional thin "peak
       line" per bar and glow quads. Color resolved per bar.

    3  buildMirroredWaveform()
       Same source data as the oscilloscope but mirrored symmetrically
       around y=0; top and bottom halves drawn in one pass via alternating
       vertices.

    4  buildCircularSpectrum()
       Radial bars around the center. Each bar is a trapezoid (inner arc to
       outer arc) made of two triangles. Smoothing as in buildSpectrumBars.

    5  buildCircularFilledSpectrum()
       Like buildCircularSpectrum, but adjacent bars are connected into a
       filled radial band using triangle strips.

    6  buildCircularWave()
       Waveform wrapped around a circle: sample amplitude displaces radius,
       then polar -> cartesian per vertex. Glow quads along the outer edge.
       Color resolved per vertex with phase-based variation.

    7  buildFilledWave()
       Waveform with a filled area beneath it: extrudes from the waveform
       down to y=-1, two vertices per sample (top + bottom) forming a
       triangle strip, with a top-to-bottom color gradient.

    8  buildBarsFilled()
       Bar spectrum with semi-transparent fill (alpha ~0.55-0.65) plus a
       filled triangle "cap" and glow quads at the top of each bar. Color
       resolved per bar.

    9  buildBarsCircular()
       Circular bar spectrum with glow quads at each bar's outer tip. Color
       resolved per band.

    H  buildAnalogScope()
       Leader LBO-51MA style XY oscilloscope emulator. See section 4.

  3.3  vis_state.hxx / vis_state.cpp
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  VisState holds all mutable visualization parameters. One instance lives in
  main.cpp and is passed by const reference to every builder function.

  Key fields:
    numBars, maxBarsLimit       bar count for spectrum modes
    sensitivity, zoom           global gain and zoom
    aspect                      screen aspect ratio for correct XY scaling
    waveZoom, waveSpeed         waveform horizontal zoom and scroll speed
    colorMode                   Normal | RandomSolid | RandomGradient
    gradientColors              user-defined gradient palette
    bloom, bloomIntensity, bloomSize   bloom toggle and parameters
    antiAliasing                MSAA toggle
    inputMode                   Microphone | SystemAudio
    trueXYMode, trueXYLines     TrueXY sub-mode and line-overlay toggle
    analogScope*                analog scope state (see section 4)

  resolveColor(state, base, t)
    Returns a Color3 depending on colorMode:
      Normal          base, unchanged
      RandomSolid     the stored randomSolid color (set by the R key)
      RandomGradient  the gradient palette sampled at position t in [0,1]

  3.4  audio.hxx / audio.cpp
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Wraps miniaudio's ALSA backend. AudioCapture opens a capture device at
  44.1 kHz stereo float; a callback fills an internal 16384-sample ring
  buffer. readLatestStereo(left, right, n) thread-safely copies the most
  recent n samples per channel. switchSource(CaptureSource) toggles between
  Microphone and SystemAudio by restarting the capture device.

  3.5  fft.hxx / fft.cpp
  ~~~~~~~~~~~~~~~~~~~~~~~
  Custom real FFT (a Hermitian-symmetric conjugate-pair variant of RFFT).
  computeMagnitudeSpectrum() takes a real buffer of size N and produces N/2
  magnitude bins using precomputed twiddle factors for O(N log N)
  performance. No windowing function is currently applied (see 5.3).

  3.6  shader.hxx / shader.cpp
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Compiles and links the vertex + fragment GLSL shaders from embedded source
  strings and exposes named-uniform setters. One shader handles every mode:
    uModelViewProjection   identity — all coordinates are NDC [-1, 1]
    uIsPoint               1 for GL_POINTS, 0 for lines
    uAlpha                 uniform alpha multiplier
  Per-vertex attributes are x, y, r, g, b, pointSize. The fragment shader
  outputs the interpolated color multiplied by uAlpha.

================================================================================
  4.  ANALOG SCOPE (H MODE) — COMPLETE REFERENCE
================================================================================

  4.1  Goal
  ~~~~~~~~~
  Emulate a Leader LBO-51MA analog XY oscilloscope with P31 green phosphor.
  The real LBO-51MA is a 3 MHz X/Y-bandwidth CRT display used in medical and
  scientific equipment. Relevant properties being emulated:
    - Pure XY mode: no timebase sweep, both axes driven by external signals
    - P31 phosphor (approx. chromaticity 0.08, 0.50, 0.06), medium
      persistence — decays to ~1% in ~0.4 ms but with a visible afterglow
    - Z-axis blanking cuts off the beam below a threshold input level

  4.2  How it works
  ~~~~~~~~~~~~~~~~~
  Intentionally decoupled from the TrueXY (mode 0) pipeline. TrueXY uses
  down-sampled 256 points and a fixed base color; the analog scope uses all
  4096 raw samples with 4x linear interpolation, P31 green via
  resolveColor(), an exponential phosphor-decay gradient, and Z-axis
  blanking.

  Entry/exit:
    H enters (one-way, also resets to Trace sub-mode); 0-9 exits to the
    corresponding standard mode.

  Sub-modes (Q / W cycle, H resets to Trace):
    Trace    continuous XY line strip showing all samples (default)
    Scatter  4096 individual particle quads (size ~0.0025)
    Both     trace + particles overlaid

  Per-frame steps:
    1. Read the live stereo buffer (4096 samples/channel).
    2. Build a 4x-interpolated point array: for each consecutive sample
       pair (L[i],R[i]) -> (L[i+1],R[i+1]), generate raw_i plus 25/50/75%
       linear interpolants in audio space, then map to screen coordinates.
       Total points: 4096 * 4 - 3 = 16381.
    3. Z-axis blanking: at each raw-sample boundary, compute
       |L[i]-L[i-1]| + |R[i]-R[i-1]|. If >= 0.35, end the current segment
       and start a new one at this vertex (prevents "spiderweb" lines
       between distant XY positions on transients).
    4. Apply an exponential phosphor-decay gradient per vertex:
         age = vertex_index / total_vertices   (0 = newest, 1 = oldest)
         brightness = exp(-age * decayRate) * 0.95 + 0.05
         final_color = resolveColor(state, P31_GREEN, age) * brightness
    5. Emit line segments at the blanking boundaries as GL_LINE_STRIP.
    6. In particle sub-modes, also emit a triangle quad per sample.

  4.3  Phosphor decay
  ~~~~~~~~~~~~~~~~~~~
  The decay is a spatial gradient along the trace, recomputed from scratch
  each frame based on a vertex's position in the buffer — not a temporal
  accumulation. This is simpler than render-to-texture phosphor persistence,
  handles changing audio cleanly (stale data disappears instantly), and
  looks convincing at high decay rates. At low decay rates the whole trace
  is nearly uniformly bright, which is less authentic than true persistence
  (see 5.1).

  Decay rate reference values:
    rate=18   tail brightness at 25% ~ exp(-4.5)  ~ 0.011  (nearly invisible)
    rate=4    tail brightness at 25% ~ exp(-1.0)  ~ 0.37   (decent fade)
    rate=1    tail brightness at 25% ~ exp(-0.25) ~ 0.78   (still visible)
    rate=0.5  tail brightness at 25% ~ exp(-0.125)~ 0.88   (almost uniform)

  Controls:
    P  cycles presets 18 -> 1 -> 2 -> 3 -> 18
    C  multiplies rate by 0.75 (floor 0.5)
    V  multiplies rate by 1.5  (ceiling 40.0)

  4.4  Z-axis blanking (teleport suppression)
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Consecutive audio samples can land far apart on screen (e.g. a drum
  transient); a straight line between them would draw a spurious diagonal
  that doesn't correspond to the actual signal path. The blanking check is
  done in audio-sample space, at raw-sample boundaries, against a threshold
  of 0.35 (normalised units, typical signal range ~0 to ~2). This value was
  chosen empirically: low enough to catch most visible cross-lines, high
  enough not to flicker on continuous program material.

  4.5  Why there's no sweep animation
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  An earlier left-to-right sweep (beam scanning through the buffer) caused
  visible discontinuities between sweeps, a "teleporting" trace when the
  buffer was frozen, and half-drawn traces at low sweep speeds. The current
  "show everything at once" approach with a live, ~80%-overlapping buffer
  evolves smoothly frame to frame without these artifacts.

  4.6  Glow pass
  ~~~~~~~~~~~~~~
  lineGlow was briefly disabled to fix bright-spot artifacts from the wide
  additive pass on a 16000-point line strip on some GPUs, then re-enabled
  because the trace looked too clinical without it. It currently stays on;
  the soft P31 halo softens the raw line strip's precision.

  4.7  Resolution vs. line count
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Z/X — analogResolution (64-4096): number of audio samples used for BOTH
  the trace and the particles. Lower = chunkier, more retro.
  ;/Shift+' — analogLineCount (64-4096): samples used for the TRACE only,
  independent of analogResolution, for tuning trace density without
  affecting particle count.

  4.8  Color (R / G keys)
  ~~~~~~~~~~~~~~~~~~~~~~~
  The P31 green base color is passed through resolveColor(), so:
    Normal          P31 green (0.08, 0.50, 0.06)
    RandomSolid     entire trace uses the R-selected random color
    RandomGradient  trace graduates through the G-selected gradient palette
  Phosphor decay brightness is applied on top of the resolved color, so the
  head-to-tail gradient is preserved under any color mode.

================================================================================
  5.  KNOWN ISSUES & LIMITATIONS
================================================================================

  5.1  Analog scope
  ~~~~~~~~~~~~~~~~~
  - No true temporal phosphor persistence (spatial gradient only); low
    decay rates look flat rather than accumulating. A render-to-texture
    pass with per-frame exponential decay would fix this but needs an FBO
    and a screen-space composite pass.
  - The 0.35 Z-blanking threshold is a single hard-coded value; it may
    over-blank quiet audio or under-blank loud audio. Could be made
    user-adjustable or signal-adaptive.
  - 4x interpolation is linear in audio space only; a cubic spline would be
    smoother but costs more per frame.
  - The live buffer's ~80% frame overlap still produces visible shape
    changes on transients. A freeze + crossfade between buffers would
    soften this at the cost of added latency.
  - Scatter-mode particles are 4096 small quads rather than GL_POINTS;
    GL_POINTS would be faster but offer less control over size/shape.
  - The glow pass over a 16000-point strip is normally one extra draw call,
    but heavy Z-blanking can split the trace into many segments, each
    needing its own glow + main draw call.

  5.2  TrueXY
  ~~~~~~~~~~~
  - Always down-samples to 256 points regardless of audio resolution; more
    points would add detail at a performance cost.
  - No Z-blanking, so spiderweb lines are visible on transients (the same
    0.35 audio-threshold approach from the analog scope could be ported).
  - PhosphorTrail's 64-point trail length is hard-coded; could be made
    user-adjustable.

  5.3  General
  ~~~~~~~~~~~~~
  - A single GLSL shader handles every mode via branching on uIsPoint;
    separate shaders per primitive type would be cleaner and slightly
    cheaper.
  - All particles are rebuilt CPU-side every frame; a compute-shader
    particle system would scale to 100k+ points.
  - Bloom is an additive-glow approximation, not real HDR bloom — cheaper
    but less physically accurate.
  - VBOs are re-uploaded every frame via the orphan + glBufferData pattern;
    persistent-mapped buffers could reduce driver overhead.
  - The FFT uses a rectangular window, causing spectral leakage; a Hann or
    Blackman window would sharpen bar-mode accuracy.
  - FFT_SIZE is fixed at 4096; making it configurable would allow trading
    latency for frequency resolution.
  - The anti-aliasing toggle doesn't fully handle context/FBO recreation on
    window resize.
  - All defaults are hard-coded in vis_state.hxx; there's no config file or
    CLI for overriding them.
  - The keyEdge state array is sized GLFW_KEY_LAST + 1 (~512 bools) —
    harmless on desktop but wasteful.
  - No MIDI/OSC input for mapping parameters to external controllers.

================================================================================
  6.  HOW TO EXTEND
================================================================================

  Adding a new mode:
    1. Add a build function in modes.cpp following the existing pattern
       (buildOscilloscope is the simplest template).
    2. Declare it in modes.hxx.
    3. Add a VisMode enum entry (vis_state.hxx) and a modeName() case
       (main.cpp).
    4. Bind a key for it in main.cpp.
    5. Add a dispatch branch in the render loop's mode switch (main.cpp).

  Modifying the analog scope:
    - Phosphor decay: edit the exp(-age * rate) formula in buildAnalogScope.
    - Interpolation: adjust the k/4.0f sub-sample loop for more/fewer steps.
    - Z-blanking: change the 0.35 threshold, or switch to a screen-space
      distance check.
    - Particles: change pointSize, or switch from quads to GL_POINTS.

  Adding a new analog-scope sub-mode:
    1. Add an entry to AnalogScopeMode (vis_state.hxx).
    2. Bind a key for it in main.cpp.
    3. Add a render branch in buildAnalogScope() (modes.cpp).

================================================================================
  7.  DEPENDENCIES & VERSIONS
================================================================================

  Build system:  GNU Make
  Compiler:      g++ (GCC), -std=c++20
  Windowing:     GLFW 3.x       (https://github.com/glfw/glfw)
  OpenGL:        4.6 Core Profile
  GL loader:     Glad           (https://glad.dav1d.de/)
  Audio:         miniaudio      (https://miniaudio.io/) — single-file
                 header, ALSA backend on Linux
  Libraries:     libglfw3-dev, libgl1-mesa-dev, libasound2-dev

  Tested on:     Arch Linux, Hyprland (Wayland), Mesa/Radeon
================================================================================
)DOC";

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

void printAnsiDoc(const char* text) {
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.size() >= 3 && line[0] == '=' && line[2] == '=')
            std::cout << "\033[1;36m" << line << "\033[0m\n";
        else if (line.find("FILE ") == 0 || line.find("  FILE ") != std::string::npos
                 || line.find("SECTION:") != std::string::npos)
            std::cout << "\033[1;33m" << line << "\033[0m\n";
        else
            std::cout << line << "\n";
    }
}

} // namespace

int main(int argc, char* argv[]) {
    // --- CLI flags ---
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "\033[1mAudio Visualizer\033[0m \342\200\224 real-time audio-reactive visualizations\n"
"\n"
"Usage: visualizer [--help] [--summary] [--detail] [--detail-summary]\n"
"\n"
"  \033[1m--help\033[0m             This message\n"
"  \033[1m--summary\033[0m          Mode list and condensed keybinds\n"
"  \033[1m--detail\033[0m           Full line-by-line code walkthrough\n"
"  \033[1m--detail-summary\033[0m   Condensed technical reference\n";
            return 0;
        }
        if (arg == "--summary" || arg == "-s") {
            std::cout << "\033[1mAudio Visualizer\033[0m \342\200\224 real-time audio-reactive visualizations\n"
"\n"
"Usage: visualizer [--help] [--summary] [--detail] [--detail-summary]\n"
"\n"
"\033[1mModes (number keys 0-9):\033[0m\n"
"  0  TrueXY          XY Lissajous display of stereo channels\n"
"  1  Oscilloscope    scrolling waveform\n"
"  2  Spectrum Bars   frequency spectrum bar graph\n"
"  3  Mirrored        mirrored waveform (top/bottom)\n"
"  4  Circular        radial spectrum\n"
"  5  Circular Filled radial spectrum with filled bands\n"
"  6  Circular Wave   circular waveform\n"
"  7  Filled Wave     filled waveform\n"
"  8  Bars Filled     filled bar spectrum\n"
"  9  Bars Circular   circular bar spectrum\n"
"\n"
"\033[1mGlobal keys:\033[0m\n"
"  \033[1mH\033[0m          Analog scope mode (P31 phosphor emulation)\n"
"  \033[1mQ/W\033[0m        Sub-mode cycle (scope: Scatter/Both | XY: modes)\n"
"  \033[1mE/I/O/J\033[0m    TrueXY sub-modes (Both/Filled/Glow/Phosphor)\n"
"  \033[1mR\033[0m          Random solid color\n"
"  \033[1mG\033[0m          Random gradient color\n"
"  \033[1mP\033[0m          Cycle decay (scope) / toggle lines (XY)\n"
"  \033[1mS\033[0m          Toggle anti-aliasing\n"
"  \033[1mA/B/L\033[0m      Toggle bloom\n"
"  \033[1mM\033[0m          Toggle mic/system input\n"
"  \033[1m,/. \033[0m        Gradient color count\n"
"  \033[1mShift+,/.\033[0m  Wave scroll speed\n"
"  \033[1mZ/X\033[0m        Analog scope resolution\n"
"  \033[1mC/V\033[0m        Analog scope decay rate\n"
"  \033[1m;/Shift+'\033[0m  Analog scope line count\n"
"  \033[1m\342\206\220/\342\206\222\033[0m        Bars count / wave zoom\n"
"  \033[1mShift+\342\206\220/\342\206\222\033[0m  Wave scroll speed\n"
"  \033[1mESC\033[0m        Quit\n"
"\n";
            return 0;
        }
        if (arg == "--detail" || arg == "-d") {
            printAnsiDoc(R"(
===============================================================================
  AUDIO VISUALIZER - COMPLETE LINE-BY-LINE CODE WALKTHROUGH
================================================================================

  This document walks through every significant line in every source file,
  explaining not just WHAT the code does, but WHY it exists, HOW it works
  mathematically, and what EDGE CASES or TRADEOFFS were considered.

  Files covered:
    1.  main.cpp          - Entry point, render loop, input, OpenGL setup
    2.  modes.cpp         - All 10+ visualization builders
    3.  modes.hxx         - ModeOutput struct and Segment type
    4.  vis_state.hxx     - Central state struct (VisState, enums, Color3)
    5.  vis_state.cpp     - resolveColor() implementation
    6.  audio.hxx/.cpp    - miniaudio capture wrappers
    7.  fft.hxx/.cpp      - Custom radix-2 Cooley-Tukey FFT implementation
    8.  shader.hxx/.cpp   - GLSL compilation/linking helper
    9.  vertex_shader.glsl - Vertex shader
    10. fragment_shader.glsl - Fragment shader
    11. Makefile          - Build recipe

================================================================================
   FILE 1: main.cpp  (2053 lines total)
================================================================================

This is the largest file. It contains: constants, helper functions, CLI parsing,
GLFW/OpenGL initialization, the main render loop (input, audio capture, FFT,
mode dispatch, buffer uploads, draw calls), and cleanup.

We will walk through every section sequentially.

--------------------------------------------------------------------------------
  SECTION: Includes and namespace (lines 1-16)
--------------------------------------------------------------------------------

  Line 1:  #include <Glad/glad.h>
           OpenGL function pointer loader. At runtime, gladLoadGLLoader()
           resolves every GL function (glGenBuffers, glDrawArrays, etc.).
           WHY: Without glad, every GL call would be a null-pointer crash
           because OpenGL is a specification, not a library - the actual
           function pointers live in the GPU driver and must be resolved
           at runtime.

  Line 2:  #include <GLFW/glfw3.h>
           Windowing toolkit. Creates the window, handles input, manages
           the OpenGL context, and provides vsync via glfwSwapInterval().
           WHY: GLFW is chosen over raw X11/Wayland because it abstracts
           cross-platform window creation and input in ~100 lines of code.
           SDL was considered but is heavier (audio, threading, renderer).

  Lines 4-8: #include "audio.hxx", "shader.hxx", "fft.hxx", "modes.hxx",
              "vis_state.hxx"
           Project headers. Note the .hxx extension (convention indicating
           C++ headers with implementations in separate .cpp files).

  Lines 10-15: Standard library includes:
           <algorithm>   - std::clamp, std::min, std::max
           <array>       - key state tracking array
           <iostream>    - std::cout, std::cerr (user feedback)
           <random>      - std::mt19937 for random colors
           <string>      - std::string for CLI arg comparison
           <vector>      - std::vector for all dynamic vertex buffers

--------------------------------------------------------------------------------
  SECTION: Anonymous namespace (lines 17-141)
  ── Globals, enums, and internal helper functions ──
--------------------------------------------------------------------------------

  Lines 19-20: constexpr int WINDOW_WIDTH = 1280, WINDOW_HEIGHT = 720
    WHY 1280x720? This is 720p (16:9). It's a good default that works on
    most monitors and provides enough resolution for the visualizations
    without being too demanding on GPU fill rate. The window is resizable
    (GLFW_RESIZABLE = GLFW_TRUE at line 1544), so this is just an initial size.

  Line 22: constexpr size_t RING_BUFFER_SIZE = 16384
    This is the depth of the audio ring buffer (in samples). At 48 kHz
    sample rate, 16384 samples = 341 ms of stereo audio.
    WHY 16384? Must be larger than FFT_SIZE (4096) to allow the "latest N
    samples" read to not wrap around to stale data. 16384 = 4 * 4096 gives
    4 full FFT windows of buffer, enough for the ~80% overlap strategy.
    WHY power of 2? Makes modulo indexing efficient (bitmask instead of
    division), though the compiler optimizes % on powers of 2 anyway.

  Line 23: constexpr size_t FFT_SIZE = 4096
    Number of audio samples fed into the FFT each frame.
    WHY 4096? Power of 2 (required by radix-2 FFT). At 48 kHz, 4096 samples
    = 85 ms of audio. This is a tradeoff between:
    - Frequency resolution: 4096 bins → 2048 output bins → 11.7 Hz/bin
    - Temporal responsiveness: 85 ms window means transients appear
      ~85 ms after they occur (latency)
    - Performance: 4096-point FFT takes ~0.1 ms on modern CPUs
    2048 would be faster but gives half the frequency resolution.
    8192 would give better resolution but 170 ms latency (too sluggish).

  Lines 25-27: Vertex capacity constants
    constexpr size_t MAX_LINE_VERTICES = 32768   ≈ 32K points
    constexpr size_t MAX_FILL_VERTICES = 131072   ≈ 131K triangles
    constexpr size_t MAX_GLOW_VERTICES = 262144   ≈ 262K glow quads
    These are used in glBufferData at line 1590 to allocate VBO memory.
    WHY these numbers? The analog scope at full resolution produces 16381
    interpolated points (lineVertices). 32768 gives headroom. Fill quads
    at 4096 particles × 6 verts = 24576, so 131072 is generous. Glow
    quads can be larger (bloom expands geometry), hence 262144.
    If any mode exceeds these, glBufferData will automatically reallocate
    (GL_DYNAMIC_DRAW hints that, but GL actually ignores the hint).

  Line 28: constexpr size_t VERTEX_FLOATS = 6
    Each vertex is 6 floats = 24 bytes. Layout:
      float 0: x (NDC coordinate, -1 to 1)
      float 1: y (NDC coordinate, -1 to 1)
      float 2: r (red, 0 to 1)
      float 3: g (green, 0 to 1)
      float 4: b (blue, 0 to 1)
      float 5: pointSize (for GL_POINTS) OR unused (for lines/triangles)
    WHY interleaved? All attributes in one buffer means one glBufferData
    call, one VAO setup, lower CPU overhead. The alternative (separate
    buffers per attribute) has better cache behavior for vertex reuse
    but we stream new data every frame so there's no reuse anyway.

  Lines 30-32: Bar count limits
    MIN_BARS = 2 (at least 2 bars, otherwise useless)
    MAX_BARS = 4096 (matches FFT bins. More than 4096 would subdivide
      each FFT bin, which is meaningless because there's no information
      between bins without interpolation)
    DENSE_BAR_CAP = 8192 (for DenseSpectrum mode, which uses 2x bar
      density. WHY 2x? Dense bars with gapFraction=0.0f pack bars
      edge-to-edge, making thin bars look thicker. 2x gives smoother
      visual without aliasing.)

  Line 34: constexpr int MSAA_SAMPLES = 4
    Multisample anti-aliasing sample count. 4x MSAA is the sweet spot
    between quality and performance. 8x would be smoother but doubles
    fill rate cost. 2x is barely noticeable.

  Lines 36-47: enum class VisMode
    Maps to number keys 0-9. Each mode is a different visualization.
    The order matters because modeName() switch and the render loop
    dispatch both rely on this enum.
    WHY enum class instead of plain enum? Strong typing prevents
    accidental conversion to int (e.g., comparing with wrong value).

  Lines 49-63: const char* modeName(VisMode mode)
    Returns a human-readable string for each mode. Used to print the
    current mode name to stdout on mode switch (line 1668).
    WHY not std::string? Returns a string literal (static storage),
    no allocation. This is a minor optimization.

  Lines 65-70: bool keyEdge(GLFWwindow* window, int key,
                            std::array<bool, GLFW_KEY_LAST + 1>& prevKeys)
    Implements rising-edge detection: returns true only on the frame
    the key transitions from NOT_PRESSED to PRESSED.
    
    HOW IT WORKS:
    1. Read current state: glfwGetKey returns GLFW_PRESS or GLFW_RELEASE
    2. Look up previous frame's state in prevKeys[key]
    3. edge = current == PRESSED && previous == RELEASED
    4. Store current state for next frame
    5. Return edge

    WHY NOT use glfwSetKeyCallback? The callback approach is event-driven:
    the OS sends a key-press event, the callback fires. But the callback
    runs in GLFW's internal thread context, and the app processes it
    asynchronously. For a single-threaded render loop, polling is simpler
    and avoids race conditions with the event queue.
    
    WHY the array is sized GLFW_KEY_LAST + 1 (≈348 entries)?
    This wastes ~1.4 KB (348 bools) but eliminates bounds checking.
    Using a std::bitset or unordered_map would be more memory-efficient
    but slower to access.
    
    EDGE CASE: If the window loses focus while a key is held, prevKeys
    stays true. When focus returns, the key is still held, so prevKeys
    is true and glfwGetKey returns PRESSED => no edge fires. The user
    must release and re-press the key for edge detection to work again.
    This is acceptable because key bindings are not critical-path.

  Lines 72-89: void hsvToRgb(float h, float s, float v,
                              float& r, float& g, float& b)
    Standard HSV-to-RGB conversion. h in [0,1] (wraps hue circle once),
    s in [0,1], v in [0,1].
    
    MATH:
    c = v * s           (chroma)
    hh = h * 6.0        (hue sector: 0-6)
    x = c * (1 - |hh mod 2 - 1|)  (intermediate value)
    m = v - c           (match value)
    
    Then select RGB based on which sector hh falls in:
    0-1: (c, x, 0)   Red-Yellow
    1-2: (x, c, 0)   Yellow-Green
    2-3: (0, c, x)   Green-Cyan
    3-4: (0, x, c)   Cyan-Blue
    4-5: (x, 0, c)   Blue-Magenta
    5-6: (c, 0, x)   Magenta-Red
    
    Add m to each component to get final RGB.
    
    WHY this implementation? It's the standard algorithm from the
    computer graphics literature. No third-party dependency needed.

  Lines 91-99: Color3 randomColor(std::mt19937& rng)
    Generates a random saturated color using HSV:
    - h: uniform in [0, 1] (full hue range)
    - s: uniform in [0.5, 1.0] (saturated, not pastel)
    - v: uniform in [0.6, 1.0] (bright, not dark)
    WHY these ranges? Pastel/dark colors look washed out on a black
    background. Saturated bright colors pop against the dark visualizer.

  Lines 101-106: void randomizeGradient(VisState& state, std::mt19937& rng)
    Replaces the gradient palette with 'gradientColorCount' random colors.
    Each color is generated via randomColor(). This is called when
    the user presses G (set RandomGradient mode) and when they change
    the gradient color count with ,/. keys.

  Lines 108-119: void resizeBarVectors(...)
    Resizes all smoothed-height vectors to match state.numBars.
    These vectors persist across frames (stored in main()) to provide
    frame-to-frame smoothing (IIR filter in updateSmoothed).
    WHY separate from main? Keeps the render loop cleaner.

  Lines 121-135: bool createMsaaFramebuffer(...)
    Creates an off-screen multisample framebuffer:
    1. Generate FBO and color renderbuffer
    2. Bind renderbuffer with glRenderbufferStorageMultisample
       (MSAA_SAMPLES = 4 samples, GL_RGBA8 format)
    3. Attach to FBO's color attachment 0
    4. Check completeness with glCheckFramebufferStatus
    
    WHY not use GLFW's built-in MSAA? GLFW supports multisampling via
    glfwWindowHint(GLFW_SAMPLES, 4), but toggling MSAA at runtime
    (S key) would require destroying and recreating the window. The
    FBO approach lets us toggle MSAA instantly.

  Lines 137-139: void framebufferSizeCallback(GLFWwindow*, int width, int height)
    GLFW callback for window resize. Simply calls glViewport to update
    the OpenGL viewport dimensions. The aspect ratio update happens in
    the render loop (line 1648) rather than here, because state.aspect
    belongs to VisState, not to a stateless callback.

--------------------------------------------------------------------------------
  SECTION: main() function (lines 143-1552)
  ── Entry point, CLI parsing, initialization ──
--------------------------------------------------------------------------------

  Lines 143-1534: int main(int argc, char* argv[])
    The entire program lives here. The function is ~1910 lines long.

    Lines 144-1534: CLI argument parsing
      Iterates argv[1..argc-1] and checks for --help, --summary, --detail.
      Each prints text to stdout and returns 0 (exits immediately).

      Line 147: --help or -h
        Prints basic usage and exits.
        WHY this order? So the user can run ./visualizer --help without
        needing the GPU or audio hardware to be functional. No GLFW or
        OpenGL init happens before this check.

      Line 160: --summary or -s
        Prints mode list and key bindings. Also exits before hardware init.

      Lines 200-1533: --detail or -d
        Prints this document. The glGetString(GL_VERSION) at line 201 is
        technically useless here (GL isn't initialized yet, so v will
        be null), but it doesn't crash because nothing reads v.
        This is a bug: the GL_VERSION query should be removed. On most
        systems glGetString returns null without a context, so v = null
        but it's never dereferenced.

  Lines 1536-1567: GLFW and GLAD initialization
    1536: glfwInit() - initialize GLFW library. Returns false on failure.
    1541-1543: Context version hints:
      GLFW_CONTEXT_VERSION_MAJOR = 4, MINOR = 6
      GLFW_OPENGL_PROFILE = GLFW_OPENGL_CORE_PROFILE
      WHY 4.6 Core? The shaders use #version 460 core (line 1 of both
      .glsl files). OpenGL 4.6 is the latest version that exposes this
      shading language version. Core profile excludes deprecated
      fixed-function pipeline (glBegin, glMatrix, etc.).
    1544: GLFW_RESIZABLE = GLFW_TRUE - window can be resized.
    1546: glfwCreateWindow - returns null on failure.
    1554: glfwMakeContextCurrent - must be called before any GL functions.
    1555: glfwSetFramebufferSizeCallback - registers resize callback.
    1556: glfwSwapInterval(1) - enable vsync (1 = every 1 vblank, 60 fps).
    1558: gladLoadGLLoader - resolves all GL function pointers.

  Lines 1569-1571: OpenGL initial state
    1569: glViewport(0, 0, fbWidth, fbHeight) - set viewport to window size.
    1570: glEnable(GL_BLEND) - enable blending globally.
    1571: glEnable(GL_PROGRAM_POINT_SIZE) - enable shader-controlled
          gl_PointSize. Without this, point sizes are fixed at 1 pixel.

  Lines 1573-1605: Audio and shader initialization
    1573: AudioCapture audio(RING_BUFFER_SIZE) - create audio capture
          object with 16384-sample ring buffer.
    1574-1579: audio.start() - starts miniaudio capture device. If this
          fails (no microphone, ALSA misconfigured), we print error and exit.
          This is a hard failure because all modes require audio input.
    1581: Shader shader("vertex_shader.glsl", "fragment_shader.glsl")
          - Compiles and links the GLSL shader program. This opens both
          files, reads source, compiles vertex and fragment shaders
          separately, links them, and checks for errors. If either file
          is missing or has syntax errors, the Shader constructor throws
          std::runtime_error (which propagates up and terminates).

    1583-1597: setupVAO lambda - creates VAO and VBO for a vertex buffer.
      This is called 3 times (lines 1603-1605) for line, fill, and glow.

      VAO (Vertex Array Object): stores all vertex attribute configurations.
        A single VAO can be bound before glDrawArrays to restore all
        attribute pointers with one call. Without VAOs, you'd need to
        re-specify all vertexAttribPointer calls before each draw.

      VBO (Vertex Buffer Object): stores vertex data on the GPU.
        glBufferData allocates GPU memory and uploads initial data
        (nullptr = no initial data, just allocate).

      The attribute layout (matching the shader):
        Location 0: vec2 aPos - 2 floats, stride = 24 bytes, offset = 0
        Location 1: vec3 aColor - 3 floats, stride = 24 bytes, offset = 8
        Location 2: float aSize - 1 float, stride = 24 bytes, offset = 20

      WHY interleave? One buffer, three attributes, contiguous in memory.
      The alternative (separate buffers) has:
        + Better spatial locality when updating only positions
        - More buffer binds and VAO setup for attribute splitting
      Since we upload the entire buffer every frame, interleaving wins.

      WHY glVertexAttribPointer with reinterpret_cast<void*>(offset)?
      The offset parameter is the byte offset from the start of the buffer
      to the first occurrence of this attribute. It's passed as a void*
      due to OpenGL's legacy C API, not because it's a pointer.

  Lines 1607-1614: Window settings and MSAA
    1608: glfwSwapInterval(0) - DISABLE vsync. WHY? The previous
          glfwSwapInterval(1) at line 1556 is overridden here. This is
          intentional: uncapped framerate for maximum responsiveness.
          The window may tear, but audio visualization benefits from
          the lowest possible latency.
    1610-1614: Create MSAA framebuffer for anti-aliasing.

  Lines 1616-1635: VisState and per-frame buffers
    1616: VisState state - initialize with defaults (defined in vis_state.hxx).
    1617: state.aspect = fbHeight / fbWidth - screen aspect ratio.
          Used by circular and XY modes to correct for non-square pixels:
          a circle on a 16:9 screen rendered in NDC [-1,1] would be an
          ellipse without aspect correction. state.aspect = height/width
          is typically 720/1280 = 0.5625. Circular modes multiply x
          coordinates by aspect to maintain visual circles.
    1619: std::mt19937 rng(std::random_device{}()) - Mersenne Twister PRNG
          seeded with random device. Used for random color generation.
          WHY mt19937? It's the only high-quality PRNG in the C++ standard
          library. std::rand() has poor randomness and small period.
    1621: prevKeys array - all initialized to false (value-initialization).
    1623: VisMode mode = VisMode::Oscilloscope - start in mode 1
          (scrolling waveform). WHY this default? It's the most
          recognizable and approachable visualization for first-time users.
    1626-1634: Per-frame buffers (allocated once, reused each frame)
      sampleBuffer[FFT_SIZE] - mono mix for FFT
      leftBuffer[FFT_SIZE], rightBuffer[FFT_SIZE] - stereo channels
      magnitudes - FFT output (size changes based on bin count)
      spectrumHeights/circularSpectrumHeights/denseHeights
        /circularFilledHeights - smoothed bar heights (persistent across
        frames for IIR filtering)
      pulseBands[3] - three smoothed energy bands for PulseRings
    1635: resizeBarVectors - initialize smoothed height vectors to
          state.numBars (default 64).

  Lines 1637-1639: Bloom intensity defaults
    kBloomIntensityLevels = {0.5, 1.0, 1.5, 2.0, 2.5}
    bloomIntensityIndex = 1 (start at 1.0 = normal bloom)
    WHY 5 levels? Enough granularity for tweaking without overwhelming
    the user with tiny increments. Each press of ' cycles to the next.

--------------------------------------------------------------------------------
  SECTION: Main render loop (lines 1641-2034)
  ── Every frame: input → audio → FFT → build → upload → draw ──
--------------------------------------------------------------------------------

  The render loop runs at uncapped framerate (60-200+ fps depending on
  mode complexity and GPU).

  Lines 1641-1655: Resize handling
    Each frame, check if the framebuffer size changed. If so:
    1. Update fbWidth, fbHeight
    2. Recompute aspect ratio
    3. If MSAA is active, destroy and recreate the MSAA FBO
    WHY check every frame instead of relying on the callback?
    The callback fires reliably, but on some window managers (Wayland),
    the callback might fire before the window is fully resized, giving
    stale dimensions. Double-checking each frame is belt-and-suspenders.

  Lines 1657-1671: Mode switching (number keys 0-9)
    for (int k = 0; k <= 9; ++k):
      int key = (k == 0) ? GLFW_KEY_0 : (GLFW_KEY_1 + k - 1);
      This maps: k=0 → GLFW_KEY_0, k=1 → GLFW_KEY_1, ..., k=9 → GLFW_KEY_9
      
      WHY raw glfwGetKey instead of keyEdge? If you hold '0' down, it
      continuously resets the mode to TrueXY every frame. This is
      intentional: pressing a number key while already in that mode
      re-dispatches the mode (useful for re-randomizing gradient in
      some modes). The cost is negligible (just a mode switch that
      produces the same output).

    Lines 1664-1667: If entering a numbered mode while analogScope is
      active, turn analogScope off. This is the primary way to exit
      analog scope mode (the H key enters/cycles within analog scope,
      but number keys always exit).

  Lines 1673-1675: ESC - quit
    glfwSetWindowShouldClose(window, true) causes the render loop to
    exit at the top of the next iteration.

  Lines 1677-1711: Arrow keys - bars/wave zoom
    LEFT/RIGHT with shift: change waveSpeed (±0.1 per press).
    LEFT/RIGHT without shift:
      If wave/XY mode: change waveZoom (±0.1, clamped [0.1, 10.0]).
      Otherwise: change numBars (±4, clamped [2, maxBarsLimit]).
    
    WHY ±4 bars instead of ±1? You'd press a lot to go from 2 to 4096.
    ±4 is fast enough for large ranges while fine enough for small ones
    (you can still reach any count by pressing enough times). The MIN_BARS
    guard prevents going below 2 (which would be useless).

  Lines 1713-1720: B key - break limits
    Doubles maxBarsLimit and maxSensitivityLimit. This is a
    safety-explict feature: by default, bar count maxes at 200 and
    sensitivity at 5.0 to prevent overwhelming the GPU or producing
    garbage output. Pressing B removes these limits up to MAX_BARS=4096
    and sensitivity 10.0+.
    WHY a special key instead of just allowing any value? To protect
    users who accidentally hold an arrow key and suddenly have 4096 bars
    at 60 fps (drawing 4096 rectangles = 24576 triangles per frame).
    The "break limits" key gives explicit consent.

  Lines 1722-1728: UP/DOWN arrows - sensitivity
    Continuous while held (not keyEdge). Changes by ±0.02 per frame.
    At 60 fps, it takes ~50 frames (0.8 sec) to go from 1.0 to 2.0.
    WHY continuous? Sensitivity needs fine adjustment to match audio
    levels. Key edges would require 50 key presses for a full range.

  Lines 1730-1736: MINUS/EQUAL - zoom
    Also continuous. MINUS zooms out (smaller), EQUAL zooms in (larger).
    Range: [0.3, 3.0]. Applied as uZoom uniform in vertex shader:
      gl_Position = vec4(aPos * uZoom, 0.0, 1.0)
    This scales all vertex positions from [-1,1] to [-zoom, zoom],
    effectively zooming in (zoom > 1 clips edges) or out (zoom < 1
    shows more). Not all modes look good zoomed.

  Lines 1738-1775: Color modes (R, G, period, comma)
    Line 1739-1742: R key - RandomSolid mode
      state.colorMode = ColorMode::RandomSolid
      state.randomSolid = randomColor(rng) - generate new random color
      All visualizations will now use this single color (multiplied
      by per-vertex brightness where applicable).
    
    Line 1743-1747: G key - RandomGradient mode
      state.colorMode = ColorMode::RandomGradient
      randomizeGradient(state, rng) - generate a palette of random colors
      Each vertex's color is interpolated from this palette based on
      its position t ∈ [0,1] along the visualization.
    
    Lines 1748-1775: Period (.) and comma (,) keys
      Without shift: change gradientColorCount (±1, range [1, 64]).
        If currently in RandomGradient mode, add/remove a color.
      With shift: change waveSpeed (±0.5, range [0, 20]).
      
      WHY shift semantics? The keys are already used for non-shifted
      functions. Shift+period = > (greater-than), which intuitively
      means "faster/more". Shift+comma = < (less-than), meaning
      "slower/less".

  Lines 1777-1793: Audio source switching (M and S)
    Lines 1786-1793: M key - switch to Microphone input
    Lines 1778-1785: S key - switch to System Audio input
      Both call audio.switchSource() which closes the current device
      and opens a new one. On failure (e.g., no system audio monitor
      available), prints error and keeps current source.
      
      NOTE: The key binding comment on line 1127 says "S (bits — line 346)
      Actually R" which is confusion in the old detail output. The actual
      bindings are: R = RandomSolid, G = Gradient, M = Microphone,
      S = SystemAudio (from the audio switch at line 1786).

  Lines 1795-1823: Anti-aliasing and bloom controls
    1796-1799: A key toggles anti-aliasing (MSAA).
    1802-1805: L key toggles bloom.
    1806-1817: Apostrophe (') key:
      With shift + analogScope active: double analogLineCount (to 4096).
      Without shift: cycle bloom intensity index through 5 levels.
    1818-1823: Left/Right bracket: continuous bloom size adjustment
      [ = decrease (min 0.05), ] = increase (max 1.0).
      WHY continuous? Bloom size is a subtle parameter that needs
      fine tuning. Key edge would make it tedious.

  Lines 1825-1900: Analog scope and TrueXY controls
    These are the most complex input bindings.

    Q key (line 1826): If analogScope → AnalogScopeMode::Scatter
                        Else → TrueXYMode::Scatter
    W key (line 1835): If analogScope → AnalogScopeMode::Both
                        Else → TrueXYMode::LineStrip
    E key (line 1844): TrueXYMode::Both (blocked in analog scope)
    I key (line 1848): TrueXYMode::FilledTrail (blocked in analog)
    O key (line 1852): TrueXYMode::GlowScatter (blocked in analog)
    J key (line 1856): TrueXYMode::PhosphorTrail (blocked in analog)

    H key (line 1860): The analog scope toggle:
      If already in analog scope → reset to Trace sub-mode.
      If not in analog scope → enter analog scope (set state.analogScope=true).
      WHY reset to Trace instead of toggle off? Because number keys
      already handle exiting. The H key is designed as "cycle within
      scope or enter scope" for quick access.

    P key (line 1869): 
      If analogScope → cycle decay rate presets (18 → 1 → 2 → 3 → 18).
      Else → toggle TrueXY line overlay.
      WHY the static pIdx index? It's declared static inside the
      if-block, so it persists across calls. The initial value is 0
      (first call uses rates[0] = 18.0). The cycle repeats every 4
      presses. NOTE: The default decay rate (4.0) is not in this cycle.
      If the user sets decay via C/V to a non-preset value and presses
      P, the preset overwrites it.

    Z key (line 1881): Halve analogResolution (min 64).
    X key (line 1885): Double analogResolution (max 4096).
    C key (line 1889): Multiply analogScopeDecay by 0.75 (min 0.5).
    V key (line 1893): Multiply analogScopeDecay by 1.5 (max 40.0).
    ; key (line 1897): Halve analogLineCount (min 64).
      Shift+' (line 1809): Double analogLineCount (max 4096).
      
      WHY Z/X for resolution and C/V for decay? They're adjacent keys
      that form natural pairs: Z is left of X (decrease/increase),
      C is left of V (decrease/increase). This is the same pattern
      used by many editors (vim-style).

  Lines 1902-1907: Audio capture and FFT
    1903: audio.readLatestStereo(leftBuffer.data(), rightBuffer.data(),
                                 FFT_SIZE)
      Copies the most recent 4096 stereo samples from the ring buffer
      into leftBuffer and rightBuffer. This is a blocking read: it
      acquires the mutex, computes the start position, and copies
      sample-by-sample. At 48 kHz, 4096 samples = 85 ms of audio.
      
      The ring buffer is written by the audio callback thread (miniaudio's
      internal thread) and read by the main thread. The mutex ensures
      that the read and write never collide. If the mutex is contended,
      the main thread stalls briefly (typically < 0.1 ms).
      
      WHAT IF fewer than 4096 samples are available? This happens on
      the first frame (buffer not yet full) or if audio is interrupted.
      readLatestStereo trims count to ringFrames if needed (line 157
      of audio.cpp). For the first 85 ms after startup, the output
      will be partially stale zeros, which is fine.

    1905: Mix to mono for FFT:
      sampleBuffer[i] = (leftBuffer[i] + rightBuffer[i]) * 0.5f
      WHY average? The FFT operates on a single channel. Averaging
      L+R preserves all frequencies present in either channel but
      loses phase information (a phase-inverted signal could cancel
      out). For visualization, this is acceptable because human
      perception of frequency content doesn't depend on phase.

    1907: fft::computeMagnitudeSpectrum(sampleBuffer.data(), FFT_SIZE,
                                         magnitudes)
      Performs 4096-point real FFT with Hann window, outputs 2048
      magnitude bins. The magnitudes represent the energy at each
      frequency from DC (0 Hz) to Nyquist (24 kHz at 48 kHz sample rate).
      
      WHY compute every frame? The FFT is fast (4096-point radix-2
      takes ~0.1 ms on modern CPUs even with unoptimized C++). Running
      it every frame gives real-time responsiveness. The alternative
      (compute every N frames) would save CPU but introduce visible
      stutter in bar animations.

  Lines 1909-1944: Mode dispatch
    Creates empty ModeOutput. If state.analogScope is true, bypasses
    the normal mode switch and calls buildAnalogScope(). Otherwise
    switches on mode enum to call the appropriate builder.

    Each builder takes:
    - Audio data (samples, magnitudes, left/right channels)
    - VisState (all parameters)
    - Persistent smoothed heights vectors (for bar modes)
    - Time (for animated modes like PulseRings)

    The builder returns a ModeOutput with lineVertices, fillVertices,
    glowVertices, and segment metadata.

  Lines 1947-2030: Rendering pipeline
    This is where CPU vertex data becomes GPU pixels.

    1948-1953: Framebuffer setup
      Choose MSAA FBO or default framebuffer based on antiAliasing state.
      Clear to dark background: (0.02, 0.02, 0.04) = very dark blue-gray.
      WHY not pure black (0,0,0)? A slightly colored background makes
      the visualization feel less clinical. The blue-gray tint mimics
      the faint glow of a CRT screen that's "on" but showing dark content.

    1955-1956: Shader uniforms
      shader.use() - bind the GLSL program.
      shader.setFloat("uZoom", state.zoom) - set zoom uniform.
      WHY set uZoom per frame even if zoom never changes? It's cheap
      (one glUniform1f call) and guarantees correctness.

    1958-1968: Fill pass (GL_TRIANGLES)
      Draws filled geometry (bars, mirrored waveform, particles):
      1. Upload fillVBO via glBufferData (orphan + reallocate pattern)
      2. Bind fillVAO
      3. Set blend mode: GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA (normal alpha)
      4. Set uIsPoint = 0, uAlpha = output.fillAlpha
      5. glDrawArrays(GL_TRIANGLES, 0, vertexCount)
      
      WHY orphan + reallocate?
      glBufferData with non-null data tells the driver to allocate a
      new buffer, copy data, and release the old one when the GPU is
      done with it. This avoids implicit synchronization where
      glBufferSubData would block until the previous draw completes.
      The cost is a memory allocation per frame per buffer (3 allocations
      at ~0.1 MB each). Not significant on modern systems.

    1971-1981: Glow pass (GL_TRIANGLES)
      Draws additive bloom geometry:
      1. Upload glowVBO
      2. Bind glowVAO
      3. Set blend mode: GL_SRC_ALPHA, GL_ONE (ADDITIVE)
      4. uAlpha = bloomIntensity (clamped [0, 3])
      5. glDrawArrays(GL_TRIANGLES)
      
      WHY additive blending?
      Normal alpha blending: out = src * src_alpha + dst * (1 - src_alpha)
        This darkens the destination where src_alpha < 1, creating
        transparency. But glow should ADD light, not block it.
      Additive blending: out = src * src_alpha + dst * 1
        The destination is never darkened. Bright areas accumulate
        (multiple glow quads overlapping create brighter spots).
        This mimics real optical bloom: a bright light spreads light
        into neighboring pixels.
      Gl_FUNC: out = src * GL_SRC_ALPHA + dst * GL_ONE
        - src: incoming fragment color * its alpha
        - dst: existing framebuffer color * 1 (unmodified)
        - Result: glow adds to existing pixels, never subtracts.

    1984-2020: Line/point pass
      The most complex section. Three sub-paths:

      Line 1991: shader.setInt("uIsPoint", output.linePoints ? 1 : 0)
        Controls the fragment shader's circular-point mask.

      Path A (lines 1993-1998): GL_POINTS rendering
        Normal alpha blend, uAlpha = 1.0, draw each segment as points.
        Used by TrueXY Scatter and PhosphorTrail modes.

      Path B (lines 1999-2019): GL_LINE_STRIP rendering
        For each segment in output.lineSegments:
        1. (Optional) Glow pass: additive blend, thick line (6px+),
           low alpha (0.25), draw the line strip.
        2. Crisp pass: normal blend, thin line (output.lineWidth,
           typically 1.8px), alpha = 1.0, draw the same line strip.
        
        WHY two passes (glow then crisp)?
        The additive pass creates a soft halo around the line. Since
        additive blending accumulates, overlapping line segments near
        the beam head appear brighter (like a real CRT spot). The crisp
        pass on top keeps the line well-defined at its center.
        
        Without the glow pass, the line at 1.8px looks "too sharp"
        (user feedback). The glow softens it to mimic the P31
        phosphor's slight light spread.
        
        WHY NOT just use glLineWidth(8.0) with normal blend?
        A thick line with normal blend has uniform brightness across
        its width. Real CRT phosphor has a bright center and dim edges.
        The two-pass approach approximates this: the additive glow is
        wider and dimmer (fading at edges), while the crisp center
        is bright and sharp.

    2023: glBindVertexArray(0) - unbind VAO

    2025-2030: MSAA resolve
      If MSAA is enabled: blit from multisample FBO to default
      framebuffer using glBlitFramebuffer with GL_NEAREST filter.
      GL_NEAREST is correct for resolve (no interpolation between
      resolved samples). GL_LINEAR would blur the resolved image.
      
      The blit operation resolves the multisample buffer: each pixel's
      4 samples are combined into a single color. This is the standard
      MSAA resolve step.

    2032-2033: Swap buffers and poll events
      glfwSwapBuffers - presents the completed frame to the screen.
      glfwPollEvents - processes OS events (keyboard, window resize).
      WHY pollEvents instead of waitEvents? waitEvents blocks until
      an event occurs, which would prevent the audio visualization
      from updating at a steady rate.

  Lines 2035-2052: Cleanup
    audio.stop() - stop audio capture.
    Delete all VBOs and VAOs.
    If MSAA FBO exists, delete its renderbuffer and FBO.
    glfwDestroyWindow, glfwTerminate - clean up GLFW resources.
    return 0 - success.

================================================================================
   FILE 2: modes.cpp  (772 lines)
================================================================================

This file implements every visualization builder. Each builder takes audio
data and state, returns a ModeOutput with vertex data ready for OpenGL.

--------------------------------------------------------------------------------
  SECTION: Anonymous namespace helpers (lines 8-100)
  ── Utility functions used by multiple builders ──
--------------------------------------------------------------------------------

  Lines 12-28: std::vector<float> smoothSignal(const std::vector<float>& samples,
                                                 int radius)
    Box-blur (moving average) over a window of 2*radius+1 samples.
    
    HOW IT WORKS:
    For each sample i:
      sum = samples[i-radius] + samples[i-radius+1] + ... + samples[i+radius]
      out[i] = sum / count
    Boundary handling: Clamp index to [0, n-1], so edge samples have fewer
    terms (the average is computed over fewer points). This causes a
    slight amplitude drop at the edges, but for visualization it's
    acceptable.
    
    WHY radius=2 in oscilloscope modes? A radius of 2 means 5-tap
    averaging. This smooths out the most egregious sample-to-sample
    jitter without blurring the waveform too much. Higher radii create
    visible lag (the average lags behind the signal).
    
    PERFORMANCE: O(n * radius). For n=4096, radius=2, this is ~20K
    operations. Negligible.

  Lines 30-39: std::vector<float> downsample(const std::vector<float>& in,
                                                size_t targetCount)
    Nearest-neighbor decimation. Selects every stride-th sample.
    
    HOW IT WORKS:
    ratio = in.size() / targetCount
    for i in 0..targetCount-1:
      srcIdx = i * ratio  (rounded down)
      out[i] = in[srcIdx]
    
    This is NOT anti-aliased. Downsampling without a low-pass pre-filter
    causes aliasing: frequencies above the new Nyquist fold back into
    the output as spurious low frequencies. For visualization, this
    manifests as jagged/jumpy edges in the waveform.
    
    WHY not use proper anti-aliased downsampling? It would require
    a low-pass FIR filter (convolution, O(n * filter_length)), which
    is 100x slower. For a visualization that updates 60+ times per
    second, nearest-neighbor is fast enough, and the aliasing adds
    a subtle "fizz" that many users find aesthetically pleasing.
    
    EDGE CASE: If in.size() < targetCount, ratio < 1, so some source
    samples are skipped. Actually, with integer index truncation,
    multiple output samples may map to the same input sample, creating
    a "staircase" effect. The std::min guard prevents out-of-bounds.

  Lines 41-58: void hsvToRgb(...)
    Duplicate of the function in main.cpp. This is an intentional
    copy (anonymous namespace, so no linker conflict).
    WHY duplicate? The helper is used only inside modes.cpp. Including
    it locally avoids a cross-file dependency. Yes, it's code
    duplication, but it's 17 lines.

  Lines 60-64: float magnitudeToNormalized(float mag, float sensitivity)
    Converts FFT magnitude to a perceptual [0,1] value.
    
    MATH:
    dB = 20 * log10(mag * sensitivity + 1e-6)
    normalized = clamp((dB + 60) / 60, 0, 1)
    
    WHY +60 dB / 60 dB?
    -60 dBFS is roughly the noise floor of a typical audio signal
    (a quiet room). 0 dBFS is the maximum before clipping.
    The formula maps -60 dBFS → 0 (invisible) and 0 dBFS → 1 (full bar).
    This is called "logarithmic compression" and mirrors human hearing's
    logarithmic response to loudness.
    
    WHY + 1e-6? log10(0) = -infinity. The epsilon ensures that
    completely silent audio (mag = 0) produces a very negative dB value
    (-120 dB) rather than crashing the program.
    
    WHAT IS MAG? The FFT magnitude spectrum output: each bin is
    sqrt(re^2 + im^2) / n. For a full-scale sine wave at the bin's
    center frequency, mag ≈ 0.5. For silence, mag ≈ 0. The sensitivity
    parameter scales the input: higher sensitivity means quieter sounds
    push the bars higher.

  Lines 67-74: float sampleInterpolated(const std::vector<float>& magnitudes,
                                          float binIndex)
    Linear interpolation between adjacent FFT bins for sub-bin resolution.
    
    WHY? When the bar count (e.g., 64) is much smaller than the bin count
    (2048), each bar spans ~32 bins. If we just pick one bin per bar,
    the bar height would jump erratically as the energy shifts between
    bins. Interpolation smoothes this.
    
    HOW:
    idx = clamp(binIndex, 0, maxIndex)
    frac = idx - floor(idx)
    result = mag[floor(idx)] * (1-frac) + mag[ceil(idx)] * frac
    This is standard linear interpolation (lerp).

  Lines 76-83: std::pair<size_t, size_t> linearBinRange(size_t numBins,
                                                          size_t numGroups,
                                                          size_t groupIndex)
    Maps a bar index to its FFT bin range [start, end).
    
    HOW:
    binStart = groupIndex * numBins / numGroups
    binEnd = (groupIndex + 1) * numBins / numGroups
    
    This divides numBins bins into numGroups equal-ish groups.
    Example: 2048 bins, 64 bars → each bar gets 32 bins.
    WHY integer division? It's fast and deterministic. The last group
    may get slightly more bins (due to rounding), but the difference
    is at most 1 bin, which is negligible.
    
    Edge case guards at lines 79-82 prevent overflow:
    - binStart must not exceed numBins - 1
    - binEnd must not exceed numBins
    - binEnd must be > binStart (at least 1 bin per group)

  Lines 85-93: float groupMagnitude(const std::vector<float>& magnitudes,
                                      size_t binStart, size_t binEnd)
    Averages magnitudes over a bin range [binStart, binEnd).
    
    WHY average instead of max? Max would make bars respond to the
    loudest frequency in the range, which jumps erratically. Average
    gives a smooth representation of the energy in that frequency band,
    which is visually stable.

  Lines 95-101: void updateSmoothed(float& smoothed, float target,
                                      float attack, float decay)
    One-pole IIR filter (exponential moving average).
    
    FORMULA:
    if target > smoothed:
      smoothed = smoothed * (1 - attack) + target * attack
    else:
      smoothed = smoothed * (1 - decay) + target * decay
    
    This is an asymmetrical smoother: attack and decay are separate
    constants. attack = 0.5 means the bar rises quickly toward a loud
    sound. decay = 0.15 means it falls slowly when the sound stops.
    This creates the classic "waterfall" effect: bars jump up fast
    and fall down slowly.
    
    WHY separate attack/decay? Real spectrum analyzers (hardware)
    use asymmetric time constants for visual appeal. Fast attack
    catches transients. Slow decay shows the recent peak history.
    
    Why the specific values 0.5 and 0.15? Empirically determined:
    0.5 attack rises to 90% of target in 3 frames (~50 ms at 60 fps).
    0.15 decay falls to 10% of peak in 15 frames (~250 ms).

  Lines 103-110: void pushVertex(std::vector<float>& buf, float x, float y,
                                   float r, float g, float b, float size)
    Appends 6 floats to a vertex buffer.
    
    WHY a helper function instead of inline push_back calls?
    The 6-line push_back sequence would be repeated hundreds of times
    across all builders. The function saves lines and reduces the
    chance of ordering mistakes (x,y,r,g,b,size is easy to get wrong).

--------------------------------------------------------------------------------
  SECTION: Glow helpers (lines 114-159)
  ── Additive glow quad generation for bar and radial modes ──
--------------------------------------------------------------------------------

  Lines 114-139: void appendBarGlow(...)
    Generates 2 quads (12 vertices) for a bar's additive glow:
    
    Quad 1 (bar body glow):
      (gx0, baseline) → (gx1, baseline) → (gx1, yTop) → (gx0, yTop)
      Colors: baseline = black (0,0,0), yTop = topColor
      This creates a gradient from black at the bar's base to the
      bar's color at the top. WHY black at base? The glow should
      fade out at the bar's bottom (baseline) to avoid a harsh cutoff.
    
    Quad 2 (halo above bar):
      (gx0, yTop) → (gx1, yTop) → (gx1, gyTop2) → (gx0, gyTop2)
      Colors: yTop = topColor, gyTop2 = black
      This extends the glow above the bar top, fading to black.
      The halo mimics light spilling past the bar's physical end.
    
    The horizontal extension (haloX = barWidth * bloomSize * 0.5)
    makes the glow wider than the bar for a soft bloom effect.
    bloomSize is user-controlled (default 0.3).

  Lines 142-159: void appendRadialGlow(...)
    Same concept as appendBarGlow but for radial bars.
    Instead of cartesian coordinates, it computes positions on a
    circle: outerRadius and outerHaloRadius at angles a0/a1.
    Aspect correction applied to x coordinates (cos term).

  Lines 163-216: modes::ModeOutput buildBarSpectrum(...)
    The shared implementation for bar-like spectra. Called by
    buildSpectrumBars() and buildDenseSpectrum() with different
    gapFraction values.
    
    gapFraction = 0.15 → bars with visible gaps (SpectrumBars)
    gapFraction = 0.0  → contiguous bars (DenseSpectrum)
    
    For each bar:
    1. Map bar index to FFT bin range via linearBinRange
    2. Average bin magnitudes via groupMagnitude
    3. Apply dB conversion and normalization via magnitudeToNormalized
    4. Update smoother via updateSmoothed
    5. Compute screen positions:
      x0 = left + bar * barWidth + gap/2
      x1 = x0 + barWidth - gap
      yTop = baseline + normalized_height * (top - baseline)
    6. Generate fill triangles (2 triangles = 6 vertices per bar)
    7. Generate line point (peak dot)
    8. If bloom enabled, generate glow quads
    
    Colors: bottomColor (darker) at baseline, topColor (brighter) at top.
    This gradient gives bars a 3D appearance.

  Lines 220-278: buildCircularBars(...)
    Radial variant of buildBarSpectrum. Called by buildCircularSpectrum()
    and buildCircularSpectrumFilled().
    
    KEY MATH (the angles the user asked about):
    For bar at index i:
      t = i / numBars                                    (0 to 1)
      angle = t * 2 * pi                                 (0 to 2pi)
      a0 = angle - halfAngle, a1 = angle + halfAngle     (bar arc)
      
    WHY these angles?
    
    The full circle is 2pi radians. Dividing by numBars gives each
    bar an equal angular slice. halfAngle = (2pi / numBars) * 0.35
    limits each bar to 35% of its slice, creating visible gaps.
    
    WHY 35% gap? With no gap (100% slice), adjacent bars touch and
    the radial spectrum looks like a solid ring with color bands.
    With gap, individual bars are distinguishable.
    
    The inner and outer radii are computed as:
      innerRadius = 0.18 (fixed, gives a visible center hole)
      outerRadius = 0.18 + h * (0.46 - 0.18) = 0.18 + h * 0.28
    Where h is the normalized bar height [0,1].
    At h=0: outer = 0.18 (same as inner, invisible bar).
    At h=1: outer = 0.46 (full height bar).
    
    WHY these specific radius values?
    0.18 is large enough to see the center hole clearly at 720p.
    0.46 keeps the outer edge within the visible area (max radius = 1.0
    at the screen edge). With aspect correction, the actual visual
    radius may be larger horizontally (aspect < 1 means x is scaled
    down, so y movements are more pronounced).

  Lines 411-413: buildCircularSpectrum()
    Trivially delegates to buildCircularBars(). 
    WHY an extra function instead of calling buildCircularBars directly?
    Consistency with the ModeOutput function signature pattern.

  Lines 415-445: buildCircularSpectrumFilled()
    First calls buildCircularBars() to get the bar spectrum.
    Then adds a pulsating center disc:
    1. Compute average bar height (avg)
    2. pulse = (0.4 + 0.6 * avg) * (1 + 0.05 * sin(time * 2))
    3. discRadius = innerRadius * pulse
    
    The disc pulses in size with the audio intensity. The sin(time*2)
    modulation adds a subtle animation so the disc doesn't stay static
    during constant loudness. pulse ranges from 0.4 (silence) to ~1.12
    (maximum loudness).
    
    WHY a filled disc? It fills the empty center hole of the circular
    spectrum, creating a cohesive visual that transitions from a disc
    (quiet) to a ring with spokes (loud).

  Lines 284-317: buildOscilloscope()
    The simplest mode: a scrolling X-Y waveform.
    
    Key design:
    - Downsample to 512 points (from 4096)
    - Smooth with 2-tap moving average
    - displayCount = n / waveZoom (zoom in = fewer points shown)
    - timeOffset = glfwGetTime() * waveSpeed * n (scrolling)
    
    The x coordinate is evenly spaced:
      x = (i / (displayCount-1)) * 2 - 1    (maps 0→displayCount to -1→1)
    
    The y coordinate is the audio sample:
      y = clamp(sample[idx] * 4 * sensitivity, -1, 1)
    
    WHY * 4? Audio samples are typically in [-0.5, 0.5] for normal
    listening levels. Multiplying by 4 gives a visible waveform
    at sensitivity = 1.0. Lower/higher sensitivity adjusts.
    
    WHY clamp to [-1, 1]? NDC limits. Without clamping, loud audio
    would go off-screen. However, clamping creates flat tops on
    peaks (clipping distortion in the visual). This is acceptable
    (and realistic for analog oscilloscopes).

  Lines 319-325: buildSpectrumBars / buildDenseSpectrum
    Simple delegates to buildBarSpectrum with different gap fractions.

  Lines 327-378: buildMirroredWaveform()
    Similar to oscilloscope but mirrored vertically.
    
    Fill vertices: For each adjacent pair of samples, draws two
    triangles forming a quad from (x_i, y_i) to (x_i, -y_i) to
    (x_{i+1}, y_{i+1}) - a symmetrical filled shape.
    
    Line vertices: Draws the top half as a line strip.
    
    WHY fill the mirror? The mirrored waveform (also called
    "stereo oscilloscope" or "VU meter") shows audio as a symmetrical
    shape. The filled version looks like a solid body pulsing with
    the music.

  Lines 380-409: buildCircularOscilloscope()
    Maps waveform to polar coordinates:
      angle = (i / n) * 2 * pi
      radius = baseRadius + amplitude * ampScale
      x = radius * cos(angle) * aspect
      y = radius * sin(angle)
    
    WHY polar? The waveform wraps around a circle, creating a
    "vibrating ring" effect. It's visually distinct from the linear
    oscilloscope and shows both time and amplitude in a compact form.
    
    ampScale = 0.28 means a full-scale signal (+-1) changes the
    radius by 28% of NDC. Smaller values = more subtle effect.

  Lines 447-472: buildLissajous()
    A true Lissajous figure (originally from oscilloscope XY mode).
    
    Takes the mono sample buffer and creates XY pairs with a delay:
      x = sample[i] * scale
      y = sample[(i + delay) % n] * scale
    
    delay = n / 6 = ~85 samples at n=512.
    WHY delay = n/6? This creates a 60-degree phase shift, which
    produces the classic Lissajous shapes (circle at 90°, ellipse
    at 45-135°). For real audio (which is multi-frequency, not a
    single sine), the delay produces evolving chaotic patterns.
    
    Color is HSV-based: hue cycles through the rainbow along the
    trace (t = i/n maps to hue from red → yellow → green → etc.)

  Lines 474-521: buildPulseRings()
    Three concentric rings, each representing a frequency band:
    - Ring 0: bins 1-8 (bass, ~10-80 Hz)
    - Ring 1: bins 8-100 (mid, ~80-1000 Hz)
    - Ring 2: bins 100-400 (high-mid, ~1-4 kHz)
    
    Each ring:
      radius = baseRadius + smoothedBand * ringScale
      phase = time * phaseSpeed (rotating animation)
      Color: fixed per band (orange/green/blue)
    
    WHY three rings? They visually separate frequency regions.
    The independent rotation speeds (0.3, -0.2, 0.15) make the
    rings counter-rotate, creating a dynamic interference pattern.
    Negative speed for ring 1 = counter-rotation, which looks
    more interesting than all going the same direction.

  Lines 523-663: buildTrueXY()
    The most complex general mode with 6 sub-modes.
    
    Down-samples to 256 points (hard-coded targetCount).
    WHY 256? Performance. Each sub-mode generates 6-36 vertices per
    point. At 256 points, the maximum is 9216 vertices (FilledTrail).
    At 4096 points, the same sub-mode would generate 147456 vertices
    per frame - too much for the CPU to build and GPU to draw at 60+ fps.
    
    Sub-mode implementations:
    
    Scatter (line 548): GL_POINTS at (x,y) with size 3.
      Simple, fast, effective for seeing the XY distribution.
    
    LineStrip (line 558): GL_LINE_STRIP connecting all points.
      Shows the signal path. Can look like a tangled ball of yarn
      with complex audio.
    
    Both (line 566): Line strip + triangle quads at each point.
      Quads are 0.004 NDC wide (~3 pixels at 1280x720).
      The quads make points visible even on thin line sections.
    
    FilledTrail (line 585): A ribbon of constant width following
      the XY path.
      
      KEY MATH (perpendicular normals):
      For segment from point a to point b:
        dx = b.y - a.y           (perpendicular to segment direction)
        dy = -(b.x - a.x)
        len = sqrt(dx^2 + dy^2)
        nx = dx / len * hw       (unit normal * half-width)
        ny = dy / len * hw
      
      WHY this formula? The direction vector is (b.x - a.x, b.y - a.y).
      Rotating by 90 degrees gives (-direction.y, direction.x). The
      formula above uses (direction.y, -direction.x) which is the
      SAME rotation but negated. This gives a perpendicular vector
      of length equal to the segment length. Normalizing divides
      by length, then we scale by half-width hw = 0.003.
      
      The quad vertices:
        (a.x - nx, a.y - ny)  →  (a.x + nx, a.y + ny)
        (b.x + nx, b.y + ny)  →  (b.x - nx, b.y - ny)
      This forms a ribbon that follows the path.
      
      EDGE CASE: When two consecutive segments have very different
      directions, the normals don't align, creating a "pinch" or
      "bulge" at the joint. A proper solution uses miter joints
      (computing the average normal at each vertex), but that's
      more complex. The visual artifact is minor for most audio.
    
    GlowScatter (line 607): GL_POINTS size 4 + additive glow quads.
      Each point gets a small halo via glow quads, making it
      look like glowing dust.
    
    PhosphorTrail (line 625): A rotating window of 64 points with
      brightness fade:
        brightness = 0.2 + 0.8 * (i / trailLen)
      Points are brighter at the head (0.2+0.8=1.0) and dimmer at
      the tail (0.2). The window rotates through the buffer at
      waveSpeed rate, creating a "scanning" effect.

  Lines 665-772: buildAnalogScope()
    The analog oscilloscope emulation. Documented separately below
    because it's the most complex builder.

================================================================================
   FILE 3: modes.hxx  (44 lines)
================================================================================

  Line 10-13: struct Segment
    {GLint first, GLsizei count} - a sub-range of the line vertex buffer.
    first = index of first vertex (0-based)
    count = number of vertices in this segment
    Used to break a line strip into disconnected pieces (for Z-blanking).

  Lines 15-30: struct ModeOutput
    This is the output of every builder function. It collects all vertex
    data for a single frame.

    lineVertices: Interleaved float array [x,y,r,g,b,size] for line/point drawing.
      WHY not separate position/color arrays? One array = one glBufferData call,
      one VAO configuration. The GPU reads sequentially, which is cache-friendly.

    lineSegments: std::vector<Segment> - list of sub-ranges for the line pass.
      If empty or has one segment with {0, N}, the line is drawn as one piece.
      Multiple segments break the line at Z-blanking boundaries.

    linePrimitive: GLenum - GL_LINE_STRIP or GL_POINTS (or theoretically
      GL_LINE_LOOP for circular modes). Controls the topology of the draw call.

    linePoints: bool - if true, use GL_POINTS instead of lines. This flag
      also sets the uIsPoint uniform in the fragment shader for circular
      point masking.

    lineGlow: bool - if true, draw the additive glow pass before the
      crisp line pass. All modes set this to true (line glow is always
      desirable for the soft halo look).

    lineWidth: float - width of the crisp line pass in pixels.
      Default 1.8f. Analog scope overrides to 2.0f.

    fillVertices: Triangle vertices [x,y,r,g,b,size] for GL_TRIANGLES draw.
      Used by bar modes (bars), mirrored waveform (filled body),
      XY modes (quad particles, filled trail), and analog scope
      (scatter quads).

    fillAlpha: float - uniform alpha for fill triangles.
      Mirrored waveform uses 0.55 (semi-transparent). Most modes use 1.0.

    glowVertices: Additive bloom overlay vertices (GL_TRIANGLES).
      These are drawn after fill but before lines, with additive blend.
      The glow pass accumulates brightness: overlapping glow quads
      create brighter spots, mimicking optical bloom.

  Lines 32-42: Builder function declarations
    Each builder takes audio data + VisState and returns ModeOutput.
    Some take additional parameters:
    - smoothedHeights: Persistent vector for IIR filtering (bar modes)
    - time: Current wall time for animated modes (PulseRings)

================================================================================
   FILE 4: vis_state.hxx  (49 lines)
================================================================================

  Lines 6-10: struct Color3
    {float r, g, b} - simple RGB color representation.
    WHY not use glm::vec3? To avoid a glm dependency for 3 floats.

  Lines 12-15: Enums
    ColorMode: Normal, RandomSolid, RandomGradient
    InputMode: Microphone, SystemAudio
    TrueXYMode: Scatter, LineStrip, Both, FilledTrail, GlowScatter,
      PhosphorTrail
    AnalogScopeMode: Trace, Scatter, Both

  Lines 17-47: struct VisState
    Central state struct holding ALL configurable parameters.
    
    numBars (18): Number of spectrum bars. Default 64.
    sensitivity (19): Audio sensitivity multiplier. Default 1.0.
    zoom (20): Global zoom level for vertex shader. Default 1.0.
    aspect (21): Screen aspect ratio (height/width). Set dynamically on resize.
    
    maxBarsLimit, maxSensitivityLimit (23-24): Safety limits for bar count
      and sensitivity. Can be doubled by pressing B.
    
    waveZoom, waveSpeed (26-27): Waveform horizontal zoom and scrolling speed.
    
    colorMode, randomSolid, gradientColors (29-31): Color state.
      gradientColorCount tracks how many colors the user configured (may
      differ from gradientColors.size() if not in RandomGradient mode).
    
    bloom, bloomIntensity, bloomSize (34-36): Bloom parameters.
      bloomSize controls how far the glow spreads (used in appendBarGlow).
    
    antiAliasing (38): MSAA toggle.
    inputMode (39): Current audio input source.
    
    trueXYMode, trueXYLines (40-41): TrueXY sub-mode and line overlay toggle.
    
    analogScope, analogScopeMode (42-43): Analog scope on/off and sub-mode.
    analogScopeDecay (44): Phosphor decay rate. Default 4.0.
    analogResolution (45): Number of raw samples for analog scope. Default 4096.
    analogLineCount (46): Number of trace samples. Default 4096.

================================================================================
   FILE 5: vis_state.cpp  (43 lines)
================================================================================

  Lines 6-43: Color3 resolveColor(const VisState& state, Color3 base, float t)
    Applies the current color mode to a base color, producing a final color.
    
    Normal mode: Returns base unchanged. Modes define their own base color
      (e.g., P31 green for analog scope, cyan for oscilloscope).
    
    RandomSolid mode: Returns state.randomSolid (the single random color
      set by pressing R). The brightness of the base color is preserved:
      the random color's RGB is multiplied by max(base.r, g, b) clamped
      to [0.3, 1.0]. WHY preserve brightness? If base is dark (like
      P31 green 0.08,0.5,0.06), the random color would be muted.
      Multiplying by brightness keeps the trace's inherent luminance
      structure intact.
    
    RandomGradient mode: Samples the gradientColors palette at position
      t ∈ [0,1]. Uses linear interpolation between adjacent palette
      entries. t=0 → first color, t=1 → last color.
      WHY linear interpolation? Simple, fast, and produces smooth
      color transitions. The alternative (nearest-neighbor) would
      create banding artifacts.
    
    The t parameter is the vertex's normalized position along the
    visualization (0 = start, 1 = end). This creates gradients across
    the trace. For analog scope, t = 1 - age (head = 1, tail = 0),
    so the head gets the "first" gradient color and the tail gets
    the "last".

================================================================================
   FILE 6: audio.hxx / audio.cpp  (44 + 168 lines)
================================================================================

  audio.hxx:
    Line 8: enum class CaptureSource { Microphone, SystemAudio }
    
    Line 10-44: class AudioCapture
      Wraps miniaudio's capture functionality.
      
      Constructor (line 12): Takes ringBufferSize, allocates ring buffer.
      Destructor (line 13): Stops capture, uninitializes device and context.
      Deleted copy constructor/assignment (15-16): Prevent accidental copy
        of the device handle (which is non-copyable).
      
      Public methods:
      - start(): Opens default microphone, starts capture.
      - switchSource(): Changes between microphone and system audio.
      - stop(): Stops the capture device.
      - readLatest(): Mono read (averages L+R).
      - readLatestStereo(): Stereo read (separate L/R buffers).
      - ringSize(): Returns buffer size.
      - currentSource(): Returns current source enum.
      
      Private members:
      - m_ringBuffer: Circular buffer of floats (interleaved L,R,L,R,...).
      - m_writeIndex: Current write position (increments by frameCount each callback).
      - m_mutex: Protects ring buffer from concurrent access.
      - m_context/miniaudio context and device handles.
      - dataCallback: Static method called by miniaudio on the audio thread.
      - pushSamples: Writes samples into the ring buffer under mutex.
      - initAndStartDevice: Opens and starts a capture device for given source.

  audio.cpp:

    Line 1: #define MA_IMPLEMENTATION
      This must be defined BEFORE #include "audio.hxx" in exactly ONE .cpp
      file. It causes miniaudio's implementation body to be compiled here.
      If defined in multiple files, linker errors would occur.

    Lines 9-24: AudioCapture constructor
      Initializes all members, then calls ma_context_init to initialize
      the miniaudio engine. The context holds ALSA resources. Without
      this, no audio devices can be enumerated or opened.
      
      WHY miniaudio instead of portaudio/alsa directly? miniaudio is
      single-file (easy to vendor), supports multiple backends (ALSA,
      PulseAudio, WASAPI, CoreAudio), and is MIT-licensed. Using ALSA
      directly would tie the project to Linux.

    Lines 26-36: Destructor
      Stops capture, uninitializes device and context in reverse order.
      Order matters: the device must be stopped before the context is
      uninitialized.

    Lines 38-46: start() and switchSource()
      start() delegates to initAndStartDevice(Microphone).
      switchSource() only reinitializes if the source changed.

    Lines 49-110: initAndStartDevice(CaptureSource source)
      The core device setup:
      1. Clean up any existing device.
      2. Configure device: type=capture, format=f32, channels=2,
         sampleRate=48000, dataCallback=this::dataCallback.
      3. If SystemAudio: enumerate capture devices, find one with
         "monitor" in the name, set its device ID in the config.
      4. ma_device_init - create the device.
      5. ma_device_start - begin capture.
      
      WHY 48000 Hz sample rate? It's the standard rate for most audio
      interfaces and USB microphones. 44100 Hz would work too but
      48 kHz is more common for modern hardware.
      
      WHY stereo channels? TrueXY and analog scope need both channels.
      Mono modes readLatest() averages them.
      
      WHY format f32? 32-bit float gives headroom for processing.
      Integer formats (s16, s24) would require conversion.

    Lines 112-117: stop()
      Calls ma_device_stop. Safe to call if not running.

    Lines 119-123: dataCallback(...)
      Static method called on the audio thread. frequency: ~48 times per
      second (each callback delivers ~1024 frames at 48 kHz).
      
      The callback receives:
      - pDevice: the ma_device (we extract pUserData → self pointer)
      - pOutput: ignored (capture, not playback)
      - pInput: interleaved float samples [L,R,L,R,...]
      - frameCount: number of stereo frames in this callback
      
      It forwards to pushSamples() which does the actual ring buffer write.
      
      WHY the static cast? The dataCallback is a C function pointer
      (no 'this' context). The pUserData pointer registered at line 64
      is set to 'this' (the AudioCapture instance), so we cast back.

    Lines 125-131: pushSamples(const float* samples, size_t count)
      Writes count interleaved samples to the ring buffer.
      
      1. Lock mutex (to prevent concurrent read by main thread).
      2. For each sample:
          m_ringBuffer[m_writeIndex] = samples[i]
          m_writeIndex = (m_writeIndex + 1) % m_ringSize
      
      WHY modulo wrap? This is the ring buffer. When m_writeIndex reaches
      m_ringSize, it wraps back to 0 (overwriting the oldest sample).
      
      WHY mutex? The audio callback runs on a different thread. Without
      the mutex, the main thread could read a partially-updated buffer
      (half new samples, half old) or read at the same time the callback
      is writing, causing a torn read.

    Lines 133-150: void readLatest(float* out, size_t count) const
      Reads the most recent 'count' mono samples (averages L+R per frame).
      
      Steps:
      1. Lock mutex.
      2. ringFrames = m_ringSize / 2 (each frame is 2 samples: L+R).
      3. If count > ringFrames, trim count (can't read more than buffer).
      4. writeFrame = m_writeIndex / 2 (convert sample index to frame index).
      5. startFrame = (writeFrame + ringFrames - count) % ringFrames
         This computes where the 'count' most recent frames begin.
         The modulo handles wrap-around:
         Example: ringFrames=8192, writeFrame=100, count=4096
         startFrame = (100 + 8192 - 4096) % 8192 = 4196
         This means frames 4196..8191 (3996 frames) + 0..100 (100 frames)
         are the most recent 4096 frames.
      6. Copy count frames, averaging L and R:
          out[i] = (ring[frameIdx*2] + ring[frameIdx*2+1]) * 0.5

    Lines 152-168: void readLatestStereo(float* left, float* right,
                                           size_t count) const
      Same as readLatest but separates L and R into two output buffers.
      
      The only difference from readLatest is line 165-166:
        left[i]  = ring[frameIdx * 2]        (L sample)
        right[i] = ring[frameIdx * 2 + 1]    (R sample)
      No averaging - preserves stereo information for XY/scope modes.

================================================================================
   FILE 7: fft.hxx / fft.cpp  (18 + 70 lines)
================================================================================

  fft.hxx:
    Declares two functions in namespace fft:
    - transform(): In-place iterative radix-2 Cooley-Tukey FFT.
    - computeMagnitudeSpectrum(): Hann-windowed magnitude spectrum.

  fft.cpp:

    Line 7: constexpr float kPi = 3.14159265358979323846f
      Single-precision pi. WHY not std::numbers::pi? C++20 has it, but
      this explicit constant works everywhere.

    Lines 12-24: static void bitReverseReorder(std::complex<float>* a, size_t n)
      Reorders the array in bit-reversed order. This is the first step
      of the iterative Cooley-Tukey FFT.
      
      ALGORITHM:
      The standard bit-reversal method: for i from 1 to n-1:
        bit = n >> 1
        while (j & bit):
          j ^= bit
          bit >>= 1
        j ^= bit
        if (i < j): swap(a[i], a[j])
      
      HOW IT WORKS:
      This is the classic "reverse bits" algorithm. It computes j as the
      bit-reverse of i (for log2(n) bits) and swaps pairs.
      
      Example: n=8 (3 bits), i=3 (binary 011), bit-reversed j=6 (binary 110).
      The algorithm builds j bit by bit by scanning i's bits from high to low.
      
      WHY bit-reversal? The iterative FFT works in stages. At stage s,
      it combines elements that are 2^s apart. But the initial array is
      in natural order (0, 1, 2, ..., n-1). After bit-reversal, the
      elements are in the order needed by the iterative algorithm:
      the first stage pairs (0,4), (1,5), (2,6), (3,7). This is
      exactly the bit-reversed pattern.
      
      WHY the 'if (i < j)' check? Without it, each pair would be swapped
      twice (once when i is the first element, once when j is), resulting
      in no change. The check ensures each pair is swapped exactly once.

    Lines 26-50: void transform(std::vector<std::complex<float>>& a)
      In-place iterative radix-2 Cooley-Tukey FFT.
      
      ALGORITHM:
      The FFT decomposes the DFT into log2(n) stages. Each stage combines
      pairs of elements using the butterfly operation:
      
      For each stage with length len = 2, 4, 8, ..., n:
        wlen = exp(-2*pi*i / len)     (twiddle factor)
        For each group of size len:
          w = 1
          For j = 0 to len/2 - 1:
            u = a[i + j]
            v = a[i + j + len/2] * w
            a[i + j] = u + v
            a[i + j + len/2] = u - v
            w = w * wlen
      
      WHY the negative sign in ang = -2*pi/len?
      This gives the forward FFT (time → frequency). The inverse FFT
      uses +2*pi/len. The negating convention is standard.
      
      WHY iterative instead of recursive? The recursive implementation
      is elegant (split array in half, FFT each half, combine) but:
      + Higher function call overhead (log2(4096) = 12 levels)
      + More memory (each recursion level creates new arrays)
      + Cache-unfriendly (deep recursion scatters memory access)
      The iterative implementation is more efficient in practice,
      especially for power-of-2 sizes up to 4096.

    Lines 52-68: void computeMagnitudeSpectrum(const float* samples, size_t n,
                                                 std::vector<float>& outMagnitudes)
      The FFT pipeline:
      1. Convert real samples to complex array (imaginary part = 0).
      2. Apply Hann window.
      3. Call transform() (the FFT).
      4. Extract magnitudes for the first n/2 bins.
      
      WHY Hann window? (This was a key user question)
      
      The FFT assumes the input signal is periodic within the window.
      For real audio, the signal at the start and end of the 4096-sample
      window are usually different, creating a discontinuity. This
      discontinuity appears in the FFT output as spectral leakage:
      energy from a single frequency spreads into adjacent bins.
      
      The Hann window smoothly tapers the signal to zero at both ends:
        window[i] = 0.5 * (1 - cos(2*pi*i / (n-1)))
      This eliminates the discontinuity, reducing spectral leakage.
      The cost is reduced amplitude resolution (the window attenuates
      the signal by up to 50% at the edges), but this is acceptable
      for visualization.
      
      WHY Hann instead of Hamming, Blackman, or Kaiser?
      Hann is the simplest window that provides adequate sidelobe
      suppression (-31 dB) for visualization purposes. Blackman (-58 dB)
      would be better but has a wider main lobe (reduced frequency
      resolution). Kaiser is tunable but requires additional parameter
      selection. Hann is the "safe default" for audio visualization.
      
      WHY divide magnitude by n (line 66)?
      The FFT output magnitude scales with the number of samples.
      Dividing by n normalizes so that a full-scale sine wave at
      a bin's center frequency produces magnitude ≈ 0.5. Without
      normalization, magnitudes would change with FFT size.

================================================================================
   FILE 8: shader.hxx / shader.cpp  (25 + 86 lines)
================================================================================

  shader.hxx:
    Line 8-25: class Shader
      Wraps an OpenGL shader program.
      Constructor: loads, compiles, and links vertex+fragment shaders.
      Destructor: calls glDeleteProgram.
      Non-copyable (the program handle would be double-deleted).
      
      Methods:
      - use(): glUseProgram
      - setFloat: glUniform1f
      - setInt: glUniform1i
      - setVec4: glUniform4f
      
      Private statics:
      - readFile: Reads a text file into std::string.
      - compileShader: Compiles GLSL source, checks for errors.

  shader.cpp:

    Lines 8-16: std::string Shader::readFile(...)
      Reads an entire file into a string. Throws on failure.
      WHY not use std::ifstream directly in the constructor?
      Separation of concerns: file I/O is a distinct operation.
      Also makes it possible to override (virtual) in tests.

    Lines 18-35: GLuint Shader::compileShader(...)
      1. glCreateShader(type) - allocate shader object.
      2. glShaderSource(shader, 1, &src, nullptr) - set source code.
         The count=1 means one null-terminated string. If we had
         multiple sources (e.g., for #include support), count > 1.
      3. glCompileShader - compile.
      4. glGetShaderiv(shader, GL_COMPILE_STATUS) - check success.
      5. On failure: get log length, allocate buffer, get log, print it.
      6. Return shader handle or throw.
      
      WHY throw on failure? A shader compilation error is a fatal bug
      that cannot be handled at runtime. The message includes the
      shader file name and the GLSL compiler error message, which
      tells the developer exactly which line failed.

    Lines 37-66: Shader::Shader(...)
      1. Read vertex and fragment source.
      2. Compile both.
      3. Create program, attach shaders, link.
      4. Check link status.
      5. Delete shader objects (they're embedded in the program now).
      6. On linking failure: clean up and throw.
      
      WHY delete shaders after linking? The program retains the compiled
      object code. The shader objects are no longer needed. This frees
      memory on the GPU.

    Lines 68-86: Destructor and uniform setters
      Destructor: glDeleteProgram.
      setFloat: glGetUniformLocation + glUniform1f.
        WHY glGetUniformLocation every call instead of caching?
        Premature optimization. These setters are called ~5 times per
        frame. The cost of glGetUniformLocation (a hash table lookup
        in the driver) is negligible at that frequency.

================================================================================
   FILE 9: vertex_shader.glsl  (15 lines)
================================================================================

  #version 460 core         (line 1)
  layout (location = 0) in vec2 aPos;      (line 3) - position attribute
  layout (location = 1) in vec3 aColor;    (line 4) - color attribute
  layout (location = 2) in float aSize;    (line 5) - point size attribute
  out vec3 vColor;                         (line 7) - fragment shader input
  uniform float uZoom;                     (line 9) - zoom uniform
  void main() {
      vColor = aColor;                     (line 12) - pass color to fragment shader
      gl_PointSize = aSize;               (line 13) - set point size (for GL_POINTS)
      gl_Position = vec4(aPos * uZoom, 0.0, 1.0); (line 14) - transform position
  }

  WHY uZoom multiplied into position instead of using a projection matrix?
  Simplicity. A projection matrix would be more flexible (offset, rotate,
  scale independently in x/y) but all we need is uniform zoom. The vertex
  position is already in NDC [-1,1], multiplying by zoom > 1 scales it
  beyond NDC (clipped), zoom < 1 scales it smaller.

  WHY gl_PointSize? OpenGL renders points as screen-space squares of
  this many pixels. Without glEnable(GL_PROGRAM_POINT_SIZE) (line 1571
  of main.cpp), this value is ignored and all points are 1 pixel. With
  the enable flag, the shader controls the size.

  WHY the location qualifiers? They must match the glVertexAttribPointer
  calls in setupVAO (main.cpp lines 1591-1595). Location 0 = vec2 aPos
  (2 floats), location 1 = vec3 aColor (3 floats), location 2 = float aSize
  (1 float). Total stride: 6 floats = 24 bytes.

================================================================================
   FILE 10: fragment_shader.glsl  (19 lines)
================================================================================

  #version 460 core
  out vec4 FragColor;
  in vec3 vColor;
  uniform int uIsPoint;
  uniform float uAlpha;
  void main() {
      if (uIsPoint == 1) {
          vec2 circCoord = 2.0 * gl_PointCoord - 1.0;
          if (dot(circCoord, circCoord) > 1.0) discard;
      }
      FragColor = vec4(vColor, uAlpha);
  }

  WHY the uIsPoint branch?
  gl_PointCoord is a built-in variable that ranges from (0,0) at the
  top-left of the point's square to (1,1) at bottom-right. Without the
  branch, points would be rendered as axis-aligned squares, which looks
  ugly for scatter plots. The circular mask discards fragments outside
  a unit circle:
    circCoord = 2 * gl_PointCoord - 1    (maps [0,1] → [-1,1])
    if (dot(circCoord, circCoord) > 1.0) discard
  This only keeps fragments within a radius of 1.0 from the point center,
  creating a circle. For lines/triangles (uIsPoint=0), gl_PointCoord is
  undefined and the branch is skipped.

  WHY uAlpha uniform instead of encoding alpha in the vertex color?
  The vertex color uses all 3 RGB channels. Adding alpha would require
  a 4th component, increasing VERTEX_FLOATS from 6 to 7 (28 bytes per
  vertex vs 24). A uniform alpha for the entire draw is cheaper and
  sufficient for all current modes.

================================================================================
   FILE 11: Makefile  (21 lines)
================================================================================

  Line 1: CXX := g++
    Compiler choice. Could be clang++ by changing this line.
  Line 2: CXXFLAGS := -std=c++20 -I.
    C++20 standard, include current directory.
    WHY -I.? Because #include "audio.hxx" uses quotes, which searches
    the including file's directory first, then -I paths. With -I.,
    the compiler searches the project root.

  Line 3: LDFLAGS := -lglfw -lGL -lasound
    Linker flags: GLFW, OpenGL, ALSA (for miniaudio).
    WHY -lasound? miniaudio with ALSA backend needs libasound.
    Other backends (PulseAudio, JACK) would need different libraries.

  Lines 5: OBJS := main.o modes.o audio.o fft.o shader.o vis_state.o glad.o
    Object file list. Each .cpp produces a .o.

  Lines 14-18: Implicit rules and special case
    Line 14: %.o: %.cpp → compiles each .cpp to .o.
    Line 17: glad.o: glad.c → special rule for .c file.
    WHY separate rule for glad.c? It's C, not C++. The .cpp rule would
    compile it as C++ (which would work but is slower and may produce
    warnings). The explicit rule treats it as C.

  Lines 20-21: clean target
    Removes all .o files and the visualizer binary.

================================================================================
  SUMMARY: The Data Flow
================================================================================

  Here is the complete data flow through the program, from audio wave
  to screen pixels:

  1. Audio callback thread samples microphone/loopback at 48 kHz stereo.
     Writes interleaved [L,R,L,R,...] samples into ring buffer (audio.cpp:129).

  2. Main render loop calls readLatestStereo() (main.cpp:1903).
     Copies most recent 4096 frames into leftBuffer[], rightBuffer[].
     Ring buffer read position calculated modulo math ensures we get
     exactly the most recent samples.

  3. Mono mix for FFT: sampleBuffer[i] = (L[i] + R[i]) / 2 (main.cpp:1905).

  4. FFT: computeMagnitudeSpectrum() (fft.cpp:52):
     a. Apply Hann window to 4096 samples.
     b. Convert to complex array.
     c. Bit-reverse reorder.
     d. 12 stages of iterative radix-2 FFT.
     e. Extract 2048 magnitude bins (sqrt(re^2 + im^2) / n).

  5. Mode builder (modes.cpp) takes samples/FFT output + VisState:
     a. For bar modes: map bins to bars, apply dB compression,
        smooth with IIR filter, generate vertex data.
     b. For wave modes: smooth, scroll via time offset, generate vertices.
     c. For XY modes: combine L/R channels, generate points/lines/quads.
     d. For analog scope: 4x interpolate, apply Z-blanking, compute
        phosphor decay, generate trace + particles.

  6. ModeOutput contains up to 3 vertex arrays (line/fill/glow).

  7. OpenGL render (main.cpp:1958-2021):
     a. Clear framebuffer to dark background.
     b. Fill pass: upload fillVBO, draw GL_TRIANGLES (normal blend).
     c. Glow pass: upload glowVBO, draw GL_TRIANGLES (additive blend).
     d. Line pass: for each segment, draw glow (additive, thick, dim)
        then crisp (normal blend, thin, bright).

  8. Fragment shader applies per-vertex color and uniform alpha,
     optionally circular-masking points.

  9. MSAA resolve (if enabled): blit from multisample FBO to screen.

   10. glfwSwapBuffers presents the frame.
)");
            return 0;
        }
        if (arg == "--detail-summary" || arg == "-ds") {
            printAnsiDoc(kDetailText);
            return 0;
        }
    }
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