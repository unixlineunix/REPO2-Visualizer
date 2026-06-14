#define MA_IMPLEMENTATION
#include "audio.hxx"

#include <algorithm>
#include <iostream>

AudioCapture::AudioCapture(size_t ringBufferSize)
    : m_ringSize(ringBufferSize),
      m_ringBuffer(ringBufferSize, 0.0f),
      m_writeIndex(0),
      m_device{},
      m_deviceInitialized(false),
      m_running(false) {}

AudioCapture::~AudioCapture() {
    stop();
    if (m_deviceInitialized) {
        ma_device_uninit(&m_device);
        m_deviceInitialized = false;
    }
}

bool AudioCapture::start() {
    ma_device_config config = ma_device_config_init(ma_device_type_capture);
    config.capture.format   = ma_format_f32;
    config.capture.channels = 1;
    config.sampleRate       = 48000;
    config.dataCallback     = AudioCapture::dataCallback;
    config.pUserData        = this;

    if (ma_device_init(nullptr, &config, &m_device) != MA_SUCCESS) {
        std::cerr << "AudioCapture: failed to initialize capture device\n";
        return false;
    }
    m_deviceInitialized = true;

    if (ma_device_start(&m_device) != MA_SUCCESS) {
        std::cerr << "AudioCapture: failed to start capture device\n";
        ma_device_uninit(&m_device);
        m_deviceInitialized = false;
        return false;
    }

    m_running = true;
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

    if (count > m_ringSize) {
        count = m_ringSize;
    }

    size_t start = (m_writeIndex + m_ringSize - count) % m_ringSize;
    for (size_t i = 0; i < count; ++i) {
        out[i] = m_ringBuffer[(start + i) % m_ringSize];
    }
}