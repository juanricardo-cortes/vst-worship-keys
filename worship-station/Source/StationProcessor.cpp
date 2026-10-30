#include "StationProcessor.h"
#include "StationEditor.h"

#if JucePlugin_Build_Standalone
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

namespace
{
using Range = juce::NormalisableRange<float>;

Range levelRange()
{
    Range r (-60, 6, 0.1f);
    r.setSkewForCentre (-14);
    return r;
}

constexpr float offDb = -59.9f;
constexpr int typeCC = 1, typeNote = 2, typeProgram = 3;
const char* volIds[] = { "keysVol", "synthVol", "orchVol" };
const char* controlIds[] = { "wheel", "next", "prev", "keys", "synth", "orch" };

int makeCode (int type, int channel, int number) { return type << 16 | channel << 8 | number; }
int codeType (int code) { return code >> 16; }
int codeChannel (int code) { return (code >> 8) & 0xff; }
int codeNumber (int code) { return code & 0xff; }

// Pedals and channel-mode messages always mean something to a keyboard rig: never learn them.
bool isLearnableCC (int cc) { return cc != 64 && cc != 66 && cc != 67 && cc < 120; }

juce::File settingsFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("WorshipStation").getChildFile ("controller.xml");
}
} // namespace

const juce::StringArray StationProcessor::packNames { "Keys", "Synth", "Orchestra" };

juce::AudioProcessorValueTreeState::ParameterLayout StationProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "pack", 1 }, "Sound Pack", packNames, 0));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "keysVol", 1 }, "Keys Volume", levelRange(), 0.0f,
                                                              juce::AudioParameterFloatAttributes().withLabel ("dB")));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "synthVol", 1 }, "Synth Volume", levelRange(), -3.0f,
                                                              juce::AudioParameterFloatAttributes().withLabel ("dB")));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "orchVol", 1 }, "Orchestra Volume", levelRange(), 0.0f,
                                                              juce::AudioParameterFloatAttributes().withLabel ("dB")));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "master", 1 }, "Master", Range (-24, 6, 0.1f), 0.0f,
                                                              juce::AudioParameterFloatAttributes().withLabel ("dB")));
    return { p.begin(), p.end() };
}

StationProcessor::StationProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "WorshipStation", createLayout())
{
    packs[0] = std::make_unique<keyspack::WorshipKeysProcessor>();
    packs[1] = std::make_unique<synthpack::AnthemSynthProcessor>();
    packs[2] = std::make_unique<orch::OrchestraProcessor>();

    selectedParam = apvts.getRawParameterValue ("pack");
    for (int i = 0; i < numPacks; ++i)
        volParam[i] = apvts.getRawParameterValue (volIds[i]);
    masterParam = apvts.getRawParameterValue ("master");

    for (auto& b : binding) b = 0;
    for (int i = 0; i < numPacks; ++i)
    {
        packChannel[(size_t) i] = i + 1; // split routing default: Keys ch 1, Synth ch 2, Orchestra ch 3
        packSlot[(size_t) i] = -1;
    }
    loadSettings();
    startTimer (1000);
}

StationProcessor::~StationProcessor()
{
    stopTimer();
   #if JucePlugin_Build_Standalone
    if (deviceInputsActive.load())
        if (auto* holder = juce::StandalonePluginHolder::getInstance())
            holder->deviceManager.removeMidiInputDeviceCallback ({}, this);
   #endif
}

bool StationProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void StationProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = juce::jmax (samplesPerBlock, 512);
    for (int i = 0; i < numPacks; ++i)
    {
        auto& p = *packs[(size_t) i];
        p.setPlayConfigDetails (0, 2, sr, maxBlock);
        p.prepareToPlay (sr, maxBlock);
        packBuf[(size_t) i].setSize (2, maxBlock);
        packMidi[(size_t) i].ensureSize (4096);
        packGain[(size_t) i].reset (sr, 0.03);
        awake[(size_t) i] = true;
        silentSamples[(size_t) i] = 0;
    }
    masterGain.reset (sr, 0.03);
    deviceMidi.ensureSize (4096);
    chunkMidi.ensureSize (4096);
    for (auto& c : collectors)
        c.reset (sr);
    for (auto& ch : noteOwner) ch.fill (0);
    for (auto& ch : controlNoteDown) ch.fill (false);
}

void StationProcessor::releaseResources()
{
    for (auto& p : packs)
        p->releaseResources();
}

