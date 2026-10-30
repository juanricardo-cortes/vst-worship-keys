#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <map>

// Shared "real hardware" drawing: shaded knobs with LED rings, fader caps, backlit buttons,
// LCD windows, textured panels and screws. Used by Worship Keys, Anthem Synth and Worship Station,
// each with its own colours. Everything is drawn in code (no image files) so it stays sharp at any scale.
namespace hw
{
enum class KnobStyle { black, aluminium, brass };

inline juce::Colour mix (juce::Colour a, juce::Colour b, float t) { return a.interpolatedWith (b, t); }

// Fine grain to break up flat colour fills (powder coat / anodizing).
inline const juce::Image& noiseTile()
{
    static juce::Image img = []
    {
        juce::Image im (juce::Image::ARGB, 128, 128, true);
        juce::Random rng (1234);
        for (int y = 0; y < 128; ++y)
            for (int x = 0; x < 128; ++x)
            {
                auto v = (juce::uint8) rng.nextInt (256);
                im.setPixelAt (x, y, juce::Colour (v, v, v, (juce::uint8) 10));
            }
        return im;
    }();
    return img;
}

inline void addGrain (juce::Graphics& g, juce::Rectangle<float> r, float opacity = 1.0f)
{
    juce::Graphics::ScopedSaveState s (g);
    g.reduceClipRegion (r.toNearestInt());
    g.setTiledImageFill (noiseTile(), 0, 0, opacity);
    g.fillRect (r);
}

// Horizontal brushed-metal streaks.
inline void addBrushing (juce::Graphics& g, juce::Rectangle<float> r, float strength = 0.05f)
{
    juce::Random rng (99);
    for (float y = r.getY(); y < r.getBottom(); y += 1.0f)
    {
        auto a = rng.nextFloat() * strength;
        g.setColour ((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (a));
        g.fillRect (r.getX(), y, r.getWidth(), 1.0f);
    }
}

// Metal panel: soft top-to-bottom light falloff, grain and optional brushing.
inline void drawChassis (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour base, bool brushed = false)
{
    g.setGradientFill (juce::ColourGradient (base.brighter (0.12f), r.getX(), r.getY(), base.darker (0.25f), r.getX(), r.getBottom(), false));
    g.fillRect (r);
    if (brushed) addBrushing (g, r, 0.035f);
    addGrain (g, r);
}

// Procedural walnut: wavy grain lines over a warm gradient.
inline void drawWood (juce::Graphics& g, juce::Rectangle<float> r, bool vertical = false)
{
    juce::Graphics::ScopedSaveState s (g);
    g.reduceClipRegion (r.toNearestInt());
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5a3520), r.getX(), r.getY(), juce::Colour (0xff3a2112),
                                             vertical ? r.getRight() : r.getX(), vertical ? r.getY() : r.getBottom(), false));
    g.fillRect (r);
    juce::Random rng (7);
    auto len = vertical ? r.getHeight() : r.getWidth();
    auto across = vertical ? r.getWidth() : r.getHeight();
    for (float o = 0; o < across; o += 1.6f + rng.nextFloat() * 2.4f)
    {
        juce::Path p;
        auto phase = rng.nextFloat() * 6.28f, amp = 0.6f + rng.nextFloat() * 2.2f, freq = 0.004f + rng.nextFloat() * 0.01f;
        for (float t = 0; t <= len; t += 6.0f)
        {
            auto d = o + amp * std::sin (phase + t * freq) + 0.6f * std::sin (t * freq * 3.7f);
            auto pt = vertical ? juce::Point<float> (r.getX() + d, r.getY() + t) : juce::Point<float> (r.getX() + t, r.getY() + d);
            if (t == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        g.setColour ((rng.nextFloat() < 0.6f ? juce::Colour (0xff1e0f06) : juce::Colour (0xff8a5a36)).withAlpha (0.12f + rng.nextFloat() * 0.25f));
        g.strokePath (p, juce::PathStrokeType (0.6f + rng.nextFloat() * 1.2f));
    }
    // satin finish
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.08f), r.getX(), r.getY(), juce::Colours::transparentBlack,
                                             r.getX(), r.getY() + r.getHeight() * 0.5f, false));
    g.fillRect (r);
}

