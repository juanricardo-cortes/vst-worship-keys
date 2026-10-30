#include "StationEditor.h"

namespace
{
namespace colours
{
const juce::Colour bg { 0xff111114 };
const juce::Colour panel { 0xff1c1c21 };
const juce::Colour knob { 0xff0b0b0d };
const juce::Colour line { 0xff34343c };
const juce::Colour text { 0xfff2f2f2 };
const juce::Colour dim { 0xff9a9aa3 };
const juce::Colour pack[] = { juce::Colour (0xffd8343c), juce::Colour (0xff2fb8e6), juce::Colour (0xffe0a83a) };
}

const char* packTitles[] = { "KEYS", "SYNTH", "ORCHESTRA" };
} // namespace

StationLookAndFeel::StationLookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, colours::knob);
    setColour (juce::ComboBox::textColourId, juce::Colour (0xffbfe6ff));
    setColour (juce::ComboBox::outlineColourId, colours::line);
    setColour (juce::ComboBox::arrowColourId, colours::dim);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff3a3a46));
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::TextButton::buttonColourId, colours::knob);
    setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff3a3a46));
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::Slider::textBoxTextColourId, colours::dim);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

void StationLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    hw::drawButton (g, b.getLocalBounds().toFloat(), b.getToggleState(), over, down, b.findColour (juce::TextButton::buttonOnColourId), 6.0f);
}

void StationLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    hw::drawLCD (g, juce::Rectangle<int> (w, h).toFloat(), juce::Colour (0xff9fd8ff), box.isEnabled());
    hw::drawArrow (g, (float) w - 14.0f, (float) h * 0.5f, juce::Colour (0xff9fd8ff));
}

void StationLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    hw::drawKnob (g, juce::Rectangle<int> (x, y, w, h).toFloat(), pos, start, end, hw::KnobStyle::aluminium,
                  s.findColour (juce::Slider::rotarySliderFillColourId));
}

//==============================================================================
SetupPanel::SetupPanel (StationProcessor& p) : proc (p)
{
    content.onPaint = [this] (juce::Graphics& g) { paintContent (g); };
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false, true, false);
    addAndMakeVisible (viewport);
    auto addRow = [this] (StationProcessor::Control c, const juce::String& label)
    {
        auto row = std::make_unique<Row>();
        row->control = c;
        row->label = label;
        row->value.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
        row->learn.setClickingTogglesState (false);
        row->learn.onClick = [this, c]
        {
            if (proc.getLearnTarget() == c) proc.cancelLearn();
            else proc.startLearn (c);
        };
        row->clear.onClick = [this, c] { proc.clearBinding (c); };
        content.addAndMakeVisible (row->value);
        content.addAndMakeVisible (row->learn);
        content.addAndMakeVisible (row->clear);
        rows.push_back (std::move (row));
    };
    addRow (StationProcessor::wheel, "Pack selector wheel / knob");
    addRow (StationProcessor::nextPack, "Next pack button");
    addRow (StationProcessor::prevPack, "Previous pack button");
    addRow (StationProcessor::selectKeys, "Keys button");
    addRow (StationProcessor::selectSynth, "Synth button");
    addRow (StationProcessor::selectOrch, "Orchestra button");

    wheelType.addItem ("Knob / fader (3 zones)", 1);
    wheelType.addItem ("Endless wheel (Arturia, 64 +/-)", 2);
    wheelType.addItem ("Endless wheel (1 / 127)", 3);
    wheelType.onChange = [this] { proc.setWheelMode ((StationProcessor::WheelMode) (wheelType.getSelectedId() - 1)); };
    content.addAndMakeVisible (wheelType);

    switchButton.onClick = [this] { proc.setRouting (StationProcessor::switchPacks); };
    splitButton.onClick = [this] { proc.setRouting (StationProcessor::splitInputs); };
    content.addAndMakeVisible (switchButton);
    content.addAndMakeVisible (splitButton);

    for (int i = 0; i < StationProcessor::numPacks; ++i)
    {
        auto& ch = channelBoxes[(size_t) i];
        ch.addItem ("Off", 1);
        ch.addItem ("Any channel", 2);
        for (int c = 1; c <= 16; ++c)
            ch.addItem ("Channel " + juce::String (c), c + 2);
        ch.onChange = [this, i] { proc.setPackChannel (i, channelBoxes[(size_t) i].getSelectedId() - 2); };
        content.addAndMakeVisible (ch);

        auto& dev = deviceBoxes[(size_t) i];
        dev.onChange = [this, i]
        {
            auto& box = deviceBoxes[(size_t) i];
            proc.setPackDevice (i, box.getSelectedId() <= 1 ? juce::String() : box.getText());
        };
        content.addAndMakeVisible (dev);
    }

    doneButton.onClick = [this] { proc.cancelLearn(); if (onClose) onClose(); };
    addAndMakeVisible (doneButton);   // stays put while the page scrolls

    monitor.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
    monitor.setColour (juce::Label::textColourId, juce::Colour (0xff7fe08a));
    content.addAndMakeVisible (monitor);
    deviceNote.setFont (juce::Font (juce::FontOptions (12.0f)));
    deviceNote.setColour (juce::Label::textColourId, colours::dim);
    content.addAndMakeVisible (deviceNote);
}

