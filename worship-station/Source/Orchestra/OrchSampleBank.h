#pragma once
#include "../Packs.h"

namespace orch
{
// The orchestra's instrument sets. Folder names under Samples/Orchestra (see Tools/prepare_orchestra.py).
enum Layer
{
    vlnStac, vlaStac, vcStac, cbStac, vlnSus, vlaSus, vcSus, cbSus,
    tptStac, hrnStac, tbnStac, tubStac, tptSus, hrnSus, tbnSus, tubSus,
    timp, crash, boom,
    numLayers
};

inline const char* layerFolder (int l)
{
    static const char* names[numLayers] = { "VlnStac", "VlaStac", "VcStac", "CbStac", "VlnSus", "VlaSus", "VcSus", "CbSus",
                                            "TptStac", "HrnStac", "TbnStac", "TubStac", "TptSus", "HrnSus", "TbnSus", "TubSus",
                                            "Timp", "Crash", "Boom" };
    return names[l];
}

// One recording: interleaved 16-bit stereo plus its SFZ-style key / velocity / round-robin slot.
struct Region
{
    int root = 60, loKey = 0, hiKey = 127, loVel = 0, hiVel = 127, rr = 1;
    float gain = 1.0f; // undoes the prep normalisation, restoring the library's section balance
    double sampleRate = 44100.0;
    int numFrames = 0;
    int loopStart = 0, loopEnd = 0, loopFade = 0; // sustains loop on their steady part (frames); loopEnd 0 = one-shot
    std::vector<int16_t> data;

    void findLoop(); // picks loop points from the amplitude envelope
};

inline bool isSustainLayer (int l) { return l == vlnSus || l == vlaSus || l == vcSus || l == cbSus || l == tptSus || l == hrnSus || l == tbnSus || l == tubSus; }

struct LayerSet
{
    std::vector<std::unique_ptr<Region>> regions;
    int loKey = 127, hiKey = 0, numRR = 1;

    // Best region for a note / velocity / round robin. Notes outside the mapped range use the nearest
    // region (the voice transposes it). Allocation-free: safe on the audio thread.
    const Region* find (int note, int vel, int rr) const noexcept;
};

class OrchSampleBank
{
public:
    bool loadFromFolder (const juce::File& folder, const std::atomic<bool>& shouldAbort, std::atomic<float>& progress);
    const LayerSet& layer (int l) const noexcept { return layers[(size_t) l]; }
    size_t getMemoryBytes() const noexcept;

private:
    std::array<LayerSet, numLayers> layers;
};

// Loaded once per process in the background and shared by every instance.
class SharedOrchBank
{
public:
    SharedOrchBank();
    ~SharedOrchBank();

    const OrchSampleBank* get() const noexcept { return ready.load (std::memory_order_acquire) ? &bank : nullptr; }
    juce::String getStatus() const;
    float getProgress() const noexcept { return progress.load(); }
    static juce::Array<juce::File> getSearchFolders();

private:
    void setStatus (const juce::String&);

    OrchSampleBank bank;
    std::atomic<bool> ready { false }, abort { false };
    std::atomic<float> progress { 0.0f };
    mutable std::mutex statusLock;
    juce::String status;
    std::thread loader;
};
} // namespace orch
