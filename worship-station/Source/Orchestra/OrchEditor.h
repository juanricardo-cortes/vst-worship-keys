#pragma once
#include "OrchProcessor.h"

namespace orch
{
class OrchLookAndFeel : public juce::LookAndFeel_V4
{
public:
    OrchLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float min, float max, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    juce::Font getComboBoxFont (juce::ComboBox& b) override { return juce::Font (juce::FontOptions (b.getWidth() < 90 ? 11.0f : 13.0f, juce::Font::bold)); }
    void positionComboBoxText (juce::ComboBox& b, juce::Label& l) override
    {
        l.setBounds (3, 1, b.getWidth() - 19, b.getHeight() - 2);
        l.setFont (getComboBoxFont (b));
        l.setMinimumHorizontalScale (0.7f);
    }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return juce::Font (juce::FontOptions (juce::jmin (14.0f, h * 0.5f), juce::Font::bold)); }
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

class OrchestraEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit OrchestraEditor (OrchestraProcessor&);
    ~OrchestraEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Control
    {
        juce::String paramId, label;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::ComboBox> combo;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachment;
    };

    void timerCallback() override;
    Control& addSlider (const juce::String& id, const juce::String& label, bool fader, juce::Component& parent);
    Control& addChoice (const juce::String& id, const juce::String& label);
    void stepPreset (int delta);
    void setArtic (int index);
    void setEasyView (bool easy);
    void placeRow (std::vector<Control*>& row, juce::Rectangle<int> area, int labelH);
    void layoutFullPage (juce::Rectangle<int> area);
    void layoutEasyPage();
    void paintFullPage (juce::Graphics&);
    void paintEasyPage (juce::Graphics&);

    OrchestraProcessor& proc;
    OrchLookAndFeel lnf;
    std::vector<std::unique_ptr<Control>> controls;
    std::vector<Control*> faders, playKnobs, mixKnobs, easyFaders;
    juce::Rectangle<int> faderArea, playArea, mixArea, hintArea, easyArticArea, titleArea;
    hw::Page fullPage, easyPage;
    juce::Viewport viewport;   // scrolls the full page on small screens
    bool compact = false;

    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, easyButton { "EASY" }, fullButton { "FULL" };
    std::array<juce::TextButton, 4> articButtons, easyArticButtons;
    juce::Label statusLabel;
    hw::Keyboard keyboard;
    hw::CachedBackground background, fullBackground, easyBackground;
    int shownProgram = -1, shownArtic = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OrchestraEditor)
};
} // namespace orch
