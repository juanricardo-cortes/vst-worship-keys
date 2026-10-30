#include "PluginEditor.h"

namespace colours
{
const juce::Colour body { 0xff0b0d17 };
const juce::Colour panel { 0xff141829 };
const juce::Colour panelEdge { 0xff252b45 };
const juce::Colour knob { 0xff090b14 };
const juce::Colour cyan { 0xff3fd8ff };
const juce::Colour violet { 0xff9b6bff };
const juce::Colour magenta { 0xffff4fb8 };
const juce::Colour amber { 0xffffb547 };
const juce::Colour text { 0xffeef1fb };
const juce::Colour dim { 0xff8890ab };
const juce::Colour track { 0xff262c45 };
const juce::Colour plate { 0xff1c2033 };
}

//==============================================================================
AnthemLookAndFeel::AnthemLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, colours::dim);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::backgroundColourId, colours::knob);
    setColour (juce::ComboBox::textColourId, colours::cyan.brighter (0.3f));
    setColour (juce::ComboBox::outlineColourId, colours::panelEdge);
    setColour (juce::ComboBox::arrowColourId, colours::cyan);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::violet.darker (0.5f));
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
}

void AnthemLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    auto accent = s.getProperties().contains ("accent") ? juce::Colour ((juce::uint32) (juce::int64) s.getProperties()["accent"]) : colours::cyan;
    auto range = s.getRange();
    auto bipolar = range.getStart() < 0 && range.getEnd() > 0;
    auto from = bipolar ? (float) s.valueToProportionOfLength (0.0) : 0.0f;
    hw::drawKnob (g, juce::Rectangle<int> (x, y, w, h).toFloat().reduced (2.0f), pos, start, end, hw::KnobStyle::aluminium, accent, from);
}

void AnthemLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    hw::drawLCD (g, juce::Rectangle<int> (w, h).toFloat(), colours::cyan, box.isEnabled());
    hw::drawArrow (g, (float) w - 12.0f, (float) h * 0.5f, colours::cyan);
}

void AnthemLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    hw::drawButton (g, b.getLocalBounds().toFloat(), b.getToggleState(), highlighted, down, colours::violet);
}

juce::Label* AnthemLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    auto big = s.getProperties().contains ("big");
    l->setFont (juce::Font (juce::FontOptions (big ? 15.0f : 11.0f, big ? juce::Font::bold : juce::Font::plain)));
    if (big)
        l->setColour (juce::Label::textColourId, colours::text.withAlpha (0.85f));
    return l;
}

