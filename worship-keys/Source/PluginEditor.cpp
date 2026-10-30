#include "PluginEditor.h"

namespace colours
{
const juce::Colour body { 0xffb5121b };   // Nord red
const juce::Colour panel { 0xff1d1d20 };
const juce::Colour knob { 0xff0e0e10 };
const juce::Colour arc { 0xffff4d4d };
const juce::Colour text { 0xfff2f2f2 };
const juce::Colour dim { 0xff9a9aa0 };
const juce::Colour led { 0xffff3b30 };     // red LEDs / display
const juce::Colour plate { 0xff26262a };
}

//==============================================================================
NordLookAndFeel::NordLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, colours::dim);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::backgroundColourId, colours::knob);
    setColour (juce::ComboBox::textColourId, juce::Colour (0xffff6a5c));
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff3a3a40));
    setColour (juce::ComboBox::arrowColourId, colours::arc);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::body);
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::TextButton::buttonColourId, colours::knob);
    setColour (juce::TextButton::buttonOnColourId, colours::arc);
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
}

void NordLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&)
{
    hw::drawKnob (g, juce::Rectangle<int> (x, y, w, h).toFloat().reduced (2.0f), pos, start, end, hw::KnobStyle::black, colours::led);
}

void NordLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    hw::drawLCD (g, juce::Rectangle<int> (w, h).toFloat(), colours::led, box.isEnabled());
    hw::drawArrow (g, (float) w - 14.0f, (float) h * 0.5f, colours::led);
}

void NordLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float min, float max,
                                        juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearVertical)
        return LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, min, max, style, s);
    hw::drawFader (g, juce::Rectangle<int> (x, y, w, h).toFloat(), pos, colours::led, colours::text, 30.0f, 58.0f);
}

void NordLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    hw::drawButton (g, b.getLocalBounds().toFloat(), b.getToggleState(), over, down, colours::body.brighter (0.25f));
}

juce::Label* NordLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (juce::Font (juce::FontOptions (11.0f)));
    return l;
}


//==============================================================================
namespace
{
// Readable readouts, chosen by the parameter's unit label.
std::function<juce::String (double)> valueFormatter (const juce::String& unit, const juce::String& id)
{
    return [unit, id] (double v) -> juce::String
    {
        if (unit == "dB")  return v <= -59.9 && id.endsWith ("Level") ? juce::String ("OFF") : juce::String (v, 1) + " dB";
        if (unit == "s")   return v < 1.0 ? juce::String (juce::roundToInt (v * 1000)) + " ms" : juce::String (v, 2) + " s";
        if (unit == "ms")  return juce::String (juce::roundToInt (v)) + " ms";
        if (unit == "Hz")  return v >= 1000 ? juce::String (v / 1000.0, 1) + " kHz" : juce::String (v, v < 10 ? 2 : 0) + " Hz";
        if (id == "velCurve" || id == "epDecay") return juce::String (v, 2) + "x";
        if (id == "pianoTone") return v >= 0.98 ? juce::String ("OPEN") : juce::String (juce::roundToInt (v * 100)) + "%";
        return juce::String (juce::roundToInt (v * 100)) + "%";
    };
}
} // namespace

