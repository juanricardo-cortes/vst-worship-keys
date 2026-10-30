// Offline renderer + test for Anthem Synth.
//   AnthemRender [outDir]                      renders every preset (test phrase) to outDir/*.wav and checks stability
//   AnthemRender --snapshot <out.png> [preset] renders the editor to a PNG
//   AnthemRender --demo <preset> <events.csv> <out.wav> [tailSec] [bpm]
//       events.csv lines: "<sec>,on,<note>,<vel 1-127>" | "<sec>,off,<note>" | "<sec>,cc,<num>,<value>" | "<sec>,bend,<-8192..8191>"
//       The render runs with a host transport that starts at 0 s, so pump / gate / arp sit on the beat grid.
#include "../Source/PluginProcessor.h"
#include <iomanip>
#include <iostream>

namespace
{
constexpr double sr = 48000.0;
constexpr int block = 256;

struct FakeHost : juce::AudioPlayHead
{
    double bpm = 120, sample = 0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setBpm (bpm);
        info.setIsPlaying (true);
        info.setPpqPosition (sample / sr * bpm / 60.0);
        info.setTimeInSeconds (sample / sr);
        return info;
    }
};

void writeWav (const juce::File& f, const juce::AudioBuffer<float>& buf)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream (f.createOutputStream());
    if (auto* writer = wav.createWriterFor (stream.get(), sr, 2, 24, {}, 0))
    {
        stream.release();
        std::unique_ptr<juce::AudioFormatWriter> w (writer);
        w->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
    }
}

void setParam (AnthemSynthProcessor& p, const char* id, float value)
{
    if (auto* r = p.apvts.getParameter (id))
        r->setValueNotifyingHost (r->convertTo0to1 (value));
}

// Renders events through the processor. Returns false on NaN / runaway output.
bool render (AnthemSynthProcessor& proc, FakeHost& host, juce::MidiBuffer& events, juce::AudioBuffer<float>& out)
{
    auto total = out.getNumSamples();
    juce::AudioBuffer<float> buf (2, block);
    bool ok = true;
    for (int pos = 0; pos < total; pos += block)
    {
        auto n = juce::jmin (block, total - pos);
        buf.setSize (2, n, false, false, true);
        juce::MidiBuffer midi;
        for (auto m : events)
            if (m.samplePosition >= pos && m.samplePosition < pos + n)
                midi.addEvent (m.getMessage(), m.samplePosition - pos);
        host.sample = pos;
        proc.processBlock (buf, midi);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = buf.getReadPointer (ch);
            for (int i = 0; i < n; ++i)
                if (! std::isfinite (d[i]) || std::abs (d[i]) > 1.5f) ok = false;
            out.copyFrom (ch, pos, buf, ch, 0, n);
        }
    }
    return ok;
}

