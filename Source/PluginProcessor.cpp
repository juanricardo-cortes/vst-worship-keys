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
    add ("pianoLevel", "Piano Level", Range (-60, 6, 0.1f), 0, "dB");
    add ("pianoRelease", "Piano Release", skewed (0.05f, 3.0f, 0.5f), 0.4f, "s");
    add ("velCurve", "Touch", Range (0.4f, 2.0f), 1.0f);
    add ("epLevel", "EP Level", Range (-60, 6, 0.1f), -60, "dB");
    add ("epBell", "EP Bell", Range (0, 1), 0.5f);
    add ("epBark", "EP Bark", Range (0, 1), 0.4f);
    add ("epDecay", "EP Decay", Range (0.3f, 2.5f), 1.0f);
    add ("padLevel", "Pad Level", Range (-60, 6, 0.1f), -12, "dB");
    add ("padAttack", "Pad Attack", skewed (0.005f, 6.0f, 1.0f), 1.5f, "s");
    add ("padRelease", "Pad Release", skewed (0.05f, 10.0f, 2.5f), 4.0f, "s");
    add ("padCutoff", "Pad Cutoff", skewed (200, 12000, 2000), 2500, "Hz");
    add ("padReso", "Pad Resonance", Range (0, 1), 0.15f);
    add ("padDetune", "Pad Detune", Range (0, 1), 0.4f);
    add ("padMotion", "Pad Motion", Range (0, 1), 0.25f);
    p.push_back (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "padOctave", 1 }, "Pad Octave", -1, 1, 0));

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
    layerBuf.setSize (6, juce::jmax (samplesPerBlock, 4096));
    for (auto* g : { &pianoGain, &epGain, &padGain })
        g->reset (sr, 0.03);
    keyboardState.reset();
}

void WorshipKeysProcessor::readParams (EngineParams& e, fx::Params& f)
{
    e.velCurve = param ("velCurve");
    e.pianoRelease = param ("pianoRelease");
    e.epBell = param ("epBell");
    e.epBark = param ("epBark");
    e.epDecay = param ("epDecay");
    e.padAttack = param ("padAttack");
    e.padRelease = param ("padRelease");
    e.padCutoff = param ("padCutoff");
    e.padReso = param ("padReso");
    e.padDetune = param ("padDetune");
    e.padOctave = (int) std::lround (param ("padOctave"));
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
    padGain.setTargetValue (levelToGain (param ("padLevel")));
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
}

void WorshipKeysProcessor::renderVoices (int start, int end, const EngineParams& e, float padCutoffMod)
{
    if (end <= start) return;
    auto n = end - start;
    engine.renderPiano (layerBuf.getWritePointer (0, start), layerBuf.getWritePointer (1, start), n);
    engine.renderEP (layerBuf.getWritePointer (2, start), layerBuf.getWritePointer (3, start), n);
    engine.renderPad (layerBuf.getWritePointer (4, start), layerBuf.getWritePointer (5, start), n, e, padCutoffMod);
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
            auto gp = pianoGain.getNextValue(), ge = epGain.getNextValue(), gq = padGain.getNextValue();
            L[i] = layerBuf.getSample (0, i) * gp + layerBuf.getSample (2, i) * ge + layerBuf.getSample (4, i) * gq;
            R[i] = layerBuf.getSample (1, i) * gp + layerBuf.getSample (3, i) * ge + layerBuf.getSample (5, i) * gq;
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
            apvts.replaceState (state);
        }
}

juce::AudioProcessorEditor* WorshipKeysProcessor::createEditor() { return new WorshipKeysEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WorshipKeysProcessor(); }