void SetupPanel::rebuildDeviceBoxes()
{
    auto names = proc.getMidiDeviceNames();
    for (int i = 0; i < StationProcessor::numPacks; ++i)
    {
        auto current = proc.getPackDevice (i);
        auto list = names;
        if (current.isNotEmpty() && ! list.contains (current))
            list.add (current); // keep a saved device listed while it's unplugged
        auto& box = deviceBoxes[(size_t) i];
        box.clear (juce::dontSendNotification);
        box.addItem ("Any device", 1);
        box.addItemList (list, 2);
        box.setSelectedId (current.isEmpty() ? 1 : list.indexOf (current) + 2, juce::dontSendNotification);
    }
    shownDevices = names;
}

void SetupPanel::refresh (int blink)
{
    auto learning = proc.getLearnTarget();
    for (auto& row : rows)
    {
        auto isLearning = learning == row->control;
        row->value.setText (isLearning ? (row->control == StationProcessor::wheel ? "turn the wheel now..." : "press the button / pad now...")
                                       : proc.describeBinding (row->control),
                            juce::dontSendNotification);
        row->value.setColour (juce::Label::textColourId, isLearning ? juce::Colour (0xffffd24d) : colours::text);
        row->learn.setToggleState (isLearning && (blink / 4) % 2 == 0, juce::dontSendNotification);
        row->learn.setButtonText (isLearning ? "CANCEL" : "LEARN");
    }
    wheelType.setSelectedId ((int) proc.getWheelMode() + 1, juce::dontSendNotification);

    auto split = proc.getRouting() == StationProcessor::splitInputs;
    switchButton.setToggleState (! split, juce::dontSendNotification);
    splitButton.setToggleState (split, juce::dontSendNotification);

    auto byDevice = proc.canRouteByDevice();
    if (byDevice && proc.getMidiDeviceNames() != shownDevices)
        rebuildDeviceBoxes();
    for (int i = 0; i < StationProcessor::numPacks; ++i)
    {
        channelBoxes[(size_t) i].setSelectedId (proc.getPackChannel (i) + 2, juce::dontSendNotification);
        channelBoxes[(size_t) i].setEnabled (split);
        deviceBoxes[(size_t) i].setVisible (byDevice);
        deviceBoxes[(size_t) i].setEnabled (split);
    }
    deviceNote.setText (byDevice ? "Give each keyboard its own pack by device, by MIDI channel, or both."
                                 : "In a DAW all keyboards arrive merged: set each keyboard to its own MIDI channel. "
                                   "(Per-device routing works in the Worship Station app.)",
                        juce::dontSendNotification);
    monitor.setText (proc.describeLastMidi(), juce::dontSendNotification);
}

void SetupPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.82f));
    hw::drawPlate (g, plateArea.toFloat(), colours::panel, colours::text, {}, 10.0f, ! hw::isCompact (getLocalBounds()));
    g.setColour (colours::text);
    g.setFont (juce::Font (juce::FontOptions (hw::isCompact (getLocalBounds()) ? 17.0f : 22.0f, juce::Font::bold)));
    g.drawText ("CONTROLLER SETUP", plateArea.reduced (20, 0).removeFromTop (44), juce::Justification::centredLeft);
}

