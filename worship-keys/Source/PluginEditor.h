#pragma once
#include "PluginProcessor.h"
#include "HardwareLook.h"

class NordLookAndFeel : public juce::LookAndFeel_V4
{
public:
    NordLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float min, float max, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (14.0f, juce::Font::bold)); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return juce::Font (juce::FontOptions (juce::jmin (13.0f, h * 0.55f), juce::Font::bold)); }
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

// A slider that reports right-clicks (for the MIDI learn menu) instead of treating them as drags.
struct LearnSlider : public juce::Slider
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

class WorshipKeysEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit WorshipKeysEditor (WorshipKeysProcessor&);
    ~WorshipKeysEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Control
    {
        juce::String paramId, label;
        std::unique_ptr<LearnSlider> slider;
        std::unique_ptr<juce::ComboBox> combo;
        std::unique_ptr<juce::TextButton> learnButton; // Easy page faders only
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachment;
        juce::Rectangle<int> bounds;
    };

    struct Section
    {
        juce::String title;
        std::vector<Control*> controls;
        juce::Rectangle<int> bounds;
    };

    // Hosts a page of controls; the full page scrolls when the screen is small (phones).
    struct Panel : public juce::Component
    {
        std::function<void (juce::Graphics&)> onPaint;
        void paint (juce::Graphics& g) override { if (onPaint) onPaint (g); }
    };

    void timerCallback() override;
    void paintSections (juce::Graphics&);
    void paintEasy (juce::Graphics&);
    void setupSlider (Control&, juce::Component& parent);
    Control* addKnob (const juce::String& id, const juce::String& label);
    Control* addFader (const juce::String& id, const juce::String& label);
    Control* addChoice (const juce::String& id, const juce::String& label, const juce::StringArray& items, int firstItemId);
    void showMidiMenu (const juce::String& paramId);
    void toggleLearn (const juce::String& paramId);
    void setEasyView (bool easy);
    void stepPreset (int delta);
    void layoutRow (std::vector<Section*> row, juce::Rectangle<int> area);
    void layoutEasy();
    juce::String midiSignature() const;

    WorshipKeysProcessor& proc;
    NordLookAndFeel lnf;
    std::vector<std::unique_ptr<Control>> controls, faders;
    std::vector<std::unique_ptr<Section>> sections;
    std::vector<std::vector<Section*>> wideRows, compactRows;
    Panel panel, easyPanel;
    juce::Viewport viewport;
    juce::Rectangle<int> titleArea;
    bool compact = false;

    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, easyButton { "EASY" }, fullButton { "FULL" };
    juce::Label statusLabel;
    hw::Keyboard keyboard;
    hw::CachedBackground background;
    int shownProgram = -1, blink = 0;
    juce::String shownMidi;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WorshipKeysEditor)
};