//==============================================================================
void StationProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    auto total = buffer.getNumSamples();
    buffer.clear();
    if (buffer.getNumChannels() < 2 || total == 0)
        return;

    for (auto& p : packMidi)
        p.clear();

    if (panicPending.exchange (false)) // routing changed: release everything so no note hangs
    {
        for (int p = 0; p < numPacks; ++p)
            for (int ch = 1; ch <= 16; ++ch)
                send (p, juce::MidiMessage::allNotesOff (ch), 0);
        for (auto& ch : noteOwner) ch.fill (0);
    }

    // 1. Controls + routing for the whole block (packs then render in chunks).
    auto handle = [this] (const juce::MidiMessage& m, int pos, int slot)
    {
        noteLastMidi (m, slot);
        if (learnTarget.load() >= 0) { learn (m); return; }
        if (handleControls (m)) return;
        route (m, pos, slot);
    };

    if (deviceInputsActive.load())
    {
        // Standalone: our own per-device inputs replace the merged stream the app hands us.
        for (int s = 0; s < maxDevices; ++s)
        {
            deviceMidi.clear();
            collectors[(size_t) s].removeNextBlockOfMessages (deviceMidi, total);
            for (const auto meta : deviceMidi)
            {
                clockPos = meta.samplePosition;
                handle (meta.getMessage(), meta.samplePosition, s);
            }
        }
    }
    else
    {
        for (const auto meta : midi)
        {
            clockPos = meta.samplePosition;
            handle (meta.getMessage(), meta.samplePosition, -1);
        }
    }
    midi.clear();

    // 2. Render each awake pack and mix.
    for (int i = 0; i < numPacks; ++i)
    {
        auto db = volParam[i]->load();
        packGain[(size_t) i].setTargetValue (db <= offDb ? 0.0f : juce::Decibels::decibelsToGain (db));
        packs[(size_t) i]->setPlayHead (getPlayHead());
    }
    masterGain.setTargetValue (juce::Decibels::decibelsToGain (masterParam->load()));
    auto selected = getSelectedPack();

    for (int start = 0; start < total; start += maxBlock)
    {
        auto n = juce::jmin (maxBlock, total - start);
        auto* L = buffer.getWritePointer (0, start);
        auto* R = buffer.getWritePointer (1, start);

        for (int i = 0; i < numPacks; ++i)
        {
            auto& events = packMidi[(size_t) i];
            chunkMidi.clear();
            for (const auto meta : events)
                if (meta.samplePosition >= start && meta.samplePosition < start + n)
                    chunkMidi.addEvent (meta.data, meta.numBytes, meta.samplePosition - start);

            auto& gain = packGain[(size_t) i];
            if (! chunkMidi.isEmpty() || i == selected)
            {
                awake[(size_t) i] = true;
                silentSamples[(size_t) i] = 0;
            }
            if (! awake[(size_t) i].load())
            {
                gain.skip (n);
                continue;
            }

            juce::AudioBuffer<float> view (packBuf[(size_t) i].getArrayOfWritePointers(), 2, n);
            view.clear();
            packs[(size_t) i]->processBlock (view, chunkMidi);

            auto peak = view.getMagnitude (0, n);
            if (peak > packPeak[(size_t) i].load()) packPeak[(size_t) i] = peak;

            // A pack that has gone quiet (tails finished) and isn't the one on screen stops using CPU
            // until MIDI arrives for it again.
            if (peak < 1.0e-5f)
            {
                silentSamples[(size_t) i] += n;
                if (silentSamples[(size_t) i] > (int) (3.0 * sr) && i != selected)
                    awake[(size_t) i] = false;
            }
            else
                silentSamples[(size_t) i] = 0;

            auto* pl = view.getReadPointer (0);
            auto* pr = view.getReadPointer (1);
            for (int s = 0; s < n; ++s)
            {
                auto g = gain.getNextValue();
                L[s] += pl[s] * g;
                R[s] += pr[s] * g;
            }
        }

        for (int s = 0; s < n; ++s)
        {
            auto g = masterGain.getNextValue();
            L[s] *= g;
            R[s] *= g;
        }
    }
    clock += total;
}

void StationProcessor::noteLastMidi (const juce::MidiMessage& m, int slot)
{
    int type = 0, value = 0;
    if (m.isController())          { type = typeCC; value = m.getControllerValue(); }
    else if (m.isNoteOn())         { type = typeNote; value = m.getVelocity(); }
    else if (m.isProgramChange())  { type = typeProgram; }
    else return;
    auto number = m.isController() ? m.getControllerNumber() : m.isNoteOn() ? m.getNoteNumber() : m.getProgramChangeNumber();
    lastMidiValue = value;
    lastMidiSlot = slot;
    lastMidiCode = makeCode (type, m.getChannel(), number);
}