void SetupPanel::paintContent (juce::Graphics& g)
{
    auto inner = content.getLocalBounds().reduced (20, 6);
    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    g.setColour (colours::dim);
    g.drawFittedText ("Press LEARN, then move the control on your keyboard. Arturia: use User / Arturia mode (not DAW mode) so the center wheel sends MIDI CC.",
                      inner.removeFromTop (34), juce::Justification::centredLeft, 2);

    g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
    g.setColour (colours::text);
    for (auto& row : rows)
        g.drawText (row->label, row->value.getBounds().withX (controlsArea.getX()).withWidth (240), juce::Justification::centredLeft);

    g.setColour (colours::dim);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText ("ROUTING", routingArea.withHeight (20), juce::Justification::centredLeft);
    for (int i = 0; i < StationProcessor::numPacks; ++i)
    {
        g.setColour (colours::pack[i]);
        g.drawText (packTitles[i], channelBoxes[(size_t) i].getBounds().withX (routingArea.getX()).withWidth (120), juce::Justification::centredLeft);
    }
}

void SetupPanel::resized()
{
    auto compactScreen = hw::isCompact (getLocalBounds());
    auto safe = hw::safeAreaInsets().subtractedFrom (getLocalBounds());
    plateArea = compactScreen ? safe.reduced (6, 4) : getLocalBounds().reduced (40, 24);
    auto top = plateArea.withHeight (44);
    doneButton.setBounds (top.removeFromRight (120).reduced (6, 7));
    viewport.setBounds (plateArea.withTrimmedTop (44).reduced (8, 0).withTrimmedBottom (8));

    // Page is at least this big; a smaller screen scrolls instead of squeezing.
    content.setSize (juce::jmax (viewport.getMaximumVisibleWidth(), 740), juce::jmax (viewport.getMaximumVisibleHeight(), 600));
    auto inner = content.getLocalBounds().reduced (20, 6);
    inner.removeFromTop (40);

    monitor.setBounds (inner.removeFromBottom (26));
    inner.removeFromBottom (6);

    controlsArea = inner.removeFromTop (6 * 38);
    auto c = controlsArea;
    for (auto& row : rows)
    {
        auto r = c.removeFromTop (38).reduced (0, 4);
        r.removeFromLeft (250);
        row->clear.setBounds (r.removeFromRight (80));
        r.removeFromRight (6);
        row->learn.setBounds (r.removeFromRight (100));
        r.removeFromRight (10);
        if (row->control == StationProcessor::wheel)
        {
            wheelType.setBounds (r.removeFromRight (260));
            r.removeFromRight (10);
        }
        row->value.setBounds (r);
    }

    inner.removeFromTop (16);
    routingArea = inner;
    auto rr = inner;
    rr.removeFromTop (24);
    auto modeRow = rr.removeFromTop (40);
    switchButton.setBounds (modeRow.removeFromLeft (modeRow.getWidth() / 2).withTrimmedRight (6));
    splitButton.setBounds (modeRow.withTrimmedLeft (6));
    rr.removeFromTop (8);
    for (int i = 0; i < StationProcessor::numPacks; ++i)
    {
        auto r = rr.removeFromTop (34).reduced (0, 3);
        r.removeFromLeft (130);
        channelBoxes[(size_t) i].setBounds (r.removeFromLeft (180));
        r.removeFromLeft (10);
        deviceBoxes[(size_t) i].setBounds (r.removeFromLeft (320));
    }
    deviceNote.setBounds (rr.removeFromTop (24));
}

//==============================================================================
StationEditor::StationEditor (StationProcessor& p) : AudioProcessorEditor (p), proc (p), setup (p)
{
    setLookAndFeel (&lnf);
    static const char* volIds[] = { "keysVol", "synthVol", "orchVol" };
    for (int i = 0; i < StationProcessor::numPacks; ++i)
    {
        auto& b = packButtons[(size_t) i];
        b.setButtonText (packTitles[i]);
        b.setColour (juce::TextButton::buttonOnColourId, colours::pack[i]);
        b.onClick = [this, i] { proc.selectPack (i); };
        addAndMakeVisible (b);

        auto& k = volumeKnobs[(size_t) i];
        k.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        k.setColour (juce::Slider::rotarySliderFillColourId, colours::pack[i]);
        k.setPopupDisplayEnabled (true, true, this);
        k.setTooltip (juce::String (packTitles[i]) + " volume");
        volumeAttachments[(size_t) i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, volIds[i], k);
        addAndMakeVisible (k);
    }
    setupButton.onClick = [this] { setup.setVisible (true); setup.toFront (true); };
    addAndMakeVisible (setupButton);

    setup.onClose = [this] { setup.setVisible (false); };
    addChildComponent (setup);

    setResizable (true, true);
   #if JUCE_IOS || JUCE_ANDROID
    setResizeLimits (320, 240, 4000, 4000); // the standalone window stretches the editor to fill the phone
   #else
    setResizeLimits (960, 620, 2600, 1600);
   #endif
    setSize (1280, 800);
    startTimerHz (20);
    timerCallback();
}