inline void drawScrew (juce::Graphics& g, juce::Point<float> c, float rad, float angle = 0.6f)
{
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (juce::Rectangle<float> (rad * 2.3f, rad * 2.3f).withCentre (c.translated (0, rad * 0.25f)));
    auto head = juce::Rectangle<float> (rad * 2, rad * 2).withCentre (c);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd9d9de), head.getX(), head.getY(), juce::Colour (0xff55555c), head.getRight(), head.getBottom(), false));
    g.fillEllipse (head);
    juce::Path slot;
    slot.addRectangle (-rad * 0.75f, -rad * 0.14f, rad * 1.5f, rad * 0.28f);
    g.setColour (juce::Colour (0xff2a2a2e));
    g.fillPath (slot, juce::AffineTransform::rotation (angle).translated (c));
}

// Raised panel plate with drop shadow, bevelled edges, screws and a silkscreened title.
inline void drawPlate (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour base, juce::Colour titleColour,
                       const juce::String& title = {}, float corner = 6.0f, bool screws = true)
{
    for (int i = 4; i >= 1; --i)
    {
        g.setColour (juce::Colours::black.withAlpha (0.07f * (float) (5 - i)));
        g.fillRoundedRectangle (r.translated (0, (float) i * 0.8f).expanded ((float) i * 0.6f), corner + (float) i);
    }
    g.setGradientFill (juce::ColourGradient (base.brighter (0.1f), r.getX(), r.getY(), base.darker (0.2f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, corner);
    addGrain (g, r.reduced (corner * 0.5f), 0.8f);
    // bevel: light top edge, dark bottom edge
    g.setColour (juce::Colours::white.withAlpha (0.13f));
    g.drawLine (r.getX() + corner, r.getY() + 0.75f, r.getRight() - corner, r.getY() + 0.75f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);

    if (screws && r.getWidth() > 80 && r.getHeight() > 60)
    {
        auto in = r.reduced (8.0f);
        drawScrew (g, in.getTopLeft(), 3.0f, 0.4f);
        drawScrew (g, in.getTopRight(), 3.0f, 1.3f);
        drawScrew (g, in.getBottomLeft(), 3.0f, 2.2f);
        drawScrew (g, in.getBottomRight(), 3.0f, 0.9f);
    }
    if (title.isNotEmpty())
    {
        auto font = juce::Font (juce::FontOptions (12.0f, juce::Font::bold)).withExtraKerningFactor (0.08f);
        g.setFont (font);
        auto tx = r.getX() + (screws ? 18.0f : 12.0f), ty = r.getY() + 6.0f;
        auto tw = juce::GlyphArrangement::getStringWidth (font, title);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawText (title, juce::Rectangle<float> (tx, ty + 1, tw + 4, 16), juce::Justification::centredLeft);
        g.setColour (titleColour);
        g.drawText (title, juce::Rectangle<float> (tx, ty, tw + 4, 16), juce::Justification::centredLeft);
        auto lineX = tx + tw + 10, lineEnd = r.getRight() - (screws ? 18.0f : 12.0f);
        if (lineEnd > lineX)
        {
            g.setColour (titleColour.withAlpha (0.3f));
            g.fillRect (lineX, ty + 8.0f, lineEnd - lineX, 1.0f);
        }
    }
}

// A recessed window (scope / meter / LCD surround).
inline void drawInset (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fill, float corner = 4.0f)
{
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.fillRoundedRectangle (r.translated (0, 1.0f), corner);
    g.setColour (fill);
    g.fillRoundedRectangle (r, corner);
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.55f), r.getX(), r.getY(), juce::Colours::transparentBlack,
                                             r.getX(), r.getY() + juce::jmin (10.0f, r.getHeight() * 0.4f), false));
    g.fillRoundedRectangle (r, corner);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
}

inline void drawGlass (juce::Graphics& g, juce::Rectangle<float> r, float corner = 4.0f)
{
    juce::Path glare;
    glare.startNewSubPath (r.getX(), r.getY());
    glare.lineTo (r.getRight(), r.getY());
    glare.lineTo (r.getRight(), r.getY() + r.getHeight() * 0.22f);
    glare.quadraticTo (r.getCentreX(), r.getY() + r.getHeight() * 0.5f, r.getX(), r.getY() + r.getHeight() * 0.42f);
    glare.closeSubPath();
    juce::Graphics::ScopedSaveState s (g);
    juce::Path clip;
    clip.addRoundedRectangle (r, corner);
    g.reduceClipRegion (clip);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.09f), r.getX(), r.getY(), juce::Colours::white.withAlpha (0.01f),
                                             r.getX(), r.getY() + r.getHeight() * 0.5f, false));
    g.fillPath (glare);
}