WorshipKeysEditor::WorshipKeysEditor (WorshipKeysProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);

    auto section = [this] (const juce::String& title, std::vector<Control*> items)
    {
        sections.push_back (std::make_unique<Section> (Section { title, std::move (items), {} }));
        return sections.back().get();
    };

    auto* piano = section ("PIANO", { addKnob ("pianoLevel", "Level"), addKnob ("pianoTone", "Tone"), addKnob ("pianoRelease", "Release"),
                                      addKnob ("velCurve", "Touch") });
    auto* ep = section ("E.PIANO", { addKnob ("epLevel", "Level"), addKnob ("epBell", "Bell"), addKnob ("epBark", "Bark"), addKnob ("epDecay", "Decay"),
                                     addKnob ("epTrem", "Tremolo"), addKnob ("epTremRate", "Rate") });
    auto* pad = section ("PAD", { addKnob ("padLevel", "Level"), addKnob ("padAttack", "Attack"), addKnob ("padRelease", "Release"),
                                  addKnob ("padCutoff", "Cutoff"), addKnob ("padReso", "Reso"), addKnob ("padDetune", "Detune"),
                                  addKnob ("padMotion", "Motion"), addChoice ("padOctave", "Octave", { "-1", "0", "+1" }, 1) });
    auto* eq = section ("EQ", { addKnob ("hpf", "Low Cut"), addKnob ("eqLow", "Low"), addKnob ("eqLowMid", "Low-Mid"),
                                addKnob ("eqPresence", "Presence"), addKnob ("eqAir", "Air") });
    auto* dyn = section ("DYNAMICS", { addKnob ("comp", "Comp"), addKnob ("drive", "Drive") });
    auto* mod = section ("CHORUS / DELAY", { addKnob ("chorusMix", "Chorus"), addKnob ("chorusRate", "Rate"), addKnob ("delayMix", "Delay"),
                                             addKnob ("delayFeedback", "Repeats"),
                                             addChoice ("delayTime", "Time", { "1/4", "1/8 dot", "1/8", "1/4 dot", "1/2" }, 1) });
    auto* rev = section ("REVERB", { addKnob ("revMix", "Mix"), addKnob ("revDecay", "Decay"), addKnob ("revSize", "Size"),
                                     addKnob ("revPreDelay", "Pre-Delay"), addKnob ("revTone", "Tone"), addKnob ("shimmer", "Shimmer") });
    auto* bells = section ("BELLS", { addKnob ("bellLevel", "Level"), addKnob ("bellTone", "Tone"), addKnob ("bellDecay", "Decay"),
                                      addChoice ("bellOctave", "Octave", { "0", "+1", "+2" }, 1) });
    auto* out = section ("OUTPUT", { addKnob ("master", "Master") });
    wideRows = { { piano, ep, pad }, { eq, dyn, mod }, { bells, rev, out } };
    compactRows = { { piano, ep }, { pad }, { eq, dyn }, { mod }, { bells, out }, { rev } };

    // The Easy page: the handful of things you reach for mid-song.
    addFader ("pianoLevel", "PIANO");
    addFader ("epLevel", "E.PIANO");
    addFader ("padLevel", "PAD");
    addFader ("bellLevel", "BELLS");
    addFader ("pianoTone", "TONE");
    addFader ("revMix", "REVERB");
    addFader ("delayMix", "DELAY");
    addFader ("master", "MASTER");

    panel.onPaint = [this] (juce::Graphics& g) { paintSections (g); };
    viewport.setViewedComponent (&panel, false);
    viewport.setScrollBarsShown (false, false, true, false);
    addChildComponent (viewport);

    easyPanel.onPaint = [this] (juce::Graphics& g) { paintEasy (g); };
    addChildComponent (easyPanel);

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.onChange = [this]
    {
        auto idx = presetBox.getSelectedId() - 1;
        if (idx >= 0 && idx != proc.getCurrentProgram())
        {
            proc.setCurrentProgram (idx);
            shownProgram = idx;
        }
    };
    addAndMakeVisible (presetBox);

    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    easyButton.onClick = [this] { setEasyView (true); };
    fullButton.onClick = [this] { setEasyView (false); };
    for (auto* b : { &prevButton, &nextButton, &easyButton, &fullButton })
        addAndMakeVisible (*b);

    statusLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    statusLabel.setJustificationType (juce::Justification::centredRight);
    statusLabel.setColour (juce::Label::textColourId, colours::text.withAlpha (0.85f));
    addAndMakeVisible (statusLabel);

    keyboard.setAvailableRange (36, 96);
    keyboard.setScrollButtonsVisible (false);
    keyboard.setKeyPressBaseOctave (5);
    addAndMakeVisible (keyboard);

   #if JUCE_IOS || JUCE_ANDROID
    setResizeLimits (320, 240, 4000, 4000); // the standalone window stretches the editor to fill the screen
   #endif
    setSize (1120, 640);
    setEasyView (proc.easyView);
    timerCallback();
    startTimerHz (15);
}

WorshipKeysEditor::~WorshipKeysEditor()
{
    proc.cancelMidiLearn();
    setLookAndFeel (nullptr);
}

void WorshipKeysEditor::setupSlider (Control& c, juce::Component& parent)
{
    auto& s = *c.slider;
    s.setPopupDisplayEnabled (false, false, nullptr);
    s.setViewportIgnoreDragFlag (true); // dragging a knob turns it instead of scrolling
    parent.addAndMakeVisible (s);
    c.sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, c.paramId, s);
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

    auto* param = proc.apvts.getParameter (c.paramId);
    s.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    s.textFromValueFunction = valueFormatter (param->getLabel(), c.paramId);
    s.updateText();

    auto id = c.paramId;
    s.onRightClick = [this, id] { showMidiMenu (id); };
}

