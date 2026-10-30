#include "SampleBank.h"
#include <map>

bool SampleBank::loadFromFolder (const juce::File& folder, const std::atomic<bool>& shouldAbort, std::atomic<float>& progress)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    auto files = folder.findChildFiles (juce::File::findFiles, false, "*.flac;*.wav");
    files.sort();

    // Optional "<midi> <cents>" per line.
    std::map<int, float> tuning;
    juce::StringArray tuneLines;
    folder.getChildFile ("tuning.txt").readLines (tuneLines);
    for (auto& line : tuneLines)
        if (line.trim().isNotEmpty())
            tuning[line.upToFirstOccurrenceOf (" ", false, false).getIntValue()] = line.fromFirstOccurrenceOf (" ", false, false).getFloatValue();

    int maxLayer = -1;
    for (int i = 0; i < files.size() && ! shouldAbort.load(); ++i)
    {
        progress = (float) i / (float) files.size();

        // "<midi>_v<layer>"
        auto name = files[i].getFileNameWithoutExtension();
        auto note = name.upToFirstOccurrenceOf ("_v", false, false).getIntValue();
        auto layer = name.fromFirstOccurrenceOf ("_v", false, false).getIntValue() - 1;
        if (! name.contains ("_v") || note < 0 || note > 127 || layer < 0 || layer >= maxLayers)
            continue;

        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (files[i]));
        if (reader == nullptr || reader->lengthInSamples <= 0)
            continue;

        auto z = std::make_unique<SampleZone>();
        z->rootNote = note;
        z->layer = layer;
        if (auto t = tuning.find (note); t != tuning.end())
            z->tuneCents = t->second;
        z->sampleRate = reader->sampleRate;
        z->numFrames = (int) reader->lengthInSamples;
        z->data.resize ((size_t) z->numFrames * 2);

        juce::AudioBuffer<float> chunk (2, 32768);
        for (int start = 0; start < z->numFrames; start += chunk.getNumSamples())
        {
            auto n = juce::jmin (chunk.getNumSamples(), z->numFrames - start);
            reader->read (&chunk, 0, n, start, true, true); // mono files fill both channels
            auto* l = chunk.getReadPointer (0);
            auto* r = chunk.getReadPointer (reader->numChannels > 1 ? 1 : 0);
            for (int s = 0; s < n; ++s)
            {
                z->data[(size_t) (start + s) * 2]     = (int16_t) juce::jlimit (-32767.0f, 32767.0f, l[s] * 32767.0f);
                z->data[(size_t) (start + s) * 2 + 1] = (int16_t) juce::jlimit (-32767.0f, 32767.0f, r[s] * 32767.0f);
            }
        }

        maxLayer = juce::jmax (maxLayer, layer);
        zones.push_back (std::move (z));
    }

    if (zones.empty() || shouldAbort.load())
        return false;

    numLayers = maxLayer + 1;

    // Per note & layer, the zone with the nearest root (falls back to nearest existing layer).
    for (int note = 0; note < 128; ++note)
        for (int layer = 0; layer < numLayers; ++layer)
        {
            const SampleZone* best = nullptr;
            int bestScore = std::numeric_limits<int>::max();
            for (auto& z : zones)
            {
                auto score = std::abs (z->rootNote - note) * 100 + std::abs (z->layer - layer);
                if (score < bestScore) { bestScore = score; best = z.get(); }
            }
            map[(size_t) note][(size_t) layer] = best;
        }

    progress = 1.0f;
    return true;
}

size_t SampleBank::getMemoryBytes() const noexcept
{
    size_t total = 0;
    for (auto& z : zones)
        total += z->data.size() * sizeof (int16_t);
    return total;
}

//==============================================================================
juce::Array<juce::File> SharedSampleBank::getSearchFolders()
{
    juce::Array<juce::File> dirs;
    dirs.add (juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory).getChildFile ("WorshipKeys/Samples"));

    // Next to the plugin / exe, walking up out of a .vst3 bundle.
    auto dir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
    for (int i = 0; i < 5 && dir.exists(); ++i, dir = dir.getParentDirectory())
        dirs.add (dir.getChildFile ("Samples"));

   #ifdef WK_DEV_SAMPLE_DIR
    dirs.add (juce::File (WK_DEV_SAMPLE_DIR));
   #endif
    return dirs;
}

SharedSampleBank::SharedSampleBank()
{
    setStatus ("Loading piano samples...");
    loader = std::thread ([this]
    {
        for (auto& dir : getSearchFolders())
        {
            if (! dir.isDirectory())
                continue;
            if (bank.loadFromFolder (dir, abort, progress))
            {
                setStatus ("Piano: " + juce::String (bank.getNumLayers()) + " velocity layers, "
                           + juce::String (juce::roundToInt ((double) bank.getMemoryBytes() / (1024.0 * 1024.0))) + " MB");
                ready.store (true, std::memory_order_release);
                return;
            }
            if (abort.load())
                return;
        }
       #if JUCE_IOS || JUCE_ANDROID
        setStatus ("Piano samples missing from the app - reinstall it");
       #else
        setStatus ("Piano samples not found - copy them to C:\\ProgramData\\WorshipKeys\\Samples");
       #endif
    });
}

SharedSampleBank::~SharedSampleBank()
{
    abort = true;
    if (loader.joinable())
        loader.join();
}

juce::String SharedSampleBank::getStatus() const
{
    std::lock_guard<std::mutex> l (statusLock);
    return status;
}

void SharedSampleBank::setStatus (const juce::String& s)
{
    std::lock_guard<std::mutex> l (statusLock);
    status = s;
}