// LCD display window (used for preset boxes / dropdowns).
inline void drawLCD (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour tint, bool enabled = true)
{
    auto bezel = r;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff050506), bezel.getX(), bezel.getY(), juce::Colour (0xff2a2a2e), bezel.getX(), bezel.getBottom(), false));
    g.fillRoundedRectangle (bezel, 4.0f);
    auto screen = r.reduced (2.5f);
    auto c0 = mix (juce::Colour (0xff07090a), tint, 0.10f), c1 = mix (juce::Colour (0xff0d1012), tint, 0.18f);
    g.setGradientFill (juce::ColourGradient (c0, screen.getX(), screen.getY(), c1, screen.getX(), screen.getBottom(), false));
    g.fillRoundedRectangle (screen, 2.5f);
    // faint scanlines like a dot-matrix display
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    for (float y = screen.getY() + 1; y < screen.getBottom(); y += 2.0f)
        g.fillRect (screen.getX() + 2, y, screen.getWidth() - 4, 0.6f);
    drawGlass (g, screen, 2.5f);
    if (! enabled)
    {
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillRoundedRectangle (r, 4.0f);
    }
}

inline void drawArrow (juce::Graphics& g, float cx, float cy, juce::Colour c)
{
    juce::Path arrow;
    arrow.addTriangle (cx - 4, cy - 2, cx + 4, cy - 2, cx, cy + 3);
    g.setColour (c.withAlpha (0.35f));
    g.strokePath (arrow, juce::PathStrokeType (3.0f));
    g.setColour (c);
    g.fillPath (arrow);
}

inline void drawLed (juce::Graphics& g, juce::Point<float> c, float rad, juce::Colour colour, float level)
{
    if (level > 0.01f)
    {
        g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.45f * level), c.x, c.y, colour.withAlpha (0.0f), c.x + rad * 3.2f, c.y, true));
        g.fillEllipse (juce::Rectangle<float> (rad * 6.4f, rad * 6.4f).withCentre (c));
    }
    auto body = juce::Rectangle<float> (rad * 2, rad * 2).withCentre (c);
    auto lit = mix (colour.darker (2.2f).withSaturation (0.5f), colour.brighter (0.3f), level);
    g.setGradientFill (juce::ColourGradient (lit.brighter (0.5f * level + 0.1f), body.getX(), body.getY(), lit.darker (0.3f), body.getRight(), body.getBottom(), false));
    g.fillEllipse (body);
    g.setColour (juce::Colours::white.withAlpha (0.25f + 0.35f * level));
    g.fillEllipse (body.getX() + rad * 0.45f, body.getY() + rad * 0.3f, rad * 0.7f, rad * 0.5f);
}

