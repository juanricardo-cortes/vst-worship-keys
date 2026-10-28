#include "PluginEditor.h"

namespace colours
{
const juce::Colour body { 0xffb5121b };   // Nord red
const juce::Colour panel { 0xff1d1d20 };
const juce::Colour knob { 0xff0e0e10 };
const juce::Colour arc { 0xffff4d4d };
const juce::Colour text { 0xfff2f2f2 };
const juce::Colour dim { 0xff9a9aa0 };
}

//==============================================================================
NordLookAndFeel::NordLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, colours::dim);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::backgroundColourId, colours::knob);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff3a3a40));
    setColour (juce::ComboBox::arrowColourId, colours::arc);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::body);
    setColour (juce::Label::textColourId, colours::text);
}

void NordLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&)
{
    auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    auto size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    auto r = bounds.withSizeKeepingCentre (size, size);
    auto centre = r.getCentre();
    auto radius = size * 0.5f;
    auto angle = start + pos * (end - start);

    juce::Path track, value;
    track.addCentredArc (centre.x, centre.y, radius - 2, radius - 2, 0, start, end, true);
    value.addCentredArc (centre.x, centre.y, radius - 2, radius - 2, 0, start, angle, true);
    g.setColour (juce::Colour (0xff3a3a40));
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (colours::arc);
    g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    auto knobR = r.reduced (radius * 0.22f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c2c30), knobR.getX(), knobR.getY(), colours::knob, knobR.getRight(), knobR.getBottom(), false));
    g.fillEllipse (knobR);
    g.setColour (juce::Colour (0xff45454b));
    g.drawEllipse (knobR, 1.0f);

    juce::Path pointer;
    auto pr = knobR.getWidth() * 0.5f;
    pointer.addRoundedRectangle (-1.5f, -pr + 3.0f, 3.0f, pr * 0.55f, 1.5f);
    g.setColour (colours::text);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
}

void NordLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<int> (w, h).toFloat().reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    juce::Path arrow;
    auto ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (arrow);
}

juce::Label* NordLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (juce::Font (juce::FontOptions (11.0f)));
    return l;
}

//==============================================================================
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

    auto* piano = section ("PIANO", { addKnob ("pianoLevel", "Level"), addKnob ("pianoRelease", "Release"), addKnob ("velCurve", "Touch") });
    auto* ep = section ("E.PIANO", { addKnob ("epLevel", "Level"), addKnob ("epBell", "Bell"), addKnob ("epBark", "Bark"), addKnob ("epDecay", "Decay") });
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
    auto* out = section ("OUTPUT", { addKnob ("master", "Master") });
    wideRows = { { piano, ep, pad }, { eq, dyn, mod }, { rev, out } };
    compactRows = { { piano, ep }, { pad }, { eq, dyn }, { mod }, { rev, out } };

    panel.onPaint = [this] (juce::Graphics& g) { paintSections (g); };
    viewport.setViewedComponent (&panel, false);
    viewport.setScrollBarsShown (false, false, true, false);
    addAndMakeVisible (viewport);

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
    timerCallback();
    startTimerHz (10);
}

WorshipKeysEditor::~WorshipKeysEditor() { setLookAndFeel (nullptr); }

WorshipKeysEditor::Control* WorshipKeysEditor::addKnob (const juce::String& id, const juce::String& label)
{
    auto c = std::make_unique<Control>();
    c->paramId = id;
    c->label = label;
    c->slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
    c->slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 14);
    c->slider->setPopupDisplayEnabled (false, false, nullptr);
    c->slider->setViewportIgnoreDragFlag (true); // dragging a knob turns it instead of scrolling
    panel.addAndMakeVisible (*c->slider);
    c->sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, *c->slider);
    c->slider->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

    // Readable readouts, chosen by the parameter's unit label.
    auto unit = proc.apvts.getParameter (id)->getLabel();
    c->slider->textFromValueFunction = [unit, id] (double v) -> juce::String
    {
        if (unit == "dB")  return v <= -59.9 && id.endsWith ("Level") ? juce::String ("OFF") : juce::String (v, 1) + " dB";
        if (unit == "s")   return v < 1.0 ? juce::String (juce::roundToInt (v * 1000)) + " ms" : juce::String (v, 2) + " s";
        if (unit == "ms")  return juce::String (juce::roundToInt (v)) + " ms";
        if (unit == "Hz")  return v >= 1000 ? juce::String (v / 1000.0, 1) + " kHz" : juce::String (v, v < 10 ? 2 : 0) + " Hz";
        if (id == "velCurve" || id == "epDecay") return juce::String (v, 2) + "x";
        return juce::String (juce::roundToInt (v * 100)) + "%";
    };
    c->slider->updateText();
    controls.push_back (std::move (c));
    return controls.back().get();
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

void WorshipKeysEditor::timerCallback()
{
    statusLabel.setText (proc.getSampleStatus(), juce::dontSendNotification);
    if (proc.getCurrentProgram() != shownProgram)
    {
        shownProgram = proc.getCurrentProgram();
        presetBox.setSelectedId (shownProgram + 1, juce::dontSendNotification);
    }
}

void WorshipKeysEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::body);

    g.setColour (colours::text);
    if (compact)
    {
        g.setFont (juce::Font (juce::FontOptions (18.0f, juce::Font::bold)));
        g.drawText ("WORSHIP KEYS", titleArea, juce::Justification::centredLeft);
        return;
    }
    g.setFont (juce::Font (juce::FontOptions (24.0f, juce::Font::bold)));
    g.drawText ("WORSHIP KEYS", titleArea.getX() + 6, titleArea.getY() + 2, 260, 34, juce::Justification::centredLeft);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.setColour (colours::text.withAlpha (0.7f));
    g.drawText ("stage piano / pad / ep", titleArea.getX() + 8, titleArea.getY() + 30, 260, 14, juce::Justification::centredLeft);
}

void WorshipKeysEditor::paintSections (juce::Graphics& g)
{
    for (auto& s : sections)
    {
        g.setColour (colours::panel);
        g.fillRoundedRectangle (s->bounds.toFloat(), 6.0f);
        g.setColour (colours::arc);
        g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        g.drawText (s->title, s->bounds.getX() + 12, s->bounds.getY() + 6, s->bounds.getWidth() - 24, 16, juce::Justification::centredLeft);

        g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        g.setColour (colours::text);
        for (auto* c : s->controls)
            g.drawText (c->label.toUpperCase(), c->bounds.getX(), c->bounds.getY(), c->bounds.getWidth(), 14, juce::Justification::centred);
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

void WorshipKeysEditor::resized()
{
    auto area = getLocalBounds();
   #if JUCE_IOS || JUCE_ANDROID
    // Keep clear of the notch / home indicator.
    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        area = display->safeAreaInsets.subtractedFrom (area);
   #endif

    compact = area.getWidth() < 1000 || area.getHeight() < 560;
    area.reduce (compact ? 8 : 14, compact ? 6 : 10);

    auto header = area.removeFromTop (compact ? 40 : 48);
    if (compact)
    {
        titleArea = header.removeFromLeft (150);
        presetBox.setBounds (header.removeFromLeft (juce::jmin (260, header.getWidth() / 2)).withSizeKeepingCentre (juce::jmin (260, header.getWidth() / 2), 30));
        statusLabel.setBounds (header);
    }
    else
    {
        titleArea = header;
        presetBox.setBounds (header.withSizeKeepingCentre (340, 30));
        statusLabel.setBounds (header.removeFromRight (360));
    }

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
