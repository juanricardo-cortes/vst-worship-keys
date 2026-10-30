// Renders a list of note events through one preset to a WAV. Used to make the ad/demo tracks.
// Usage: WKDemo <presetIndex> <events.csv> <out.wav> [tailSeconds]
// events.csv lines: "<seconds>,on,<note>,<velocity 1-127>" | "<seconds>,off,<note>" | "<seconds>,cc,<number>,<value>"
#include "../Source/PluginProcessor.h"
#include <iostream>
#include <thread>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 4)
    {
        std::cerr << "Usage: WKDemo <presetIndex> <events.csv> <out.wav> [tailSeconds]\n";
        return 1;
    }

    const int presetIndex = juce::String (argv[1]).getIntValue();
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const auto eventsFile = cwd.getChildFile (argv[2]);
    const auto outFile = cwd.getChildFile (argv[3]);
    const double tailSec = argc > 4 ? juce::String (argv[4]).getDoubleValue() : 4.0;

    constexpr double sr = 48000.0;
    constexpr int block = 256;

    WorshipKeysProcessor proc;
    proc.setPlayConfigDetails (0, 2, sr, block);
    for (int i = 0; i < 600 && ! proc.samplesReady(); ++i)
        std::this_thread::sleep_for (std::chrono::milliseconds (100));
    if (! proc.samplesReady())
    {
        std::cerr << proc.getSampleStatus() << "\n";
        return 1;
    }
    proc.setCurrentProgram (presetIndex);
    proc.prepareToPlay (sr, block);

    juce::MidiBuffer events;
    double lastTime = 0.0;
    juce::StringArray lines;
    eventsFile.readLines (lines);
    for (auto& line : lines)
    {
        auto f = juce::StringArray::fromTokens (line, ",", {});
        if (f.size() < 3)
            continue;
        const double t = f[0].getDoubleValue();
        const int pos = (int) std::round (t * sr);
        const auto type = f[1].trim();
        if (type == "on")
            events.addEvent (juce::MidiMessage::noteOn (1, f[2].getIntValue(), (juce::uint8) juce::jlimit (1, 127, f[3].getIntValue())), pos);
        else if (type == "off")
            events.addEvent (juce::MidiMessage::noteOff (1, f[2].getIntValue()), pos);
        else if (type == "cc")
            events.addEvent (juce::MidiMessage::controllerEvent (1, f[2].getIntValue(), f[3].getIntValue()), pos);
        lastTime = juce::jmax (lastTime, t);
    }

    const int totalSamples = (int) ((lastTime + tailSec) * sr);
    juce::AudioBuffer<float> out (2, totalSamples), buf (2, block);
    for (int pos = 0; pos < totalSamples; pos += block)
    {
        const int n = juce::jmin (block, totalSamples - pos);
        buf.setSize (2, n, false, false, true);
        juce::MidiBuffer midi;
        for (auto m : events)
            if (m.samplePosition >= pos && m.samplePosition < pos + n)
                midi.addEvent (m.getMessage(), m.samplePosition - pos);
        proc.processBlock (buf, midi);
        for (int ch = 0; ch < 2; ++ch)
            out.copyFrom (ch, pos, buf, ch, 0, n);
    }

    outFile.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream (outFile.createOutputStream());
    if (auto* writer = wav.createWriterFor (stream.get(), sr, 2, 24, {}, 0))
    {
        stream.release();
        std::unique_ptr<juce::AudioFormatWriter> w (writer);
        w->writeFromAudioSampleBuffer (out, 0, totalSamples);
    }
    std::cout << proc.getProgramName (presetIndex) << " -> " << outFile.getFullPathName()
              << "  peak " << juce::Decibels::gainToDecibels (out.getMagnitude (0, totalSamples)) << " dBFS\n";
    return 0;
}
