#pragma once
#include "OrchEngine.h"

namespace orch
{
// The orchestra pack: sampled sections (VSCO 2 CE) + the Worship Keys FX chain (EQ, glue comp, hall).
class OrchestraProcessor : public juce::AudioProcessor
{
public:
    OrchestraProcessor();
    ~OrchestraProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Praise Orchestra"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

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
    int getActiveVoices() const { return activeVoices.load(); }
    float takeOutputPeak() { return outPeak.exchange (0.0f); }

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    bool easyView = true; // which editor page is showing, saved with the project

private:
    void readParams (Params&, keyspack::fx::Params&);
    void handleMidi (const juce::MidiMessage&, const Params&);
    float param (const char* id) const { return params.at (id)->load(); }
    int choice (const char* id) const { return (int) std::lround (params.at (id)->load()); }

    juce::SharedResourcePointer<SharedOrchBank> sharedBank;
    std::vector<std::string> paramIds;
    std::map<std::string_view, std::atomic<float>*> params;

    Engine engine;
    keyspack::fx::Chain chain;
    juce::AudioBuffer<float> busBuf, mixBuf;
    std::array<juce::SmoothedValue<float>, numBuses> busGain;
    double sr = 44100.0;
    int maxBlock = 512;
    float bend = 1.0f, modWheel = 0.0f, expression = 1.0f, lastSwell = 1.0f;
    std::atomic<int> activeVoices { 0 };
    std::atomic<float> outPeak { 0.0f };
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OrchestraProcessor)
};
} // namespace orch
