#include "XOLookAndFeel.h"
#include "BinaryData.h"

using namespace juce;

namespace
{
    Typeface::Ptr bodySemi()
    {
        static Typeface::Ptr t = Typeface::createSystemTypefaceFor (BinaryData::InterMedium_ttf, BinaryData::InterMedium_ttfSize);
        return t;
    }
    Typeface::Ptr bodyBold()
    {
        static Typeface::Ptr t = Typeface::createSystemTypefaceFor (BinaryData::InterBold_ttf, BinaryData::InterBold_ttfSize);
        return t;
    }
    Typeface::Ptr titleFace()
    {
        static Typeface::Ptr t = Typeface::createSystemTypefaceFor (BinaryData::SpaceGroteskBold_ttf, BinaryData::SpaceGroteskBold_ttfSize);
        return t;
    }

    void glowStroke (Graphics& g, const Path& p, Colour c, float w)
    {
        g.setColour (c.withAlpha (0.12f)); g.strokePath (p, PathStrokeType (w * 2.4f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (c);                   g.strokePath (p, PathStrokeType (w, PathStrokeType::curved, PathStrokeType::rounded));
    }
}

XOLookAndFeel::XOLookAndFeel()
{
    setColour (ComboBox::backgroundColourId, Colours_::knobBody);
    setColour (ComboBox::textColourId, Colours_::text);
    setColour (ComboBox::outlineColourId, Colours_::panelEdge);
    setColour (ComboBox::arrowColourId, Colours_::red);
    setColour (PopupMenu::backgroundColourId, Colour (0xff0d1016));
    setColour (PopupMenu::textColourId, Colours_::text);
    setColour (PopupMenu::highlightedBackgroundColourId, Colours_::red.withAlpha (0.85f));
    setColour (PopupMenu::highlightedTextColourId, Colours_::onAccent);
    setColour (PopupMenu::headerTextColourId, Colours_::cyan);
    setColour (TextButton::buttonColourId, Colours_::knobBody);
    setColour (TextButton::buttonOnColourId, Colours_::red);
    setColour (TextButton::textColourOffId, Colours_::text);
    setColour (TextButton::textColourOnId, Colours_::onAccent);
    setColour (Label::textColourId, Colours_::text);
    setColour (TextEditor::backgroundColourId, Colours_::knobBody);
    setColour (TextEditor::textColourId, Colours_::text);
    setColour (TextEditor::highlightColourId, Colours_::red.withAlpha (0.4f));
    setColour (TextEditor::outlineColourId, Colours_::panelEdge);
    setColour (TextEditor::focusedOutlineColourId, Colours_::red);
    setColour (CaretComponent::caretColourId, Colours_::red);
    setColour (TooltipWindow::backgroundColourId, Colour (0xff0d1016));
    setColour (TooltipWindow::textColourId, Colours_::text);
    setColour (TooltipWindow::outlineColourId, Colours_::red);
    setColour (AlertWindow::backgroundColourId, Colours_::panel);
    setColour (AlertWindow::textColourId, Colours_::text);
    setColour (AlertWindow::outlineColourId, Colours_::red);
    setColour (ScrollBar::thumbColourId, Colours_::red.withAlpha (0.6f));
    setColour (ListBox::backgroundColourId, Colours::transparentBlack);
}

Typeface::Ptr XOLookAndFeel::getTypefaceForFont (const Font& f)
{
    if (f.getTypefacePtr() != nullptr && f.getTypefaceName() != Font::getDefaultSansSerifFontName())
        return LookAndFeel_V4::getTypefaceForFont (f);
    return f.isBold() ? bodyBold() : bodySemi();
}

Font XOLookAndFeel::font (float size, bool bold)
{
    return Font (FontOptions (bold ? bodyBold() : bodySemi()).withHeight (size * 1.08f));
}

Font XOLookAndFeel::title (float size)
{
    return Font (FontOptions (titleFace()).withHeight (size));
}

Path XOLookAndFeel::chamfer (Rectangle<float> r, float c)
{
    Path p;
    p.addRoundedRectangle (r, jmin (c * 0.45f, 6.0f));
    return p;
}

void XOLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                            float startAngle, float endAngle, Slider& s)
{
    const auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float size = jmin (bounds.getWidth(), bounds.getHeight());
    const auto c = bounds.getCentre();
    const float radius = size * 0.5f;
    const bool hover = s.isMouseOverOrDragging();
    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const bool small = size < 36.0f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const Colour accent = Colours_::red;

    // outer tick ring
    if (! small)
    {
        const int ticks = 25;
        for (int i = 0; i < ticks; ++i)
        {
            const float t = (float) i / (float) (ticks - 1);
            const float a = startAngle + t * (endAngle - startAngle);
            const bool lit = bipolar ? ((t >= 0.5f && t <= pos + 0.001f) || (t <= 0.5f && t >= pos - 0.001f)) : t <= pos + 0.001f;
            const float r1 = radius - 1.0f, r0 = radius - (i % 6 == 0 ? 5.0f : 3.0f);
            g.setColour (lit ? accent.withAlpha (0.9f) : Colours_::track);
            g.drawLine (c.x + r0 * std::sin (a), c.y - r0 * std::cos (a), c.x + r1 * std::sin (a), c.y - r1 * std::cos (a), 1.4f);
        }
    }

    // value arc with glow
    const float arcR = small ? radius - 1.5f : radius - 8.0f;
    const float lineW = small ? 2.0f : jmax (2.0f, size * 0.045f);
    Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (Colours_::track);
    g.strokePath (track, PathStrokeType (lineW, PathStrokeType::curved, PathStrokeType::rounded));
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - from) > 0.01f)
    {
        Path val;
        val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, jmin (from, angle), jmax (from, angle), true);
        glowStroke (g, val, hover ? accent.brighter (0.35f) : accent, lineW);
        g.setColour (accent.withAlpha (0.08f)); g.strokePath (val, PathStrokeType (lineW * 4.0f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // body
    const float bodyR = arcR - lineW - (small ? 1.5f : 3.0f);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillEllipse (c.x - bodyR, c.y - bodyR + 2.5f, bodyR * 2.0f, bodyR * 2.0f);
    ColourGradient grad (Colour (0xff232935), c.x - bodyR * 0.4f, c.y - bodyR * 0.7f,
                         Colour (0xff080a0e), c.x + bodyR * 0.3f, c.y + bodyR, true);
    g.setGradientFill (grad);
    g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (hover ? accent.withAlpha (0.8f) : Colour (0xff2e3746));
    g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);
    if (! small)
    {
        g.setColour (Colours_::red.withAlpha (0.08f));
        g.drawEllipse (c.x - bodyR * 0.62f, c.y - bodyR * 0.62f, bodyR * 1.24f, bodyR * 1.24f, 1.0f);
    }

    // pointer with glowing tip
    const float pIn = bodyR * 0.35f, pOut = bodyR * 0.88f;
    const Point<float> a (c.x + pIn * std::sin (angle), c.y - pIn * std::cos (angle));
    const Point<float> b (c.x + pOut * std::sin (angle), c.y - pOut * std::cos (angle));
    g.setColour (Colours_::text);
    g.drawLine ({ a, b }, small ? 1.5f : 2.0f);
    g.setColour (accent.withAlpha (0.25f));
    g.fillEllipse (b.x - 3.5f, b.y - 3.5f, 7.0f, 7.0f);
    g.setColour (accent);
    g.fillEllipse (b.x - 1.8f, b.y - 1.8f, 3.6f, 3.6f);
}

void XOLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                            Slider::SliderStyle, Slider&)
{
    const auto r = Rectangle<float> ((float) x, (float) y + (float) h * 0.5f - 2.0f, (float) w, 4.0f);
    g.setColour (Colours_::track);
    g.fillRoundedRectangle (r, 2.0f);
    Path p; p.addRoundedRectangle (r.withRight (pos), 2.0f);
    g.setColour (Colours_::red.withAlpha (0.3f)); g.fillRoundedRectangle (r.withRight (pos).expanded (0, 2), 3.0f);
    g.setColour (Colours_::red); g.fillPath (p);
    g.setColour (Colours_::knobBody);
    g.fillEllipse (pos - 6.0f, r.getCentreY() - 6.0f, 12.0f, 12.0f);
    g.setColour (Colours_::red);
    g.drawEllipse (pos - 6.0f, r.getCentreY() - 6.0f, 12.0f, 12.0f, 1.5f);
}

void XOLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    const auto r = Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
    auto shape = chamfer (r, 4.0f);
    g.setColour (Colours_::knobBody);
    g.fillPath (shape);
    g.setColour (box.isMouseOver (true) ? Colours_::red.withAlpha (0.8f) : Colours_::panelEdge);
    g.strokePath (shape, PathStrokeType (1.0f));

    Path arrow;
    const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
    arrow.startNewSubPath (ax - 4.0f, ay - 2.0f);
    arrow.lineTo (ax, ay + 2.0f);
    arrow.lineTo (ax + 4.0f, ay - 2.0f);
    g.setColour (Colours_::red);
    g.strokePath (arrow, PathStrokeType (1.6f));
}