StationEditor::~StationEditor()
{
    packEditor.reset();
    setLookAndFeel (nullptr);
}

void StationEditor::showPack (int index)
{
    shownPack = index;
    packEditor.reset();
    packEditor.reset (proc.getPack (index).createEditorIfNeeded());
    if (packEditor != nullptr)
    {
        addAndMakeVisible (*packEditor, 0);
        layoutPackEditor();
    }
    for (int i = 0; i < StationProcessor::numPacks; ++i)
        packButtons[(size_t) i].setToggleState (i == index, juce::dontSendNotification);
    setup.toFront (false);
    repaint (header);
}

void StationEditor::layoutPackEditor()
{
    if (packEditor == nullptr || body.isEmpty())
        return;
    if (compact)
    {
        // A phone: the pack uses its own compact layout at real size (no scaling).
        packEditor->setTransform ({});
        packEditor->setBounds (body);
        return;
    }
    // Each pack keeps its own layout; it's scaled to fit the window.
    auto w = (float) packEditor->getWidth(), h = (float) packEditor->getHeight();
    auto scale = juce::jmin ((float) body.getWidth() / w, (float) body.getHeight() / h, 1.3f);
    auto x = (float) body.getX() + ((float) body.getWidth() - w * scale) * 0.5f;
    auto y = (float) body.getY() + ((float) body.getHeight() - h * scale) * 0.5f;
    packEditor->setTopLeftPosition (0, 0);
    packEditor->setTransform (juce::AffineTransform::scale (scale).translated (x, y));
}

void StationEditor::timerCallback()
{
    auto selected = proc.getSelectedPack();
    if (selected != shownPack)
        showPack (selected);

    ++blink;
    for (int i = 0; i < StationProcessor::numPacks; ++i)
    {
        auto peak = proc.takePackPeak (i);
        meters[(size_t) i] = juce::jmax (peak, meters[(size_t) i] * 0.8f);
    }
    repaint (header);

    if (setup.isVisible())
        setup.refresh (blink);
    proc.saveSettingsIfChanged();
}

