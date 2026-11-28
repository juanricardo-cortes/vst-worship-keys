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
    static constexpr int numPads = 8;        // transpose pads
    static constexpr int maxTranspose = 12;
    static const juce::StringArray packNames;

    // Learnable controls.
    // wheel = pack selector; patchWheel / nextPatch / prevPatch step the presets of the pack you're on.
    enum Control { wheel, nextPack, prevPack, selectKeys, selectSynth, selectOrch, patchWheel, nextPatch, prevPatch, numControls };
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
    void startLearn (Control c) { learnPad = -1; learnAllPads = false; learnTarget = c; }
    void cancelLearn() { learnTarget = -1; learnPad = -1; learnAllPads = false; }
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
    void applyPendingPatchChange(); // message thread: patch wheel / buttons -> the selected pack's preset
    void requestPatchStep (int pack, int delta) { pendingPatchSteps[(size_t) pack] += delta; applyPendingPatchChange(); }
    void useArturiaDefaults();      // KeyLab Essential mk3: wheel = patches, arrow buttons = packs
    static inline bool persistSettings = true; // the test tool turns this off so it never touches your setup
    // Transpose (semitones, saved with the project). Pads: press = jump to that pad's transpose, press the lit pad
    // again = back to 0, hold a pad 1 s = store the current transpose on it.
    int getTranspose() const { return (int) std::lround (transposeParam->load()); }
    void setTranspose (int semitones);
    void startPadLearn (int pad, bool thenLearnTheRest);
    int getPadLearnTarget() const { return learnPad.load(); }
    juce::String describePad (int pad) const;
    void clearPad (int pad);
    int getPadSemitones (int pad) const { return padSemis[(size_t) pad].load(); }
    void setPadSemitones (int pad, int semitones);
    void pressPad (int pad);                       // what a pad press does (also the on-screen pads)
    int getActivePad() const;                      // lit pad, -1 none
    int takePadSavedEvent() { return padSavedEvent.exchange (-1); }
    static juce::String formatSemitones (int);

    // Favorites (message thread): saved globally by patch name. "Favorites only" makes the patch wheel / buttons
    // step through just the favorites of a pack (all patches while a pack has none).
    bool isFavorite (int pack, int program);
    void toggleFavorite (int pack, int program);
    juce::Array<int> getFavoritePrograms (int pack);
    bool getFavoritesOnly() const { return favoritesOnly.load(); }
    void setFavoritesOnly (bool);
    int getFavoritesVersion() const { return favoritesVersion; }

    // Test tool: MIDI as if it arrived from the named device in the standalone app (per-device inputs).
    void injectDeviceMidi (const juce::String& device, const juce::MidiMessage&);

    juce::AudioProcessorValueTreeState apvts;

private:
    struct Event { juce::MidiMessage msg; int pos, slot; };

    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;
    void queueFromDevice (const juce::String& name, const juce::MidiMessage&);
    void timerCallback() override;

    void processChunk (juce::AudioBuffer<float>& out, int start, int num, const juce::MidiBuffer& midi, int slotForBuffer);
    bool handleControls (const juce::MidiMessage&, int slot);
    int patchTargetPack (int channel, int slot) const;
    bool listensTo (int pack, int channel, int slot) const;
    void learn (const juce::MidiMessage&);
    void route (const juce::MidiMessage&, int pos, int slot);
    void send (int pack, const juce::MidiMessage&, int pos);
    void stepPack (int delta);
    void handleWheel (int control, int value, int targetPack);
    bool handlePads (const juce::MidiMessage&);
    void learnPadMessage (const juce::MidiMessage&);
    void checkPadHolds();
    void loadFavorites();
    void saveFavorites();
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
    std::atomic<float>* transposeParam = nullptr;
    std::array<std::array<int8_t, 128>, 16> playedNote {};   // the (transposed) note each held key is sounding, -1 none
    double sr = 44100.0, clock = 0, lastStepClock = -1.0e9;
    int maxBlock = 512, lastWheelZone = -1, lastPatchZone = -1, clockPos = 0, timerTicks = 0;
    juce::MidiBuffer chunkMidi;
    std::atomic<bool> panicPending { false };

    // Controls: code = type << 16 | channel << 8 | number. type: 0 none, 1 CC, 2 note, 3 program change.
    std::array<std::atomic<int>, numControls> binding {};
    std::atomic<int> learnTarget { -1 }, wheelMode { relative64 }, routing { switchPacks };
    std::atomic<int> lastMidiCode { -1 }, lastMidiValue { 0 }, lastMidiSlot { -1 };
    std::atomic<bool> settingsChanged { false };
    std::array<std::atomic<int>, numPacks> pendingPatchSteps {}, pendingPatchZone {};
    std::atomic<bool> arturiaDefaults { true };
    std::array<std::atomic<int>, numPads> padBinding {}, padSemis {};
    std::array<bool, numPads> padDown {}, padStored {};
    std::array<double, numPads> padDownClock {};
    std::atomic<int> learnPad { -1 }, lastPad { -1 }, padSavedEvent { -1 };
    std::atomic<bool> learnAllPads { false };
    std::array<juce::StringArray, numPacks> favorites;
    std::atomic<bool> favoritesOnly { false };
    int favoritesVersion = 0;   // built-in KeyLab controls: only answer Arturia keyboards
    std::array<std::atomic<int>, numPacks> packChannel {};
    std::array<juce::String, numPacks> packDevice;
    std::array<std::atomic<int>, numPacks> packSlot {};      // device slot index, -1 = any

    // Standalone app: per-device MIDI (hosts merge all devices, so this only works outside a DAW).
    static constexpr int maxDevices = 8;
    std::array<juce::MidiMessageCollector, maxDevices> collectors;
    std::array<juce::String, maxDevices> slotNames;
    std::array<std::atomic<bool>, maxDevices> slotIsArturia {};
    mutable juce::SpinLock slotLock;
    juce::MidiBuffer deviceMidi;
    std::atomic<bool> deviceInputsActive { false };
    juce::StringArray seenDevices;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StationProcessor)
};