void XOLookAndFeel::positionComboBoxText (ComboBox& box, Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 22, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

Font XOLookAndFeel::getComboBoxFont (ComboBox&) { return font (13.0f); }
Font XOLookAndFeel::getPopupMenuFont()          { return font (14.5f); }
Font XOLookAndFeel::getLabelFont (Label& l)     { return font (jmax (12.0f, l.getFont().getHeight() * 0.85f)); }

void XOLookAndFeel::drawPopupMenuBackground (Graphics& g, int w, int h)
{
    g.fillAll (findColour (PopupMenu::backgroundColourId));
    g.setColour (Colours_::red.withAlpha (0.5f));
    g.drawRect (0, 0, w, h);
}

void XOLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool highlighted, bool)
{
    const auto r = b.getLocalBounds().toFloat();
    const auto pill = Rectangle<float> (0.0f, 0.0f, 32.0f, 16.0f).withCentre ({ 18.0f, r.getCentreY() });
    const bool on = b.getToggleState();
    g.setColour (on ? Colours_::red.withAlpha (0.25f) : Colours_::track);
    g.fillRoundedRectangle (pill, 8.0f);
    g.setColour (on ? Colours_::red : (highlighted ? Colours_::textDim : Colours_::panelEdge));
    g.drawRoundedRectangle (pill, 8.0f, 1.2f);
    const float knobX = on ? pill.getRight() - 14.0f : pill.getX() + 2.0f;
    if (on) { g.setColour (Colours_::red.withAlpha (0.35f)); g.fillEllipse (knobX - 3.0f, pill.getY() - 1.0f, 18.0f, 18.0f); }
    g.setColour (on ? Colours_::red : Colours_::textDim);
    g.fillEllipse (knobX, pill.getY() + 2.0f, 12.0f, 12.0f);

    g.setColour (on ? Colours_::text : Colours_::textDim);
    g.setFont (font (13.0f, true));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (40.0f), Justification::centredLeft);
}

void XOLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool highlighted, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto shape = chamfer (r, 3.5f);
    const bool on = b.getToggleState();
    if (b.getProperties()["accent"])   // filled call-to-action button (GENERATE)
    {
        g.setColour (down ? Colours_::red.darker (0.25f) : (highlighted ? Colours_::red.brighter (0.15f) : Colours_::red));
        g.fillPath (shape);
        return;
    }
    g.setColour (down || on ? Colours_::red.withAlpha (0.28f) : Colours_::knobBody);
    g.fillPath (shape);
    g.setColour (highlighted || on ? Colours_::red : Colours_::panelEdge);
    g.strokePath (shape, PathStrokeType (1.0f));
}

void XOLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool highlighted, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    if (b.getProperties()["accent"]) { g.setFont (font (15.0f, true)); g.setColour (Colours_::onAccent); }
    else g.setColour (b.getToggleState() ? Colours_::deep : (highlighted ? Colours_::red : Colours_::text));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), Justification::centred, 1);
}

Font XOLookAndFeel::getTextButtonFont (TextButton&, int h) { return font (h < 28 ? 12.5f : 13.5f, true); }

void XOLookAndFeel::fillTextEditorBackground (Graphics& g, int w, int h, TextEditor&)
{
    g.setColour (Colours_::knobBody);
    g.fillPath (chamfer (Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f), 4.0f));
}

void XOLookAndFeel::drawTextEditorOutline (Graphics& g, int w, int h, TextEditor& te)
{
    g.setColour (te.hasKeyboardFocus (true) ? Colours_::red : Colours_::panelEdge);
    g.strokePath (chamfer (Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f), 4.0f), PathStrokeType (1.0f));
}

void XOLookAndFeel::drawScrollbar (Graphics& g, ScrollBar&, int x, int y, int w, int h, bool vertical,
                                         int thumbStart, int thumbSize, bool over, bool)
{
    const auto thumb = vertical ? Rectangle<float> ((float) x + (float) w * 0.5f - 2.0f, (float) thumbStart, 4.0f, (float) thumbSize)
                                : Rectangle<float> ((float) thumbStart, (float) y + (float) h * 0.5f - 2.0f, (float) thumbSize, 4.0f);
    g.setColour (Colours_::red.withAlpha (over ? 0.9f : 0.5f));
    g.fillRoundedRectangle (thumb, 2.0f);
}