void StationProcessor::learn (const juce::MidiMessage& m)
{
    auto target = learnTarget.load();
    int code = 0;
    if (m.isController() && isLearnableCC (m.getControllerNumber()))
    {
        if (target != wheel && m.getControllerValue() == 0)
            return; // button release
        code = makeCode (typeCC, m.getChannel(), m.getControllerNumber());
        if (target == wheel)
        {
            // Guess the wheel type from the first value: endless encoders send small steps around 64 (or 1 / 127).
            auto v = m.getControllerValue();
            wheelMode = (v <= 15 || v >= 113) ? relative2s : (v >= 49 && v <= 79 && v != 64) ? relative64 : absolute;
            lastWheelZone = -1;
        }
    }
    else if (target != wheel && m.isNoteOn())
    {
        code = makeCode (typeNote, m.getChannel(), m.getNoteNumber());
        controlNoteDown[(size_t) m.getChannel() - 1][(size_t) m.getNoteNumber()] = true;
    }
    else if (target != wheel && m.isProgramChange())
        code = makeCode (typeProgram, m.getChannel(), m.getProgramChangeNumber());
    else
        return;

    for (auto& b : binding) // one control per MIDI message
        if (b.load() == code) b = 0;
    binding[(size_t) target] = code;
    learnTarget = -1;
    settingsChanged = true;
}

bool StationProcessor::handleControls (const juce::MidiMessage& m)
{
    int type = 0, number = 0, value = 0;
    auto ch = m.getChannel();
    if (ch < 1) return false;
    if (m.isController())                    { type = typeCC; number = m.getControllerNumber(); value = m.getControllerValue(); }
    else if (m.isNoteOn() || m.isNoteOff())  { type = typeNote; number = m.getNoteNumber(); value = m.isNoteOn() ? m.getVelocity() : 0; }
    else if (m.isProgramChange())            { type = typeProgram; number = m.getProgramChangeNumber(); value = 127; }
    else return false;

    auto code = makeCode (type, ch, number);

    if (type == typeCC && binding[wheel].load() == code)
    {
        auto mode = wheelMode.load();
        // A "relative" guess that later sends big jumps was really a knob or fader.
        if ((mode == relative64 && (value < 40 || value > 88)) || (mode == relative2s && value >= 20 && value <= 107))
        {
            wheelMode = mode = absolute;
            settingsChanged = true;
        }

        if (mode == absolute)
        {
            auto zone = juce::jlimit (0, numPacks - 1, value * numPacks / 128);
            if (zone != lastWheelZone)
            {
                lastWheelZone = zone;
                selectPack (zone);
            }
        }
        else
        {
            auto delta = mode == relative64 ? value - 64 : (value < 64 ? value : value - 128);
            auto now = clock + clockPos;
            // One detent = one pack; a quick spin moves at most one pack per 80 ms so it can't overshoot wildly.
            if (delta != 0 && now - lastStepClock > 0.08 * sr)
            {
                lastStepClock = now;
                stepPack (delta > 0 ? 1 : -1);
            }
        }
        return true;
    }

    for (int c = nextPack; c < numControls; ++c)
        if (binding[(size_t) c].load() == code)
        {
            if (type == typeNote)
                controlNoteDown[(size_t) ch - 1][(size_t) number] = m.isNoteOn();
            if (value > 0)
            {
                if (c == nextPack) stepPack (1);
                else if (c == prevPack) stepPack (-1);
                else selectPack (c - selectKeys);
            }
            return true;
        }

    // The release of a note that was a control button when it went down.
    if (type == typeNote && m.isNoteOff() && controlNoteDown[(size_t) ch - 1][(size_t) number])
    {
        controlNoteDown[(size_t) ch - 1][(size_t) number] = false;
        return true;
    }
    return false;
}

