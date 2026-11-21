#include "SampleBank.h"
#include "AndroidAssets.h"
#include <chrono>
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

    // Optional "level.txt": dB to add on playback (pianos stored lower to keep headroom in 16-bit).
    if (auto levelFile = folder.getChildFile ("level.txt"); levelFile.existsAsFile())
        playbackGain = juce::Decibels::decibelsToGain (levelFile.loadFileAsString().trim().getFloatValue());

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

const char* const SharedSampleBank::pianoNames[numPianos] = { "Classic Grand", "Concert Grand", "Clear Grand", "Session Grand" };
const char* const SharedSampleBank::pianoFolders[numPianos] = { "", "Piano-ConcertD", "Piano-Clear", "Piano-StudioC3" };

SharedSampleBank::SharedSampleBank()
{
    wanted[0] = true;
    setStatus ("Loading piano samples...");
    loader = std::thread ([this]
    {
       #if JUCE_ANDROID
        unpackAndroidAssets ("WorshipKeys/Samples", [this] (const juce::String& s) { setStatus ("Piano: " + s); });
        setStatus ("Loading piano samples...");
       #endif
        if (! loadPiano (0))
        {
            if (abort.load())
                return;
           #if JUCE_IOS || JUCE_ANDROID
            setStatus ("Piano samples missing from the app - reinstall it");
           #else
            setStatus ("Piano samples not found - copy them to C:\\ProgramData\\WorshipKeys\\Samples");
           #endif
            return;
        }
        updateStatus();

        // The other pianos load when a patch first asks for one.
        while (! abort.load())
        {
            for (int p = 1; p < numPianos && ! abort.load(); ++p)
                if (wanted[(size_t) p].load() && ! ready[(size_t) p].load() && ! failed[(size_t) p].load())
                {
                    setStatus ("Loading " + juce::String (pianoNames[p]) + "...");
                    if (! loadPiano (p))
                        failed[(size_t) p] = true;
                    updateStatus();
                }
            std::this_thread::sleep_for (std::chrono::milliseconds (40));
        }
    });
}

bool SharedSampleBank::loadPiano (int piano)
{
    if (piano == 0)
    {
        for (auto& dir : getSearchFolders())
        {
            if (! dir.isDirectory())
                continue;
            if (banks[0].loadFromFolder (dir, abort, progress))
            {
                sampleRoot = dir;
                ready[0].store (true, std::memory_order_release);
                return true;
            }
            if (abort.load())
                return false;
        }
        return false;
    }

    // Next to the original grand first, then anywhere else the search looks.
    juce::Array<juce::File> dirs { sampleRoot };
    dirs.addArray (getSearchFolders());
    for (auto& dir : dirs)
    {
        auto folder = dir.getChildFile (pianoFolders[piano]);
        if (folder.isDirectory() && banks[(size_t) piano].loadFromFolder (folder, abort, progress))
        {
            ready[(size_t) piano].store (true, std::memory_order_release);
            return true;
        }
        if (abort.load())
            return false;
    }
    return false;
}

void SharedSampleBank::updateStatus()
{
    size_t bytes = 0;
    juce::StringArray loaded;
    for (int p = 0; p < numPianos; ++p)
        if (ready[(size_t) p].load())
        {
            bytes += banks[(size_t) p].getMemoryBytes();
            loaded.add (p == 0 ? juce::String (banks[0].getNumLayers()) + " velocity layers" : juce::String (pianoNames[p]));
        }
    for (int p = 1; p < numPianos; ++p)
        if (failed[(size_t) p].load())
            loaded.add (juce::String (pianoNames[p]) + " missing");
    setStatus ("Piano: " + loaded.joinIntoString (" + ") + ", " + juce::String (juce::roundToInt ((double) bytes / (1024.0 * 1024.0))) + " MB");
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