//==============================================================================
AnthemSynthEditor::AnthemSynthEditor (AnthemSynthProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard, colours::cyan)
{
    setLookAndFeel (&lnf);
    using namespace colours;

    // The pages go in first so the controls built below pick up the look-and-feel.
    fullPage.onPaint = [this] (juce::Graphics& g) { paintFullPage (g); };
    viewport.setViewedComponent (&fullPage, false);
    viewport.setScrollBarsShown (false, false, true, false);
    addChildComponent (viewport);
    easyPage.onPaint = [this] (juce::Graphics& g) { paintEasyPage (g); };
    addChildComponent (easyPage);

    auto* oscA = addSection ("OSC A", cyan, { addChoice ("oscAWave", "Wave"), addKnob ("oscAOct", "Octave"),
                                              addKnob ("detune", "Detune"), addKnob ("oscALevel", "Level") });
    auto* oscB = addSection ("OSC B", cyan, { addChoice ("oscBWave", "Wave"), addKnob ("oscBOct", "Octave"), addKnob ("oscBSemi", "Semi"),
                                              addKnob ("oscBFine", "Fine"), addKnob ("oscBLevel", "Level") });
    auto* mix = addSection ("MIX", cyan, { addKnob ("pw", "PW"), addKnob ("subLevel", "Sub"), addKnob ("noise", "Noise"), addKnob ("width", "Width") });
    auto* filter = addSection ("FILTER", violet, { addKnob ("cutoff", "Cutoff"), addKnob ("reso", "Reso"), addKnob ("fltEnv", "Env"),
                                                   addKnob ("keyTrack", "Key Trk"), addKnob ("fltVel", "Vel"), addKnob ("fltDrive", "Drive") });

    auto* fenv = addSection ("FILTER ENV", violet, { addKnob ("fAtk", "Attack"), addKnob ("fDec", "Decay"), addKnob ("fSus", "Sustain"), addKnob ("fRel", "Release") });
    auto* aenv = addSection ("AMP ENV", violet, { addKnob ("aAtk", "Attack"), addKnob ("aDec", "Decay"), addKnob ("aSus", "Sustain"),
                                                  addKnob ("aRel", "Release"), addKnob ("velSens", "Velocity"), addKnob ("level", "Level") });
    auto* lfo = addSection ("LFO", violet, { addKnob ("lfoRate", "Rate"), addKnob ("lfoPitch", "Vibrato"), addKnob ("lfoFilter", "Filter"), addKnob ("lfoPwm", "PWM") });
    auto* voice = addSection ("VOICE", violet, { addChoice ("voiceMode", "Mode"), addKnob ("glide", "Glide"), addKnob ("bendRange", "Bend"), addKnob ("swell", "Swell") });

    auto* motion = addSection ("MOTION", magenta, { addChoice ("motion", "Type"), addKnob ("motionDepth", "Depth") });
    auto* arp = addSection ("ARP", magenta, { addChoice ("arpMode", "Mode"), addChoice ("arpRate", "Rate"), addKnob ("arpOct", "Oct"), addKnob ("arpGate", "Gate") });
    auto* tone = addSection ("TONE", amber, { addKnob ("lowCut", "Low Cut"), addKnob ("bright", "Bright") });
    auto* chorus = addSection ("CHORUS", amber, { addKnob ("chorusMix", "Mix"), addKnob ("chorusRate", "Rate"), addKnob ("chorusDepth", "Depth") });
    auto* delay = addSection ("DELAY", amber, { addKnob ("delayMix", "Mix"), addKnob ("delayFeedback", "Repeats"), addChoice ("delayTime", "Time") });
    auto* reverb = addSection ("SHIMMER REVERB", amber, { addKnob ("revMix", "Mix"), addKnob ("revDecay", "Decay"), addKnob ("revSize", "Size"),
                                                          addKnob ("revTone", "Tone"), addKnob ("shimmer", "Shimmer") });
    auto* master = addSection ("MASTER", amber, { addKnob ("bpm", "Tempo"), addKnob ("master", "Level") });

    wideRows = { { oscA, oscB, mix, filter }, { fenv, aenv, lfo, voice }, { motion, arp, tone, chorus, delay, reverb, master } };
    compactRows = { { oscA, oscB }, { mix, filter }, { fenv, aenv }, { lfo, voice }, { motion, arp, tone }, { chorus, delay }, { reverb, master } };

    // The Easy page: the big moves you make on a preset mid-song.
    addEasyKnob ("cutoff", "BRIGHTNESS", violet);
    addEasyKnob ("aAtk", "ATTACK", violet);
    addEasyKnob ("aRel", "RELEASE", violet);
    addEasyKnob ("motionDepth", "MOTION", magenta);
    addEasyKnob ("chorusMix", "CHORUS", amber);
    addEasyKnob ("delayMix", "DELAY", amber);
    addEasyKnob ("revMix", "REVERB", amber);
    addEasyKnob ("master", "VOLUME", cyan);

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

    tempoLabel.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    tempoLabel.setJustificationType (juce::Justification::centredRight);
    tempoLabel.setColour (juce::Label::textColourId, colours::cyan);
    addAndMakeVisible (tempoLabel);

    keyboard.setAvailableRange (36, 96);
    keyboard.setOctaveForMiddleC (4);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, colours::cyan.withAlpha (0.75f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, colours::violet.withAlpha (0.3f));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (keyboard);

   #if JUCE_IOS || JUCE_ANDROID
    setResizeLimits (320, 240, 4000, 4000); // the standalone window stretches the editor to fill the screen
   #endif
    setSize (1400, 840);
    setEasyView (proc.easyView);
    timerCallback();
    startTimerHz (30);
}

AnthemSynthEditor::~AnthemSynthEditor() { setLookAndFeel (nullptr); }

AnthemSynthEditor::Section* AnthemSynthEditor::addSection (const juce::String& title, juce::Colour accent, std::vector<Control*> items)
{
    auto s = std::make_unique<Section>();
    s->title = title;
    s->accent = accent;
    s->controls = std::move (items);
    for (auto* c : s->controls)
        if (c->slider != nullptr)
            c->slider->getProperties().set ("accent", (juce::int64) accent.getARGB());
    sections.push_back (std::move (s));
    return sections.back().get();
}

