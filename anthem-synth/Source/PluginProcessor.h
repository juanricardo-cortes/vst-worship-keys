#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "SynthEngine.h"
#include "SynthFx.h"
#include <map>
#include <string>
#include <string_view>

class AnthemSynthProcessor : public juce::AudioProcessor
{
public:
    AnthemSynthProcessor();
    ~AnthemSynthProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Anthem Synth"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 15.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    double getEffectiveBpm() const { return effectiveBpm.load(); }
    bool isTempoFromHost() const { return tempoFromHost.load(); }
    float takeOutputPeak() { return outPeak.exchange (0.0f); }
    int getActiveVoices() const { return activeVoices.load(); }

    // Waveform scope for the editor (mono mix of the last ~43 ms, written by the audio thread).
    static constexpr int scopeSize = 1024;
    void copyScope (float* dest) const;

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    bool easyView = true; // which editor page is showing, saved with the project

private:
    void readParams (syn::Params&, sfx::Params&);
    void handleMidi (const juce::MidiMessage&, const syn::Params&);
    void emitNoteOn (int note, float vel, const syn::Params&);
    float param (const char* id) const { return params.at (id)->load(); }
    int choice (const char* id) const { return (int) std::lround (params.at (id)->load()); }

    std::vector<std::string> paramIds;                         // owns the keys below
    std::map<std::string_view, std::atomic<float>*> params;    // allocation-free lookup on the audio thread

    syn::Engine engine;
    syn::Arpeggiator arp;
    syn::Motion gate, pump;
    sfx::Chain chain;
    std::vector<float> pumpBuf;
    double sr = 44100.0, internalBeat = 0.0;
    int lastArpMode = 0, lastVoiceMode = 0;
    bool anyKeyDown = false;
    int keysDown = 0;

    std::atomic<double> effectiveBpm { 120.0 };
    std::atomic<bool> tempoFromHost { false };
    std::atomic<float> outPeak { 0.0f };
    std::atomic<int> activeVoices { 0 };
    std::array<float, scopeSize> scope {};
    std::atomic<int> scopeWrite { 0 };
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnthemSynthProcessor)
};
