// WSRender: offline checks for Worship Station without a DAW.
//   WSRender                 renders every orchestra preset to renders/ and runs the switching / routing tests
//   WSRender --ui <dir>      also writes PNG snapshots of each pack page and the controller setup
#include "../Source/StationProcessor.h"
#include "../Source/Orchestra/OrchPresets.h"

namespace
{
constexpr double rate = 48000.0;
constexpr int block = 512;

struct Ev { double t; juce::MidiMessage m; };

struct Result
{
    juce::AudioBuffer<float> audio;
    std::array<float, StationProcessor::numPacks> packPeak {};      // whole render
    std::array<float, StationProcessor::numPacks> packTailPeak {};  // last 0.5 s
    bool finite = true;
};

Result render (StationProcessor& st, std::vector<Ev> events, double seconds)
{
    std::sort (events.begin(), events.end(), [] (auto& a, auto& b) { return a.t < b.t; });
    Result r;
    auto total = (int) (seconds * rate);
    r.audio.setSize (2, total);
    r.audio.clear();
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    size_t next = 0;
    for (int i = 0; i < StationProcessor::numPacks; ++i) st.takePackPeak (i);

    for (int pos = 0; pos < total; pos += block)
    {
        auto n = juce::jmin (block, total - pos);
        midi.clear();
        while (next < events.size() && (int) (events[next].t * rate) < pos + n)
        {
            midi.addEvent (events[next].m, juce::jmax (0, (int) (events[next].t * rate) - pos));
            ++next;
        }
        juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(), 2, n);
        st.processBlock (view, midi);
        for (int ch = 0; ch < 2; ++ch)
            r.audio.copyFrom (ch, pos, view, ch, 0, n);
        for (int i = 0; i < StationProcessor::numPacks; ++i)
        {
            auto pk = st.takePackPeak (i);
            r.packPeak[(size_t) i] = juce::jmax (r.packPeak[(size_t) i], pk);
            if (pos > total - (int) (0.5 * rate))
                r.packTailPeak[(size_t) i] = juce::jmax (r.packTailPeak[(size_t) i], pk);
        }
    }
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < total; ++i)
            if (! std::isfinite (r.audio.getSample (ch, i))) r.finite = false;
    return r;
}

void writeWav (const juce::AudioBuffer<float>& audio, const juce::File& file)
{
    file.getParentDirectory().createDirectory();
    file.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (file), rate, 2, 24, {}, 0));
    if (w != nullptr)
        w->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
}

juce::MidiMessage on (int note, int vel, int ch = 1) { return juce::MidiMessage::noteOn (ch, note, (juce::uint8) vel); }
juce::MidiMessage off (int note, int ch = 1) { return juce::MidiMessage::noteOff (ch, note); }
juce::MidiMessage cc (int num, int val, int ch = 1) { return juce::MidiMessage::controllerEvent (ch, num, val); }

void chord (std::vector<Ev>& ev, double t, double len, std::initializer_list<int> notes, int vel)
{
    for (auto n : notes)
    {
        ev.push_back ({ t, on (n, vel) });
        ev.push_back ({ t + len, off (n) });
    }
}

// Up-tempo praise figure in D: stabs on the off-beats, a held chord that swells, a big accented ending.
std::vector<Ev> praisePhrase()
{
    std::vector<Ev> ev;
    chord (ev, 0.00, 0.14, { 38, 62, 66, 69 }, 122);
    chord (ev, 0.50, 0.10, { 62, 66, 69 }, 98);
    chord (ev, 0.75, 0.10, { 62, 66, 69 }, 104);
    chord (ev, 1.00, 0.14, { 43, 67, 71, 74 }, 112);
    chord (ev, 1.50, 0.10, { 67, 71, 74 }, 100);
    chord (ev, 1.75, 0.10, { 67, 71, 74 }, 106);
    chord (ev, 2.00, 1.80, { 45, 69, 73, 76 }, 118);
    chord (ev, 4.00, 2.40, { 38, 74, 78, 81 }, 127);
    return ev;
}

float db (float g) { return juce::Decibels::gainToDecibels (g, -120.0f); }

int failures = 0;
void check (bool ok, const juce::String& what)
{
    std::cout << (ok ? "  PASS  " : "  FAIL  ") << what << "\n";
    if (! ok) ++failures;
}