std::unique_ptr<AnthemSynthEditor::Control> AnthemSynthEditor::makeKnob (const juce::String& id, const juce::String& label,
                                                                          juce::Component& parent, bool big)
{
    auto c = std::make_unique<Control>();
    c->paramId = id;
    c->label = label;
    auto* param = proc.apvts.getParameter (id);
    c->slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
    if (big)
        c->slider->getProperties().set ("big", true);
    parent.addAndMakeVisible (*c->slider);   // pick up the look-and-feel before the value box is built
    c->slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, big ? 110 : 70, big ? 22 : 14);
    c->slider->setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    c->slider->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    c->slider->setViewportIgnoreDragFlag (true); // dragging a knob turns it instead of scrolling the page
    c->sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, *c->slider);

    auto unit = param->getLabel();
    auto isInt = dynamic_cast<juce::AudioParameterInt*> (param) != nullptr;
    c->slider->textFromValueFunction = [unit, isInt, id] (double v) -> juce::String
    {
        if (isInt)        return (v > 0 && id != "bendRange" && id != "arpOct" ? "+" : "") + juce::String (juce::roundToInt (v));
        if (unit == "dB") return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB";
        if (unit == "s")  return v < 1.0 ? juce::String (juce::roundToInt (v * 1000)) + " ms" : juce::String (v, 1) + " s";
        if (unit == "Hz") return v >= 1000 ? juce::String (v / 1000.0, 1) + " kHz" : juce::String (v, v < 10 ? 2 : 0) + " Hz";
        if (unit == "ct") return juce::String (v, 1) + " ct";
        if (unit == "BPM") return juce::String (v, 1);
        if (id == "fltEnv") return (v > 0 ? "+" : "") + juce::String (juce::roundToInt (v * 100)) + "%";
        return juce::String (juce::roundToInt (v * 100)) + "%";
    };
    c->slider->updateText();
    return c;
}

AnthemSynthEditor::Control* AnthemSynthEditor::addKnob (const juce::String& id, const juce::String& label)
{
    controls.push_back (makeKnob (id, label, fullPage, false));
    return controls.back().get();
}

void AnthemSynthEditor::addEasyKnob (const juce::String& id, const juce::String& label, juce::Colour accent)
{
    auto c = makeKnob (id, label, easyPage, true);
    c->slider->getProperties().set ("accent", (juce::int64) accent.getARGB());
    easyKnobs.push_back (std::move (c));
}

AnthemSynthEditor::Control* AnthemSynthEditor::addChoice (const juce::String& id, const juce::String& label)
{
    auto c = std::make_unique<Control>();
    c->paramId = id;
    c->label = label;
    c->combo = std::make_unique<juce::ComboBox>();
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
        c->combo->addItemList (choice->choices, 1);
    c->combo->setViewportIgnoreDragFlag (true);
    fullPage.addAndMakeVisible (*c->combo);
    c->comboAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, *c->combo);
    controls.push_back (std::move (c));
    return controls.back().get();
}

void AnthemSynthEditor::setEasyView (bool easy)
{
    proc.easyView = easy;
    easyButton.setToggleState (easy, juce::dontSendNotification);
    fullButton.setToggleState (! easy, juce::dontSendNotification);
    easyPage.setVisible (easy);
    viewport.setVisible (! easy);
}

void AnthemSynthEditor::stepPreset (int delta)
{
    auto n = proc.getNumPrograms();
    proc.setCurrentProgram ((proc.getCurrentProgram() + delta + n) % n);
}

void AnthemSynthEditor::timerCallback()
{
    if (proc.getCurrentProgram() != shownProgram)
    {
        shownProgram = proc.getCurrentProgram();
        presetBox.setSelectedId (shownProgram + 1, juce::dontSendNotification);
    }

    auto bpmText = juce::String (proc.getEffectiveBpm(), 1) + " BPM" + (proc.isTempoFromHost() ? "  (host)" : "");
    if (tempoLabel.getText() != bpmText)
        tempoLabel.setText (bpmText, juce::dontSendNotification);

    proc.copyScope (scopeData.data());
    auto peak = sfx::toDb (proc.takeOutputPeak());
    meterLevel = peak > meterLevel ? peak : juce::jmax (-60.0f, meterLevel - 1.5f);
    if (! scopeArea.isEmpty())
        repaint (scopeArea.expanded (4));
}

