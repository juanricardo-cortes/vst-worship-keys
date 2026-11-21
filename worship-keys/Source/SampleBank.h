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
    float getPlaybackGain() const noexcept { return playbackGain; } // from level.txt: makes up level the files store lower
    size_t getMemoryBytes() const noexcept;

private:
    std::vector<std::unique_ptr<SampleZone>> zones;
    std::array<std::array<const SampleZone*, maxLayers>, 128> map {};
    int numLayers = 0;
    float playbackGain = 1.0f;
};

// The pianos, loaded once per process and shared by every plugin instance (via juce::SharedResourcePointer).
// Piano 0 (the original grand) sits in Samples/ itself and loads at startup. The others live in Samples/<folder>/
// and load in the background the first time a patch asks for them, so they only cost RAM when used.
class SharedSampleBank
{
public:
    static constexpr int numPianos = 4;
    static const char* const pianoNames[numPianos];
    static const char* const pianoFolders[numPianos]; // "" = Samples/ itself

    SharedSampleBank();
    ~SharedSampleBank();

    // Null until that piano has loaded. request() is safe to call from the audio thread.
    const SampleBank* get (int piano = 0) const noexcept
    {
        piano = juce::jlimit (0, numPianos - 1, piano);
        return ready[(size_t) piano].load (std::memory_order_acquire) ? &banks[(size_t) piano] : nullptr;
    }
    void request (int piano) noexcept { wanted[(size_t) juce::jlimit (0, numPianos - 1, piano)].store (true); }
    bool isLoaded (int piano) const noexcept { return get (piano) != nullptr; }
    juce::String getStatus() const;
    float getProgress() const noexcept { return progress.load(); }

    static juce::Array<juce::File> getSearchFolders();

private:
    void setStatus (const juce::String&);
    bool loadPiano (int piano);
    void updateStatus();

    std::array<SampleBank, numPianos> banks;
    std::array<std::atomic<bool>, numPianos> ready {}, wanted {}, failed {};
    std::atomic<bool> abort { false };
    std::atomic<float> progress { 0.0f };
    juce::File sampleRoot; // the Samples/ folder piano 0 came from
    mutable std::mutex statusLock;
    juce::String status;
    std::thread loader;
};
