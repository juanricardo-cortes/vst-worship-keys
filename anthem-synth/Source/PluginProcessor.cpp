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

constexpr double arpRateBeats[] = { 1.0, 0.5, 0.75, 0.25, 0.125 };
constexpr double delayBeats[] = { 1.0, 0.75, 0.5, 1.5, 2.0, 0.25 };
constexpr int chunk = 32;   // MIDI / arp resolution (~0.7 ms at 48 kHz)
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout AnthemSynthProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto add = [&] (const char* id, const char* name, Range range, float def, const char* unit = "")
    {
        p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, def,
                                                                  juce::AudioParameterFloatAttributes().withLabel (unit)));
    };
    auto addInt = [&] (const char* id, const char* name, int lo, int hi, int def)
    {
        p.push_back (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id, 1 }, name, lo, hi, def));
    };
    auto addChoice = [&] (const char* id, const char* name, juce::StringArray items, int def)
    {
        p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def));
    };

    // Oscillators
    addChoice ("oscAWave", "Osc A Wave", { "Supersaw", "Saw", "Square", "Triangle" }, 0);
    addInt ("oscAOct", "Osc A Octave", -2, 2, 0);
    add ("detune", "Supersaw Detune", Range (0, 1), 0.35f);
    add ("oscALevel", "Osc A Level", Range (0, 1), 0.8f);
    addChoice ("oscBWave", "Osc B Wave", { "Saw", "Square", "Triangle", "Sine" }, 0);
    addInt ("oscBOct", "Osc B Octave", -2, 2, 0);
    addInt ("oscBSemi", "Osc B Semitone", -12, 12, 0);
    add ("oscBFine", "Osc B Fine", Range (-50, 50, 0.1f), 7, "ct");
    add ("oscBLevel", "Osc B Level", Range (0, 1), 0.0f);
    add ("pw", "Pulse Width", Range (0, 1), 0.0f);
    add ("subLevel", "Sub Level", Range (0, 1), 0.0f);
    add ("noise", "Noise", Range (0, 1), 0.0f);
    add ("width", "Stereo Width", Range (0, 1), 0.7f);

    // Filter
    add ("cutoff", "Cutoff", skewed (20, 20000, 1000), 4000, "Hz");
    add ("reso", "Resonance", Range (0, 1), 0.15f);
    add ("fltEnv", "Filter Env Amount", Range (-1, 1), 0.3f);
    add ("keyTrack", "Key Tracking", Range (0, 1), 0.5f);
    add ("fltVel", "Filter Velocity", Range (0, 1), 0.2f);
    add ("fltDrive", "Filter Drive", Range (0, 1), 0.1f);
    add ("fAtk", "Filter Attack", skewed (0.001f, 10, 0.3f), 0.005f, "s");
    add ("fDec", "Filter Decay", skewed (0.001f, 10, 0.6f), 0.6f, "s");
    add ("fSus", "Filter Sustain", Range (0, 1), 0.4f);
    add ("fRel", "Filter Release", skewed (0.001f, 12, 0.6f), 0.5f, "s");

    // Amp
    add ("aAtk", "Attack", skewed (0.001f, 10, 0.3f), 0.005f, "s");
    add ("aDec", "Decay", skewed (0.001f, 10, 0.6f), 0.5f, "s");
    add ("aSus", "Sustain", Range (0, 1), 0.9f);
    add ("aRel", "Release", skewed (0.001f, 12, 0.6f), 0.4f, "s");
    add ("velSens", "Velocity", Range (0, 1), 0.5f);
    add ("level", "Patch Level", Range (-18, 18, 0.1f), 0, "dB");   // per-preset loudness trim

    // LFO / voice
    add ("lfoRate", "LFO Rate", skewed (0.05f, 20, 2), 5, "Hz");
    add ("lfoPitch", "LFO > Pitch", Range (0, 1), 0.0f);
    add ("lfoFilter", "LFO > Filter", Range (0, 1), 0.0f);
    add ("lfoPwm", "LFO > PWM", Range (0, 1), 0.0f);
    addChoice ("voiceMode", "Voice Mode", { "Poly", "Mono", "Legato" }, 0);
    add ("glide", "Glide", skewed (0, 2, 0.2f), 0.0f, "s");
    addInt ("bendRange", "Bend Range", 1, 12, 2);
    add ("swell", "Swell", Range (0, 1), 0.0f);

    // Motion / arp
    addChoice ("motion", "Motion", { "Off", "Pump 1/4", "Pump 1/8", "Gate 1/8", "Gate 1/16", "Trance Gate" }, 0);
    add ("motionDepth", "Motion Depth", Range (0, 1), 0.7f);
    addChoice ("arpMode", "Arp", { "Off", "Up", "Down", "Up/Down", "Random", "Chord" }, 0);
    addChoice ("arpRate", "Arp Rate", { "1/4", "1/8", "1/8 dot", "1/16", "1/32" }, 3);
    addInt ("arpOct", "Arp Octaves", 1, 3, 1);
    add ("arpGate", "Arp Gate", Range (0.1f, 1), 0.6f);

    // FX
    add ("lowCut", "Low Cut", skewed (20, 500, 100), 30, "Hz");
    add ("bright", "Brightness", Range (-6, 6, 0.1f), 0, "dB");
    add ("chorusMix", "Chorus", Range (0, 1), 0.25f);
    add ("chorusRate", "Chorus Rate", skewed (0.1f, 5, 0.6f), 0.5f, "Hz");
    add ("chorusDepth", "Chorus Depth", Range (0, 1), 0.5f);
    add ("delayMix", "Delay", Range (0, 1), 0.15f);
    add ("delayFeedback", "Delay Repeats", Range (0, 0.9f), 0.35f);
    addChoice ("delayTime", "Delay Time", { "1/4", "1/8 dot", "1/8", "1/4 dot", "1/2", "1/16" }, 1);
    add ("revMix", "Reverb", Range (0, 1), 0.25f);
    add ("revDecay", "Reverb Decay", skewed (0.4f, 15, 3), 3.5f, "s");
    add ("revSize", "Reverb Size", Range (0, 1), 0.75f);
    add ("revTone", "Reverb Tone", skewed (1500, 14000, 6000), 7000, "Hz");
    add ("shimmer", "Shimmer", Range (0, 1), 0.0f);
    add ("bpm", "Tempo", Range (40, 220, 0.1f), 120, "BPM");
    add ("master", "Master", Range (-24, 12, 0.1f), 0, "dB");

    return { p.begin(), p.end() };
}