void AnthemSynthEditor::paint (juce::Graphics& g)
{
    // Everything that never changes is cached; only the scope + meter redraw every frame.
    background.draw (g, getLocalBounds(), [this] (juce::Graphics& bg, juce::Rectangle<float> r)
    {
        hw::drawChassis (bg, r, juce::Colour (0xff161a28), true);
        bg.setGradientFill (juce::ColourGradient (colours::violet.withAlpha (0.10f), 0, 0, juce::Colours::transparentBlack, 0, 200, false));
        bg.fillRect (r.withHeight (200));
        for (auto cheek : { r.withWidth (8.0f), r.withLeft (r.getRight() - 8.0f) })
        {
            bg.setGradientFill (juce::ColourGradient (juce::Colour (0xff30344a), cheek.getX(), 0, juce::Colour (0xff07080d), cheek.getRight(), 0, false));
            bg.fillRect (cheek);
        }

        // logo: three rising bars, then the name
        auto t = titleArea.toFloat();
        auto s = compact ? 0.7f : 1.0f;
        auto baseY = t.getCentreY() + (compact ? 9.0f : 11.0f);
        for (int i = 0; i < 3; ++i)
        {
            auto h = (12.0f + 8.0f * (float) i) * s;
            bg.setGradientFill (juce::ColourGradient (colours::cyan, 0, baseY - h, colours::violet, 0, baseY, false));
            bg.fillRoundedRectangle (t.getX() + 4.0f + (float) i * 9.0f * s, baseY - h, 6.0f * s, h, 2.0f);
        }
        auto nameX = t.getX() + 40.0f * s;
        auto nameArea = juce::Rectangle<float> (nameX, t.getY(), t.getRight() - nameX, compact ? t.getHeight() : t.getHeight() * 0.62f);
        bg.setFont (juce::Font (juce::FontOptions (compact ? 16.0f : 24.0f, juce::Font::bold)).withExtraKerningFactor (0.06f));
        bg.setColour (juce::Colours::black.withAlpha (0.5f));
        bg.drawText ("ANTHEM SYNTH", nameArea.translated (0, 1.5f), compact ? juce::Justification::centredLeft : juce::Justification::bottomLeft);
        bg.setColour (colours::text);
        bg.drawText ("ANTHEM SYNTH", nameArea, compact ? juce::Justification::centredLeft : juce::Justification::bottomLeft);
        if (! compact)
        {
            bg.setFont (juce::Font (juce::FontOptions (11.0f)));
            bg.setColour (colours::dim);
            bg.drawText ("worship & anthem synthesizer", nameArea.withY (nameArea.getBottom() + 2).withHeight (14), juce::Justification::centredLeft);
        }

        if (! scopeArea.isEmpty())
            hw::drawInset (bg, scopeArea.toFloat(), juce::Colour (0xff04060a), 6.0f);
    });

    if (scopeArea.isEmpty())
        return;

    // scope + output meter
    auto sa = scopeArea.reduced (6, 4).toFloat();
    auto meter = sa.removeFromRight (8);
    sa.removeFromRight (6);
    g.setColour (colours::cyan.withAlpha (0.07f));   // graticule
    for (int i = 1; i < 8; ++i)
        g.fillRect (sa.getX() + sa.getWidth() * (float) i / 8.0f, sa.getY(), 1.0f, sa.getHeight());
    g.fillRect (sa.getX(), sa.getCentreY(), sa.getWidth(), 1.0f);
    // find a rising zero crossing so the trace sits still
    int startIdx = 0;
    for (int i = 1; i < AnthemSynthProcessor::scopeSize / 2; ++i)
        if (scopeData[(size_t) i - 1] < 0 && scopeData[(size_t) i] >= 0) { startIdx = i; break; }
    juce::Path wave;
    auto n = AnthemSynthProcessor::scopeSize / 2;
    for (int i = 0; i < n; ++i)
    {
        auto x = sa.getX() + sa.getWidth() * (float) i / (float) (n - 1);
        auto y = sa.getCentreY() - juce::jlimit (-1.0f, 1.0f, scopeData[(size_t) (startIdx + i)] * 1.4f) * sa.getHeight() * 0.48f;
        if (i == 0) wave.startNewSubPath (x, y); else wave.lineTo (x, y);
    }
    g.setColour (colours::cyan.withAlpha (0.18f));
    g.strokePath (wave, juce::PathStrokeType (5.0f));
    g.setColour (colours::cyan);
    g.strokePath (wave, juce::PathStrokeType (1.5f));

    // segmented LED meter
    auto frac = juce::jlimit (0.0f, 1.0f, (meterLevel + 60.0f) / 60.0f);
    const int segs = 14;
    auto segH = meter.getHeight() / (float) segs;
    for (int i = 0; i < segs; ++i)
    {
        auto t = (float) (i + 1) / (float) segs;
        auto seg = juce::Rectangle<float> (meter.getX(), meter.getBottom() - segH * (float) (i + 1), meter.getWidth(), segH - 1.5f);
        auto col = t > 0.9f ? colours::magenta : t > 0.75f ? colours::amber : colours::cyan;
        g.setColour (frac >= t - 0.5f / (float) segs ? col : col.withAlpha (0.12f));
        g.fillRoundedRectangle (seg, 1.0f);
    }
    hw::drawGlass (g, scopeArea.toFloat().reduced (1.0f), 6.0f);
}

