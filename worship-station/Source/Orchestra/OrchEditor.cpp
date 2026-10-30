#include "OrchEditor.h"
#include "OrchPresets.h"

namespace orch
{
namespace colours
{
const juce::Colour body { 0xff17140f };
const juce::Colour panel { 0xff221e17 };
const juce::Colour knob { 0xff0e0c09 };
const juce::Colour gold { 0xffe0a83a };
const juce::Colour text { 0xfff4efe6 };
const juce::Colour dim { 0xffa39a8a };
const juce::Colour line { 0xff3d3528 };
}

OrchLookAndFeel::OrchLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, colours::dim);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::backgroundColourId, colours::knob);
    setColour (juce::ComboBox::textColourId, colours::gold.brighter (0.3f));
    setColour (juce::ComboBox::outlineColourId, colours::line);
    setColour (juce::ComboBox::arrowColourId, colours::gold);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::gold.darker (0.4f));
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::TextButton::buttonColourId, colours::knob);
    setColour (juce::TextButton::buttonOnColourId, colours::gold);
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
}

void OrchLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&)
{
    hw::drawKnob (g, juce::Rectangle<int> (x, y, w, h).toFloat().reduced (2.0f), pos, start, end, hw::KnobStyle::brass, colours::gold);
}

void OrchLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float min, float max,
                                        juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearVertical)
        return LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, min, max, style, s);
    hw::drawFader (g, juce::Rectangle<int> (x, y, w, h).toFloat(), pos, colours::gold, colours::dim, 28.0f, 52.0f);
}

void OrchLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    hw::drawLCD (g, juce::Rectangle<int> (w, h).toFloat(), colours::gold, box.isEnabled());
    hw::drawArrow (g, (float) w - 14.0f, (float) h * 0.5f, colours::gold);
}

void OrchLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    hw::drawButton (g, b.getLocalBounds().toFloat(), b.getToggleState(), over, down, colours::gold);
}

juce::Label* OrchLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (juce::Font (juce::FontOptions (11.0f)));
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    return l;
}