AnthemSynthProcessor::AnthemSynthProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AnthemSynth", createLayout())
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

bool AnthemSynthProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void AnthemSynthProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    engine.prepare (sr);
    arp.reset();
    gate.prepare (sr);
    pump.prepare (sr);
    chain.prepare (sr, samplesPerBlock);
    pumpBuf.assign ((size_t) chain.maxBlock(), 1.0f);
    keysDown = 0;
    internalBeat = 0;
}

void AnthemSynthProcessor::readParams (syn::Params& s, sfx::Params& f)
{
    s.waveA = (syn::WaveA) choice ("oscAWave");
    s.octA = choice ("oscAOct");
    s.detune = param ("detune");
    s.levelA = param ("oscALevel");
    s.waveB = (syn::WaveB) choice ("oscBWave");
    s.octB = choice ("oscBOct");
    s.semiB = choice ("oscBSemi");
    s.fineB = param ("oscBFine");
    s.levelB = param ("oscBLevel");
    s.pw = param ("pw");
    s.sub = param ("subLevel");
    s.noise = param ("noise");
    s.width = param ("width");
    s.cutoff = param ("cutoff");
    s.reso = param ("reso");
    s.fltEnv = param ("fltEnv");
    s.keyTrack = param ("keyTrack");
    s.fltVel = param ("fltVel");
    s.drive = param ("fltDrive");
    s.fA = param ("fAtk"); s.fD = param ("fDec"); s.fS = param ("fSus"); s.fR = param ("fRel");
    s.aA = param ("aAtk"); s.aD = param ("aDec"); s.aS = param ("aSus"); s.aR = param ("aRel");
    s.velSens = param ("velSens");
    s.lfoRate = param ("lfoRate");
    s.lfoPitch = param ("lfoPitch");
    s.lfoFilter = param ("lfoFilter");
    s.lfoPwm = param ("lfoPwm");
    s.mode = (syn::VoiceMode) choice ("voiceMode");
    s.glide = param ("glide");
    s.bendRange = choice ("bendRange");
    s.swell = param ("swell");

    f.lowCut = param ("lowCut");
    f.brightDb = param ("bright");
    f.chorusMix = param ("chorusMix");
    f.chorusRate = param ("chorusRate");
    f.chorusDepth = param ("chorusDepth");
    f.delayMix = param ("delayMix");
    f.delayFeedback = param ("delayFeedback");
    f.revMix = param ("revMix");
    f.revDecay = param ("revDecay");
    f.revSize = param ("revSize");
    f.revToneHz = param ("revTone");
    f.shimmer = param ("shimmer");
    f.masterDb = param ("master") + param ("level");
}

