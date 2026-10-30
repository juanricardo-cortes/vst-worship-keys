#include "OrchSampleBank.h"

namespace orch
{
// Held chords in worship last far longer than the recordings: loop each sustain over its steady part
// (after the attack, before the level sags or the trim fade starts), with a crossfade at the seam.
void Region::findLoop()
{
    constexpr int win = 2048;
    auto numWins = numFrames / win;
    if (numWins < 20)
        return;

    std::vector<float> rms ((size_t) numWins);
    for (int w = 0; w < numWins; ++w)
    {
        double sum = 0;
        for (int i = w * win; i < (w + 1) * win; ++i)
        {
            auto v = 0.5 * (data[(size_t) i * 2] + data[(size_t) i * 2 + 1]);
            sum += v * v;
        }
        rms[(size_t) w] = (float) std::sqrt (sum / win);
    }
    auto peak = *std::max_element (rms.begin(), rms.end());
    if (peak <= 0)
        return;

    int attackEnd = 0;
    while (attackEnd < numWins && rms[(size_t) attackEnd] < 0.7f * peak) ++attackEnd;
    auto start = attackEnd + (int) (0.35 * sampleRate / win);

    // Steady level = median after the attack; the loop ends where the note falls well below it.
    std::vector<float> tail (rms.begin() + juce::jmin (start, numWins - 1), rms.end());
    std::nth_element (tail.begin(), tail.begin() + (long) tail.size() / 2, tail.end());
    auto steady = tail[tail.size() / 2];
    int end = start;
    for (int w = start; w < numWins; ++w)
        if (rms[(size_t) w] >= 0.6f * steady) end = w;
    end -= (int) (0.15 * sampleRate / win);

    auto len = (end - start) * win;
    if (len < (int) (0.8 * sampleRate))
        return;
    loopStart = start * win;
    loopEnd = end * win;
    loopFade = juce::jmin ((int) (0.25 * sampleRate), len / 3, loopStart);
}

const Region* LayerSet::find (int note, int vel, int rr) const noexcept
{
    const Region* best = nullptr;
    int bestScore = std::numeric_limits<int>::max();
    for (auto& r : regions)
    {
        auto keyDist = note < r->loKey ? r->loKey - note : note > r->hiKey ? note - r->hiKey : 0;
        auto velDist = vel < r->loVel ? r->loVel - vel : vel > r->hiVel ? vel - r->hiVel : 0;
        auto score = keyDist * 100000 + velDist * 10 + (r->rr == rr ? 0 : 1);
        if (score < bestScore) { bestScore = score; best = r.get(); }
    }
    return best;
}

bool OrchSampleBank::loadFromFolder (const juce::File& folder, const std::atomic<bool>& shouldAbort, std::atomic<float>& progress)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    int total = 0, done = 0;
    std::array<juce::StringArray, numLayers> maps;
    for (int l = 0; l < numLayers; ++l)
    {
        folder.getChildFile (layerFolder (l)).getChildFile ("map.txt").readLines (maps[(size_t) l]);
        maps[(size_t) l].removeEmptyStrings();
        total += maps[(size_t) l].size();
    }
    if (total == 0)
        return false;