void AnthemSynthEditor::paintFullPage (juce::Graphics& g)
{
    fullBackground.draw (g, fullPage.getLocalBounds(), [this] (juce::Graphics& bg, juce::Rectangle<float>)
    {
        for (auto& s : sections)
        {
            hw::drawPlate (bg, s->bounds.toFloat(), colours::plate, s->accent, s->title, 7.0f, ! compact);
            bg.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
            bg.setColour (colours::text.withAlpha (0.85f));
            for (auto* c : s->controls)
                bg.drawText (c->label.toUpperCase(), c->bounds.getX(), c->bounds.getY(), c->bounds.getWidth(), 14, juce::Justification::centred);
        }
    });
}

void AnthemSynthEditor::paintEasyPage (juce::Graphics& g)
{
    easyBackground.draw (g, easyPage.getLocalBounds(), [this] (juce::Graphics& bg, juce::Rectangle<float>)
    {
        for (auto& c : easyKnobs)
        {
            auto accent = juce::Colour ((juce::uint32) (juce::int64) c->slider->getProperties()["accent"]);
            hw::drawPlate (bg, c->bounds.toFloat(), colours::plate, accent, {}, 8.0f, ! compact);
            bg.setFont (juce::Font (juce::FontOptions (compact ? 13.0f : 16.0f, juce::Font::bold)).withExtraKerningFactor (0.05f));
            bg.setColour (colours::text);
            bg.drawText (c->label, c->bounds.reduced (4, 6).removeFromTop (compact ? 18 : 24), juce::Justification::centred);
        }
    });
}

void AnthemSynthEditor::layoutRow (std::vector<Section*> row, juce::Rectangle<int> area)
{
    auto weight = [] (const Control* c) { return c->combo != nullptr ? 1.7f : 1.0f; };
    auto sectionWeight = [&] (const Section* s) { float w = 0; for (auto* c : s->controls) w += weight (c); return juce::jmax (w, 1.2f); };
    float totalWeight = 0;
    for (auto* s : row) totalWeight += sectionWeight (s);
    const int gap = compact ? 8 : 10, pad = 16;
    auto widthPerControl = (float) (area.getWidth() - gap * ((int) row.size() - 1) - pad * (int) row.size()) / totalWeight;

    for (size_t k = 0; k < row.size(); ++k)
    {
        auto* s = row[k];
        auto w = k + 1 == row.size() ? area.getWidth() : (int) (widthPerControl * sectionWeight (s)) + pad;
        s->bounds = area.removeFromLeft (w);
        area.removeFromLeft (gap);

        auto inner = s->bounds.reduced (8).withTrimmedTop (compact ? 20 : 24);
        auto unit = (float) inner.getWidth() / sectionWeight (s);
        for (auto* c : s->controls)
        {
            c->bounds = inner.removeFromLeft ((int) (unit * weight (c)));
            auto r = c->bounds.withTrimmedTop (16);
            if (c->slider != nullptr)
                c->slider->setBounds (r.withSizeKeepingCentre (juce::jmin (r.getWidth(), 76), juce::jmin (r.getHeight(), 84)));
            else
                c->combo->setBounds (r.withSizeKeepingCentre (juce::jmin (r.getWidth() - 4, 112), 28));
        }
    }
}