// Knob with an LED dot ring. `from` is where the lit part of the ring starts (for bipolar controls).
inline void drawKnob (juce::Graphics& g, juce::Rectangle<float> area, float pos, float start, float end,
                      KnobStyle style, juce::Colour accent, float fromPos = 0.0f)
{
    auto size = juce::jmin (area.getWidth(), area.getHeight());
    auto r = area.withSizeKeepingCentre (size, size);
    auto c = r.getCentre();
    auto R = size * 0.5f;
    auto angle = start + pos * (end - start);

    // LED ring
    const int dots = size > 56 ? 21 : 15;
    auto dotR = juce::jlimit (1.1f, 2.0f, R * 0.055f);
    auto ringR = R - dotR - 1.0f;
    auto lo = juce::jmin (fromPos, pos), hi = juce::jmax (fromPos, pos);
    for (int i = 0; i < dots; ++i)
    {
        auto t = (float) i / (float) (dots - 1);
        auto a = start + t * (end - start);
        auto p = c.getPointOnCircumference (ringR, a);
        auto halfStep = 0.5f / (float) (dots - 1);
        auto on = t >= lo - halfStep && t <= hi + halfStep;
        drawLed (g, p, dotR, accent, on ? 1.0f : 0.0f);
    }

    auto kR = R * 0.70f;
    auto body = juce::Rectangle<float> (kR * 2, kR * 2).withCentre (c);

    // soft contact shadow, light from the top
    for (int i = 5; i >= 1; --i)
    {
        g.setColour (juce::Colours::black.withAlpha (0.08f));
        g.fillEllipse (body.translated (0, kR * 0.06f * (float) i).expanded (kR * 0.035f * (float) i));
    }

    const bool metal = style != KnobStyle::black;
    juce::Colour skirtA, skirtB, capHi, capLo;
    switch (style)
    {
        case KnobStyle::black:     skirtA = juce::Colour (0xff2e2e33); skirtB = juce::Colour (0xff050506); capHi = juce::Colour (0xff3c3c42); capLo = juce::Colour (0xff0b0b0d); break;
        case KnobStyle::aluminium: skirtA = juce::Colour (0xff3a3d44); skirtB = juce::Colour (0xff0c0d10); capHi = juce::Colour (0xffe8e9ed); capLo = juce::Colour (0xff7d8088); break;
        case KnobStyle::brass:     skirtA = juce::Colour (0xff3a3226); skirtB = juce::Colour (0xff0d0a06); capHi = juce::Colour (0xfff6dc9a); capLo = juce::Colour (0xff8a6526); break;
    }

    // skirt with knurled grip that turns with the knob
    g.setGradientFill (juce::ColourGradient (skirtA, body.getX(), body.getY(), skirtB, body.getRight(), body.getBottom(), false));
    g.fillEllipse (body);
    const int ridges = 36;
    for (int i = 0; i < ridges; ++i)
    {
        auto a = angle + juce::MathConstants<float>::twoPi * (float) i / (float) ridges;
        auto lit = 0.5f + 0.5f * std::cos (a + 0.8f);   // ridges facing the light are brighter
        g.setColour (juce::Colours::white.withAlpha (0.03f + 0.10f * lit));
        g.drawLine (juce::Line<float> (c.getPointOnCircumference (kR * 0.84f, a), c.getPointOnCircumference (kR * 0.985f, a)), 1.0f);
    }
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawEllipse (body, 1.0f);

    // cap
    auto capR = kR * 0.80f;
    auto cap = juce::Rectangle<float> (capR * 2, capR * 2).withCentre (c);
    g.setGradientFill (juce::ColourGradient (capHi, cap.getX() + capR * 0.3f, cap.getY(), capLo, cap.getRight() - capR * 0.3f, cap.getBottom(), false));
    g.fillEllipse (cap);
    if (metal)
    {
        // concentric machining rings
        for (float rr = capR * 0.96f; rr > 1.5f; rr -= 1.3f)
        {
            g.setColour ((((int) (rr * 3.7f)) % 2 == 0 ? juce::Colours::white : juce::Colours::black).withAlpha (0.045f));
            g.drawEllipse (juce::Rectangle<float> (rr * 2, rr * 2).withCentre (c), 0.7f);
        }
        // fake conical reflections (fixed: the light doesn't turn with the knob)
        for (auto base : { -0.75f, 2.39f })
        {
            juce::Path wedge;
            wedge.addPieSegment (cap, base - 0.45f, base + 0.45f, 0.0f);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.38f), c.x, c.y,
                                                     juce::Colours::white.withAlpha (0.0f), c.getPointOnCircumference (capR, base + 0.45f).x,
                                                     c.getPointOnCircumference (capR, base + 0.45f).y, false));
            g.fillPath (wedge);
        }
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawEllipse (cap, 0.8f);
    }
    else
    {
        // soft-touch dome highlight
        auto hl = cap.reduced (capR * 0.18f).withHeight (capR * 0.9f).translated (0, -capR * 0.12f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.16f), hl.getCentreX(), hl.getY(), juce::Colours::white.withAlpha (0.0f),
                                                 hl.getCentreX(), hl.getBottom(), false));
        g.fillEllipse (hl);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawEllipse (cap.reduced (0.5f), 0.8f);
    }

    // pointer
    auto p0 = c.getPointOnCircumference (capR * 0.22f, angle), p1 = c.getPointOnCircumference (capR * 0.93f, angle);
    auto lw = juce::jmax (2.0f, capR * 0.12f);
    if (metal)
    {
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.drawLine (juce::Line<float> (p0.translated (0, 0.8f), p1.translated (0, 0.8f)), lw);
        g.setColour (juce::Colour (0xff141416));
        g.drawLine (juce::Line<float> (p0, p1), lw);
    }
    else
    {
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawLine (juce::Line<float> (p0.translated (0, 1), p1.translated (0, 1)), lw);
        g.setColour (juce::Colour (0xfff4f4f4));
        g.drawLine (juce::Line<float> (p0, p1), lw);
    }
}

