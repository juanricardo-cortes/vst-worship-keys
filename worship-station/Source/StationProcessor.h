#pragma once
#include "Packs.h"
#include "Orchestra/OrchProcessor.h"

// Worship Station: Worship Keys + Anthem Synth + Praise Orchestra in one instrument.
// One keyboard can switch between the packs (a learned wheel / knob / buttons), or each pack can listen
// to its own MIDI channel / MIDI device ("split" routing). Controller settings are global (one file),
// the sounds themselves are saved with the project.
class StationProcessor : public juce::AudioProcessor,
                         private juce::MidiInputCallback,
                         private juce::Timer
{
public:
    static constexpr int numPacks = 3;
    static const juce::StringArray packNames;

    // Learnable controls.
    enum Control { wheel, nextPack, prevPack, selectKeys, selectSynth, selectOrch, numControls };
    enum WheelMode { absolute, relative64, relative2s };   // knob/fader, endless (64 = no move), endless (1 / 127)
    enum Routing { switchPacks, splitInputs };

    StationProcessor();
    ~StationProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Worship Station"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 15.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    //==============================================================================
    juce::AudioProcessor& getPack (int index) { return *packs[(size_t) index]; }
    int getSelectedPack() const { return juce::jlimit (0, numPacks - 1, (int) std::lround (selectedParam->load())); }
    void selectPack (int index);
    float takePackPeak (int index) { return packPeak[(size_t) index].exchange (0.0f); }
    bool isPackAwake (int index) const { return awake[(size_t) index].load(); }

    // MIDI controls (message thread)
    void startLearn (Control c) { learnTarget = c; }
    void cancelLearn() { learnTarget = -1; }
    int getLearnTarget() const { return learnTarget.load(); }
    juce::String describeBinding (Control) const;
    void clearBinding (Control);
    WheelMode getWheelMode() const { return (WheelMode) wheelMode.load(); }
    void setWheelMode (WheelMode);
    juce::String describeLastMidi() const;

    // Routing (message thread)
    Routing getRouting() const { return (Routing) routing.load(); }
    void setRouting (Routing);
    int getPackChannel (int pack) const { return packChannel[(size_t) pack].load(); } // -1 off, 0 any, 1..16
    void setPackChannel (int pack, int channel);
    juce::String getPackDevice (int pack) const { return packDevice[(size_t) pack]; } // "" = any
    void setPackDevice (int pack, const juce::String& deviceName);
    bool canRouteByDevice() const { return deviceInputsActive.load(); }   // standalone app only
    juce::StringArray getMidiDeviceNames() const;

    bool saveSettingsIfChanged(); // message thread: persists learned controls
    static inline bool persistSettings = true; // the test tool turns this off so it never touches your setup

    juce::AudioProcessorValueTreeState apvts;

private:
    struct Event { juce::MidiMessage msg; int pos, slot; };

    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;
    void timerCallback() override;

    void processChunk (juce::AudioBuffer<float>& out, int start, int num, const juce::MidiBuffer& midi, int slotForBuffer);
    bool handleControls (const juce::MidiMessage&);
    void learn (const juce::MidiMessage&);
    void route (const juce::MidiMessage&, int pos, int slot);
    void send (int pack, const juce::MidiMessage&, int pos);
    void stepPack (int delta);
    void noteLastMidi (const juce::MidiMessage&, int slot);

    void loadSettings();
    void saveSettings();

    std::array<std::unique_ptr<juce::AudioProcessor>, numPacks> packs;
    std::array<juce::AudioBuffer<float>, numPacks> packBuf;
    std::array<juce::MidiBuffer, numPacks> packMidi;
    std::array<juce::SmoothedValue<float>, numPacks> packGain;
    juce::SmoothedValue<float> masterGain;
    std::array<std::atomic<float>, numPacks> packPeak {};
    std::array<std::atomic<bool>, numPacks> awake {};
    std::array<int, numPacks> silentSamples {};
    std::array<std::array<uint8_t, 128>, 16> noteOwner {};   // switch mode: which packs got each note-on (bit mask)
    std::array<std::array<bool, 128>, 16> controlNoteDown {}; // notes swallowed as control buttons
    std::atomic<float>* selectedParam = nullptr;
    std::atomic<float>* volParam[numPacks] {};
    std::atomic<float>* masterParam = nullptr;
    double sr = 44100.0, clock = 0, lastStepClock = -1.0e9;
    int maxBlock = 512, lastWheelZone = -1, clockPos = 0;
    juce::MidiBuffer chunkMidi;
    std::atomic<bool> panicPending { false };

    // Controls: code = type << 16 | channel << 8 | number. type: 0 none, 1 CC, 2 note, 3 program change.
    std::array<std::atomic<int>, numControls> binding {};
    std::atomic<int> learnTarget { -1 }, wheelMode { relative64 }, routing { switchPacks };
    std::atomic<int> lastMidiCode { -1 }, lastMidiValue { 0 }, lastMidiSlot { -1 };
    std::atomic<bool> settingsChanged { false };
    std::array<std::atomic<int>, numPacks> packChannel {};
    std::array<juce::String, numPacks> packDevice;
    std::array<std::atomic<int>, numPacks> packSlot {};      // device slot index, -1 = any

    // Standalone app: per-device MIDI (hosts merge all devices, so this only works outside a DAW).
    static constexpr int maxDevices = 8;
    std::array<juce::MidiMessageCollector, maxDevices> collectors;
    std::array<juce::String, maxDevices> slotNames;
    mutable juce::SpinLock slotLock;
    juce::MidiBuffer deviceMidi;
    std::atomic<bool> deviceInputsActive { false };
    juce::StringArray seenDevices;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StationProcessor)
};
