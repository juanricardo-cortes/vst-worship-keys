#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

namespace
{
using Range = juce::NormalisableRange<float>;

Range skewed (float lo, float hi, float centre)
{
    Range r (lo, hi);
    r.setSkewForCentre (centre);
    return r;
}

const juce::StringArray delayTimeNames { "1/4", "1/8 dotted", "1/8", "1/4 dotted", "1/2" };
constexpr float delayTimeBeats[] = { 1.0f, 0.75f, 0.5f, 1.5f, 2.0f };
constexpr float offDb = -59.9f; // a layer at or below this is switched off (no voices)
// The pad synth is ~10 dB hotter than the sampled piano at the same setting; trim it so equal levels sound equal.
constexpr float padTrimDb = -10.0f;

// Faders spend most of their travel in the useful -30..+6 dB zone; the bottom end is OFF.
Range levelRange()
{
    Range r (-60, 6, 0.1f);
    r.setSkewForCentre (-14);
    return r;
}

// CCs that already mean something to a keyboard rig: never learn these.
bool isLearnableCC (int cc) { return cc != 64 && cc != 66 && cc != 67 && cc < 120; }

juce::File midiMapFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("WorshipKeys").getChildFile ("midi-map.xml");
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout WorshipKeysProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto add = [&] (const char* id, const char* name, Range range, float def, const char* unit = "")
    {
        p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, def,
                                                                  juce::AudioParameterFloatAttributes().withLabel (unit)));
    };

    // Layers
    add ("pianoLevel", "Piano Level", levelRange(), 0, "dB");
    add ("pianoTone", "Piano Tone", Range (0, 1), 0.45f);
    add ("pianoRelease", "Piano Release", skewed (0.05f, 3.0f, 0.5f), 0.4f, "s");
    add ("velCurve", "Touch", Range (0.4f, 2.0f), 1.0f);
    add ("epLevel", "EP Level", levelRange(), -60, "dB");
    add ("epBell", "EP Bell", Range (0, 1), 0.5f);
    add ("epBark", "EP Bark", Range (0, 1), 0.4f);
    add ("epDecay", "EP Decay", Range (0.3f, 2.5f), 1.0f);
    add ("epTrem", "EP Tremolo", Range (0, 1), 0.0f);
    add ("epTremRate", "EP Tremolo Rate", skewed (0.5f, 10.0f, 4.0f), 4.5f, "Hz");
    add ("padLevel", "Pad Level", levelRange(), -12, "dB");
    add ("padAttack", "Pad Attack", skewed (0.005f, 6.0f, 1.0f), 1.5f, "s");
    add ("padRelease", "Pad Release", skewed (0.05f, 10.0f, 2.5f), 4.0f, "s");
    add ("padCutoff", "Pad Cutoff", skewed (200, 12000, 2000), 2500, "Hz");
    add ("padReso", "Pad Resonance", Range (0, 1), 0.15f);
    add ("padDetune", "Pad Detune", Range (0, 1), 0.4f);
    add ("padMotion", "Pad Motion", Range (0, 1), 0.25f);
    p.push_back (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "padOctave", 1 }, "Pad Octave", -1, 1, 0));
    add ("bellLevel", "Bells Level", levelRange(), -60, "dB");
    add ("bellTone", "Bells Tone", Range (0, 1), 0.5f);
    add ("bellDecay", "Bells Decay", skewed (0.4f, 8.0f, 2.0f), 2.5f, "s");
    p.push_back (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "bellOctave", 1 }, "Bells Octave", 0, 2, 1));

    // EQ / dynamics
    add ("hpf", "Low Cut", skewed (20, 400, 80), 60, "Hz");
    add ("eqLow", "Low (100 Hz)", Range (-12, 12, 0.1f), 0, "dB");
    add ("eqLowMid", "Low-Mid (300 Hz)", Range (-12, 12, 0.1f), 0, "dB");
    add ("eqPresence", "Presence (4 kHz)", Range (-12, 12, 0.1f), 0, "dB");
    add ("eqAir", "Air (10 kHz)", Range (-12, 12, 0.1f), 0, "dB");
    add ("comp", "Compression", Range (0, 1), 0.3f);
    add ("drive", "Drive", Range (0, 1), 0.1f);

    // Space
    add ("chorusMix", "Chorus", Range (0, 1), 0.2f);
    add ("chorusRate", "Chorus Rate", skewed (0.05f, 3.0f, 0.5f), 0.3f, "Hz");
    add ("delayMix", "Delay", Range (0, 1), 0.0f);
    add ("delayFeedback", "Delay Feedback", Range (0, 0.9f), 0.3f);
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "delayTime", 1 }, "Delay Time", delayTimeNames, 1));
    add ("revMix", "Reverb", Range (0, 1), 0.3f);
    add ("revDecay", "Reverb Decay", skewed (0.5f, 15.0f, 3.0f), 3.0f, "s");
    add ("revSize", "Reverb Size", Range (0, 1), 0.7f);
    add ("revPreDelay", "Pre-Delay", Range (0, 250, 1), 30, "ms");
    add ("revTone", "Reverb Tone", skewed (1000, 12000, 5000), 6000, "Hz");
    add ("shimmer", "Shimmer", Range (0, 1), 0.0f);
    add ("master", "Master", Range (-24, 6, 0.1f), 0, "dB");

    return { p.begin(), p.end() };
}