// Keeps an expensive static background (wood, brushed metal) as an image; redrawn only on resize or zoom.
struct CachedBackground
{
    template <typename PaintFn>
    void draw (juce::Graphics& g, juce::Rectangle<int> area, PaintFn&& paintFn)
    {
        auto scale = g.getInternalContext().getPhysicalPixelScaleFactor();
        if (img.isNull() || area != lastArea || std::abs (scale - lastScale) > 0.01f)
        {
            lastArea = area;
            lastScale = scale;
            img = juce::Image (juce::Image::ARGB, juce::jmax (1, juce::roundToInt ((float) area.getWidth() * scale)),
                               juce::jmax (1, juce::roundToInt ((float) area.getHeight() * scale)), true);
            juce::Graphics ig (img);
            ig.addTransform (juce::AffineTransform::scale (scale));
            paintFn (ig, area.withZeroOrigin().toFloat());
        }
        g.drawImage (img, area.toFloat());
    }
    void invalidate() { img = {}; }

private:
    juce::Image img;
    juce::Rectangle<int> lastArea;
    float lastScale = 0;
};

// Mixing-desk fader: recessed slot with a glowing level strip, printed scale and a ridged cap.
inline void drawFader (juce::Graphics& g, juce::Rectangle<float> r, float capY, juce::Colour accent, juce::Colour scale,
                       float capH = 30.0f, float maxCapW = 56.0f)
{
    auto capW = juce::jmin (r.getWidth() - 4.0f, maxCapW);
    auto cx = r.getCentreX();
    auto slot = juce::Rectangle<float> (cx - 3.5f, r.getY() + capH * 0.5f, 7.0f, r.getHeight() - capH);

    g.setColour (scale.withAlpha (0.55f));
    for (int i = 0; i <= 10; ++i)
    {
        auto ty = slot.getY() + slot.getHeight() * (float) i / 10.0f;
        auto len = i % 5 == 0 ? 12.0f : 7.0f;
        g.fillRect (cx - 12.0f - len, ty, len, 1.0f);
        g.fillRect (cx + 12.0f, ty, len, 1.0f);
    }

    drawInset (g, slot, juce::Colour (0xff030304), 3.5f);
    auto fill = slot.reduced (2.0f).withTop (juce::jmin (capY, slot.getBottom() - 2.0f));
    if (fill.getHeight() > 0)
    {
        g.setColour (accent.withAlpha (0.25f));
        g.fillRoundedRectangle (fill.expanded (2.0f, 0), 2.0f);
        g.setGradientFill (juce::ColourGradient (accent.brighter (0.3f), fill.getX(), fill.getY(), accent.darker (0.4f), fill.getX(), fill.getBottom(), false));
        g.fillRoundedRectangle (fill, 1.5f);
    }

    auto cap = juce::Rectangle<float> (capW, capH).withCentre ({ cx, capY });
    for (int i = 4; i >= 1; --i)
    {
        g.setColour (juce::Colours::black.withAlpha (0.1f));
        g.fillRoundedRectangle (cap.translated (0, (float) i * 1.5f).expanded ((float) i * 0.7f), 3.0f + (float) i);
    }
    // ridged profile: light top bevel, dark dip, light bottom lip
    juce::ColourGradient grad (juce::Colour (0xff6a6a72), cap.getX(), cap.getY(), juce::Colour (0xff141416), cap.getX(), cap.getBottom(), false);
    grad.addColour (0.18, juce::Colour (0xff3c3c42));
    grad.addColour (0.48, juce::Colour (0xff1c1c20));
    grad.addColour (0.52, juce::Colour (0xff101012));
    grad.addColour (0.82, juce::Colour (0xff2c2c31));
    g.setGradientFill (grad);
    g.fillRoundedRectangle (cap, 3.0f);
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    for (int i = 1; i <= 3; ++i)
    {
        g.fillRect (cap.getX() + 5, cap.getY() + cap.getHeight() * 0.12f * (float) i, cap.getWidth() - 10, 1.0f);
        g.fillRect (cap.getX() + 5, cap.getBottom() - cap.getHeight() * 0.12f * (float) i, cap.getWidth() - 10, 1.0f);
    }
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawLine (cap.getX() + 3, cap.getY() + 0.8f, cap.getRight() - 3, cap.getY() + 0.8f, 1.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (cap, 3.0f, 1.0f);
    g.setColour (juce::Colour (0xfff0f0f0));
    g.fillRect (cap.getX() + 4.0f, cap.getCentreY() - 1.0f, cap.getWidth() - 8.0f, 2.0f);
}

// Rubber / backlit push button. `on` lights it in the accent colour.
inline void drawButton (juce::Graphics& g, juce::Rectangle<float> r, bool on, bool over, bool down, juce::Colour accent, float corner = 5.0f)
{
    r = r.reduced (1.0f, 1.5f);
    for (int i = 3; i >= 1; --i)
    {
        g.setColour (juce::Colours::black.withAlpha (down ? 0.05f : 0.12f));
        g.fillRoundedRectangle (r.translated (0, (float) i * (down ? 0.4f : 0.9f)).expanded ((float) i * 0.5f), corner + (float) i);
    }
    auto face = down ? r.translated (0, 1.0f) : r;
    juce::Colour top, bottom;
    if (on) { top = accent.brighter (0.15f); bottom = accent.darker (0.55f); }
    else    { top = juce::Colour (0xff36363c); bottom = juce::Colour (0xff141417); }
    if (over && ! down) { top = top.brighter (0.12f); bottom = bottom.brighter (0.08f); }
    if (down) { top = top.darker (0.15f); }
    g.setGradientFill (juce::ColourGradient (top, face.getX(), face.getY(), bottom, face.getX(), face.getBottom(), false));
    g.fillRoundedRectangle (face, corner);
    if (on)
    {
        // light glowing through the cap
        g.setGradientFill (juce::ColourGradient (accent.brighter (0.6f).withAlpha (0.45f), face.getCentreX(), face.getCentreY(),
                                                 accent.withAlpha (0.0f), face.getCentreX(), face.getY() - face.getHeight() * 0.4f, true));
        g.fillRoundedRectangle (face, corner);
    }
    g.setColour (juce::Colours::white.withAlpha (on ? 0.35f : 0.14f));
    g.drawLine (face.getX() + corner, face.getY() + 0.8f, face.getRight() - corner, face.getY() + 0.8f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.85f));
    g.drawRoundedRectangle (face.reduced (0.5f), corner, 1.0f);
}

