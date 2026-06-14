#define MA_IMPLEMENTATION
#include "audio.hxx"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>

AudioCapture::AudioCapture(size_t ringBufferSize)
    : m_ringSize(ringBufferSize),
      m_ringBuffer(ringBufferSize, 0.0f),
      m_writeIndex(0),
      m_context{},
      m_contextInitialized(false),
      m_device{},
      m_deviceInitialized(false),
      m_running(false),
      m_source(CaptureSource::Microphone) {
    if (ma_context_init(nullptr, 0, nullptr, &m_context) == MA_SUCCESS) {
        m_contextInitialized = true;
    } else {
        std::cerr << "AudioCapture: failed to initialize audio context\n";
    }
}

AudioCapture::~AudioCapture() {
    stop();
    if (m_deviceInitialized) {
        ma_device_uninit(&m_device);
        m_deviceInitialized = false;
    }
    if (m_contextInitialized) {
        ma_context_uninit(&m_context);
        m_contextInitialized = false;
    }
}

bool AudioCapture::start() {
    return initAndStartDevice(CaptureSource::Microphone);
}

bool AudioCapture::switchSource(CaptureSource source) {
    if (source == m_source && m_running) {
        return true;
    }
    return initAndStartDevice(source);
}

bool AudioCapture::initAndStartDevice(CaptureSource source) {
    if (!m_contextInitialized) {
        return false;
    }

    if (m_deviceInitialized) {
        ma_device_uninit(&m_device);
        m_deviceInitialized = false;
    }

    ma_device_config config = ma_device_config_init(ma_device_type_capture);
    config.capture.format   = ma_format_f32;
    config.capture.channels = 2; // Stereo for XY mode
    config.sampleRate       = 48000;
    config.dataCallback     = AudioCapture::dataCallback;
    config.pUserData        = this;

    ma_device_id monitorId{};
    bool useMonitor = false;

    if (source == CaptureSource::SystemAudio) {
        ma_device_info* pCaptureInfos = nullptr;
        ma_uint32 captureCount = 0;

        if (ma_context_get_devices(&m_context, nullptr, nullptr, &pCaptureInfos, &captureCount) == MA_SUCCESS) {
            for (ma_uint32 i = 0; i < captureCount; ++i) {
                std::string name(pCaptureInfos[i].name);
                std::transform(name.begin(), name.end(), name.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

                if (name.find("monitor") != std::string::npos) {
                    monitorId = pCaptureInfos[i].id;
                    useMonitor = true;
                    break;
                }
            }
        }

        if (!useMonitor) {
            std::cerr << "AudioCapture: no system-audio monitor device found; staying on current input\n";
            return false;
        }

        config.capture.pDeviceID = &monitorId;
    }

    if (ma_device_init(&m_context, &config, &m_device) != MA_SUCCESS) {
        std::cerr << "AudioCapture: failed to initialize capture device\n";
        return false;
    }

    if (ma_device_start(&m_device) != MA_SUCCESS) {
        std::cerr << "AudioCapture: failed to start capture device\n";
        ma_device_uninit(&m_device);
        return false;
    }

    m_deviceInitialized = true;
    m_running = true;
    m_source = source;
    return true;
}

void AudioCapture::stop() {
    if (m_running && m_deviceInitialized) {
        ma_device_stop(&m_device);
        m_running = false;
    }
}

void AudioCapture::dataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    (void)pOutput;
    auto* self = static_cast<AudioCapture*>(pDevice->pUserData);
    self->pushSamples(static_cast<const float*>(pInput), static_cast<size_t>(frameCount));
}

void AudioCapture::pushSamples(const float* samples, size_t count) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (size_t i = 0; i < count; ++i) {
        m_ringBuffer[m_writeIndex] = samples[i];
        m_writeIndex = (m_writeIndex + 1) % m_ringSize;
    }
}

void AudioCapture::readLatest(float* out, size_t count) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Each frame has 2 samples (L/R)
    size_t ringFrames = m_ringSize / 2;
    if (count > ringFrames) {
        count = ringFrames;
    }

    size_t writeFrame = m_writeIndex / 2;
    size_t startFrame = (writeFrame + ringFrames - count) % ringFrames;

    for (size_t i = 0; i < count; ++i) {
        size_t frameIdx = (startFrame + i) % ringFrames;
        // Average L and R for mono output
        out[i] = (m_ringBuffer[frameIdx * 2] + m_ringBuffer[frameIdx * 2 + 1]) * 0.5f;
    }
}

void AudioCapture::readLatestStereo(float* left, float* right, size_t count) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    size_t ringFrames = m_ringSize / 2;
    if (count > ringFrames) {
        count = ringFrames;
    }

    size_t writeFrame = m_writeIndex / 2;
    size_t startFrame = (writeFrame + ringFrames - count) % ringFrames;

    for (size_t i = 0; i < count; ++i) {
        size_t frameIdx = (startFrame + i) % ringFrames;
        left[i]  = m_ringBuffer[frameIdx * 2];
        right[i] = m_ringBuffer[frameIdx * 2 + 1];
    }
}