//==============================================================================
OrchestraEditor::OrchestraEditor (OrchestraProcessor& p)
    : AudioProcessorEditor (p), proc (p), keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard, colours::gold)
{
    setLookAndFeel (&lnf);

    // The pages go in first so the controls built below pick up the look-and-feel.
    fullPage.onPaint = [this] (juce::Graphics& g) { paintFullPage (g); };
    viewport.setViewedComponent (&fullPage, false);
    viewport.setScrollBarsShown (false, false, true, false);
    addChildComponent (viewport);
    easyPage.onPaint = [this] (juce::Graphics& g) { paintEasyPage (g); };
    addChildComponent (easyPage);

    for (auto [id, label] : { std::pair { "strings", "STRINGS" }, { "brass", "BRASS" }, { "lowEnd", "LOW END" }, { "timpani", "TIMPANI" },
                              { "hits", "CYMBALS" }, { "octUp", "OCT UP" }, { "octDown", "OCT DOWN" }, { "ensemble", "SIZE" } })
        faders.push_back (&addSlider (id, label, true, fullPage));

    for (auto [id, label] : { std::pair { "holdTime", "HOLD TIME" }, { "susAttack", "SWELL" }, { "release", "RELEASE" },
                              { "velSens", "VELOCITY" }, { "split", "L.H. SPLIT" }, { "accent", "ACCENT AT" } })
        playKnobs.push_back (&addSlider (id, label, false, fullPage));

    for (auto [id, label] : { std::pair { "hpf", "LOW CUT" }, { "eqLow", "LOW" }, { "eqLowMid", "LOW-MID" }, { "eqPresence", "PRESENCE" },
                              { "eqAir", "AIR" }, { "comp", "GLUE" }, { "drive", "DRIVE" }, { "revMix", "HALL" }, { "revDecay", "DECAY" },
                              { "revSize", "SIZE" }, { "revPreDelay", "PRE-DLY" }, { "revTone", "TONE" }, { "delayMix", "DELAY" },
                              { "delayFeedback", "REPEATS" } })
        mixKnobs.push_back (&addSlider (id, label, false, fullPage));
    mixKnobs.push_back (&addChoice ("delayTime", "DLY TIME"));
    mixKnobs.push_back (&addSlider ("master", "MASTER", false, fullPage));

    // The Easy page: section levels, swell, hall and master on big faders.
    for (auto [id, label] : { std::pair { "strings", "STRINGS" }, { "brass", "BRASS" }, { "lowEnd", "LOW END" }, { "timpani", "TIMPANI" },
                              { "hits", "CYMBALS" }, { "susAttack", "SWELL" }, { "revMix", "HALL" }, { "master", "MASTER" } })
        easyFaders.push_back (&addSlider (id, label, true, easyPage));

    static const char* articNames[] = { "STABS", "SUSTAIN", "STAB + HOLD", "MARCATO" };
    for (int i = 0; i < 4; ++i)
    {
        for (auto* set : { &articButtons, &easyArticButtons })
        {
            auto& b = (*set)[(size_t) i];
            b.setButtonText (articNames[i]);
            b.setClickingTogglesState (false);
            b.onClick = [this, i] { setArtic (i); };
        }
        fullPage.addAndMakeVisible (articButtons[(size_t) i]);
        easyPage.addAndMakeVisible (easyArticButtons[(size_t) i]);
    }

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.onChange = [this]
    {
        auto index = presetBox.getSelectedId() - 1;
        if (index >= 0 && index != proc.getCurrentProgram())
            proc.setCurrentProgram (index);
    };
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    easyButton.onClick = [this] { setEasyView (true); };
    fullButton.onClick = [this] { setEasyView (false); };
    addAndMakeVisible (presetBox);
    for (auto* b : { &prevButton, &nextButton, &easyButton, &fullButton })
        addAndMakeVisible (*b);

    statusLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    statusLabel.setColour (juce::Label::textColourId, colours::dim);
    statusLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (statusLabel);

    keyboard.setAvailableRange (24, 96);
    keyboard.setLowestVisibleKey (24);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, colours::gold);
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, colours::gold.withAlpha (0.3f));
    keyboard.setWantsKeyboardFocus (false);
    addAndMakeVisible (keyboard);

    setSize (1120, 640);
    setEasyView (proc.easyView);
    startTimerHz (15);
    timerCallback();
}

OrchestraEditor::~OrchestraEditor()
{
    setLookAndFeel (nullptr);
}

OrchestraEditor::Control& OrchestraEditor::addSlider (const juce::String& id, const juce::String& label, bool fader, juce::Component& parent)
{
    auto c = std::make_unique<Control>();
    c->paramId = id;
    c->label = label;
    c->slider = std::make_unique<juce::Slider> (fader ? juce::Slider::LinearVertical : juce::Slider::RotaryHorizontalVerticalDrag,
                                                juce::Slider::TextBoxBelow);
    parent.addAndMakeVisible (*c->slider);   // pick up our look-and-feel before the value box is built
    c->slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
    c->slider->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    c->slider->setViewportIgnoreDragFlag (true); // dragging a control moves it instead of scrolling the page
    c->sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, *c->slider);
    controls.push_back (std::move (c));
    return *controls.back();
}

OrchestraEditor::Control& OrchestraEditor::addChoice (const juce::String& id, const juce::String& label)
{
    auto c = std::make_unique<Control>();
    c->paramId = id;
    c->label = label;
    c->combo = std::make_unique<juce::ComboBox>();
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
        c->combo->addItemList (choice->choices, 1);
    c->comboAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, *c->combo);
    c->combo->setViewportIgnoreDragFlag (true);
    fullPage.addAndMakeVisible (*c->combo);
    controls.push_back (std::move (c));
    return *controls.back();
}

void OrchestraEditor::stepPreset (int delta)
{
    auto n = proc.getNumPrograms();
    proc.setCurrentProgram ((proc.getCurrentProgram() + delta + n) % n);
}

void OrchestraEditor::setArtic (int index)
{
    if (auto* p = proc.apvts.getParameter ("artic"))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) index));
}