WorshipKeysEditor::Control* WorshipKeysEditor::addKnob (const juce::String& id, const juce::String& label)
{
    auto c = std::make_unique<Control>();
    c->paramId = id;
    c->label = label;
    c->slider = std::make_unique<LearnSlider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
    c->slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 14);
    setupSlider (*c, panel);
    controls.push_back (std::move (c));
    return controls.back().get();
}

WorshipKeysEditor::Control* WorshipKeysEditor::addFader (const juce::String& id, const juce::String& label)
{
    auto c = std::make_unique<Control>();
    c->paramId = id;
    c->label = label;
    c->slider = std::make_unique<LearnSlider> (juce::Slider::LinearVertical, juce::Slider::TextBoxBelow);
    c->slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    setupSlider (*c, easyPanel);

    c->learnButton = std::make_unique<juce::TextButton> ("LEARN");
    c->learnButton->setTooltip ("Click, then move a slider or knob on your MIDI controller");
    c->learnButton->onClick = [this, id] { toggleLearn (id); };
    easyPanel.addAndMakeVisible (*c->learnButton);

    faders.push_back (std::move (c));
    return faders.back().get();
}

WorshipKeysEditor::Control* WorshipKeysEditor::addChoice (const juce::String& id, const juce::String& label, const juce::StringArray& items, int firstItemId)
{
    auto c = std::make_unique<Control>();
    c->paramId = id;
    c->label = label;
    c->combo = std::make_unique<juce::ComboBox>();
    c->combo->addItemList (items, firstItemId);
    c->combo->setViewportIgnoreDragFlag (true);
    panel.addAndMakeVisible (*c->combo);
    c->comboAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, *c->combo);
    controls.push_back (std::move (c));
    return controls.back().get();
}

void WorshipKeysEditor::toggleLearn (const juce::String& paramId)
{
    if (proc.isLearning (paramId))
        proc.cancelMidiLearn();
    else
        proc.startMidiLearn (paramId);
    timerCallback();
}

void WorshipKeysEditor::showMidiMenu (const juce::String& paramId)
{
    juce::PopupMenu m;
    m.addSectionHeader (proc.apvts.getParameter (paramId)->getName (40));
    if (proc.isLearning (paramId))
        m.addItem ("Cancel MIDI Learn", [this] { proc.cancelMidiLearn(); });
    else
        m.addItem ("MIDI Learn (then move a controller slider)", [this, paramId] { proc.startMidiLearn (paramId); });

    auto cc = proc.getMappedCC (paramId);
    m.addItem ("Clear MIDI mapping" + (cc >= 0 ? " (CC " + juce::String (cc) + ")" : juce::String()), cc >= 0,
               false, [this, paramId] { proc.clearMidiMapping (paramId); });
    m.addSeparator();
    m.addItem ("Clear ALL MIDI mappings", [this] { proc.clearAllMidiMappings(); });
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition());
}

void WorshipKeysEditor::setEasyView (bool easy)
{
    proc.easyView = easy;
    easyButton.setToggleState (easy, juce::dontSendNotification);
    fullButton.setToggleState (! easy, juce::dontSendNotification);
    easyPanel.setVisible (easy);
    viewport.setVisible (! easy);
}

void WorshipKeysEditor::stepPreset (int delta)
{
    auto n = proc.getNumPrograms();
    proc.setCurrentProgram ((proc.getCurrentProgram() + delta + n) % n);
    timerCallback();
}

juce::String WorshipKeysEditor::midiSignature() const
{
    juce::String sig;
    auto add = [&] (const Control& c)
    {
        if (c.slider != nullptr)
            sig << c.paramId << proc.getMappedCC (c.paramId) << (proc.isLearning (c.paramId) ? "L" : "") << ";";
    };
    for (auto& c : controls) add (*c);
    for (auto& c : faders) add (*c);
    return sig;
}

void WorshipKeysEditor::timerCallback()
{
    proc.saveMidiMapIfChanged();
    statusLabel.setText (proc.getSampleStatus(), juce::dontSendNotification);
    if (proc.getCurrentProgram() != shownProgram)
    {
        shownProgram = proc.getCurrentProgram();
        presetBox.setSelectedId (shownProgram + 1, juce::dontSendNotification);
    }

    for (auto& f : faders)
    {
        auto learning = proc.isLearning (f->paramId);
        f->learnButton->setToggleState (learning, juce::dontSendNotification);
        f->learnButton->setButtonText (learning ? "MOVE SLIDER" : "LEARN");
    }

    // Repaint the labels when a mapping changes, and keep the "learning" label blinking.
    auto sig = midiSignature();
    if (sig != shownMidi || proc.isAnyLearning())
    {
        shownMidi = sig;
        ++blink;
        panel.repaint();
        easyPanel.repaint();
    }
}