void AnthemSynthProcessor::emitNoteOn (int note, float vel, const syn::Params& s)
{
    engine.noteOn (note, vel, s);
}

void AnthemSynthProcessor::handleMidi (const juce::MidiMessage& m, const syn::Params& s)
{
    auto arpOn = choice ("arpMode") != 0;
    if (m.isNoteOn())
    {
        ++keysDown;
        if (arpOn) arp.noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else emitNoteOn (m.getNoteNumber(), m.getFloatVelocity(), s);
    }
    else if (m.isNoteOff())
    {
        keysDown = juce::jmax (0, keysDown - 1);
        if (arpOn) arp.noteOff (m.getNoteNumber());
        else engine.noteOff (m.getNoteNumber(), s);
    }
    else if (m.isSustainPedalOn() || m.isSustainPedalOff())
    {
        engine.setSustain (m.isSustainPedalOn());
        arp.setSustain (m.isSustainPedalOn());
    }
    else if (m.isController() && m.getControllerNumber() == 1)
        engine.setModWheel (m.getControllerValue() / 127.0f);
    else if (m.isPitchWheel())
        engine.setPitchBend ((float) (m.getPitchWheelValue() - 8192) / 8192.0f);
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        keysDown = 0;
        engine.allNotesOff();
        arp.reset();
    }
}

void AnthemSynthProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    auto total = buffer.getNumSamples();
    buffer.clear();
    if (buffer.getNumChannels() < 2 || total == 0)
        return;

    keyboardState.processNextMidiBuffer (midi, 0, total, true);

    syn::Params s;
    sfx::Params f;
    readParams (s, f);

    // switching arp / voice mode mid-chord would leave hanging notes
    auto arpMode = choice ("arpMode"), voiceMode = choice ("voiceMode");
    if (arpMode != lastArpMode || voiceMode != lastVoiceMode)
    {
        arp.flush ([&] (const syn::Arpeggiator::Event&) {});
        arp.reset();
        engine.allNotesOff();
        keysDown = 0;
        lastArpMode = arpMode;
        lastVoiceMode = voiceMode;
    }

    // Host tempo + position when the host is playing (FL / any DAW), otherwise the Tempo knob
    // and a free-running clock that restarts on the first key so pump / arp line up with you.
    double bpm = param ("bpm");
    bool fromHost = false, hostPlaying = false;
    double hostPpq = 0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) { bpm = juce::jlimit (30.0, 300.0, *b); fromHost = true; }
            if (pos->getIsPlaying())
                if (auto ppq = pos->getPpqPosition()) { hostPpq = *ppq; hostPlaying = true; }
        }
    effectiveBpm = bpm;
    tempoFromHost = fromHost;
    auto beatsPerSample = bpm / 60.0 / sr;
    f.delaySamples = (float) (60.0 / bpm * delayBeats[juce::jlimit (0, 5, choice ("delayTime"))] * sr);

    auto motionType = (syn::Motion::Type) choice ("motion");
    auto motionDepth = param ("motionDepth");
    auto isGate = motionType == syn::Motion::Type::gate8 || motionType == syn::Motion::Type::gate16 || motionType == syn::Motion::Type::trance;
    auto isPump = motionType == syn::Motion::Type::pump4 || motionType == syn::Motion::Type::pump8;
    auto arpStep = arpRateBeats[juce::jlimit (0, 4, choice ("arpRate"))];
    auto arpOct = choice ("arpOct");
    auto arpGate = param ("arpGate");

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);
    double beatBase = hostPlaying ? hostPpq : internalBeat;

    // Hosts may send blocks larger than announced: work in pieces the FX scratch buffers fit.
    auto cap = chain.maxBlock();
    auto it = midi.cbegin();
    for (int blockStart = 0; blockStart < total; blockStart += cap)
    {
        auto blockEnd = juce::jmin (total, blockStart + cap);
        for (int pos = blockStart; pos < blockEnd; pos += chunk)
        {
            auto end = juce::jmin (blockEnd, pos + chunk);
            for (; it != midi.cend() && (*it).samplePosition < end; ++it)
            {
                auto msg = (*it).getMessage();
                if (msg.isNoteOn() && keysDown == 0 && ! hostPlaying)
                    beatBase = -pos * beatsPerSample;   // restart the clock on this key
                handleMidi (msg, s);
            }

            auto beat = beatBase + pos * beatsPerSample;
            if (arpMode != 0)
                arp.tick (beat, (syn::Arpeggiator::Mode) arpMode, arpStep, arpOct, arpGate,
                          [&] (const syn::Arpeggiator::Event& e)
                          {
                              if (e.on) engine.noteOn (e.note, e.vel, s);
                              else engine.noteOff (e.note, s);
                          });

            engine.render (L + pos, R + pos, end - pos, s);

            for (int i = pos; i < end; ++i)
            {
                auto b = beatBase + i * beatsPerSample;
                if (isGate)
                {
                    auto g = gate.next (b, motionType, motionDepth);
                    L[i] *= g;
                    R[i] *= g;
                }
                pumpBuf[(size_t) (i - blockStart)] = isPump ? pump.next (b, motionType, motionDepth) : 1.0f;
            }
        }
        chain.process (L + blockStart, R + blockStart, blockEnd - blockStart, f, pumpBuf.data());
    }
    internalBeat = beatBase + total * beatsPerSample;

    float peak = 0;
    auto w = scopeWrite.load();
    for (int i = 0; i < total; ++i)
    {
        peak = juce::jmax (peak, std::abs (L[i]), std::abs (R[i]));
        scope[(size_t) w] = 0.5f * (L[i] + R[i]);
        w = (w + 1) % scopeSize;
    }
    scopeWrite = w;
    if (peak > outPeak.load()) outPeak = peak;
    activeVoices = engine.activeVoices();
}

void AnthemSynthProcessor::copyScope (float* dest) const
{
    auto w = scopeWrite.load();
    for (int i = 0; i < scopeSize; ++i)
        dest[i] = scope[(size_t) ((w + i) % scopeSize)];
}

//==============================================================================
int AnthemSynthProcessor::getNumPrograms() { return (int) getPresets().size(); }

const juce::String AnthemSynthProcessor::getProgramName (int index)
{
    auto& presets = getPresets();
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? presets[(size_t) index].name : "";
}

void AnthemSynthProcessor::setCurrentProgram (int index)
{
    auto& presets = getPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;
    currentProgram = index;

    // Presets leave tempo, master level and bend range alone.
    static const juce::StringArray keep { "bpm", "master", "bendRange" };
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! keep.contains (ranged->getParameterID()))
                ranged->setValueNotifyingHost (ranged->getDefaultValue());

    for (auto& [id, value] : presets[(size_t) index].values)
        if (auto* ranged = apvts.getParameter (id))
            ranged->setValueNotifyingHost (ranged->convertTo0to1 (value));
}

void AnthemSynthProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("easyView", easyView, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void AnthemSynthProcessor::setStateInformation (const void* data, int size)
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

juce::AudioProcessorEditor* AnthemSynthProcessor::createEditor() { return new AnthemSynthEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AnthemSynthProcessor(); }
