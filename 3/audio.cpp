#define MA_IMPLEMENTATION
#include "audio.hxx"

#include <algorithm>
#include <iostream>

AudioCapture::AudioCapture(size_t ringBufferSize, bool systemAudio)
    : m_ringSize(ringBufferSize),
      m_ringBuffer(ringBufferSize, 0.0f),
      m_writeIndex(0),
      m_device{},
      m_contextInit(false),
      m_deviceInit(false),
      m_running(false),
      m_systemAudio(systemAudio)
{
    if (ma_context_init(nullptr, 0, nullptr, &m_context) != MA_SUCCESS) {
        std::cerr << "AudioCapture: failed to init context\n";
    } else {
        m_contextInit = true;
    }
}

AudioCapture::~AudioCapture() {
    stop();
    uninitDevice();
    if (m_contextInit) {
        ma_context_uninit(&m_context);
        m_contextInit = false;
    }
}

void AudioCapture::uninitDevice() {
    if (m_deviceInit) {
        ma_device_uninit(&m_device);
        m_deviceInit = false;
    }
}

void AudioCapture::clearBuffer() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::fill(m_ringBuffer.begin(), m_ringBuffer.end(), 0.0f);
    m_writeIndex = 0;
}

bool AudioCapture::initCapture() {
    uninitDevice();

    ma_device_config config = ma_device_config_init(ma_device_type_capture);
    config.capture.format   = ma_format_f32;
    config.capture.channels = 1;
    config.sampleRate       = 48000;
    config.dataCallback     = AudioCapture::dataCallback;
    config.pUserData        = this;

    if (m_systemAudio) {
        ma_device_info* captureInfos = nullptr;
        ma_uint32 captureCount = 0;
        ma_device_id monitorId;
        bool found = false;

        if (ma_context_get_devices(&m_context, nullptr, nullptr,
                                   &captureInfos, &captureCount) == MA_SUCCESS) {
            std::cout << "AudioCapture(sys): " << captureCount << " devices\n";
            for (ma_uint32 i = 0; i < captureCount; ++i)
                std::cout << "  [" << i << "] " << captureInfos[i].name << "\n";

            for (ma_uint32 i = 0; i < captureCount; ++i) {
                const char* name = captureInfos[i].name;
                for (const char* p = name; *p; ++p) {
                    const char* q = p;
                    const char* pat = "monitor";
                    while (*q && *pat) {
                        char c = *q;
                        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
                        if (c != *pat) break;
                        ++q; ++pat;
                    }
                    if (!*pat) {
                        monitorId = captureInfos[i].id;
                        found = true;
                        std::cout << "AudioCapture(sys): using '" << name << "'\n";
                        break;
                    }
                }
                if (found) break;
            }

            if (!found && captureCount > 1) {
                monitorId = captureInfos[1].id;
                found = true;
                std::cout << "AudioCapture(sys): using [1] '" << captureInfos[1].name << "'\n";
            }
        }

        if (found) config.capture.pDeviceID = &monitorId;
        else       std::cout << "AudioCapture(sys): no monitor, using default\n";
    }

    if (ma_device_init(m_contextInit ? &m_context : nullptr, &config, &m_device) != MA_SUCCESS) {
        std::cerr << "AudioCapture: failed to init device\n";
        return false;
    }
    m_deviceInit = true;
    clearBuffer();
    return true;
}

bool AudioCapture::start() {
    if (!m_deviceInit && !initCapture()) return false;
    if (ma_device_start(&m_device) != MA_SUCCESS) {
        std::cerr << "AudioCapture: failed to start device\n";
        return false;
    }
    m_running = true;
    return true;
}

void AudioCapture::stop() {
    if (m_running && m_deviceInit) {
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
    if (count > m_ringSize) count = m_ringSize;
    size_t start = (m_writeIndex + m_ringSize - count) % m_ringSize;
    for (size_t i = 0; i < count; ++i)
        out[i] = m_ringBuffer[(start + i) % m_ringSize];
}