void WorshipKeysEditor::paint (juce::Graphics& g)
{
    background.draw (g, getLocalBounds(), [] (juce::Graphics& bg, juce::Rectangle<float> r)
    {
        hw::drawChassis (bg, r, colours::body);
        // dark end cheeks, like a stage piano's side panels
        for (auto cheek : { r.withWidth (8.0f), r.withLeft (r.getRight() - 8.0f) })
        {
            bg.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2a2e), cheek.getX(), 0, juce::Colour (0xff0e0e10), cheek.getRight(), 0, false));
            bg.fillRect (cheek);
        }
    });

    auto drawTitle = [&] (juce::Rectangle<float> r, float size)
    {
        g.setFont (juce::Font (juce::FontOptions (size, juce::Font::bold)).withExtraKerningFactor (0.06f));
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawText ("WORSHIP KEYS", r.translated (0, 1.5f), juce::Justification::centredLeft);
        g.setColour (colours::text);
        g.drawText ("WORSHIP KEYS", r, juce::Justification::centredLeft);
    };
    if (compact)
    {
        drawTitle (titleArea.toFloat(), 16.0f);
        return;
    }
    drawTitle (juce::Rectangle<float> ((float) titleArea.getX() + 6, (float) titleArea.getY() + 2, 260, 34), 24.0f);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.setColour (colours::text.withAlpha (0.75f));
    g.drawText ("stage piano / pad / ep", titleArea.getX() + 8, titleArea.getY() + 30, 260, 14, juce::Justification::centredLeft);
}

void WorshipKeysEditor::paintSections (juce::Graphics& g)
{
    auto learnOn = (blink / 4) % 2 == 0;
    for (auto& s : sections)
    {
        hw::drawPlate (g, s->bounds.toFloat(), colours::plate, colours::arc, s->title, 6.0f, ! compact);

        g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        for (auto* c : s->controls)
        {
            auto learning = c->slider != nullptr && proc.isLearning (c->paramId);
            auto cc = c->slider != nullptr ? proc.getMappedCC (c->paramId) : -1;
            auto text = learning ? juce::String ("MOVE CC") : c->label.toUpperCase();
            if (! learning && cc >= 0)
                text << " CC" << cc;
            g.setColour (learning ? (learnOn ? colours::arc : colours::dim) : cc >= 0 ? colours::arc.brighter (0.4f) : colours::text);
            g.drawText (text, c->bounds.getX(), c->bounds.getY(), c->bounds.getWidth(), 14, juce::Justification::centred);
        }
    }
}

void WorshipKeysEditor::paintEasy (juce::Graphics& g)
{
    auto learnOn = (blink / 4) % 2 == 0;
    for (auto& f : faders)
    {
        hw::drawPlate (g, f->bounds.toFloat(), colours::plate, colours::arc, {}, 8.0f, ! compact);

        auto r = f->bounds.reduced (6);
        g.setColour (colours::text);
        g.setFont (juce::Font (juce::FontOptions (compact ? 13.0f : 16.0f, juce::Font::bold)));
        g.drawText (f->label, r.removeFromTop (compact ? 18 : 26), juce::Justification::centred);

        auto learning = proc.isLearning (f->paramId);
        auto cc = proc.getMappedCC (f->paramId);
        g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        g.setColour (learning ? (learnOn ? colours::arc : colours::dim) : cc >= 0 ? colours::arc.brighter (0.4f) : colours::dim);
        auto ccText = learning ? juce::String ("waiting for MIDI...") : cc >= 0 ? "CC " + juce::String (cc) : juce::String ("not mapped");
        g.drawText (ccText, f->bounds.getX(), f->learnButton->getY() - 17, f->bounds.getWidth(), 14, juce::Justification::centred);
    }
}