WorshipKeysProcessor::WorshipKeysProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "WorshipKeys", createLayout())
{
    paramIds.reserve ((size_t) getParameters().size());
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            paramIds.push_back (ranged->getParameterID().toStdString());
            params[paramIds.back()] = apvts.getRawParameterValue (ranged->getParameterID());
        }
    for (auto& m : ccMap) m = -1;
    loadMidiMap();
    setCurrentProgram (0);
}

bool WorshipKeysProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void WorshipKeysProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    engine.prepare (sr);
    chain.prepare (sr, juce::jmax (samplesPerBlock, 512));
    layerBuf.setSize (8, juce::jmax (samplesPerBlock, 4096));
    for (auto* g : { &pianoGain, &epGain, &padGain, &bellGain })
        g->reset (sr, 0.03);
    keyboardState.reset();
}

void WorshipKeysProcessor::readParams (EngineParams& e, fx::Params& f)
{
    e.velCurve = param ("velCurve");
    e.pianoRelease = param ("pianoRelease");
    e.pianoTone = param ("pianoTone");
    e.epBell = param ("epBell");
    e.epBark = param ("epBark");
    e.epDecay = param ("epDecay");
    e.padAttack = param ("padAttack");
    e.padRelease = param ("padRelease");
    e.padCutoff = param ("padCutoff");
    e.padReso = param ("padReso");
    e.padDetune = param ("padDetune");
    e.padOctave = (int) std::lround (param ("padOctave"));
    e.bellTone = param ("bellTone");
    e.bellDecay = param ("bellDecay");
    e.bellOctave = (int) std::lround (param ("bellOctave"));
    e.bellOn = param ("bellLevel") > offDb;
    e.pianoOn = param ("pianoLevel") > offDb;
    e.epOn = param ("epLevel") > offDb;
    e.padOn = param ("padLevel") > offDb;

    f.hpf = param ("hpf");
    f.low = param ("eqLow");
    f.lowMid = param ("eqLowMid");
    f.presence = param ("eqPresence");
    f.air = param ("eqAir");
    f.comp = param ("comp");
    f.drive = param ("drive");
    f.chorusMix = param ("chorusMix");
    f.chorusRate = param ("chorusRate");
    f.delayMix = param ("delayMix");
    f.delayFeedback = param ("delayFeedback");
    f.revMix = param ("revMix");
    f.revDecay = param ("revDecay");
    f.revSize = param ("revSize");
    f.revPreDelayMs = param ("revPreDelay");
    f.revToneHz = param ("revTone");
    f.shimmer = param ("shimmer");
    f.masterDb = param ("master");

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                bpm = juce::jlimit (30.0, 300.0, *b);
    auto beats = delayTimeBeats[juce::jlimit (0, 4, (int) std::lround (param ("delayTime")))];
    f.delaySamples = (float) (60.0 / bpm * beats * sr);

    auto levelToGain = [] (float db) { return db <= offDb ? 0.0f : juce::Decibels::decibelsToGain (db); };
    pianoGain.setTargetValue (levelToGain (param ("pianoLevel")));
    epGain.setTargetValue (levelToGain (param ("epLevel")));
    padGain.setTargetValue (levelToGain (param ("padLevel")) * juce::Decibels::decibelsToGain (padTrimDb));
    bellGain.setTargetValue (levelToGain (param ("bellLevel")));
}