float rmsDb (const juce::AudioBuffer<float>& b, double from, double to)
{
    auto s0 = juce::jlimit (0, b.getNumSamples(), (int) (from * sr));
    auto s1 = juce::jlimit (0, b.getNumSamples(), (int) (to * sr));
    if (s1 <= s0) return -120;
    double acc = 0;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = s0; i < s1; ++i) acc += (double) b.getSample (ch, i) * b.getSample (ch, i);
    return (float) (10.0 * std::log10 (acc / (2.0 * (s1 - s0)) + 1e-12));
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    auto cwd = juce::File::getCurrentWorkingDirectory();

    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
    {
        AnthemSynthProcessor proc;
        if (argc > 3) proc.setCurrentProgram (juce::String (argv[3]).getIntValue());
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        auto f = cwd.getChildFile (argv[2]);
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
        return 0;
    }

    if (argc > 4 && juce::String (argv[1]) == "--demo")
    {
        AnthemSynthProcessor proc;
        FakeHost host;
        host.bpm = argc > 6 ? juce::String (argv[6]).getDoubleValue() : 120.0;
        proc.setPlayHead (&host);
        proc.setPlayConfigDetails (0, 2, sr, block);
        proc.setCurrentProgram (juce::String (argv[2]).getIntValue());
        setParam (proc, "bpm", (float) host.bpm);
        proc.prepareToPlay (sr, block);

        juce::MidiBuffer events;
        double last = 0;
        juce::StringArray lines;
        cwd.getChildFile (argv[3]).readLines (lines);
        for (auto& line : lines)
        {
            auto f = juce::StringArray::fromTokens (line, ",", {});
            if (f.size() < 3) continue;
            auto t = f[0].getDoubleValue();
            auto pos = (int) std::round (t * sr);
            auto type = f[1].trim();
            if (type == "on") events.addEvent (juce::MidiMessage::noteOn (1, f[2].getIntValue(), (juce::uint8) juce::jlimit (1, 127, f[3].getIntValue())), pos);
            else if (type == "off") events.addEvent (juce::MidiMessage::noteOff (1, f[2].getIntValue()), pos);
            else if (type == "cc") events.addEvent (juce::MidiMessage::controllerEvent (1, f[2].getIntValue(), f[3].getIntValue()), pos);
            else if (type == "bend") events.addEvent (juce::MidiMessage::pitchWheel (1, juce::jlimit (0, 16383, f[2].getIntValue() + 8192)), pos);
            last = juce::jmax (last, t);
        }
        auto tail = argc > 5 ? juce::String (argv[5]).getDoubleValue() : 4.0;
        juce::AudioBuffer<float> out (2, (int) ((last + tail) * sr));
        auto ok = render (proc, host, events, out);
        writeWav (cwd.getChildFile (argv[4]), out);
        std::cout << proc.getProgramName (proc.getCurrentProgram()) << " -> " << argv[4] << "  peak "
                  << juce::Decibels::gainToDecibels (out.getMagnitude (0, out.getNumSamples())) << " dBFS" << (ok ? "" : "  UNSTABLE") << "\n";
        return ok ? 0 : 2;
    }

    // ---- test: every preset plays a chord, a melody line and a stress cluster
    auto outDir = cwd.getChildFile (argc > 1 ? argv[1] : "renders");
    outDir.createDirectory();
    AnthemSynthProcessor proc;
    FakeHost host;
    proc.setPlayHead (&host);
    proc.setPlayConfigDetails (0, 2, sr, block);

    juce::MidiBuffer events;
    auto at = [] (double s) { return (int) (s * sr); };
    for (int n : { 52, 59, 64, 68, 71 })   // E major add9 voicing
    {
        events.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 96), at (0.0));
        events.addEvent (juce::MidiMessage::noteOff (1, n), at (3.0));
    }
    int melody[] = { 76, 78, 80, 83, 80, 78, 76, 71 };
    for (int i = 0; i < 8; ++i)
    {
        events.addEvent (juce::MidiMessage::noteOn (1, melody[i], (juce::uint8) (80 + i * 4)), at (3.5 + i * 0.25));
        events.addEvent (juce::MidiMessage::noteOff (1, melody[i]), at (3.5 + i * 0.25 + 0.22));
    }
    for (int n = 36; n < 60; ++n)   // 24-note cluster: voice stealing stress
    {
        events.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 127), at (6.0 + (n - 36) * 0.005));
        events.addEvent (juce::MidiMessage::noteOff (1, n), at (7.0));
    }
    const double len = 16.0;

    std::cout << std::left << std::setw (38) << "preset" << std::right << std::setw (9) << "peak" << std::setw (9) << "rms"
              << std::setw (10) << "tail+3s" << std::setw (10) << "end" << std::setw (8) << "x-rt" << "\n";
    bool allOk = true;
    for (int prog = 0; prog < proc.getNumPrograms(); ++prog)
    {
        proc.setCurrentProgram (prog);
        proc.prepareToPlay (sr, block);
        juce::AudioBuffer<float> out (2, (int) (len * sr));
        auto t0 = juce::Time::getMillisecondCounterHiRes();
        auto ok = render (proc, host, events, out);
        auto elapsed = (juce::Time::getMillisecondCounterHiRes() - t0) * 0.001;
        auto peak = juce::Decibels::gainToDecibels (out.getMagnitude (0, out.getNumSamples()));
        auto name = proc.getProgramName (prog);
        std::cout << std::left << std::setw (38) << name.toStdString() << std::right << std::fixed << std::setprecision (1)
                  << std::setw (9) << peak << std::setw (9) << rmsDb (out, 0.0, 7.0)
                  << std::setw (10) << rmsDb (out, 10.0, 11.0) << std::setw (10) << rmsDb (out, 15.0, 16.0)
                  << std::setw (8) << len / elapsed << (ok ? "" : "   UNSTABLE") << "\n";
        allOk &= ok;
        writeWav (outDir.getChildFile (juce::File::createLegalFileName (name) + ".wav"), out);
    }
    std::cout << (allOk ? "\nall presets stable\n" : "\nSTABILITY FAILURE\n");
    return allOk ? 0 : 2;
}
