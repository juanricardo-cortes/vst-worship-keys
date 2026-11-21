#pragma once
// MIDI learn for the Anthem Synth and Praise Orchestra packs (Worship Keys has its own, older copy of the same idea):
// a CC moves one parameter. Mappings are global (one file per instrument, shared by every project), not per song.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <atomic>

namespace midilearn
{
class Map
{
public:
    // folder: the instrument's folder in %APPDATA% (or ~/Library/Application Support, etc).
    Map (juce::AudioProcessorValueTreeState& state, const juce::String& folder)
        : apvts (state),
          file (juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile (folder).getChildFile ("midi-map.xml"))
    {
        for (auto& m : ccMap) m = -1;
        load();
    }

    static inline bool persist = true; // the test tools turn this off so they never touch your setup

    // CCs that already mean something to a keyboard rig: never learn these.
    static bool isLearnable (int cc) { return cc != 64 && cc != 66 && cc != 67 && cc < 120; }

    // Audio thread.
    void handleCC (int cc, int value)
    {
        if (! isLearnable (cc))
            return;

        auto learning = learnParam.load();
        if (learning >= 0)
        {
            for (auto& m : ccMap) // one controller per parameter
                if (m.load() == learning) m = -1;
            ccMap[(size_t) cc] = learning;
            learnParam = -1;
            changed = true;
        }

        auto index = ccMap[(size_t) cc].load();
        auto& params = apvts.processor.getParameters();
        if (juce::isPositiveAndBelow (index, params.size()))
            params[index]->setValueNotifyingHost ((float) value / 127.0f);
    }

    void startLearn (const juce::String& id) { learnParam = indexOf (id); }
    void cancelLearn() { learnParam = -1; }
    bool isLearning (const juce::String& id) const { auto i = learnParam.load(); return i >= 0 && i == indexOf (id); }
    bool isAnyLearning() const { return learnParam.load() >= 0; }

    int getMappedCC (const juce::String& id) const // -1 if none
    {
        auto index = indexOf (id);
        for (size_t cc = 0; cc < ccMap.size(); ++cc)
            if (index >= 0 && ccMap[cc].load() == index)
                return (int) cc;
        return -1;
    }

    void clear (const juce::String& id)
    {
        auto index = indexOf (id);
        for (auto& m : ccMap)
            if (index >= 0 && m.load() == index) m = -1;
        save();
    }

    void clearAll()
    {
        for (auto& m : ccMap) m = -1;
        save();
    }

    // Message thread (the editors' timers): writes the file after a learn on the audio thread.
    bool saveIfChanged()
    {
        if (! changed.exchange (false))
            return false;
        save();
        return true;
    }

    // A slider-change signature, so an editor only repaints its CC labels when something changed.
    juce::String signature (const juce::StringArray& ids) const
    {
        juce::String sig;
        for (auto& id : ids)
            sig << id << getMappedCC (id) << (isLearning (id) ? "L" : "") << ";";
        return sig;
    }

private:
    int indexOf (const juce::String& id) const
    {
        if (auto* p = apvts.getParameter (id))
            return p->getParameterIndex();
        return -1;
    }

    void save()
    {
        if (! persist)
            return;
        juce::XmlElement xml ("MidiMap");
        auto& params = apvts.processor.getParameters();
        for (size_t cc = 0; cc < ccMap.size(); ++cc)
            if (auto index = ccMap[cc].load(); juce::isPositiveAndBelow (index, params.size()))
                if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (params[index]))
                {
                    auto* e = xml.createNewChildElement ("Map");
                    e->setAttribute ("cc", (int) cc);
                    e->setAttribute ("param", ranged->getParameterID());
                }
        file.getParentDirectory().createDirectory();
        xml.writeTo (file);
    }

    void load()
    {
        if (! persist)
            return;
        if (auto xml = juce::XmlDocument::parse (file))
            for (auto* e : xml->getChildWithTagNameIterator ("Map"))
            {
                auto cc = e->getIntAttribute ("cc", -1);
                auto index = indexOf (e->getStringAttribute ("param"));
                if (juce::isPositiveAndBelow (cc, 128) && isLearnable (cc) && index >= 0)
                    ccMap[(size_t) cc] = index;
            }
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::File file;
    std::array<std::atomic<int>, 128> ccMap;   // CC number -> parameter index, -1 = unmapped
    std::atomic<int> learnParam { -1 };
    std::atomic<bool> changed { false };
};

// A slider that reports right-clicks (for the MIDI learn menu) instead of treating them as drags.
struct Slider : public juce::Slider
{
    using juce::Slider::Slider;
    std::function<void()> onRightClick;
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() && onRightClick) { rightClicked = true; onRightClick(); return; }
        rightClicked = false;
        juce::Slider::mouseDown (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override { if (! rightClicked) juce::Slider::mouseDrag (e); }
    void mouseUp (const juce::MouseEvent& e) override   { if (! rightClicked) juce::Slider::mouseUp (e); }
    bool rightClicked = false;
};

// The right-click menu on any mappable control.
inline void showMenu (Map& map, juce::AudioProcessorValueTreeState& apvts, const juce::String& id, juce::Component& target)
{
    juce::PopupMenu m;
    if (auto* p = apvts.getParameter (id))
        m.addSectionHeader (p->getName (40));
    if (map.isLearning (id))
        m.addItem ("Cancel MIDI Learn", [&map] { map.cancelLearn(); });
    else
        m.addItem ("MIDI Learn (then move a knob or slider on your controller)", [&map, id] { map.startLearn (id); });

    auto cc = map.getMappedCC (id);
    m.addItem ("Clear MIDI mapping" + (cc >= 0 ? " (CC " + juce::String (cc) + ")" : juce::String()), cc >= 0,
               false, [&map, id] { map.clear (id); });
    m.addSeparator();
    m.addItem ("Clear ALL MIDI mappings", [&map] { map.clearAll(); });
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target).withMousePosition());
}

// What the CC readout under a control says.
inline juce::String describe (const Map& map, const juce::String& id)
{
    if (map.isLearning (id)) return "waiting for MIDI...";
    auto cc = map.getMappedCC (id);
    return cc >= 0 ? "CC " + juce::String (cc) : juce::String ("not mapped");
}
} // namespace midilearn
