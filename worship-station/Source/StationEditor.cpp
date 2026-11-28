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
const juce::Colour gold { 0xffffc93c };
}

const juce::String starChar = juce::String::charToString ((juce::juce_wchar) 0x2605);

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
    addRow (StationProcessor::patchWheel, "Patch wheel (presets)");
    addRow (StationProcessor::nextPatch, "Next patch button");
    addRow (StationProcessor::prevPatch, "Previous patch button");
    addRow (StationProcessor::nextPack, "Next pack button");
    addRow (StationProcessor::prevPack, "Previous pack button");
    addRow (StationProcessor::wheel, "Pack wheel / knob");
    addRow (StationProcessor::selectKeys, "Keys button");
    addRow (StationProcessor::selectSynth, "Synth button");
    addRow (StationProcessor::selectOrch, "Orchestra button");

    for (int i = 0; i < StationProcessor::numPads; ++i)
    {
        auto& pad = padRows[(size_t) i];
        pad.value.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
        for (int st = -StationProcessor::maxTranspose; st <= StationProcessor::maxTranspose; ++st)
            pad.semis.addItem (st == 0 ? juce::String ("0 (song key)") : StationProcessor::formatSemitones (st) + (std::abs (st) == 1 ? " semitone" : " semitones"),
                               st + StationProcessor::maxTranspose + 1);
        pad.semis.onChange = [this, i] { proc.setPadSemitones (i, padRows[(size_t) i].semis.getSelectedId() - StationProcessor::maxTranspose - 1); };
        pad.learn.onClick = [this, i]
        {
            if (proc.getPadLearnTarget() == i) proc.cancelLearn();
            else proc.startPadLearn (i, false);
        };
        pad.clear.onClick = [this, i] { proc.clearPad (i); };
        for (juce::Component* c : { (juce::Component*) &pad.value, (juce::Component*) &pad.semis, (juce::Component*) &pad.learn, (juce::Component*) &pad.clear })
            content.addAndMakeVisible (c);
    }
    learnAllPads.onClick = [this]
    {
        if (proc.getPadLearnTarget() >= 0) proc.cancelLearn();
        else proc.startPadLearn (0, true);
    };
    content.addAndMakeVisible (learnAllPads);

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
    arturiaButton.onClick = [this] { proc.cancelLearn(); proc.useArturiaDefaults(); };
    addAndMakeVisible (arturiaButton);

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
        row->value.setText (isLearning ? (row->control == StationProcessor::wheel || row->control == StationProcessor::patchWheel ? "turn the wheel now..." : "press the button / pad now...")
                                       : proc.describeBinding (row->control),
                            juce::dontSendNotification);
        row->value.setColour (juce::Label::textColourId, isLearning ? juce::Colour (0xffffd24d) : colours::text);
        row->learn.setToggleState (isLearning && (blink / 4) % 2 == 0, juce::dontSendNotification);
        row->learn.setButtonText (isLearning ? "CANCEL" : "LEARN");
    }
    wheelType.setSelectedId ((int) proc.getWheelMode() + 1, juce::dontSendNotification);

    auto learningPad = proc.getPadLearnTarget();
    for (int i = 0; i < StationProcessor::numPads; ++i)
    {
        auto& pad = padRows[(size_t) i];
        auto isLearning = learningPad == i;
        pad.value.setText (isLearning ? "hit pad " + juce::String (i + 1) + " now..." : proc.describePad (i), juce::dontSendNotification);
        pad.value.setColour (juce::Label::textColourId, isLearning ? juce::Colour (0xffffd24d) : colours::text);
        pad.learn.setToggleState (isLearning && (blink / 4) % 2 == 0, juce::dontSendNotification);
        pad.learn.setButtonText (isLearning ? "CANCEL" : "LEARN");
        pad.semis.setSelectedId (proc.getPadSemitones (i) + StationProcessor::maxTranspose + 1, juce::dontSendNotification);
    }
    learnAllPads.setButtonText (learningPad >= 0 ? "STOP LEARNING" : "LEARN ALL 8 PADS");

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
    g.drawFittedText ("KeyLab Essential mk3 works out of the box: center wheel = patches, arrow buttons = Keys / Synth / Orchestra. For other controls press LEARN, then move the control on your keyboard.",
                      inner.removeFromTop (34), juce::Justification::centredLeft, 2);

    g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
    g.setColour (colours::text);
    for (auto& row : rows)
        g.drawText (row->label, row->value.getBounds().withX (controlsArea.getX()).withWidth (240), juce::Justification::centredLeft);

    g.setColour (colours::dim);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText ("TRANSPOSE PADS", padsArea.withHeight (20), juce::Justification::centredLeft);
    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    g.drawFittedText ("Press a pad = transpose by its amount. Press the lit pad again = back to the song key. "
                      "Hold a pad for 1 second = save the current transpose on it. LEARN ALL, then hit pads 1 to 8 in order.",
                      padsArea.withTrimmedTop (20).withHeight (34).withTrimmedRight (210), juce::Justification::centredLeft, 2);
    g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
    g.setColour (colours::text);
    for (int i = 0; i < StationProcessor::numPads; ++i)
        g.drawText ("Pad " + juce::String (i + 1), padRows[(size_t) i].value.getBounds().withX (padsArea.getX()).withWidth (120),
                    juce::Justification::centredLeft);

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
    arturiaButton.setBounds (top.removeFromRight (compactScreen ? 150 : 190).reduced (6, 7));
    viewport.setBounds (plateArea.withTrimmedTop (44).reduced (8, 0).withTrimmedBottom (8));

    constexpr int padsHeight = 20 + 40 + StationProcessor::numPads * 34;
    // Page is at least this big; a smaller screen scrolls instead of squeezing.
    content.setSize (juce::jmax (viewport.getMaximumVisibleWidth(), 740), juce::jmax (viewport.getMaximumVisibleHeight(), 600 + 3 * 38 + padsHeight + 16));
    auto inner = content.getLocalBounds().reduced (20, 6);
    inner.removeFromTop (40);

    monitor.setBounds (inner.removeFromBottom (26));
    inner.removeFromBottom (6);

    controlsArea = inner.removeFromTop ((int) rows.size() * 38);
    auto c = controlsArea;
    for (auto& row : rows)
    {
        auto r = c.removeFromTop (38).reduced (0, 4);
        r.removeFromLeft (250);
        row->clear.setBounds (r.removeFromRight (80));
        r.removeFromRight (6);
        row->learn.setBounds (r.removeFromRight (100));
        r.removeFromRight (10);
        if (row->control == StationProcessor::patchWheel)
        {
            wheelType.setBounds (r.removeFromRight (260));
            r.removeFromRight (10);
        }
        row->value.setBounds (r);
    }

    inner.removeFromTop (16);
    padsArea = inner.removeFromTop (padsHeight);
    {
        auto pa = padsArea;
        auto head = pa.removeFromTop (60);
        learnAllPads.setBounds (head.withTrimmedTop (20).removeFromRight (200).reduced (0, 3));
        for (auto& pad : padRows)
        {
            auto r = pa.removeFromTop (34).reduced (0, 3);
            r.removeFromLeft (250);
            pad.clear.setBounds (r.removeFromRight (80));
            r.removeFromRight (6);
            pad.learn.setBounds (r.removeFromRight (100));
            r.removeFromRight (10);
            pad.semis.setBounds (r.removeFromRight (260));
            r.removeFromRight (10);
            pad.value.setBounds (r);
        }
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
PatchBar::PatchBar (StationProcessor& p) : proc (p)
{
    prev.onClick = [this] { proc.requestPatchStep (proc.getSelectedPack(), -1); };
    next.onClick = [this] { proc.requestPatchStep (proc.getSelectedPack(), 1); };
    prev.setTooltip ("Previous patch (only favorites when FAVORITES ONLY is on)");
    next.setTooltip ("Next patch (only favorites when FAVORITES ONLY is on)");

    patches.onChange = [this]
    {
        auto program = patches.getSelectedId() - 1;
        auto& pack = proc.getPack (proc.getSelectedPack());
        if (program >= 0 && program != pack.getCurrentProgram())
            pack.setCurrentProgram (program);
    };

    star.setButtonText (starChar);
    star.setColour (juce::TextButton::buttonOnColourId, colours::gold.darker (0.35f));
    star.setTooltip ("Favorite this patch");
    star.onClick = [this]
    {
        auto pack = proc.getSelectedPack();
        proc.toggleFavorite (pack, proc.getPack (pack).getCurrentProgram());
    };

    favOnly.setColour (juce::TextButton::buttonOnColourId, colours::gold.darker (0.35f));
    favOnly.setTooltip ("Show only favorites, and make the patch wheel / buttons step through just them");
    favOnly.onClick = [this] { proc.setFavoritesOnly (! proc.getFavoritesOnly()); };

    down.onClick = [this] { proc.setTranspose (proc.getTranspose() - 1); };
    up.onClick = [this] { proc.setTranspose (proc.getTranspose() + 1); };
    down.setTooltip ("Transpose down a semitone");
    up.setTooltip ("Transpose up a semitone");

    for (int i = 0; i < StationProcessor::numPads; ++i)
    {
        auto& pad = pads[(size_t) i];
        pad.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff2f9e5a));
        pad.setTooltip ("Pad " + juce::String (i + 1) + ": press = transpose, press again = back to 0 (set the amounts in CONTROLLER SETUP)");
        pad.onClick = [this, i] { proc.pressPad (i); };
        addAndMakeVisible (pad);
    }
    for (juce::Component* c : { (juce::Component*) &prev, (juce::Component*) &patches, (juce::Component*) &next, (juce::Component*) &star,
                                (juce::Component*) &favOnly, (juce::Component*) &down, (juce::Component*) &up })
        addAndMakeVisible (c);
}