void OrchestraEditor::setEasyView (bool easy)
{
    proc.easyView = easy;
    easyButton.setToggleState (easy, juce::dontSendNotification);
    fullButton.setToggleState (! easy, juce::dontSendNotification);
    easyPage.setVisible (easy);
    viewport.setVisible (! easy);
}

void OrchestraEditor::timerCallback()
{
    if (proc.getCurrentProgram() != shownProgram)
    {
        shownProgram = proc.getCurrentProgram();
        presetBox.setSelectedId (shownProgram + 1, juce::dontSendNotification);
    }

    auto artic = (int) std::lround (proc.apvts.getRawParameterValue ("artic")->load());
    if (artic != shownArtic)
    {
        shownArtic = artic;
        for (int i = 0; i < 4; ++i)
        {
            articButtons[(size_t) i].setToggleState (i == artic, juce::dontSendNotification);
            easyArticButtons[(size_t) i].setToggleState (i == artic, juce::dontSendNotification);
        }
    }

    statusLabel.setText (proc.samplesReady() ? proc.getSampleStatus() + "   |   " + juce::String (proc.getActiveVoices()) + " voices"
                                             : proc.getSampleStatus(),
                         juce::dontSendNotification);
}

void OrchestraEditor::paint (juce::Graphics& g)
{
    background.draw (g, getLocalBounds(), [this] (juce::Graphics& bg, juce::Rectangle<float> r)
    {
        hw::drawChassis (bg, r, colours::body);
        // walnut header strip with a brass rail underneath
        auto head = r.withBottom ((float) titleArea.getBottom());
        hw::drawWood (bg, head);
        bg.setGradientFill (juce::ColourGradient (juce::Colour (0xfff0cf80), 0, head.getBottom(), juce::Colour (0xff7a5a22), 0, head.getBottom() + 3, false));
        bg.fillRect (head.withY (head.getBottom()).withHeight (3.0f));
        if (! compact)
        {
            hw::drawScrew (bg, { 10.0f, head.getCentreY() }, 3.5f, 0.5f);
            hw::drawScrew (bg, { r.getRight() - 10.0f, head.getCentreY() }, 3.5f, 1.4f);
        }

        auto title = titleArea.toFloat();
        bg.setFont (juce::Font (juce::FontOptions (compact ? 16.0f : 24.0f, juce::Font::bold)).withExtraKerningFactor (compact ? 0.04f : 0.08f));
        bg.setColour (juce::Colours::black.withAlpha (0.6f));
        bg.drawText ("PRAISE ORCHESTRA", title.translated (0, 1.5f), juce::Justification::centredLeft);
        bg.setGradientFill (juce::ColourGradient (juce::Colour (0xfffbe3a2), 0, title.getCentreY() - 8, juce::Colour (0xffb8862e), 0, title.getCentreY() + 8, false));
        bg.drawText ("PRAISE ORCHESTRA", title, juce::Justification::centredLeft);

        if (keyboard.isVisible())
            hw::drawInset (bg, keyboard.getBounds().toFloat().expanded (3.0f), juce::Colour (0xff0a0806), 3.0f);
    });
}

void OrchestraEditor::paintFullPage (juce::Graphics& g)
{
    fullBackground.draw (g, fullPage.getLocalBounds(), [this] (juce::Graphics& bg, juce::Rectangle<float>)
    {
        auto plate = juce::Colour (0xff221e18);
        hw::drawPlate (bg, faderArea.toFloat(), plate, colours::gold, "SECTIONS", 6.0f, ! compact);
        hw::drawPlate (bg, playArea.toFloat(), plate, colours::gold, "PLAYING", 6.0f, ! compact);
        hw::drawPlate (bg, mixArea.toFloat(), plate, colours::gold, "MIX + HALL", 6.0f, ! compact);

        bg.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        bg.setColour (colours::text);
        for (auto* c : faders)
            bg.drawText (c->label, c->slider->getBounds().translated (0, -18).withHeight (16).expanded (10, 0), juce::Justification::centred);
        for (auto* group : { &playKnobs, &mixKnobs })
            for (auto* c : *group)
            {
                juce::Component* comp = c->slider != nullptr ? (juce::Component*) c->slider.get() : (juce::Component*) c->combo.get();
                bg.drawText (c->label, comp->getBounds().translated (0, -16).withHeight (14).expanded (10, 0), juce::Justification::centred);
            }

        if (! hintArea.isEmpty())
        {
            bg.setColour (colours::dim);
            bg.setFont (juce::Font (juce::FontOptions (11.0f)));
            bg.drawText ("R. hand: strings + brass  |  L. hand: low end + timpani  |  Hit hard: cymbals  |  Mod wheel: swell",
                         hintArea, juce::Justification::centredLeft);
        }
    });
}