void StationProcessor::route (const juce::MidiMessage& m, int pos, int slot)
{
    auto ch = m.getChannel();
    if (ch < 1)
        return; // sysex / clock etc.: the packs don't use them

    if (routing.load() == splitInputs)
    {
        for (int p = 0; p < numPacks; ++p)
        {
            auto want = packChannel[(size_t) p].load();
            auto wantSlot = packSlot[(size_t) p].load();
            if (want < 0 || (want > 0 && want != ch) || (wantSlot >= 0 && wantSlot != slot))
                continue;
            send (p, m, pos);
        }
        return;
    }

    auto selected = getSelectedPack();
    auto& owner = noteOwner[(size_t) ch - 1];
    if (m.isNoteOn())
    {
        owner[(size_t) m.getNoteNumber()] |= (uint8_t) (1 << selected);
        send (selected, m, pos);
    }
    else if (m.isNoteOff())
    {
        // Notes end on the pack they started on, even if you switched while holding them.
        auto mask = owner[(size_t) m.getNoteNumber()];
        if (mask == 0) mask = (uint8_t) (1 << selected);
        owner[(size_t) m.getNoteNumber()] = 0;
        for (int p = 0; p < numPacks; ++p)
            if (mask & (1 << p)) send (p, m, pos);
    }
    else if (m.isSustainPedalOn() || m.isSustainPedalOff() || m.isSostenutoPedalOn() || m.isSostenutoPedalOff()
             || m.isSoftPedalOn() || m.isSoftPedalOff() || m.isAllNotesOff() || m.isAllSoundOff() || m.isResetAllControllers())
    {
        for (int p = 0; p < numPacks; ++p)
            send (p, m, pos); // pedal up must reach a pack you switched away from
    }
    else
        send (selected, m, pos);
}

void StationProcessor::send (int pack, const juce::MidiMessage& m, int pos)
{
    packMidi[(size_t) pack].addEvent (m, pos);
}

void StationProcessor::stepPack (int delta)
{
    selectPack ((getSelectedPack() + delta + numPacks) % numPacks);
}

void StationProcessor::selectPack (int index)
{
    index = juce::jlimit (0, numPacks - 1, index);
    if (auto* p = apvts.getParameter ("pack"))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) index));
}

//==============================================================================
juce::String StationProcessor::describeBinding (Control c) const
{
    auto code = binding[(size_t) c].load();
    auto ch = " (ch " + juce::String (codeChannel (code)) + ")";
    switch (codeType (code))
    {
        case typeCC:      return "CC " + juce::String (codeNumber (code)) + ch;
        case typeNote:    return "Note " + juce::MidiMessage::getMidiNoteName (codeNumber (code), true, true, 3) + ch;
        case typeProgram: return "Program " + juce::String (codeNumber (code) + 1) + ch;
        default:          return "not set";
    }
}

void StationProcessor::clearBinding (Control c)
{
    binding[(size_t) c] = 0;
    saveSettings();
}

void StationProcessor::setWheelMode (WheelMode m)
{
    wheelMode = m;
    lastWheelZone = -1;
    saveSettings();
}

juce::String StationProcessor::describeLastMidi() const
{
    auto code = lastMidiCode.load();
    if (code < 0)
        return "Last MIDI in: nothing yet - move a knob or press a key";
    auto value = lastMidiValue.load();
    juce::String s;
    switch (codeType (code))
    {
        case typeCC:      s = "CC " + juce::String (codeNumber (code)) + " = " + juce::String (value); break;
        case typeNote:    s = "Note " + juce::MidiMessage::getMidiNoteName (codeNumber (code), true, true, 3) + " vel " + juce::String (value); break;
        case typeProgram: s = "Program " + juce::String (codeNumber (code) + 1); break;
        default: break;
    }
    s << "   ch " << codeChannel (code);
    auto slot = lastMidiSlot.load();
    if (juce::isPositiveAndBelow (slot, maxDevices))
    {
        juce::SpinLock::ScopedLockType l (slotLock);
        s << "   " << slotNames[(size_t) slot];
    }
    return "Last MIDI in: " + s;
}

void StationProcessor::setRouting (Routing r)
{
    routing = r;
    panicPending = true;
    saveSettings();
}

void StationProcessor::setPackChannel (int pack, int channel)
{
    packChannel[(size_t) pack] = juce::jlimit (-1, 16, channel);
    saveSettings();
}

void StationProcessor::setPackDevice (int pack, const juce::String& deviceName)
{
    packDevice[(size_t) pack] = deviceName;
    timerCallback(); // resolve the slot now
    saveSettings();
}

juce::StringArray StationProcessor::getMidiDeviceNames() const
{
    juce::StringArray names;
    for (auto& d : juce::MidiInput::getAvailableDevices())
        names.add (d.name);
    return names;
}

//==============================================================================
void StationProcessor::handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& m)
{
    auto name = source != nullptr ? source->getName() : juce::String();
    int slot = -1;
    {
        juce::SpinLock::ScopedLockType l (slotLock);
        for (int i = 0; i < maxDevices && slot < 0; ++i)
            if (slotNames[(size_t) i] == name) slot = i;
        for (int i = 0; i < maxDevices && slot < 0; ++i)
            if (slotNames[(size_t) i].isEmpty()) { slotNames[(size_t) i] = name; slot = i; }
    }
    if (slot >= 0)
        collectors[(size_t) slot].addMessageToQueue (m);
}