// Keyboard with ivory-ish white keys, glossy black keys and a felt strip.
class Keyboard : public juce::MidiKeyboardComponent
{
public:
    Keyboard (juce::MidiKeyboardState& s, Orientation o, juce::Colour pressedColour = juce::Colour (0xffe03b3b))
        : MidiKeyboardComponent (s, o), pressed (pressedColour)
    {
        setColour (keySeparatorLineColourId, juce::Colours::transparentBlack);
        setColour (shadowColourId, juce::Colours::transparentBlack);
        setColour (upDownButtonBackgroundColourId, juce::Colour (0xff1a1a1d));
        setColour (upDownButtonArrowColourId, juce::Colour (0xffbbbbbb));
        setColour (textLabelColourId, juce::Colour (0xff8a8a8a));
        setBlackNoteLengthProportion (0.62f);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff0b0b0c));
        MidiKeyboardComponent::paint (g);
        // felt strip along the top
        auto felt = getLocalBounds().toFloat().removeFromTop (4.0f);
        g.setColour (pressed.darker (1.4f));
        g.fillRect (felt);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.5f), 0, felt.getBottom(), juce::Colours::transparentBlack, 0, felt.getBottom() + 8, false));
        g.fillRect (getLocalBounds().toFloat().withTop (felt.getBottom()).withHeight (8.0f));
    }