void OrchestraEditor::paintEasyPage (juce::Graphics& g)
{
    easyBackground.draw (g, easyPage.getLocalBounds(), [this] (juce::Graphics& bg, juce::Rectangle<float>)
    {
        auto plate = juce::Colour (0xff221e18);
        hw::drawPlate (bg, easyArticArea.toFloat(), plate, colours::gold, {}, 8.0f, false);
        for (auto* c : easyFaders)
        {
            auto cell = c->slider->getBounds().expanded (4, 0).withTop (easyArticArea.getBottom() + (compact ? 6 : 10))
                                  .withBottom (easyPage.getHeight());
            hw::drawPlate (bg, cell.toFloat(), plate, colours::gold, {}, 8.0f, false);
            bg.setFont (juce::Font (juce::FontOptions (compact ? 13.0f : 16.0f, juce::Font::bold)));
            bg.setColour (colours::text);
            bg.drawText (c->label, cell.reduced (2, 4).removeFromTop (compact ? 18 : 26), juce::Justification::centred);
        }
    });
}

void OrchestraEditor::placeRow (std::vector<Control*>& row, juce::Rectangle<int> area, int labelH)
{
    auto w = area.getWidth() / (int) row.size();
    for (auto* c : row)
    {
        auto cell = area.removeFromLeft (w).reduced (3, 0);
        cell.removeFromTop (labelH);
        if (c->slider != nullptr) c->slider->setBounds (cell);
        else c->combo->setBounds (cell.withSizeKeepingCentre (cell.getWidth(), 26));
    }
}

void OrchestraEditor::layoutFullPage (juce::Rectangle<int> area)
{
    auto placeArtic = [this] (juce::Rectangle<int> buttons)
    {
        auto bw = buttons.getWidth() / 2, bh = buttons.getHeight() / 2;
        for (int i = 0; i < 4; ++i)
            articButtons[(size_t) i].setBounds (juce::Rectangle<int> (buttons.getX() + (i % 2) * bw, buttons.getY() + (i / 2) * bh, bw, bh).reduced (4));
    };
    std::vector<Control*> play1 (playKnobs.begin(), playKnobs.begin() + 3), play2 (playKnobs.begin() + 3, playKnobs.end());

    if (! compact)
    {
        // Desktop: everything on one screen.
        fullPage.setSize (area.getWidth(), area.getHeight());
        auto page = fullPage.getLocalBounds();
        mixArea = page.removeFromBottom (118);
        page.removeFromBottom (10);
        faderArea = page.removeFromLeft (560);
        page.removeFromLeft (10);
        playArea = page;

        auto f = faderArea.reduced (10, 8);
        f.removeFromTop (16);
        placeRow (faders, f, 20);

        auto p = playArea.reduced (12, 8);
        p.removeFromTop (20);
        hintArea = p.removeFromBottom (16).withTrimmedLeft (6);
        p.removeFromBottom (2);
        placeArtic (p.removeFromTop (88));
        p.removeFromTop (8);
        placeRow (play1, p.removeFromTop (p.getHeight() / 2), 16);
        placeRow (play2, p, 16);

        auto m = mixArea.reduced (10, 6);
        m.removeFromTop (18);
        placeRow (mixKnobs, m, 16);
        return;
    }

    // Phone: the three panels stack and the page scrolls.
    const int gap = 8, sectionsH = 200, playH = 136, mixH = 236;
    fullPage.setSize (area.getWidth(), sectionsH + playH + mixH + 2 * gap);
    auto page = fullPage.getLocalBounds();
    faderArea = page.removeFromTop (sectionsH);
    page.removeFromTop (gap);
    playArea = page.removeFromTop (playH);
    page.removeFromTop (gap);
    mixArea = page;
    hintArea = {};

    auto f = faderArea.reduced (8, 6);
    f.removeFromTop (18);
    placeRow (faders, f, 18);

    auto p = playArea.reduced (8, 6);
    p.removeFromTop (20);
    placeArtic (p.removeFromLeft (juce::jmin (280, p.getWidth() / 3)));
    p.removeFromLeft (6);
    placeRow (playKnobs, p, 16);

    auto m = mixArea.reduced (8, 6);
    m.removeFromTop (20);
    std::vector<Control*> mix1 (mixKnobs.begin(), mixKnobs.begin() + 8), mix2 (mixKnobs.begin() + 8, mixKnobs.end());
    placeRow (mix1, m.removeFromTop (m.getHeight() / 2), 16);
    placeRow (mix2, m, 16);
}