void PatchBar::rebuildList (int pack)
{
    auto& p = proc.getPack (pack);
    auto favs = proc.getFavoritePrograms (pack);
    auto onlyFavs = proc.getFavoritesOnly() && ! favs.isEmpty();
    patches.clear (juce::dontSendNotification);
    for (int i = 0; i < p.getNumPrograms(); ++i)
    {
        auto isFav = favs.contains (i);
        if (onlyFavs && ! isFav)
            continue;
        patches.addItem ((isFav ? starChar + " " : juce::String()) + p.getProgramName (i), i + 1);
    }
}

void PatchBar::refresh()
{
    auto pack = proc.getSelectedPack();
    auto program = proc.getPack (pack).getCurrentProgram();
    if (pack != shownPack || proc.getFavoritesVersion() != shownVersion)
    {
        shownPack = pack;
        shownVersion = proc.getFavoritesVersion();
        shownProgram = -1;
        rebuildList (pack);
    }
    if (program != shownProgram)
    {
        shownProgram = program;
        if (patches.indexOfItemId (program + 1) >= 0)
            patches.setSelectedId (program + 1, juce::dontSendNotification);
        else
            patches.setText (proc.getPack (pack).getProgramName (program), juce::dontSendNotification);
    }
    star.setToggleState (proc.isFavorite (pack, program), juce::dontSendNotification);
    favOnly.setToggleState (proc.getFavoritesOnly(), juce::dontSendNotification);

    auto active = proc.getActivePad();
    for (int i = 0; i < StationProcessor::numPads; ++i)
    {
        pads[(size_t) i].setButtonText (StationProcessor::formatSemitones (proc.getPadSemitones (i)));
        pads[(size_t) i].setToggleState (i == active, juce::dontSendNotification);
    }

    auto saved = proc.takePadSavedEvent();
    if (saved >= 0)
    {
        status = "PAD " + juce::String (saved + 1) + " SAVED " + StationProcessor::formatSemitones (proc.getPadSemitones (saved));
        statusTicks = 50; // ~2.5 s at 20 Hz
    }
    else if (statusTicks > 0 && --statusTicks == 0)
        status.clear();
    repaint (transposeArea);
}