void StationEditor::paint (juce::Graphics& g)
{
    // Static parts (wood, metal, title) are cached: the header repaints 20x a second for the meters.
    background.draw (g, getLocalBounds(), [this] (juce::Graphics& bg, juce::Rectangle<float> r)
    {
        hw::drawChassis (bg, r, colours::bg);
        auto head = header.toFloat().withLeft (r.getX() + cheekW).withRight (r.getRight() - cheekW);
        hw::drawChassis (bg, head, juce::Colour (0xff222226), true);
        bg.setColour (juce::Colours::white.withAlpha (0.1f));
        bg.fillRect (head.withHeight (1.0f));
        bg.setColour (juce::Colours::black);
        bg.fillRect (head.withTop (head.getBottom() - 2.0f));
        bg.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.6f), 0, head.getBottom(), juce::Colours::transparentBlack, 0, head.getBottom() + 10, false));
        bg.fillRect (r.withTop (head.getBottom()).withHeight (10.0f));
        hw::drawScrew (bg, { head.getX() + 9.0f, head.getY() + 10.0f }, 3.0f, 0.3f);
        hw::drawScrew (bg, { head.getX() + 9.0f, head.getBottom() - 10.0f }, 3.0f, 1.1f);
        hw::drawScrew (bg, { head.getRight() - 9.0f, head.getY() + 10.0f }, 3.0f, 2.0f);
        hw::drawScrew (bg, { head.getRight() - 9.0f, head.getBottom() - 10.0f }, 3.0f, 0.8f);

        // walnut end cheeks, full height
        hw::drawWood (bg, r.withWidth ((float) cheekW), true);
        hw::drawWood (bg, r.withLeft (r.getRight() - (float) cheekW), true);
        for (auto x : { (float) cheekW, r.getRight() - (float) cheekW })
        {
            bg.setColour (juce::Colours::black.withAlpha (0.6f));
            bg.fillRect (x - 1.0f, r.getY(), 2.0f, r.getHeight());
        }

        if (! compact)
        {
            auto title = header.reduced (18 + cheekW, 0).removeFromLeft (230).toFloat();
            bg.setFont (juce::Font (juce::FontOptions (21.0f, juce::Font::bold)).withExtraKerningFactor (0.06f));
            auto titleRow = title.removeFromTop ((float) header.getHeight() / 2 + 6).withTrimmedTop (8);
            bg.setColour (juce::Colours::black.withAlpha (0.6f));
            bg.drawText ("WORSHIP STATION", titleRow.translated (0, 1.5f), juce::Justification::bottomLeft);
            bg.setColour (colours::text);
            bg.drawText ("WORSHIP STATION", titleRow, juce::Justification::bottomLeft);
        }

        for (int i = 0; i < StationProcessor::numPacks; ++i)
        {
            auto bb = packButtons[(size_t) i].getBounds();
            hw::drawInset (bg, juce::Rectangle<int> (bb.getX(), bb.getBottom() + 3, bb.getWidth(), 6).toFloat(), juce::Colour (0xff050506), 2.0f);
        }
    });

    if (! compact)
    {
        auto title = header.reduced (18 + cheekW, 0).removeFromLeft (230);
        title.removeFromTop (header.getHeight() / 2 + 6);
        g.setColour (colours::dim);
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText (proc.getRouting() == StationProcessor::splitInputs ? "each pack on its own MIDI" : "one keyboard - switch packs",
                    title, juce::Justification::topLeft);
    }

    // Activity meter under each pack button: shows what's sounding, even the packs you switched away from.
    for (int i = 0; i < StationProcessor::numPacks; ++i)
    {
        auto b = packButtons[(size_t) i].getBounds();
        auto bar = juce::Rectangle<int> (b.getX(), b.getBottom() + 3, b.getWidth(), 6).toFloat().reduced (1.5f);
        auto db = juce::Decibels::gainToDecibels (meters[(size_t) i], -60.0f);
        auto frac = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        auto lit = bar.withWidth (bar.getWidth() * frac);
        auto c = colours::pack[i].withAlpha (proc.isPackAwake (i) ? 1.0f : 0.4f);
        g.setColour (c.withAlpha (0.3f * c.getFloatAlpha()));
        g.fillRoundedRectangle (lit.expanded (1.5f), 2.0f);
        g.setColour (c);
        g.fillRoundedRectangle (lit, 1.5f);
    }
}

void StationEditor::resized()
{
    auto safe = hw::safeAreaInsets().subtractedFrom (getLocalBounds()); // clear of the Dynamic Island / home bar
    compact = hw::isCompact (safe);

    auto headH = compact ? 50 : 72;
    header = getLocalBounds().withHeight (safe.getY() + headH);
    headerControls = safe.withHeight (headH);
    body = safe.withTrimmedTop (headH).getIntersection (getLocalBounds().reduced (cheekW, 0));
    if (compact)
        body = body.withTrimmedBottom (0);

    auto h = headerControls.reduced (compact ? 10 : 18, 0);
    if (! compact)
        h.removeFromLeft (240);
    setupButton.setButtonText (compact ? "SETUP" : "CONTROLLER SETUP");
    auto setupW = compact ? 84 : 180;
    setupButton.setBounds (h.removeFromRight (setupW).withSizeKeepingCentre (setupW, compact ? 34 : 40));
    h.removeFromRight (compact ? 8 : 20);
    for (int i = 0; i < StationProcessor::numPacks; ++i)
    {
        auto cell = h.removeFromLeft (juce::jmin (250, h.getWidth() / (StationProcessor::numPacks - i)));
        auto knobArea = cell.removeFromRight (compact ? 40 : 52);
        auto knob = compact ? 34 : 44;
        volumeKnobs[(size_t) i].setBounds (knobArea.withSizeKeepingCentre (knob, knob));
        packButtons[(size_t) i].setBounds (cell.reduced (compact ? 4 : 6, 0).withSizeKeepingCentre (cell.getWidth() - (compact ? 8 : 12), compact ? 34 : 42)
                                               .translated (0, compact ? -3 : -3));
    }

    setup.setBounds (getLocalBounds());
    background.invalidate();
    layoutPackEditor();
}
