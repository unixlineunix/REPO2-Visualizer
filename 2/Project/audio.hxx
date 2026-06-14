#pragma once
//#define MINIAUDIO_IMPLEMENTATION

#include "miniaudio.h"
#include <vector>
#include <mutex>
#include <cstddef>

class AudioCapture {
public:
    explicit AudioCapture(size_t ringBufferSize);
    ~AudioCapture();

    AudioCapture(const AudioCapture&) = delete;
    AudioCapture& operator=(const AudioCapture&) = delete;

    bool start();
    void stop();

    // Copies the `count` most recent samples (oldest first, newest last)
    // into `out`. If count > ringSize, count is clamped to ringSize.
    void readLatest(float* out, size_t count) const;

    size_t ringSize() const noexcept { return m_ringSize; }

private:
    static void dataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
    void pushSamples(const float* samples, size_t count);

    size_t m_ringSize;
    std::vector<float> m_ringBuffer;
    size_t m_writeIndex;
    mutable std::mutex m_mutex;

    ma_device m_device;
    bool m_deviceInitialized;
    bool m_running;
};