void wait (StationProcessor& st, double seconds) { render (st, {}, seconds); }
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    StationProcessor::persistSettings = false; // never touch the real controller setup

    auto root = juce::File::getCurrentWorkingDirectory();
    StationProcessor st;
    st.prepareToPlay (rate, block);

    auto& keys = dynamic_cast<keyspack::WorshipKeysProcessor&> (st.getPack (0));
    auto& orchestra = dynamic_cast<orch::OrchestraProcessor&> (st.getPack (2));
    for (int i = 0; i < 1200 && ! (keys.samplesReady() && orchestra.samplesReady()); ++i)
        juce::Thread::sleep (100);
    std::cout << keys.getSampleStatus() << "\n" << orchestra.getSampleStatus() << "\n";
    if (! orchestra.samplesReady())
        return 1;

    if (argc >= 3 && juce::String (argv[1]) == "--ui")
    {
        juce::File dir (argv[2]);
        dir.createDirectory();
        std::unique_ptr<juce::AudioProcessorEditor> ed (st.createEditor());
        for (int p = 0; p < StationProcessor::numPacks; ++p)
        {
            st.selectPack (p);
            for (int i = 0; i < 5; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            juce::FileOutputStream os (dir.getChildFile ("station_" + StationProcessor::packNames[p].toLowerCase() + ".png"));
            os.setPosition (0); os.truncate();
            juce::PNGImageFormat().writeImageToStream (img, os);
        }
        for (auto* c : ed->getChildren())
            if (auto* b = dynamic_cast<juce::TextButton*> (c); b != nullptr && b->getButtonText() == "CONTROLLER SETUP")
                b->triggerClick();
        for (int i = 0; i < 5; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::FileOutputStream os (dir.getChildFile ("station_setup.png"));
        os.setPosition (0); os.truncate();
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::cout << "UI snapshots -> " << dir.getFullPathName() << "\n";
        ed.reset();
        st.selectPack (0);
    }

    // Phone layouts: each pack editor at iPhone 15 Pro Max / 17 Pro Max landscape sizes (points), with Dynamic Island + home bar insets.
    if (argc >= 3 && juce::String (argv[1]) == "--phone")
    {
        juce::File dir (argv[2]);
        dir.createDirectory();
        struct Phone { const char* name; int w, h; };
        for (auto phone : { Phone { "15promax", 932, 430 }, Phone { "17promax", 956, 440 } })
        {
            hw::safeAreaOverride() = juce::BorderSize<int> (0, 59, 21, 59); // top, left, bottom, right
            for (int p = 0; p < StationProcessor::numPacks; ++p)
                for (int easy = 1; easy >= 0; --easy)
                {
                    auto& pack = st.getPack (p);
                    if (auto* k = dynamic_cast<keyspack::WorshipKeysProcessor*> (&pack)) k->easyView = easy != 0;
                    if (auto* sy = dynamic_cast<synthpack::AnthemSynthProcessor*> (&pack)) sy->easyView = easy != 0;
                    if (auto* o = dynamic_cast<orch::OrchestraProcessor*> (&pack)) o->easyView = easy != 0;
                    std::unique_ptr<juce::AudioProcessorEditor> ed (pack.createEditor());
                    ed->setSize (phone.w, phone.h);
                    for (int i = 0; i < 5; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
                    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
                    juce::FileOutputStream os (dir.getChildFile (juce::String ("phone_") + phone.name + "_" + StationProcessor::packNames[p].toLowerCase()
                                                                 + (easy ? "_easy" : "_full") + ".png"));
                    os.setPosition (0); os.truncate();
                    juce::PNGImageFormat().writeImageToStream (img, os);
                }
            // The whole Station frame (pack buttons + embedded pack) and the controller setup page.
            {
                std::unique_ptr<juce::AudioProcessorEditor> ed (st.createEditor());
                ed->setSize (phone.w, phone.h);
                auto snap = [&] (const juce::String& name)
                {
                    for (int i = 0; i < 5; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
                    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
                    juce::FileOutputStream os (dir.getChildFile (juce::String ("phone_") + phone.name + "_station_" + name + ".png"));
                    os.setPosition (0); os.truncate();
                    juce::PNGImageFormat().writeImageToStream (img, os);
                };
                for (int p = 0; p < StationProcessor::numPacks; ++p)
                {
                    st.selectPack (p);
                    snap (StationProcessor::packNames[p].toLowerCase());
                }
                for (auto* c : ed->getChildren())
                    if (auto* b = dynamic_cast<juce::TextButton*> (c); b != nullptr && b->getButtonText() == "SETUP")
                        b->triggerClick();
                snap ("setup");
                st.selectPack (0);
            }
            hw::safeAreaOverride() = {};
        }
        std::cout << "Phone snapshots done\n";
        return 0;
    }

    //==========================================================================
    std::cout << "\nOrchestra presets\n";
    st.selectPack (2);
    auto& presets = orch::getPresets();
    for (int i = 0; i < (int) presets.size(); ++i)
    {
        orchestra.setCurrentProgram (i);
        wait (st, 0.1);
        auto r = render (st, praisePhrase(), 8.0);
        auto name = juce::String (presets[(size_t) i].name);
        writeWav (r.audio, root.getChildFile ("renders").getChildFile (juce::String (i + 1).paddedLeft ('0', 2) + "_" + name.replace (" ", "_").replace ("+", "and") + ".wav"));
        float rms = 0;
        for (int ch = 0; ch < 2; ++ch) rms = juce::jmax (rms, r.audio.getRMSLevel (ch, 0, r.audio.getNumSamples()));
        std::cout << "  " << name.paddedRight (' ', 24) << " peak " << juce::String (db (r.audio.getMagnitude (0, r.audio.getNumSamples())), 1)
                  << " dB   rms " << juce::String (db (rms), 1) << " dB   " << (r.finite ? "ok" : "NaN!") << "\n";
        check (r.finite && r.audio.getMagnitude (0, r.audio.getNumSamples()) > 0.05f, name + " renders cleanly");
        wait (st, 3.0);
    }

    // Long held chord: sustains must loop, not fade out when the recordings run out (~8 s).
    for (auto index : { 6, 7, 8 })
    {
        orchestra.setCurrentProgram (index);
        wait (st, 0.1);
        std::vector<Ev> ev;
        chord (ev, 0.0, 18.0, { 45, 62, 66, 69 }, 100);
        auto r = render (st, ev, 20.0);
        auto rmsAt = [&] (double from, double to)
        {
            auto s0 = (int) (from * rate), n = (int) ((to - from) * rate);
            return juce::jmax (r.audio.getRMSLevel (0, s0, n), r.audio.getRMSLevel (1, s0, n));
        };
        auto early = db (rmsAt (2, 5)), late = db (rmsAt (13, 17));
        check (r.finite && late > early - 6.0f, juce::String (presets[(size_t) index].name) + " holds 18 s ("
                                                    + juce::String (early, 1) + " -> " + juce::String (late, 1) + " dB)");
        writeWav (r.audio, root.getChildFile ("renders").getChildFile ("hold_" + juce::String (index + 1) + ".wav"));
        wait (st, 4.0);
    }

    //==========================================================================
    std::cout << "\nSwitching + routing\n";
    keys.setCurrentProgram (0);
    orchestra.setCurrentProgram (0);
    st.getPack (1).setCurrentProgram (0);

    st.selectPack (0);
    wait (st, 4.0);
    {
        std::vector<Ev> ev;
        chord (ev, 0.0, 0.5, { 60, 64, 67 }, 100);
        auto r = render (st, ev, 1.0);
        check (r.packPeak[0] > 0.01f && r.packPeak[1] < 1e-4f && r.packPeak[2] < 1e-4f, "notes go to the selected pack only (Keys)");
    }

    // Learn the wheel: an Arturia-style endless encoder (65 = one step clockwise).
    st.startLearn (StationProcessor::wheel);
    render (st, { { 0.0, cc (114, 65) } }, 0.05);
    check (st.describeBinding (StationProcessor::wheel) == "CC 114 (ch 1)", "wheel learned: " + st.describeBinding (StationProcessor::wheel));
    check (st.getWheelMode() == StationProcessor::relative64, "wheel detected as endless 64 +/-");
    check (st.getSelectedPack() == 0, "learning doesn't switch");

    render (st, { { 0.0, cc (114, 65) }, { 0.02, cc (114, 65) } }, 0.2);
    check (st.getSelectedPack() == 1, "one detent -> Synth (fast double message debounced)");
    render (st, { { 0.0, cc (114, 66) } }, 0.2);
    check (st.getSelectedPack() == 2, "next detent -> Orchestra");
    render (st, { { 0.0, cc (114, 65) } }, 0.2);
    check (st.getSelectedPack() == 0, "wraps around -> Keys");
    render (st, { { 0.0, cc (114, 63) } }, 0.2);
    check (st.getSelectedPack() == 2, "turning back -> Orchestra");

    // A held note ends on the pack it started on.
    st.selectPack (1);
    wait (st, 4.0);
    {
        auto r = render (st, { { 0.0, on (60, 100) }, { 0.5, cc (114, 65) }, { 1.0, off (60) } }, 8.0);
        check (st.getSelectedPack() == 2, "switched while holding");
        check (r.packPeak[1] > 0.01f && r.packTailPeak[1] < 0.003f,
               "note-off reached the Synth after switching away (tail " + juce::String (db (r.packTailPeak[1]), 1) + " dB)");
        check (r.packPeak[2] < 1e-4f, "the switched-to pack didn't get the old note-off as a sound");
    }

    // Pad buttons: note 36 on ch 10 = next pack, swallowed (never played).
    st.startLearn (StationProcessor::nextPack);
    render (st, { { 0.0, on (36, 90, 10) }, { 0.05, off (36, 10) } }, 0.1);
    check (st.describeBinding (StationProcessor::nextPack) == "Note C1 (ch 10)", "next-pack pad learned: " + st.describeBinding (StationProcessor::nextPack));
    st.selectPack (0);
    wait (st, 4.0);
    {
        auto r = render (st, { { 0.0, on (36, 90, 10) }, { 0.1, off (36, 10) } }, 0.5);
        check (st.getSelectedPack() == 1, "pad press -> next pack");
        check (r.packPeak[0] < 1e-4f && r.packPeak[1] < 1e-4f, "the pad note itself is silent");
    }

    // Program change straight to a pack.
    st.startLearn (StationProcessor::selectOrch);
    render (st, { { 0.0, juce::MidiMessage::programChange (1, 2) } }, 0.05);
    st.selectPack (0);
    render (st, { { 0.0, juce::MidiMessage::programChange (1, 2) } }, 0.05);
    check (st.getSelectedPack() == 2, "program change 3 -> Orchestra");

    // Knob / fader as the selector: three zones.
    st.setWheelMode (StationProcessor::absolute);
    render (st, { { 0.0, cc (114, 10) } }, 0.05);
    auto a = st.getSelectedPack();
    render (st, { { 0.0, cc (114, 64) } }, 0.05);
    auto b = st.getSelectedPack();
    render (st, { { 0.0, cc (114, 120) } }, 0.05);
    auto c = st.getSelectedPack();
    check (a == 0 && b == 1 && c == 2, "knob zones -> Keys / Synth / Orchestra");

    // Split: each pack on its own channel (defaults 1 / 2 / 3), all playable at once.
    st.setRouting (StationProcessor::splitInputs);
    st.selectPack (0);
    wait (st, 5.0);
    {
        auto r = render (st, { { 0.0, on (60, 100, 2) }, { 0.5, off (60, 2) } }, 1.0);
        check (r.packPeak[1] > 0.01f && r.packPeak[0] < 1e-4f && r.packPeak[2] < 1e-4f, "split: channel 2 plays only the Synth");
        auto r2 = render (st, { { 0.0, on (48, 110, 1) }, { 0.0, on (62, 110, 3) }, { 0.0, on (66, 110, 3) }, { 0.3, off (48, 1) }, { 0.3, off (62, 3) }, { 0.3, off (66, 3) } }, 1.0);
        check (r2.packPeak[0] > 0.01f && r2.packPeak[2] > 0.01f, "split: Keys (ch 1) + Orchestra (ch 3) together");
    }
    st.setPackChannel (1, 0);
    {
        wait (st, 5.0);
        auto r = render (st, { { 0.0, on (60, 100, 7) }, { 0.5, off (60, 7) } }, 1.0);
        check (r.packPeak[1] > 0.01f && r.packPeak[0] < 1e-4f, "split: Synth on 'any channel' hears ch 7");
    }

    // State round trip keeps the selected pack and the packs' own presets.
    st.setRouting (StationProcessor::switchPacks);
    st.selectPack (2);
    orchestra.setCurrentProgram (3);
    juce::MemoryBlock state;
    st.getStateInformation (state);
    StationProcessor st2;
    st2.setStateInformation (state.getData(), (int) state.getSize());
    check (st2.getSelectedPack() == 2 && st2.getPack (2).getCurrentProgram() == 3, "project state restores pack + orchestra preset");

    std::cout << "\n" << (failures == 0 ? "ALL TESTS PASSED" : juce::String (failures) + " FAILURE(S)") << "\n";
    return failures == 0 ? 0 : 2;
}
