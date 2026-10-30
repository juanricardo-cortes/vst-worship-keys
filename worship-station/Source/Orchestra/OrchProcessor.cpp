#include "OrchProcessor.h"
#include "OrchEditor.h"
#include "OrchPresets.h"

namespace orch
{
namespace
{
using Range = juce::NormalisableRange<float>;

Range skewed (float lo, float hi, float centre)
{
    Range r (lo, hi);
    r.setSkewForCentre (centre);
    return r;
}

Range levelRange()
{
    Range r (-60, 6, 0.1f);
    r.setSkewForCentre (-14);
    return r;
}

constexpr float offDb = -59.9f;
const juce::StringArray delayTimeNames { "1/4", "1/8 dot", "1/8", "1/4 dot", "1/2" };
constexpr float delayTimeBeats[] = { 1.0f, 0.75f, 0.5f, 1.5f, 2.0f };
const char* busParam[numBuses] = { "strings", "brass", "lowEnd", "timpani", "hits" };
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout OrchestraProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto add = [&] (const char* id, const char* name, Range range, float def, const char* unit = "")
    {
        juce::String u (unit);
        auto lowestIsOff = u == "dB" && range.start <= -60.0f;
        auto text = [u, lowestIsOff] (float v, int) -> juce::String
        {
            if (u == "dB")  return lowestIsOff && v <= offDb ? juce::String ("OFF") : juce::String (v, 1) + " dB";
            if (u == "s")   return v < 1.0f ? juce::String (juce::roundToInt (v * 1000.0f)) + " ms" : juce::String (v, 2) + " s";
            if (u == "Hz")  return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
            if (u == "ms")  return juce::String (juce::roundToInt (v)) + " ms";
            return juce::String (juce::roundToInt (v * 100.0f)) + "%";
        };
        p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, def,
                                                                  juce::AudioParameterFloatAttributes().withLabel (unit).withStringFromValueFunction (text)));
    };
    auto noteName = [] (int v, int) { return v <= 0 ? juce::String ("Off") : juce::MidiMessage::getMidiNoteName (v, true, true, 3); };
    auto accentName = [] (int v, int) { return v >= 128 ? juce::String ("Off") : juce::String (v); };

    // Playing
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "artic", 1 }, "Articulation",
                                                               juce::StringArray { "Stabs", "Sustain", "Stab + Hold", "Marcato" }, 2));
    add ("holdTime", "Hold Time", skewed (0.05f, 0.8f, 0.2f), 0.2f, "s");
    add ("susAttack", "Swell Attack", skewed (0.01f, 3.0f, 0.3f), 0.3f, "s");
    add ("release", "Release", skewed (0.05f, 4.0f, 0.7f), 0.6f, "s");
    add ("velSens", "Velocity", Range (0, 1), 0.6f);
    p.push_back (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "split", 1 }, "Left Hand Split", 0, 72, 55,
                                                            juce::AudioParameterIntAttributes().withStringFromValueFunction (noteName)));
    p.push_back (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "accent", 1 }, "Accent Velocity", 80, 128, 118,
                                                            juce::AudioParameterIntAttributes().withStringFromValueFunction (accentName)));

    // Sections
    add ("strings", "Strings", levelRange(), 0, "dB");
    add ("brass", "Brass", levelRange(), -2, "dB");
    add ("lowEnd", "Low End", levelRange(), -4, "dB");
    add ("timpani", "Timpani", levelRange(), -6, "dB");
    add ("hits", "Cymbal Hits", levelRange(), -8, "dB");
    add ("octUp", "Octave Up", Range (0, 1), 0.25f);
    add ("octDown", "Octave Down", Range (0, 1), 0.7f);
    add ("ensemble", "Section Size", Range (0, 1), 0.55f);

    // Mix (same chain as Worship Keys)
    add ("hpf", "Low Cut", skewed (20, 400, 80), 35, "Hz");
    add ("eqLow", "Low (100 Hz)", Range (-12, 12, 0.1f), 0, "dB");
    add ("eqLowMid", "Low-Mid (300 Hz)", Range (-12, 12, 0.1f), -1.5f, "dB");
    add ("eqPresence", "Presence (4 kHz)", Range (-12, 12, 0.1f), 0, "dB");
    add ("eqAir", "Air (10 kHz)", Range (-12, 12, 0.1f), 0, "dB");
    add ("comp", "Glue", Range (0, 1), 0.45f);
    add ("drive", "Drive", Range (0, 1), 0.1f);
    add ("delayMix", "Delay", Range (0, 1), 0.0f);
    add ("delayFeedback", "Delay Feedback", Range (0, 0.9f), 0.3f);
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "delayTime", 1 }, "Delay Time", delayTimeNames, 1));
    add ("revMix", "Hall", Range (0, 1), 0.25f);
    add ("revDecay", "Hall Decay", skewed (0.5f, 10.0f, 2.8f), 2.8f, "s");
    add ("revSize", "Hall Size", Range (0, 1), 0.8f);
    add ("revPreDelay", "Pre-Delay", Range (0, 150, 1), 20, "ms");
    add ("revTone", "Hall Tone", skewed (1000, 12000, 5000), 6000, "Hz");
    add ("master", "Master", Range (-24, 6, 0.1f), 0, "dB");

    return { p.begin(), p.end() };
}

