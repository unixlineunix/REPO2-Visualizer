#pragma once

#include "miniaudio.h"
#include <vector>
#include <mutex>
#include <cstddef>

enum class CaptureSource { Microphone, SystemAudio };

class AudioCapture {
public:
    explicit AudioCapture(size_t ringBufferSize);
    ~AudioCapture();

    AudioCapture(const AudioCapture&) = delete;
    AudioCapture& operator=(const AudioCapture&) = delete;

    bool start();
    bool switchSource(CaptureSource source);
    void stop();

    void readLatest(float* out, size_t count) const;
    void readLatestStereo(float* left, float* right, size_t count) const;

    size_t ringSize() const noexcept { return m_ringSize; }
    CaptureSource currentSource() const noexcept { return m_source; }

private:
    static void dataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
    void pushSamples(const float* samples, size_t count);
    bool initAndStartDevice(CaptureSource source);

    size_t m_ringSize;
    std::vector<float> m_ringBuffer;
    size_t m_writeIndex;
    mutable std::mutex m_mutex;

    ma_context m_context;
    bool m_contextInitialized;
    ma_device m_device;
    bool m_deviceInitialized;
    bool m_running;
    CaptureSource m_source;
};