void WorshipKeysProcessor::handleMidi (const juce::MidiMessage& m, const SampleBank* bank, const EngineParams& e)
{
    if (m.isNoteOn())
        engine.noteOn (bank, m.getNoteNumber(), m.getFloatVelocity(), e);
    else if (m.isNoteOff())
        engine.noteOff (m.getNoteNumber(), e);
    else if (m.isSustainPedalOn())
        engine.setSustain (true, e);
    else if (m.isSustainPedalOff())
        engine.setSustain (false, e);
    else if (m.isAllNotesOff() || m.isAllSoundOff())
        engine.allNotesOff();
    else if (m.isController())
        handleCC (m.getControllerNumber(), m.getControllerValue());
}

void WorshipKeysProcessor::handleCC (int cc, int value)
{
    if (! isLearnableCC (cc))
        return;

    auto learning = learnParam.load();
    if (learning >= 0)
    {
        for (auto& m : ccMap) // one slider per parameter
            if (m.load() == learning) m = -1;
        ccMap[(size_t) cc] = learning;
        learnParam = -1;
        midiMapChanged = true;
    }

    auto index = ccMap[(size_t) cc].load();
    if (juce::isPositiveAndBelow (index, getParameters().size()))
        getParameters()[index]->setValueNotifyingHost ((float) value / 127.0f);
}

int WorshipKeysProcessor::paramIndex (const juce::String& id) const
{
    if (auto* p = apvts.getParameter (id))
        return p->getParameterIndex();
    return -1;
}

int WorshipKeysProcessor::getMappedCC (const juce::String& id) const
{
    auto index = paramIndex (id);
    for (size_t cc = 0; cc < ccMap.size(); ++cc)
        if (index >= 0 && ccMap[cc].load() == index)
            return (int) cc;
    return -1;
}

void WorshipKeysProcessor::startMidiLearn (const juce::String& id) { learnParam = paramIndex (id); }
void WorshipKeysProcessor::cancelMidiLearn() { learnParam = -1; }

bool WorshipKeysProcessor::isLearning (const juce::String& id) const
{
    auto index = learnParam.load();
    return index >= 0 && index == paramIndex (id);
}

void WorshipKeysProcessor::clearMidiMapping (const juce::String& id)
{
    auto index = paramIndex (id);
    for (auto& m : ccMap)
        if (index >= 0 && m.load() == index) m = -1;
    saveMidiMap();
}

void WorshipKeysProcessor::clearAllMidiMappings()
{
    for (auto& m : ccMap) m = -1;
    saveMidiMap();
}

bool WorshipKeysProcessor::saveMidiMapIfChanged()
{
    if (! midiMapChanged.exchange (false))
        return false;
    saveMidiMap();
    return true;
}

// The map lives in one file shared by every project and instance: set up your sliders once.
void WorshipKeysProcessor::saveMidiMap()
{
    juce::XmlElement xml ("MidiMap");
    for (size_t cc = 0; cc < ccMap.size(); ++cc)
        if (auto index = ccMap[cc].load(); juce::isPositiveAndBelow (index, getParameters().size()))
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (getParameters()[index]))
            {
                auto* e = xml.createNewChildElement ("Map");
                e->setAttribute ("cc", (int) cc);
                e->setAttribute ("param", ranged->getParameterID());
            }
    auto file = midiMapFile();
    file.getParentDirectory().createDirectory();
    xml.writeTo (file);
}

void WorshipKeysProcessor::loadMidiMap()
{
    if (auto xml = juce::XmlDocument::parse (midiMapFile()))
        for (auto* e : xml->getChildWithTagNameIterator ("Map"))
        {
            auto cc = e->getIntAttribute ("cc", -1);
            auto index = paramIndex (e->getStringAttribute ("param"));
            if (juce::isPositiveAndBelow (cc, 128) && index >= 0)
                ccMap[(size_t) cc] = index;
        }
}