void StationProcessor::timerCallback()
{
   #if JucePlugin_Build_Standalone
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
    {
        if (! deviceInputsActive.load())
        {
            holder->deviceManager.addMidiInputDeviceCallback ({}, this);
            deviceInputsActive = true;
        }
        // Plug and play: switch on every MIDI input the first time it shows up (incl. hot-plugged ones).
        for (auto& d : juce::MidiInput::getAvailableDevices())
            if (! seenDevices.contains (d.identifier))
            {
                seenDevices.add (d.identifier);
                holder->deviceManager.setMidiInputDeviceEnabled (d.identifier, true);
            }
    }
   #endif

    juce::SpinLock::ScopedLockType l (slotLock);
    for (int p = 0; p < numPacks; ++p)
    {
        auto& name = packDevice[(size_t) p];
        int slot = -1;
        if (name.isNotEmpty())
        {
            for (int i = 0; i < maxDevices && slot < 0; ++i)
                if (slotNames[(size_t) i] == name) slot = i;
            for (int i = 0; i < maxDevices && slot < 0; ++i) // reserve a slot for a device that hasn't played yet
                if (slotNames[(size_t) i].isEmpty()) { slotNames[(size_t) i] = name; slot = i; }
        }
        packSlot[(size_t) p] = slot;
    }
}

//==============================================================================
bool StationProcessor::saveSettingsIfChanged()
{
    if (! settingsChanged.exchange (false))
        return false;
    saveSettings();
    return true;
}

void StationProcessor::saveSettings()
{
    if (! persistSettings)
        return;
    juce::XmlElement xml ("Controller");
    xml.setAttribute ("routing", routing.load());
    xml.setAttribute ("wheelMode", wheelMode.load());
    for (int c = 0; c < numControls; ++c)
        if (auto code = binding[(size_t) c].load(); code != 0)
        {
            auto* e = xml.createNewChildElement ("Bind");
            e->setAttribute ("control", controlIds[c]);
            e->setAttribute ("type", codeType (code));
            e->setAttribute ("channel", codeChannel (code));
            e->setAttribute ("number", codeNumber (code));
        }
    for (int p = 0; p < numPacks; ++p)
    {
        auto* e = xml.createNewChildElement ("Pack");
        e->setAttribute ("name", packNames[p]);
        e->setAttribute ("channel", packChannel[(size_t) p].load());
        e->setAttribute ("device", packDevice[(size_t) p]);
    }
    auto file = settingsFile();
    file.getParentDirectory().createDirectory();
    xml.writeTo (file);
}

void StationProcessor::loadSettings()
{
    if (! persistSettings)
        return;
    auto xml = juce::XmlDocument::parse (settingsFile());
    if (xml == nullptr)
        return;
    routing = juce::jlimit (0, 1, xml->getIntAttribute ("routing", 0));
    wheelMode = juce::jlimit (0, 2, xml->getIntAttribute ("wheelMode", relative64));
    for (auto* e : xml->getChildWithTagNameIterator ("Bind"))
        for (int c = 0; c < numControls; ++c)
            if (e->getStringAttribute ("control") == controlIds[c])
                binding[(size_t) c] = makeCode (juce::jlimit (1, 3, e->getIntAttribute ("type")),
                                                juce::jlimit (1, 16, e->getIntAttribute ("channel", 1)),
                                                juce::jlimit (0, 127, e->getIntAttribute ("number")));
    int p = 0;
    for (auto* e : xml->getChildWithTagNameIterator ("Pack"))
        if (p < numPacks)
        {
            packChannel[(size_t) p] = juce::jlimit (-1, 16, e->getIntAttribute ("channel", p + 1));
            packDevice[(size_t) p] = e->getStringAttribute ("device");
            ++p;
        }
}

//==============================================================================
void StationProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    for (int i = 0; i < numPacks; ++i)
    {
        juce::MemoryBlock block;
        packs[(size_t) i]->getStateInformation (block);
        state.setProperty ("pack" + juce::String (i), block.toBase64Encoding(), nullptr);
    }
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void StationProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;
    auto state = juce::ValueTree::fromXml (*xml);
    for (int i = 0; i < numPacks; ++i)
    {
        juce::MemoryBlock block;
        if (block.fromBase64Encoding (state.getProperty ("pack" + juce::String (i)).toString()) && block.getSize() > 0)
            packs[(size_t) i]->setStateInformation (block.getData(), (int) block.getSize());
        state.removeProperty ("pack" + juce::String (i), nullptr);
    }
    apvts.replaceState (state);
}

juce::AudioProcessorEditor* StationProcessor::createEditor() { return new StationEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new StationProcessor(); }