OrchestraProcessor::OrchestraProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PraiseOrchestra", createLayout())
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

bool OrchestraProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void OrchestraProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = juce::jmax (samplesPerBlock, 512);
    engine.prepare (sr);
    chain.prepare (sr, maxBlock);
    busBuf.setSize (numBuses * 2, maxBlock);
    mixBuf.setSize (2, maxBlock);
    for (auto& g : busGain)
        g.reset (sr, 0.03);
    bend = 1.0f;
    keyboardState.reset();
}

void OrchestraProcessor::readParams (Params& e, keyspack::fx::Params& f)
{
    e.artic = (Artic) choice ("artic");
    e.holdTime = param ("holdTime");
    e.susAttack = param ("susAttack");
    e.release = param ("release");
    e.velSens = param ("velSens");
    e.split = choice ("split");
    e.accentVel = choice ("accent");
    e.octUp = param ("octUp");
    e.octDown = param ("octDown");
    e.ensemble = param ("ensemble");
    e.lowOn = param ("lowEnd") > offDb;
    e.timpOn = param ("timpani") > offDb;
    e.hitsOn = param ("hits") > offDb;

    f.hpf = param ("hpf");
    f.low = param ("eqLow");
    f.lowMid = param ("eqLowMid");
    f.presence = param ("eqPresence");
    f.air = param ("eqAir");
    f.comp = param ("comp");
    f.drive = param ("drive");
    f.chorusMix = 0.0f;
    f.delayMix = param ("delayMix");
    f.delayFeedback = param ("delayFeedback");
    f.revMix = param ("revMix");
    f.revDecay = param ("revDecay");
    f.revSize = param ("revSize");
    f.revPreDelayMs = param ("revPreDelay");
    f.revToneHz = param ("revTone");
    f.shimmer = 0.0f;
    f.masterDb = param ("master");

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                bpm = juce::jlimit (30.0, 300.0, *b);
    auto beats = delayTimeBeats[juce::jlimit (0, 4, choice ("delayTime"))];
    f.delaySamples = (float) (60.0 / bpm * beats * sr);

    for (int b = 0; b < numBuses; ++b)
    {
        auto db = param (busParam[b]);
        busGain[(size_t) b].setTargetValue (db <= offDb ? 0.0f : juce::Decibels::decibelsToGain (db));
    }
}

