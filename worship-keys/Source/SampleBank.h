#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

// One recorded note at one velocity layer, stored as interleaved 16-bit stereo to halve RAM.
struct SampleZone
{
    int rootNote = 60;
    float tuneCents = 0.0f; // correction applied on playback
    int layer = 0; // 0 = softest
    double sampleRate = 44100.0;
    int numFrames = 0;
    std::vector<int16_t> data;
};

// Velocity-layered multisample. Files are named "<midi>_v<layer>.flac" (see Tools/prepare_samples.sh).
class SampleBank
{
public:
    static constexpr int maxLayers = 16;

    bool loadFromFolder (const juce::File& folder, const std::atomic<bool>& shouldAbort, std::atomic<float>& progress);

    // Nearest-root zone for a note at a layer; never null once loaded.
    const SampleZone* find (int note, int layer) const noexcept { return map[(size_t) juce::jlimit (0, 127, note)][(size_t) juce::jlimit (0, numLayers - 1, layer)]; }
    int getNumLayers() const noexcept { return numLayers; }
    size_t getMemoryBytes() const noexcept;

private:
    std::vector<std::unique_ptr<SampleZone>> zones;
    std::array<std::array<const SampleZone*, maxLayers>, 128> map {};
    int numLayers = 0;
};

// Loaded once per process and shared by every plugin instance (via juce::SharedResourcePointer).
class SharedSampleBank
{
public:
    SharedSampleBank();
    ~SharedSampleBank();

    const SampleBank* get() const noexcept { return ready.load (std::memory_order_acquire) ? &bank : nullptr; }
    juce::String getStatus() const;
    float getProgress() const noexcept { return progress.load(); }

    static juce::Array<juce::File> getSearchFolders();

private:
    void setStatus (const juce::String&);

    SampleBank bank;
    std::atomic<bool> ready { false }, abort { false };
    std::atomic<float> progress { 0.0f };
    mutable std::mutex statusLock;
    juce::String status;
    std::thread loader;
};