void PatchBar::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff17171b));
    g.fillRect (r);
    g.setColour (juce::Colours::black);
    g.fillRect (r.withTop (r.getBottom() - 1.0f));

    // Transpose readout: an LCD between the - / + buttons. Green while transposed.
    auto t = proc.getTranspose();
    auto colour = t == 0 ? juce::Colour (0xff9fd8ff) : juce::Colour (0xff7fe08a);
    hw::drawLCD (g, transposeArea.toFloat(), colour, true);
    g.setColour (colour);
    g.setFont (juce::Font (juce::FontOptions (status.isNotEmpty() ? 11.0f : 14.0f, juce::Font::bold)));
    g.drawText (status.isNotEmpty() ? status : "KEY " + (t == 0 ? juce::String ("0") : StationProcessor::formatSemitones (t)),
                transposeArea, juce::Justification::centred);
}

void PatchBar::resized()
{
    compact = getWidth() < 900;
    auto r = getLocalBounds().reduced (compact ? 6 : 10, compact ? 4 : 6);
    auto gap = compact ? 4 : 6;
    auto btn = r.getHeight();

    prev.setBounds (r.removeFromLeft (btn));
    r.removeFromLeft (gap);
    auto right = r.removeFromRight (compact ? 0 : StationProcessor::numPads * (btn + 4) + 10);
    right.removeFromLeft (10);
    for (auto& pad : pads)
    {
        pad.setVisible (! compact);
        pad.setBounds (right.removeFromLeft (btn + 4).withTrimmedRight (4));
    }

    // - [KEY +2] +
    up.setBounds (r.removeFromRight (btn));
    transposeArea = r.removeFromRight (compact ? 76 : 120).reduced (3, 0);
    down.setBounds (r.removeFromRight (btn));
    r.removeFromRight (compact ? 8 : 16);

    favOnly.setButtonText (compact ? starChar + " ONLY" : "FAVORITES ONLY");
    favOnly.setBounds (r.removeFromRight (compact ? 70 : 150));
    r.removeFromRight (gap);
    star.setBounds (r.removeFromRight (btn + 6));
    r.removeFromRight (gap);
    next.setBounds (r.removeFromRight (btn));
    r.removeFromRight (gap);
    patches.setBounds (r);
}

//==============================================================================
StationEditor::StationEditor (StationProcessor& p) : AudioProcessorEditor (p), proc (p), patchBar (p), setup (p)
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
    addAndMakeVisible (patchBar);

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

    patchBar.refresh();
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
    patchBar.setBounds (body.removeFromTop (compact ? 38 : 46));

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