    for (int l = 0; l < numLayers && ! shouldAbort.load(); ++l)
    {
        auto& set = layers[(size_t) l];
        auto dir = folder.getChildFile (layerFolder (l));
        for (auto& line : maps[(size_t) l])
        {
            progress = (float) done++ / (float) total;
            // <file> <root> <lokey> <hikey> <lovel> <hivel> <rr> <gainDb>
            auto t = juce::StringArray::fromTokens (line, " ", "");
            if (t.size() < 8)
                continue;
            std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (dir.getChildFile (t[0])));
            if (reader == nullptr || reader->lengthInSamples <= 0)
                continue;

            auto r = std::make_unique<Region>();
            r->root = t[1].getIntValue();
            r->loKey = t[2].getIntValue();
            r->hiKey = t[3].getIntValue();
            r->loVel = t[4].getIntValue();
            r->hiVel = t[5].getIntValue();
            r->rr = juce::jmax (1, t[6].getIntValue());
            r->gain = juce::Decibels::decibelsToGain (t[7].getFloatValue());
            r->sampleRate = reader->sampleRate;
            r->numFrames = (int) reader->lengthInSamples;
            r->data.resize ((size_t) r->numFrames * 2);

            juce::AudioBuffer<float> chunk (2, 32768);
            for (int start = 0; start < r->numFrames; start += chunk.getNumSamples())
            {
                auto n = juce::jmin (chunk.getNumSamples(), r->numFrames - start);
                reader->read (&chunk, 0, n, start, true, true);
                auto* L = chunk.getReadPointer (0);
                auto* R = chunk.getReadPointer (reader->numChannels > 1 ? 1 : 0);
                for (int s = 0; s < n; ++s)
                {
                    r->data[(size_t) (start + s) * 2]     = (int16_t) juce::jlimit (-32767.0f, 32767.0f, L[s] * 32767.0f);
                    r->data[(size_t) (start + s) * 2 + 1] = (int16_t) juce::jlimit (-32767.0f, 32767.0f, R[s] * 32767.0f);
                }
            }

            if (isSustainLayer (l))
                r->findLoop();

            set.loKey = juce::jmin (set.loKey, r->loKey);
            set.hiKey = juce::jmax (set.hiKey, r->hiKey);
            set.numRR = juce::jmax (set.numRR, r->rr);
            set.regions.push_back (std::move (r));
        }
    }

    if (shouldAbort.load())
        return false;
    progress = 1.0f;
    return true;
}

size_t OrchSampleBank::getMemoryBytes() const noexcept
{
    size_t bytes = 0;
    for (auto& set : layers)
        for (auto& r : set.regions)
            bytes += r->data.size() * sizeof (int16_t);
    return bytes;
}

//==============================================================================
juce::Array<juce::File> SharedOrchBank::getSearchFolders()
{
    juce::Array<juce::File> dirs;
    dirs.add (juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory).getChildFile ("WorshipStation/Samples/Orchestra"));

    // Next to the plugin / exe, walking up out of a .vst3 bundle (portable copies).
    auto dir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
    for (int i = 0; i < 5 && dir.exists(); ++i, dir = dir.getParentDirectory())
        dirs.add (dir.getChildFile ("Samples/Orchestra"));

   #ifdef WS_DEV_SAMPLE_DIR
    dirs.add (juce::File (WS_DEV_SAMPLE_DIR).getChildFile ("Orchestra"));
   #endif
    return dirs;
}

SharedOrchBank::SharedOrchBank()
{
    setStatus ("Loading orchestra samples...");
    loader = std::thread ([this]
    {
        for (auto& dir : getSearchFolders())
        {
            if (! dir.isDirectory())
                continue;
            if (bank.loadFromFolder (dir, abort, progress))
            {
                setStatus ("Orchestra: " + juce::String (juce::roundToInt ((double) bank.getMemoryBytes() / (1024.0 * 1024.0))) + " MB loaded");
                ready.store (true, std::memory_order_release);
                return;
            }
            if (abort.load())
                return;
        }
        setStatus ("Orchestra samples not found - copy them to C:\\ProgramData\\WorshipStation\\Samples\\Orchestra");
    });
}

SharedOrchBank::~SharedOrchBank()
{
    abort = true;
    if (loader.joinable())
        loader.join();
}

juce::String SharedOrchBank::getStatus() const
{
    std::lock_guard<std::mutex> l (statusLock);
    return status;
}

void SharedOrchBank::setStatus (const juce::String& s)
{
    std::lock_guard<std::mutex> l (statusLock);
    status = s;
}
} // namespace orch
