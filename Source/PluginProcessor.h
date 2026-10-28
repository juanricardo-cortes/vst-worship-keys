#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Effects.h"
#include "Engine.h"
#include "SampleBank.h"
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

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

private:
    void readParams (EngineParams&, fx::Params&);
    void handleMidi (const juce::MidiMessage&, const SampleBank*, const EngineParams&);
    void renderVoices (int start, int end, const EngineParams&, float padCutoffMod);
    float param (const char* id) const { return params.at (id)->load(); }

    juce::SharedResourcePointer<SharedSampleBank> sharedBank;
    std::vector<std::string> paramIds;                         // owns the keys below
    std::map<std::string_view, std::atomic<float>*> params;    // allocation-free lookup on the audio thread

    KeysEngine engine;
    fx::Chain chain;
    juce::AudioBuffer<float> layerBuf; // 6 channels: piano L/R, EP L/R, pad L/R
    juce::SmoothedValue<float> pianoGain, epGain, padGain;
    double sr = 44100.0, lfoPhase = 0.0;
    std::atomic<float> grDb { 0.0f };
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WorshipKeysProcessor)
};