protected:
    void drawWhiteNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour, juce::Colour) override
    {
        auto k = area.reduced (0.6f, 0).withTrimmedBottom (isDown ? 0.0f : 1.5f);
        g.setGradientFill (juce::ColourGradient (isDown ? juce::Colour (0xffd8d4ca) : juce::Colour (0xfffaf8f2), k.getX(), k.getY(),
                                                 isDown ? juce::Colour (0xffc4c0b6) : juce::Colour (0xffe6e2d8), k.getX(), k.getBottom(), false));
        g.fillRoundedRectangle (k.withTrimmedTop (-3.0f), 2.5f);
        if (isDown)
        {
            g.setColour (pressed.withAlpha (0.35f));
            g.fillRoundedRectangle (k.withTrimmedTop (k.getHeight() * 0.55f).reduced (2, 2), 2.0f);
        }
        else if (isOver)
        {
            g.setColour (juce::Colours::black.withAlpha (0.05f));
            g.fillRect (k);
        }
        // front lip
        g.setColour (juce::Colours::black.withAlpha (isDown ? 0.08f : 0.16f));
        g.fillRect (k.withTop (k.getBottom() - 4.0f));
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRect (area.getRight() - 0.6f, area.getY(), 0.6f, area.getHeight());

        if (note % 12 == 0)
        {
            g.setColour (juce::Colour (0xff8a8a8a));
            g.setFont (juce::Font (juce::FontOptions (juce::jmin (11.0f, area.getWidth() * 0.55f))));
            g.drawText (getWhiteNoteText (note), k.withTrimmedBottom (6.0f).withTrimmedLeft (2.0f), juce::Justification::bottomLeft);
        }
    }

    void drawBlackNote (int, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour) override
    {
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillRoundedRectangle (area.translated (1.5f, 1.0f).expanded (0.5f, 0), 2.0f);
        auto k = area.reduced (0.5f, 0);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2a2d), k.getX(), k.getY(), juce::Colour (0xff050505), k.getX(), k.getBottom(), false));
        g.fillRoundedRectangle (k.withTrimmedTop (-3.0f), 2.0f);
        auto front = k.withTop (k.getBottom() - k.getHeight() * (isDown ? 0.08f : 0.14f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3a3e), front.getX(), front.getY(), juce::Colour (0xff141416), front.getX(), front.getBottom(), false));
        g.fillRoundedRectangle (front, 1.5f);
        // gloss
        g.setColour (juce::Colours::white.withAlpha (isOver ? 0.16f : 0.10f));
        g.fillRoundedRectangle (k.reduced (k.getWidth() * 0.22f, 0).withTop (k.getY() + 2).withHeight (k.getHeight() * 0.6f), 1.5f);
        if (isDown)
        {
            g.setColour (pressed.withAlpha (0.55f));
            g.fillRoundedRectangle (k.reduced (1.5f).withTop (k.getY() + k.getHeight() * 0.5f), 1.5f);
        }
    }

private:
    juce::Colour pressed;
};
// A page of controls that paints itself through a callback (used inside a Viewport on small screens).
struct Page : public juce::Component
{
    std::function<void (juce::Graphics&)> onPaint;
    void paint (juce::Graphics& g) override { if (onPaint) onPaint (g); }
};

// Phones: insets that keep controls clear of the Dynamic Island, rounded corners and home bar.
// The snapshot tests set this to fake an iPhone screen on the desktop.
inline juce::BorderSize<int>& safeAreaOverride() { static juce::BorderSize<int> b; return b; }

inline juce::BorderSize<int> safeAreaInsets()
{
    auto insets = safeAreaOverride();
   #if JUCE_IOS || JUCE_ANDROID
    if (insets.isEmpty())
        if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
            insets = d->safeAreaInsets;
   #endif
    return insets;
}

// The part of an editor that's safe to put controls in. An editor hosted inside another editor
// (a pack inside Worship Station) uses all of its bounds: the host already keeps clear of the edges.
inline juce::Rectangle<int> safeArea (juce::Component& editor)
{
    if (editor.findParentComponentOfClass<juce::AudioProcessorEditor>() != nullptr)
        return editor.getLocalBounds();
    return safeAreaInsets().subtractedFrom (editor.getLocalBounds());
}

// Small screens (phones in landscape) get compact, scrolling layouts.
inline bool isCompact (juce::Rectangle<int> area) { return area.getWidth() < 1000 || area.getHeight() < 560; }
} // namespace hw
