// Offline render: plays a I-V-vi-IV progression (in C, with pedal) through every preset,
// writes renders/<preset>.wav and prints peak / RMS / NaN checks.
// Usage: WKRender [outputDir]
#include "../Source/PluginProcessor.h"
#include <iostream>
#include <thread>
#include <tuple>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    constexpr double sr = 48000.0;
    if (argc > 2 && juce::String (argv[1]) == "--analyze") // brightness of existing renders, for before/after
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        for (int i = 2; i < argc; ++i)
            if (std::unique_ptr<juce::AudioFormatReader> r { fm.createReaderFor (juce::File (argv[i])) })
            {
                juce::AudioBuffer<float> b (2, (int) r->lengthInSamples);
                r->read (&b, 0, b.getNumSamples(), 0, true, true);
                fx::Biquad hp1, hp2;
                hp1.highPass (r->sampleRate, 2500.0);
                hp2.highPass (r->sampleRate, 2500.0);
                double all = 0, high = 0;
                for (int k = 0; k < (int) (8.0 * r->sampleRate) && k < b.getNumSamples(); ++k)
                {
                    auto x = b.getSample (0, k);
                    auto h = hp2.process (hp1.process (x, 0), 0);
                    all += (double) x * x;
                    high += (double) h * h;
                }
                std::cout << juce::File (argv[i]).getFileName() << "  bright " << juce::String (10.0 * std::log10 (high / all), 1) << " dB\n";
            }
        return 0;
    }

    if (argc > 2 && juce::String (argv[1]) == "--ui") // editor screenshots + a MIDI learn check, no audio device needed
    {
        auto dir = juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]);
        dir.createDirectory();
        {
            WorshipKeysProcessor proc;
            for (auto [easy, w, h] : { std::tuple { true, 1120, 640 }, { false, 1120, 640 }, { true, 852, 393 }, { false, 852, 393 } })
            {
                proc.easyView = easy;
                std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
                ed->setSize (w, h);
                auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
                auto f = dir.getChildFile (juce::String (easy ? "easy_" : "full_") + juce::String (w) + "x" + juce::String (h) + ".png");
                f.deleteFile();
                juce::FileOutputStream os (f);
                juce::PNGImageFormat().writeImageToStream (img, os);
                std::cout << f.getFullPathName() << "\n";
            }
        }

        WorshipKeysProcessor proc; // no editor: nothing gets saved to the user's map file
        proc.setPlayConfigDetails (0, 2, 48000.0, 256);
        proc.prepareToPlay (48000.0, 256);
        juce::AudioBuffer<float> buf (2, 256);
        auto send = [&] (int cc, int value)
        {
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::controllerEvent (1, cc, value), 0);
            proc.processBlock (buf, midi);
        };
        auto pad = proc.apvts.getParameter ("padLevel");
        proc.startMidiLearn ("padLevel");
        send (64, 127); // sustain must not be learned
        auto sustainIgnored = proc.isLearning ("padLevel");
        send (21, 0);
        auto mapped = proc.getMappedCC ("padLevel") == 21 && ! proc.isAnyLearning();
        send (21, 127);
        auto atTop = pad->getValue() > 0.99f;
        send (21, 0);
        auto atBottom = pad->getValue() < 0.01f;
        std::cout << "MIDI learn: sustain ignored " << sustainIgnored << ", mapped CC21 " << mapped
                  << ", CC moves fader " << (atTop && atBottom) << "\n";
        return sustainIgnored && mapped && atTop && atBottom ? 0 : 3;
    }

    auto outDir = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "renders");
    outDir.createDirectory();

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

    // Share of the energy above 2.5 kHz, in dB below the total: higher = brighter.
    auto brightnessDb = [&] (const juce::AudioBuffer<float>& b)
    {
        fx::Biquad hp1, hp2;
        hp1.highPass (sr, 2500.0);
        hp2.highPass (sr, 2500.0);
        double all = 0, high = 0;
        for (int i = 0; i < juce::jmin (b.getNumSamples(), (int) (8.0 * sr)); ++i)
        {
            auto x = b.getSample (0, i);
            auto h = hp2.process (hp1.process (x, 0), 0);
            all += (double) x * x;
            high += (double) h * h;
        }
        return all > 0 ? (float) (10.0 * std::log10 (high / all + 1e-12)) : -100.0f;
    };

    auto render = [&] (int prog, std::vector<std::pair<const char*, float>> overrides, bool& badSample)
    {
        proc.setCurrentProgram (prog);
        for (auto& [id, v] : overrides)
            proc.apvts.getParameter (id)->setValueNotifyingHost (proc.apvts.getParameter (id)->convertTo0to1 (v));
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
        badSample = false;
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
        return out;
    };

    bool allOk = true;
    for (int prog = 0; prog < proc.getNumPrograms(); ++prog)
    {
        bool badSample = false, unused = false;
        auto out = render (prog, {}, badSample);

        // Layer balance: pad alone vs the keys alone, through the same fx (playing section only).
        auto playLen = (int) (barSec * (double) chords.size() * sr);
        auto keys = render (prog, { { "padLevel", -60.0f } }, unused);
        auto padOnly = render (prog, { { "pianoLevel", -60.0f }, { "epLevel", -60.0f } }, unused);
        auto keysDb = juce::Decibels::gainToDecibels (keys.getRMSLevel (0, 0, playLen), -100.0f);
        auto padDb = juce::Decibels::gainToDecibels (padOnly.getRMSLevel (0, 0, playLen), -100.0f);
        auto bright = brightnessDb (out);
        auto bellsDb = -100.0f;
        if (proc.apvts.getRawParameterValue ("bellLevel")->load() > -59.9f)
        {
            auto bellsOnly = render (prog, { { "pianoLevel", -60.0f }, { "epLevel", -60.0f }, { "padLevel", -60.0f } }, unused);
            auto pianoOnly = render (prog, { { "bellLevel", -60.0f }, { "padLevel", -60.0f } }, unused);
            bellsDb = juce::Decibels::gainToDecibels (bellsOnly.getRMSLevel (0, 0, playLen), -100.0f)
                    - juce::Decibels::gainToDecibels (pianoOnly.getRMSLevel (0, 0, playLen), -100.0f);
        }

        auto peak = out.getMagnitude (0, totalSamples);
        auto rms = 0.5f * (out.getRMSLevel (0, 0, totalSamples) + out.getRMSLevel (1, 0, totalSamples));
        auto tailRms = out.getRMSLevel (0, totalSamples - (int) sr, (int) sr);
        auto name = proc.getProgramName (prog);
        std::cout << juce::String (name).paddedRight (' ', 30)
                  << " peak " << juce::String (juce::Decibels::gainToDecibels (peak), 1) << " dBFS"
                  << "  rms " << juce::String (juce::Decibels::gainToDecibels (rms), 1) << " dBFS"
                  << "  last-sec rms " << juce::String (juce::Decibels::gainToDecibels (tailRms), 1) << " dBFS"
                  << "  bright " << juce::String (bright, 1) << " dB"
                  << "  pad-vs-keys " << (padDb < -90 ? juce::String ("off") : juce::String (padDb - keysDb, 1) + " dB")
                  << (bellsDb > -90 ? "  bells-vs-piano " + juce::String (bellsDb, 1) + " dB" : juce::String())
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
                              { "epLevel", 6.0f }, { "bellLevel", 6.0f }, { "drive", 1.0f }, { "comp", 1.0f } })
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