void OrchestraEditor::layoutEasyPage()
{
    auto area = easyPage.getLocalBounds();
    easyArticArea = area.removeFromTop (compact ? 50 : 64);
    auto buttons = easyArticArea.reduced (6);
    auto bw = buttons.getWidth() / 4;
    for (auto& b : easyArticButtons)
        b.setBounds (buttons.removeFromLeft (bw).reduced (4, 0));
    area.removeFromTop (compact ? 6 : 10);

    const int gap = compact ? 6 : 10, n = (int) easyFaders.size();
    auto w = (area.getWidth() - gap * (n - 1)) / n;
    for (auto* c : easyFaders)
    {
        auto cell = area.removeFromLeft (w).reduced (4, 0);
        area.removeFromLeft (gap);
        cell.removeFromTop (compact ? 20 : 30);
        cell.removeFromBottom (6);
        c->slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, juce::jmin (80, cell.getWidth()), compact ? 16 : 20);
        c->slider->setBounds (cell);
    }
}

void OrchestraEditor::resized()
{
    auto safe = hw::safeArea (*this); // keeps clear of the notch / home bar on a phone
    compact = hw::isCompact (safe);

    auto headH = compact ? 46 : 56;
    auto headRect = safe.removeFromTop (headH);
    auto top = headRect.reduced (compact ? 12 : 20, compact ? 7 : 12);
    titleArea = top.removeFromLeft (compact ? 170 : 290).withY (headRect.getY()).withHeight (headH).withTrimmedLeft (compact ? 0 : 4);
    auto buttonH = top.getHeight();

    auto views = top.removeFromRight (compact ? 116 : 130);
    auto half = views.getWidth() / 2;
    fullButton.setBounds (views.removeFromRight (half).reduced (2, 0));
    easyButton.setBounds (views.reduced (2, 0));
    top.removeFromRight (12);

    statusLabel.setVisible (top.getWidth() > 620);
    if (statusLabel.isVisible())
        statusLabel.setBounds (top.removeFromRight (top.getWidth() - 360));

    auto presetW = juce::jmin (300, top.getWidth() - 2 * buttonH - 8);
    auto presetRow = top.withSizeKeepingCentre (presetW + 2 * buttonH, buttonH);
    prevButton.setBounds (presetRow.removeFromLeft (buttonH));
    nextButton.setBounds (presetRow.removeFromRight (buttonH));
    presetBox.setBounds (presetRow.reduced (4, 0));

    auto area = safe.reduced (compact ? 10 : 16, compact ? 6 : 4);
    area.removeFromTop (compact ? 2 : 6);

    // On a short screen the MIDI keyboard is the input, so drop the on-screen one.
    keyboard.setVisible (area.getHeight() >= 440);
    if (keyboard.isVisible())
    {
        keyboard.setBounds (area.removeFromBottom (74));
        keyboard.setKeyWidth ((float) keyboard.getWidth() / 43.0f); // C1..C7 white keys fill the width
        area.removeFromBottom (10);
    }

    easyPage.setBounds (area);
    layoutEasyPage();
    viewport.setBounds (area);
    layoutFullPage (area);

    background.invalidate();
    fullBackground.invalidate();
    easyBackground.invalidate();
    repaint();
}
} // namespace orch