void OrchestraProcessor::handleMidi (const juce::MidiMessage& m, const Params& e)
{
    if (m.isNoteOn())
        engine.noteOn (sharedBank->get(), m.getNoteNumber(), m.getVelocity(), e);
    else if (m.isNoteOff())
        engine.noteOff (m.getNoteNumber(), e);
    else if (m.isSustainPedalOn())
        engine.setSustain (true, e);
    else if (m.isSustainPedalOff())
        engine.setSustain (false, e);
    else if (m.isAllNotesOff() || m.isAllSoundOff())
        engine.allNotesOff();
    else if (m.isPitchWheel())
        bend = std::pow (2.0f, 2.0f * (float) (m.getPitchWheelValue() - 8192) / 8192.0f / 12.0f);
    else if (m.isControllerOfType (1))
        modWheel = (float) m.getControllerValue() / 127.0f;    // swells the sustains, like an orchestral library's dynamics
    else if (m.isControllerOfType (11))
        expression = (float) m.getControllerValue() / 127.0f;
}

void OrchestraProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    auto total = buffer.getNumSamples();
    keyboardState.processNextMidiBuffer (midi, 0, total, true);
    buffer.clear();
    if (buffer.getNumChannels() < 2 || total == 0)
        return;

    Params e;
    keyspack::fx::Params f;
    readParams (e, f);

    // Mod wheel: up to +6 dB on the sustains; expression scales the whole orchestra.
    auto swell = 1.0f + modWheel;
    auto swellStart = lastSwell;

    auto it = midi.cbegin();
    for (int chunkStart = 0; chunkStart < total; chunkStart += maxBlock)
    {
        auto chunkEnd = juce::jmin (total, chunkStart + maxBlock);
        auto n = chunkEnd - chunkStart;
        busBuf.clear (0, n);

        auto swellAt = [&] (int pos) { return swellStart + (swell - swellStart) * (float) (chunkStart + pos) / (float) total; };

        int pos = 0;
        for (; it != midi.cend() && (*it).samplePosition < chunkEnd; ++it)
        {
            auto t = juce::jlimit (0, n, (*it).samplePosition - chunkStart);
            if (t > pos) engine.render (busBuf, pos, t - pos, bend, swellAt (pos), swellAt (t));
            engine.advanceClock (t - pos);
            handleMidi ((*it).getMessage(), e);
            pos = t;
        }
        if (n > pos) engine.render (busBuf, pos, n - pos, bend, swellAt (pos), swellAt (n));
        engine.advanceClock (n - pos);

        auto* L = buffer.getWritePointer (0, chunkStart);
        auto* R = buffer.getWritePointer (1, chunkStart);
        for (int i = 0; i < n; ++i)
        {
            float l = 0, r = 0;
            for (int b = 0; b < numBuses; ++b)
            {
                auto g = busGain[(size_t) b].getNextValue();
                l += busBuf.getSample (b * 2, i) * g;
                r += busBuf.getSample (b * 2 + 1, i) * g;
            }
            L[i] = l * expression;
            R[i] = r * expression;
        }

        mixBuf.copyFrom (0, 0, buffer, 0, chunkStart, n);
        mixBuf.copyFrom (1, 0, buffer, 1, chunkStart, n);
        chain.process (mixBuf, n, f);
        buffer.copyFrom (0, chunkStart, mixBuf, 0, 0, n);
        buffer.copyFrom (1, chunkStart, mixBuf, 1, 0, n);
    }
    lastSwell = swell;

    activeVoices = engine.activeVoices();
    auto peak = buffer.getMagnitude (0, total);
    if (peak > outPeak.load()) outPeak = peak;
}

//==============================================================================
int OrchestraProcessor::getNumPrograms() { return (int) getPresets().size(); }

const juce::String OrchestraProcessor::getProgramName (int index)
{
    auto& presets = getPresets();
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? presets[(size_t) index].name : "";
}

void OrchestraProcessor::setCurrentProgram (int index)
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

void OrchestraProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("easyView", easyView, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void OrchestraProcessor::setStateInformation (const void* data, int size)
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

juce::AudioProcessorEditor* OrchestraProcessor::createEditor() { return new OrchestraEditor (*this); }
} // namespace orch