void WorshipKeysEditor::layoutRow (std::vector<Section*> row, juce::Rectangle<int> area)
{
    int totalControls = 0;
    for (auto* s : row) totalControls += (int) s->controls.size();
    auto gap = 10;
    auto widthPerControl = (float) (area.getWidth() - gap * ((int) row.size() - 1) - 16 * (int) row.size()) / (float) totalControls;

    for (auto* s : row)
    {
        auto w = (int) (widthPerControl * (float) s->controls.size()) + 16;
        s->bounds = area.removeFromLeft (w);
        area.removeFromLeft (gap);

        auto inner = s->bounds.reduced (8).withTrimmedTop (20);
        auto cw = inner.getWidth() / (int) s->controls.size();
        for (auto* c : s->controls)
        {
            c->bounds = inner.removeFromLeft (cw);
            auto r = c->bounds.withTrimmedTop (16);
            if (c->slider != nullptr)
                c->slider->setBounds (r.withSizeKeepingCentre (juce::jmin (r.getWidth(), 76), r.getHeight()));
            else
                c->combo->setBounds (r.withSizeKeepingCentre (juce::jmin (r.getWidth() - 4, 90), 26));
        }
    }
}

void WorshipKeysEditor::layoutEasy()
{
    auto area = easyPanel.getLocalBounds();
    const int gap = compact ? 6 : 10, n = (int) faders.size();
    auto w = (area.getWidth() - gap * (n - 1)) / n;
    for (auto& f : faders)
    {
        f->bounds = area.removeFromLeft (w);
        area.removeFromLeft (gap);

        auto r = f->bounds.reduced (6);
        r.removeFromTop (compact ? 18 : 26);
        auto buttonRow = r.removeFromBottom (compact ? 22 : 26);
        f->learnButton->setBounds (buttonRow.withSizeKeepingCentre (juce::jmin (buttonRow.getWidth(), 110), buttonRow.getHeight()));
        r.removeFromBottom (18); // CC readout
        f->slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, juce::jmin (80, r.getWidth()), compact ? 16 : 20);
        f->slider->setBounds (r);
    }
}

void WorshipKeysEditor::resized()
{
    auto area = hw::safeArea (*this); // keeps clear of the notch / home bar on a phone
    compact = hw::isCompact (area);
    area.reduce (compact ? 8 : 14, compact ? 6 : 10);

    auto header = area.removeFromTop (compact ? 40 : 48);
    auto buttonH = compact ? 28 : 30;
    titleArea = header.removeFromLeft (compact ? 176 : 260);

    auto views = header.removeFromRight (compact ? 116 : 136);
    auto half = views.getWidth() / 2;
    fullButton.setBounds (views.removeFromRight (half).withSizeKeepingCentre (half - 4, buttonH));
    easyButton.setBounds (views.withSizeKeepingCentre (half - 4, buttonH));

    statusLabel.setVisible (header.getWidth() > 600);
    if (statusLabel.isVisible())
        statusLabel.setBounds (header.removeFromRight (compact ? 180 : 240));

    auto presetW = juce::jmin (340, header.getWidth() - 2 * (buttonH + 12));
    auto presetRow = header.withSizeKeepingCentre (presetW + 2 * (buttonH + 6), buttonH);
    prevButton.setBounds (presetRow.removeFromLeft (buttonH));
    nextButton.setBounds (presetRow.removeFromRight (buttonH));
    presetBox.setBounds (presetRow.reduced (6, 0));

    // On a short screen the MIDI keyboard is the input, so drop the on-screen one.
    keyboard.setVisible (area.getHeight() >= 440);
    if (keyboard.isVisible())
    {
        auto kb = area.removeFromBottom (80);
        keyboard.setKeyWidth ((float) kb.getWidth() / 36.0f); // 36 white keys in C2..C7
        keyboard.setBounds (kb);
        keyboard.setLowestVisibleKey (36);
        area.removeFromBottom (10);
    }

    easyPanel.setBounds (area);
    layoutEasy();

    // Wide screens fit everything in 3 rows; small ones get 5 shorter rows that scroll.
    auto& rows = compact ? compactRows : wideRows;
    const int gap = 10, numRows = (int) rows.size();
    auto rowH = (area.getHeight() - (numRows - 1) * gap) / numRows;
    if (compact)
        rowH = juce::jmax (rowH, 118);

    viewport.setBounds (area);
    panel.setSize (area.getWidth(), numRows * rowH + (numRows - 1) * gap);
    auto panelArea = panel.getLocalBounds();
    for (auto& row : rows)
    {
        layoutRow (row, panelArea.removeFromTop (rowH));
        panelArea.removeFromTop (gap);
    }
    repaint();
}
