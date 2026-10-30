#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Effects.h"
#include "Engine.h"
#include "SampleBank.h"
#include <array>
#include <map>
#include <string>
#include <string_view>

class WorshipKeysProcessor : public juce::AudioProcessor
{
public:
    WorshipKeysProcessor();
    ~WorshipKeysProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Worship Keys"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::String getSampleStatus() const { return sharedBank->getStatus(); }
    bool samplesReady() const { return sharedBank->get() != nullptr; }
    float getGainReductionDb() const { return grDb.load(); }

    // MIDI learn: a CC moves one parameter. Mappings are global (saved to a file), not per project.
    void startMidiLearn (const juce::String& paramId);
    void cancelMidiLearn();
    bool isLearning (const juce::String& paramId) const;
    bool isAnyLearning() const { return learnParam.load() >= 0; }
    int getMappedCC (const juce::String& paramId) const; // -1 if none
    void clearMidiMapping (const juce::String& paramId);
    void clearAllMidiMappings();
    bool saveMidiMapIfChanged(); // call from the message thread

    bool easyView = true; // which editor page is showing, saved with the project

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

private:
    void readParams (EngineParams&, fx::Params&);
    void handleMidi (const juce::MidiMessage&, const SampleBank*, const EngineParams&);
    void renderVoices (int start, int end, const EngineParams&, float padCutoffMod);
    void handleCC (int cc, int value);
    int paramIndex (const juce::String& id) const;
    void saveMidiMap();
    void loadMidiMap();
    float param (const char* id) const { return params.at (id)->load(); }

    juce::SharedResourcePointer<SharedSampleBank> sharedBank;
    std::vector<std::string> paramIds;                         // owns the keys below
    std::map<std::string_view, std::atomic<float>*> params;    // allocation-free lookup on the audio thread

    KeysEngine engine;
    fx::Chain chain;
    juce::AudioBuffer<float> layerBuf; // 8 channels: piano L/R, EP L/R, pad L/R, bells L/R
    juce::SmoothedValue<float> pianoGain, epGain, padGain, bellGain;
    double sr = 44100.0, lfoPhase = 0.0, tremPhase = 0.0, tremInc = 0.0;
    float tremDepth = 0.0f;
    std::atomic<float> grDb { 0.0f };
    int currentProgram = 0;
    std::array<std::atomic<int>, 128> ccMap;   // CC number -> parameter index, -1 = unmapped
    std::atomic<int> learnParam { -1 };
    std::atomic<bool> midiMapChanged { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WorshipKeysProcessor)
};
