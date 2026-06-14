#pragma once

#include "miniaudio.h"
#include <vector>
#include <mutex>
#include <cstddef>

class AudioCapture {
public:
    explicit AudioCapture(size_t ringBufferSize, bool systemAudio = false);
    ~AudioCapture();

    AudioCapture(const AudioCapture&) = delete;
    AudioCapture& operator=(const AudioCapture&) = delete;

    bool start();
    void stop();
    bool running() const noexcept { return m_running; }

    void readLatest(float* out, size_t count) const;
    size_t ringSize() const noexcept { return m_ringSize; }

private:
    static void dataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
    void pushSamples(const float* samples, size_t count);
    void clearBuffer();
    void uninitDevice();
    bool initCapture();

    size_t m_ringSize;
    std::vector<float> m_ringBuffer;
    size_t m_writeIndex;
    mutable std::mutex m_mutex;

    ma_context m_context;
    ma_device m_device;
    bool m_contextInit;
    bool m_deviceInit;
    bool m_running;
    bool m_systemAudio;
};