void WorshipKeysProcessor::renderVoices (int start, int end, const EngineParams& e, float padCutoffMod)
{
    if (end <= start) return;
    auto n = end - start;
    engine.renderPiano (layerBuf.getWritePointer (0, start), layerBuf.getWritePointer (1, start), n, e);
    engine.renderEP (layerBuf.getWritePointer (2, start), layerBuf.getWritePointer (3, start), n);
    engine.renderPad (layerBuf.getWritePointer (4, start), layerBuf.getWritePointer (5, start), n, e, padCutoffMod);
    engine.renderBell (layerBuf.getWritePointer (6, start), layerBuf.getWritePointer (7, start), n);
}

void WorshipKeysProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    auto total = buffer.getNumSamples();
    keyboardState.processNextMidiBuffer (midi, 0, total, true);
    buffer.clear();
    if (buffer.getNumChannels() < 2)
        return;

    EngineParams e;
    fx::Params f;
    readParams (e, f);
    auto* bank = sharedBank->get();

    // Slow filter sweep on the pad ("Motion"), ~8 s cycle.
    lfoPhase += total * 0.12 / sr;
    lfoPhase -= std::floor (lfoPhase);
    tremDepth = param ("epTrem");
    tremInc = param ("epTremRate") / sr;

    auto padCutoffMod = std::pow (2.0f, param ("padMotion") * 1.2f * (float) std::sin (juce::MathConstants<double>::twoPi * lfoPhase));

    // Hosts may send blocks larger than announced: work in chunks the scratch buffer fits.
    auto midiIt = midi.cbegin();
    for (int chunkStart = 0; chunkStart < total; chunkStart += layerBuf.getNumSamples())
    {
        auto chunkEnd = juce::jmin (total, chunkStart + layerBuf.getNumSamples());
        auto n = chunkEnd - chunkStart;
        layerBuf.clear (0, n);

        int pos = 0;
        for (; midiIt != midi.cend() && (*midiIt).samplePosition < chunkEnd; ++midiIt)
        {
            auto t = juce::jlimit (0, n, (*midiIt).samplePosition - chunkStart);
            renderVoices (pos, t, e, padCutoffMod);
            handleMidi ((*midiIt).getMessage(), bank, e);
            pos = t;
        }
        renderVoices (pos, n, e, padCutoffMod);

        auto* L = buffer.getWritePointer (0, chunkStart);
        auto* R = buffer.getWritePointer (1, chunkStart);
        for (int i = 0; i < n; ++i)
        {
            auto gp = pianoGain.getNextValue(), ge = epGain.getNextValue(), gq = padGain.getNextValue(), gb = bellGain.getNextValue();
            // Suitcase-style stereo tremolo on the e.piano: the sound swings left and right.
            auto sw = (float) std::sin (juce::MathConstants<double>::twoPi * tremPhase);
            tremPhase += tremInc;
            if (tremPhase >= 1.0) tremPhase -= 1.0;
            auto geL = ge * (1.0f - tremDepth * (0.5f + 0.5f * sw)), geR = ge * (1.0f - tremDepth * (0.5f - 0.5f * sw));
            L[i] = layerBuf.getSample (0, i) * gp + layerBuf.getSample (2, i) * geL + layerBuf.getSample (4, i) * gq + layerBuf.getSample (6, i) * gb;
            R[i] = layerBuf.getSample (1, i) * gp + layerBuf.getSample (3, i) * geR + layerBuf.getSample (5, i) * gq + layerBuf.getSample (7, i) * gb;
        }
    }

    chain.process (buffer, total, f);
    grDb = chain.getGainReductionDb();
}

//==============================================================================
int WorshipKeysProcessor::getNumPrograms() { return (int) getPresets().size(); }

const juce::String WorshipKeysProcessor::getProgramName (int index)
{
    auto& presets = getPresets();
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? presets[(size_t) index].name : "";
}

void WorshipKeysProcessor::setCurrentProgram (int index)
{
    auto& presets = getPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;
    currentProgram = index;

    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            ranged->setValueNotifyingHost (ranged->getDefaultValue());

    for (auto& [id, value] : presets[(size_t) index].values)
        if (auto* ranged = apvts.getParameter (id))
            ranged->setValueNotifyingHost (ranged->convertTo0to1 (value));
}

void WorshipKeysProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("easyView", easyView, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void WorshipKeysProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentProgram = state.getProperty ("program", 0);
            easyView = state.getProperty ("easyView", true);
            apvts.replaceState (state);
        }
}

juce::AudioProcessorEditor* WorshipKeysProcessor::createEditor() { return new WorshipKeysEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WorshipKeysProcessor(); }