void AnthemSynthEditor::layoutEasy()
{
    // Two rows of four, or one row of eight on a very wide, short window.
    auto area = easyPage.getLocalBounds();
    const int n = (int) easyKnobs.size(), gap = compact ? 6 : 10;
    const int rows = area.getWidth() > area.getHeight() * 3 ? 1 : 2, perRow = (n + rows - 1) / rows;
    auto rowH = (area.getHeight() - gap * (rows - 1)) / rows;
    for (int i = 0; i < n; ++i)
    {
        auto& c = *easyKnobs[(size_t) i];
        auto row = i / perRow, col = i % perRow;
        auto w = (area.getWidth() - gap * (perRow - 1)) / perRow;
        c.bounds = { area.getX() + col * (w + gap), area.getY() + row * (rowH + gap), w, rowH };

        auto r = c.bounds.reduced (6);
        r.removeFromTop (compact ? 20 : 28);
        r.removeFromBottom (compact ? 4 : 10);
        auto size = juce::jmin (r.getWidth(), r.getHeight() - 22, 190);
        c.slider->setBounds (r.withSizeKeepingCentre (r.getWidth(), size + 22));
    }
}

void AnthemSynthEditor::resized()
{
    auto area = hw::safeArea (*this); // keeps clear of the notch / home bar on a phone
    compact = hw::isCompact (area);
    area.reduce (compact ? 10 : 14, compact ? 6 : 10);

    auto header = area.removeFromTop (compact ? 40 : 58);
    auto buttonH = compact ? 30 : 36;
    titleArea = header.removeFromLeft (compact ? 205 : 300);

    // scope on the right (shrinks, then goes, as the screen narrows)
    auto scopeW = compact ? (header.getWidth() > 600 ? 120 : 0) : 300;
    scopeArea = header.removeFromRight (scopeW).reduced (0, compact ? 2 : 4);
    if (scopeW > 0)
        header.removeFromRight (12);

    auto views = header.removeFromRight (compact ? 116 : 136);
    auto half = views.getWidth() / 2;
    fullButton.setBounds (views.removeFromRight (half).withSizeKeepingCentre (half - 4, buttonH));
    easyButton.setBounds (views.withSizeKeepingCentre (half - 4, buttonH));

    tempoLabel.setVisible (! compact);
    if (! compact)
        tempoLabel.setBounds (header.removeFromRight (130).reduced (0, 14));

    auto presetW = juce::jmin (compact ? 300 : 400, header.getWidth() - 2 * (buttonH + 14));
    auto presetRow = header.withSizeKeepingCentre (presetW + 2 * (buttonH + 6), buttonH);
    prevButton.setBounds (presetRow.removeFromLeft (buttonH));
    nextButton.setBounds (presetRow.removeFromRight (buttonH));
    presetBox.setBounds (presetRow.reduced (6, 0));
    area.removeFromTop (compact ? 6 : 8);

    // On a short screen the MIDI keyboard is the input, so drop the on-screen one.
    keyboard.setVisible (area.getHeight() >= 440);
    if (keyboard.isVisible())
    {
        keyboard.setBounds (area.removeFromBottom (110));
        keyboard.setKeyWidth ((float) keyboard.getWidth() / 36.0f);   // C2..C7 = 36 white keys, fills the width
        area.removeFromBottom (10);
    }

    easyPage.setBounds (area);
    layoutEasy();

    // Big screens fit everything in 3 rows; small ones get 7 shorter rows that scroll.
    auto& rows = compact ? compactRows : wideRows;
    const int gap = 10, numRows = (int) rows.size();
    auto rowH = (area.getHeight() - (numRows - 1) * gap) / numRows;
    if (compact)
        rowH = juce::jmax (rowH, 122);
    viewport.setBounds (area);
    fullPage.setSize (area.getWidth(), numRows * rowH + (numRows - 1) * gap);
    auto pageArea = fullPage.getLocalBounds();
    for (auto& row : rows)
    {
        layoutRow (row, pageArea.removeFromTop (rowH));
        pageArea.removeFromTop (gap);
    }

    background.invalidate();
    fullBackground.invalidate();
    easyBackground.invalidate();
    repaint();
}
