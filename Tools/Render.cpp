// Offline render: plays a I-V-vi-IV progression (in C, with pedal) through every preset,
// writes renders/<preset>.wav and prints peak / RMS / NaN checks.
// Usage: WKRender [outputDir]
#include "../Source/PluginProcessor.h"
#include <iostream>
#include <thread>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    auto outDir = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "renders");
    outDir.createDirectory();

    constexpr double sr = 48000.0;
    constexpr int block = 256;

    WorshipKeysProcessor proc;
    proc.setPlayConfigDetails (0, 2, sr, block);
    proc.prepareToPlay (sr, block);

    for (int i = 0; i < 600 && ! proc.samplesReady(); ++i)
        std::this_thread::sleep_for (std::chrono::milliseconds (100));
    std::cout << proc.getSampleStatus() << "\n";
    if (! proc.samplesReady())
        return 1;

    // chord tones (MIDI), bar length 2 s at 120 bpm (one bar of 4/4)
    const std::vector<std::vector<int>> chords {
        { 36, 48, 55, 60, 64, 67 }, // C
        { 43, 55, 59, 62, 67, 71 }, // G
        { 45, 57, 60, 64, 69, 72 }, // Am
        { 41, 53, 57, 60, 65, 69 }, // F
    };
    const double barSec = 2.0, tailSec = 6.0;
    auto totalSamples = (int) ((barSec * (double) chords.size() + tailSec) * sr);

    bool allOk = true;
    for (int prog = 0; prog < proc.getNumPrograms(); ++prog)
    {
        proc.setCurrentProgram (prog);
        proc.prepareToPlay (sr, block); // reset voices & fx between presets

        juce::MidiBuffer events;
        auto at = [&] (double sec) { return (int) (sec * sr); };
        juce::Random rng (prog);
        for (size_t c = 0; c < chords.size(); ++c)
        {
            auto t = barSec * (double) c;
            events.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), at (t));
            events.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), at (t + 0.03));
            for (size_t k = 0; k < chords[c].size(); ++k)
            {
                auto vel = (juce::uint8) (70 + rng.nextInt (35));
                events.addEvent (juce::MidiMessage::noteOn (1, chords[c][k], vel), at (t + 0.012 * (double) k));
                events.addEvent (juce::MidiMessage::noteOff (1, chords[c][k]), at (t + barSec - 0.1));
            }
            // a melody note on beat 3
            events.addEvent (juce::MidiMessage::noteOn (1, chords[c][5] + 5, (juce::uint8) 96), at (t + 1.0));
            events.addEvent (juce::MidiMessage::noteOff (1, chords[c][5] + 5), at (t + 1.4));
        }
        events.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), at (barSec * (double) chords.size() + 1.0));

        juce::AudioBuffer<float> out (2, totalSamples);
        juce::AudioBuffer<float> buf (2, block);
        bool badSample = false;
        for (int pos = 0; pos < totalSamples; pos += block)
        {
            auto n = juce::jmin (block, totalSamples - pos);
            buf.setSize (2, n, false, false, true);
            juce::MidiBuffer midi;
            for (auto m : events)
                if (m.samplePosition >= pos && m.samplePosition < pos + n)
                    midi.addEvent (m.getMessage(), m.samplePosition - pos);
            proc.processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch)
            {
                for (int i = 0; i < n; ++i)
                    if (! std::isfinite (buf.getSample (ch, i))) badSample = true;
                out.copyFrom (ch, pos, buf, ch, 0, n);
            }
        }

        auto peak = out.getMagnitude (0, totalSamples);
        auto rms = 0.5f * (out.getRMSLevel (0, 0, totalSamples) + out.getRMSLevel (1, 0, totalSamples));
        auto tailRms = out.getRMSLevel (0, totalSamples - (int) sr, (int) sr);
        auto name = proc.getProgramName (prog);
        std::cout << juce::String (name).paddedRight (' ', 34)
                  << " peak " << juce::String (juce::Decibels::gainToDecibels (peak), 1) << " dBFS"
                  << "  rms " << juce::String (juce::Decibels::gainToDecibels (rms), 1) << " dBFS"
                  << "  last-sec rms " << juce::String (juce::Decibels::gainToDecibels (tailRms), 1) << " dBFS"
                  << (badSample ? "  NaN/Inf!" : "") << "\n";
        allOk = allOk && ! badSample && peak < 1.01f && peak > 0.01f;

        auto file = outDir.getChildFile (juce::File::createLegalFileName (name) + ".wav");
        file.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
        if (auto* writer = wav.createWriterFor (stream.get(), sr, 2, 24, {}, 0))
        {
            stream.release();
            std::unique_ptr<juce::AudioFormatWriter> w (writer);
            w->writeFromAudioSampleBuffer (out, 0, totalSamples);
        }
    }

    // Stress: every feedback path maxed out. The tail must decay, not grow.
    {
        proc.setCurrentProgram (4);
        for (auto [id, v] : { std::pair { "shimmer", 1.0f }, { "revDecay", 15.0f }, { "revMix", 1.0f }, { "revSize", 1.0f },
                              { "delayMix", 1.0f }, { "delayFeedback", 0.9f }, { "padLevel", 6.0f }, { "pianoLevel", 6.0f },
                              { "epLevel", 6.0f }, { "drive", 1.0f }, { "comp", 1.0f } })
            proc.apvts.getParameter (id)->setValueNotifyingHost (proc.apvts.getParameter (id)->convertTo0to1 (v));
        proc.prepareToPlay (sr, block);

        const int seconds = 40;
        juce::AudioBuffer<float> buf (2, block);
        juce::String line = "Stress (all maxed) rms/5s:";
        double acc = 0;
        int accN = 0;
        float firstTail = 0, lastTail = 0;
        bool bad = false;
        for (int pos = 0, sec = 0; pos < (int) sr * seconds; pos += block)
        {
            juce::MidiBuffer midi;
            if (pos == 0)
                for (int note : { 36, 48, 55, 60, 64, 67, 72, 76, 84, 91 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 127), 0);
            if (pos == (int) sr * 2)
                for (int note : { 36, 48, 55, 60, 64, 67, 72, 76, 84, 91 })
                    midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
            proc.processBlock (buf, midi);
            for (int i = 0; i < block; ++i)
            {
                auto s = buf.getSample (0, i);
                if (! std::isfinite (s)) bad = true;
                acc += (double) s * s;
            }
            accN += block;
            if (accN >= (int) sr * 5)
            {
                auto db = juce::Decibels::gainToDecibels ((float) std::sqrt (acc / accN), -150.0f);
                line << " " << juce::String (db, 0);
                if (sec == 1) firstTail = db;
                lastTail = db;
                acc = 0;
                accN = 0;
                ++sec;
            }
        }
        std::cout << line << "\n";
        auto decays = lastTail < firstTail - 6.0f;
        std::cout << (bad ? "Stress: NaN/Inf!" : decays ? "Stress: tail decays OK" : "Stress: TAIL NOT DECAYING") << "\n";
        allOk = allOk && ! bad && decays;
    }

    std::cout << (allOk ? "ALL OK" : "PROBLEMS FOUND") << "  -> " << outDir.getFullPathName() << "\n";
    return allOk ? 0 : 2;
}
