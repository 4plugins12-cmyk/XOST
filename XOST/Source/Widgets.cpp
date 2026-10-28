#include "Widgets.h"

using namespace juce;
using namespace xo;
using ui::LnF;

// ================================================================== helpers
void ui::addModulation (XOSTProcessor& p, int source, int dest)
{
    if (source <= 0 || dest <= 0) return;
    auto& s = p.apvts;
    for (int i = 0; i < numSlots; ++i)
    {
        auto* srcP = s.getParameter (slotId (i, "src"));
        auto* dstP = s.getParameter (slotId (i, "dst"));
        const int sv = (int) s.getRawParameterValue (slotId (i, "src"))->load();
        const int dv = (int) s.getRawParameterValue (slotId (i, "dst"))->load();
        if (sv == source && dv == dest) return;   // already routed
        if (sv == 0 || dv == 0)
        {
            p.undo.beginNewTransaction ("Add modulation");
            srcP->setValueNotifyingHost (srcP->convertTo0to1 ((float) source));
            dstP->setValueNotifyingHost (dstP->convertTo0to1 ((float) dest));
            const bool pitchDest = dest == dstAPitch || dest == dstBPitch || dest == dstCPitch;
            auto* a = s.getParameter (slotId (i, "amt"));
            a->setValueNotifyingHost (a->convertTo0to1 (pitchDest ? 0.04f : 0.5f));
            return;
        }
    }
}

float ui::modAmountFor (XOSTProcessor& p, int dest)
{
    float sum = 0.0f;
    for (int i = 0; i < numSlots; ++i)
        if ((int) p.apvts.getRawParameterValue (slotId (i, "dst"))->load() == dest
            && (int) p.apvts.getRawParameterValue (slotId (i, "src"))->load() > 0)
            sum += p.apvts.getRawParameterValue (slotId (i, "amt"))->load();
    return sum;
}

// ================================================================== Knob
Knob::Knob (XOSTProcessor& p, const String& id, const String& l, Style s)
    : paramID (id), dest (destForParam (id)), proc (p), label (l), style (s)
{
    param = proc.apvts.getParameter (id);
    jassert (param != nullptr);
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (s == Style::big ? 260 : 200);
    slider.onPopup = [this] { showMenu(); };
    slider.onGestureStart = [this] { proc.undo.beginNewTransaction (param->getName (40)); };
    slider.onValueChange = [this] { repaint(); };
    slider.setTooltip (param->getName (64) + (dest > 0 ? "  - drop a mod source here to modulate it" : String()));
    const auto r = param->getNormalisableRange();
    slider.getProperties().set ("bipolar", r.start < 0.0f && r.end > 0.0f);
    addAndMakeVisible (slider);
    attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, slider);
    slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    addMouseListener (this, true);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    if (style == Style::compact) { slider.setBounds (r.removeFromLeft (r.getHeight())); return; }
    const int lab = style == Style::mini ? 15 : 17;
    slider.setBounds (r.withTrimmedBottom (lab).withSizeKeepingCentre (jmin (r.getWidth(), r.getHeight() - lab), r.getHeight() - lab));
}

void Knob::paint (Graphics& g)
{
    const bool active = slider.isMouseOverOrDragging();
    const String valueText = param->getCurrentValueAsText();
    if (style == Style::compact)
    {
        g.setFont (LnF::font (12.5f, true));
        g.setColour (active ? Colours_::deep : Colours_::textDim);
        g.drawText (valueText, getLocalBounds().withTrimmedLeft (getHeight() + 4), Justification::centredLeft);
        return;
    }
    auto labelArea = getLocalBounds().removeFromBottom (style == Style::mini ? 15 : 17);
    g.setColour (active ? Colours_::deep : Colours_::text.withAlpha (0.8f));
    g.setFont (LnF::font (style == Style::mini ? 10.5f : (style == Style::big ? 13.0f : 11.5f), true));
    g.drawFittedText (active ? valueText : label.toUpperCase(), labelArea, Justification::centred, 1, 0.8f);
}

void Knob::paintOverChildren (Graphics& g)
{
    if (dest <= 0 && ! dragOver) return;
    const auto sb = slider.getBounds().toFloat().reduced (2.0f);
    const float size = jmin (sb.getWidth(), sb.getHeight());
    const auto c = sb.getCentre();
    const float r = size * 0.5f - 1.0f;
    const float a0 = MathConstants<float>::pi * 1.25f, a1 = MathConstants<float>::pi * 2.75f;
    if (dragOver)
    {
        g.setColour (Colours_::cyan.withAlpha (0.35f));
        g.drawEllipse (c.x - r - 2, c.y - r - 2, (r + 2) * 2, (r + 2) * 2, 3.0f);
    }
    const float amt = ui::modAmountFor (proc, dest);
    if (std::abs (amt) < 0.001f) return;
    const float pos = param->getValue();
    const float span = dest == dstAPitch || dest == dstBPitch || dest == dstCPitch ? amt * 24.0f / 48.0f : amt;
    const float to = jlimit (0.0f, 1.0f, pos + span);
    Path p;
    p.addCentredArc (c.x, c.y, r, r, 0.0f, a0 + (a1 - a0) * jmin (pos, to), a0 + (a1 - a0) * jmax (pos, to), true);
    g.setColour (Colours_::cyan.withAlpha (0.25f));
    g.strokePath (p, PathStrokeType (5.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (Colours_::cyan);
    g.strokePath (p, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
}

bool Knob::isInterestedInDragSource (const SourceDetails& d)
{
    return dest > 0 && d.description.toString().startsWith ("mod:");
}

void Knob::itemDropped (const SourceDetails& d)
{
    dragOver = false;
    ui::addModulation (proc, d.description.toString().fromFirstOccurrenceOf ("mod:", false, false).getIntValue(), dest);
    repaint();
}

void Knob::showMenu()
{
    PopupMenu m;
    m.addSectionHeader (param->getName (64));
    m.addItem (1, "Reset to default");
    if (dest > 0)
    {
        PopupMenu add;
        for (int s = 1; s < numSources; ++s) add.addItem (100 + s, srcNames[s]);
        m.addSubMenu ("Modulate with", add);
        for (int i = 0; i < numSlots; ++i)
            if ((int) proc.apvts.getRawParameterValue (slotId (i, "dst"))->load() == dest)
            {
                const int s = (int) proc.apvts.getRawParameterValue (slotId (i, "src"))->load();
                if (s > 0) m.addItem (1000 + i, "Remove " + srcNames[s] + " modulation");
            }
    }
    SafePointer<Knob> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&slider), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& pr = safe->proc;
        if (r == 1) { pr.undo.beginNewTransaction ("Reset"); safe->param->setValueNotifyingHost (safe->param->getDefaultValue()); }
        else if (r >= 1000)
        {
            pr.undo.beginNewTransaction ("Remove modulation");
            for (auto* w : { "src", "dst", "amt" }) if (auto* q = pr.apvts.getParameter (slotId (r - 1000, w))) q->setValueNotifyingHost (q->getDefaultValue());
        }
        else if (r >= 100) ui::addModulation (pr, r - 100, safe->dest);
        safe->repaint();
    });
}

// ================================================================== ChoiceBox / PowerButton
ChoiceBox::ChoiceBox (XOSTProcessor& p, const String& id, const String& c) : paramID (id), caption (c)
{
    if (auto* cp = dynamic_cast<AudioParameterChoice*> (p.apvts.getParameter (id))) box.addItemList (cp->choices, 1);
    box.setTooltip (p.apvts.getParameter (id)->getName (64));
    addAndMakeVisible (box);
    attachment = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, id, box);
    auto* bp = &p; auto name = p.apvts.getParameter (id)->getName (40);
    struct Begin : MouseListener { XOSTProcessor* pr; String n; void mouseDown (const MouseEvent&) override { pr->undo.beginNewTransaction (n); } };
    auto* l = new Begin(); l->pr = bp; l->n = name;
    beginListener.reset (l);
    box.addMouseListener (l, true);
}

void ChoiceBox::resized()
{
    auto r = getLocalBounds();
    if (caption.isNotEmpty()) r.removeFromTop (14);
    box.setBounds (r.withSizeKeepingCentre (r.getWidth(), jmin (24, r.getHeight())));
}

void ChoiceBox::paint (Graphics& g)
{
    if (caption.isEmpty()) return;
    g.setColour (Colours_::textDim);
    g.setFont (LnF::font (10.5f, true));
    g.drawText (caption.toUpperCase(), getLocalBounds().removeFromTop (13), Justification::centredLeft);
}

PowerButton::PowerButton (XOSTProcessor& p, const String& id, const String& buttonText) : ToggleButton (buttonText)
{
    attachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, id, *this);
    setTooltip (p.apvts.getParameter (id)->getName (64));
    auto* pr = &p; auto name = p.apvts.getParameter (id)->getName (40);
    onStateChange = nullptr;
    struct Begin : MouseListener { XOSTProcessor* p2; String n; void mouseDown (const MouseEvent&) override { p2->undo.beginNewTransaction (n); } };
    auto* l = new Begin(); l->p2 = pr; l->n = name;
    beginListener.reset (l);
    addMouseListener (l, false);
}

void PowerButton::paintButton (Graphics& g, bool over, bool)
{
    auto r = getLocalBounds().toFloat();
    const bool on = getToggleState();
    auto c = Rectangle<float> (16, 16).withCentre ({ 10.0f, r.getCentreY() });
    g.setColour (on ? Colours_::red : Colours_::track);
    g.fillEllipse (c);
    if (on) { g.setColour (Colours_::red.withAlpha (0.25f)); g.drawEllipse (c.expanded (3), 2.0f); }
    g.setColour (on ? Colours_::onAccent : Colours_::textDim);
    Path pw; pw.addCentredArc (c.getCentreX(), c.getCentreY(), 4.5f, 4.5f, 0, 0.7f, MathConstants<float>::twoPi - 0.7f, true);
    g.strokePath (pw, PathStrokeType (1.5f));
    g.drawLine (c.getCentreX(), c.getCentreY() - 6, c.getCentreX(), c.getCentreY() - 1, 1.5f);
    if (getButtonText().isNotEmpty())
    {
        g.setColour (on ? Colours_::text : (over ? Colours_::text : Colours_::textDim));
        g.setFont (LnF::title (13.5f));
        g.drawText (getButtonText(), r.withTrimmedLeft (24), Justification::centredLeft);
    }
}

// ================================================================== ModSourceChip
void ModSourceChip::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const float v = juce::jlimit (-1.0f, 1.0f, proc.engine.uiSrc[src]);
    g.setColour (Colours_::cyan.withAlpha (0.12f + 0.25f * std::abs (v)));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (Colours_::cyan);
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.2f);
    g.fillEllipse (r.getX() + 6, r.getCentreY() - 3.5f, 7, 7);
    g.setFont (LnF::font (11.0f, true));
    g.setColour (Colours_::text);
    g.drawText (label, r.withTrimmedLeft (17), Justification::centredLeft);
}

void ModSourceChip::mouseDrag (const MouseEvent& e)
{
    if (e.getDistanceFromDragStart() < 4) return;
    if (auto* c = DragAndDropContainer::findParentDragContainerFor (this))
        if (! c->isDragAndDropActive())
        {
            Image img (Image::ARGB, getWidth(), getHeight(), true);
            Graphics g (img); paint (g);
            c->startDragging ("mod:" + String (src), this, ScaledImage (img), true);
        }
}

// ================================================================== WaveDisplay
void WaveDisplay::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours_::screen);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (hover ? Colours_::red : Colours_::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, hover ? 2.0f : 1.0f);
    g.setColour (Colours_::red.withAlpha (0.08f));
    g.drawHorizontalLine ((int) r.getCentreY(), r.getX() + 6, r.getRight() - 6);

    auto& s = proc.apvts;
    const int t = (int) s.getRawParameterValue (oscId (osc, "table"))->load();
    const float pos = s.getRawParameterValue (oscId (osc, "pos"))->load();
    const bool on = s.getRawParameterValue (oscId (osc, "on"))->load() > 0.5f;
    const Wavetable* wt = (t == 16 && proc.getUserWavetable (osc) != nullptr) ? proc.getUserWavetable (osc) : &WavetableBank::get().table (t);
    const int pts = 160;
    wt->getDisplayFrame (pos, frame, pts);

    // ghost frames to show the table's evolution, Serum-style
    auto area = r.reduced (10.0f, 8.0f);
    std::vector<float> ghost;
    for (int k = 0; k < 5; ++k)
    {
        wt->getDisplayFrame ((float) k / 4.0f, ghost, pts);
        Path gp;
        const float off = (float) (k - 2) * 3.0f;
        for (int i = 0; i < pts; ++i)
        {
            const float x = area.getX() + area.getWidth() * (float) i / (float) (pts - 1) + off;
            const float y = area.getCentreY() - ghost[(size_t) i] * area.getHeight() * 0.42f - off;
            if (i == 0) gp.startNewSubPath (x, y); else gp.lineTo (x, y);
        }
        g.setColour (Colours_::red.withAlpha (0.07f));
        g.strokePath (gp, PathStrokeType (1.0f));
    }
    Path p;
    for (int i = 0; i < pts; ++i)
    {
        const float x = area.getX() + area.getWidth() * (float) i / (float) (pts - 1);
        const float y = area.getCentreY() - frame[(size_t) i] * area.getHeight() * 0.46f;
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    Path fill (p);
    fill.lineTo (area.getRight(), area.getCentreY()); fill.lineTo (area.getX(), area.getCentreY()); fill.closeSubPath();
    g.setColour ((on ? Colours_::red : Colours_::textDim).withAlpha (0.12f));
    g.fillPath (fill);
    g.setColour (on ? Colours_::red : Colours_::textDim);
    g.strokePath (p, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));

    g.setFont (LnF::font (10.5f, true));
    g.setColour (Colours_::textDim);
    g.drawText (hover ? "DROP AUDIO TO MAKE A WAVETABLE" : (t == 16 ? (proc.getUserWavetable (osc) ? "USER TABLE" : "USER (DROP A WAV)") : Choice::tables[t].toUpperCase())
                + "   FRAME " + String (1 + roundToInt (pos * (float) (wt->getNumFrames() - 1))) + "/" + String (wt->getNumFrames()),
                r.reduced (10, 4).removeFromBottom (13).toNearestInt(), Justification::centredLeft);
}

void WaveDisplay::filesDropped (const StringArray& files, int, int)
{
    hover = false;
    if (! files.isEmpty()) proc.loadWavetableFile (osc, File (files[0]));
    repaint();
}

// ================================================================== SampleDisplay
void SampleDisplay::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours_::screen);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (hover ? Colours_::red : Colours_::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, hover ? 2.0f : 1.0f);
    const auto* s = proc.getSample();
    if (s == nullptr)
    {
        g.setColour (Colours_::textDim);
        g.setFont (LnF::font (13.0f, true));
        g.drawFittedText (hover ? "Drop it!" : "Drag & drop a sample here\n(or click to browse)", getLocalBounds().reduced (8), Justification::centred, 2);
        return;
    }
    if (cachedFor != s)
    {
        cachedFor = s;
        overview.assign (200, 0.0f);
        const int len = s->length();
        for (int i = 0; i < 200; ++i)
        {
            float pk = 0;
            for (int k = (int) ((double) i * len / 200); k < (int) ((double) (i + 1) * len / 200); ++k) pk = std::max (pk, std::abs (s->L[(size_t) k]));
            overview[(size_t) i] = pk;
        }
    }
    auto area = r.reduced (8.0f, 6.0f).withTrimmedBottom (12.0f);
    g.setColour (Colours_::red.withAlpha (0.6f));
    for (int i = 0; i < 200; ++i)
    {
        const float x = area.getX() + area.getWidth() * (float) i / 199.0f;
        const float h = overview[(size_t) i] * area.getHeight() * 0.5f;
        g.drawVerticalLine ((int) x, area.getCentreY() - h, area.getCentreY() + h);
    }
    auto marker = [&] (float pos, Colour c)
    {
        const float x = area.getX() + area.getWidth() * pos;
        g.setColour (c); g.fillRect (x - 1.0f, area.getY(), 2.0f, area.getHeight());
    };
    const int mode = (int) proc.apvts.getRawParameterValue ("smp_mode")->load();
    marker (mode == 2 ? proc.apvts.getRawParameterValue ("smp_gpos")->load() : proc.apvts.getRawParameterValue ("smp_start")->load(), Colours_::cyan);
    g.setFont (LnF::font (10.5f, true));
    g.setColour (Colours_::textDim);
    g.drawText (s->name.toUpperCase() + "  -  " + String ((double) s->length() / s->sampleRate, 2) + " s   (click to replace)",
                r.reduced (10, 3).removeFromBottom (13).toNearestInt(), Justification::centredLeft);
}

void SampleDisplay::mouseUp (const MouseEvent&)
{
    chooser = std::make_unique<FileChooser> ("Load a sample", File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    SafePointer<SampleDisplay> safe (this);
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [safe] (const FileChooser& fc)
    {
        if (safe == nullptr || fc.getResult() == File()) return;
        safe->proc.loadSampleFile (fc.getResult());
        safe->repaint();
    });
}

void SampleDisplay::filesDropped (const StringArray& files, int, int)
{
    hover = false;
    if (! files.isEmpty()) proc.loadSampleFile (File (files[0]));
    repaint();
}

// ================================================================== FilterDisplay
void FilterDisplay::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours_::screen);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (Colours_::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
    auto area = r.reduced (8.0f, 8.0f);
    auto raw = [this] (const String& id) { return proc.apvts.getRawParameterValue (id)->load(); };
    for (int f = 1; f >= 0; --f)
    {
        if (raw (fltId (f, "on")) < 0.5f) continue;
        const float fc = raw (fltId (f, "cutoff")), reso = raw (fltId (f, "reso"));
        const int type = (int) raw (fltId (f, "type"));
        Path p;
        for (int i = 0; i <= 100; ++i)
        {
            const float t = (float) i / 100.0f, fr = 20.0f * std::pow (1000.0f, t), x = fr / fc;
            const float peak = 1.0f + reso * reso * 5.0f * std::exp (-std::pow (std::log2 (x) * 2.4f, 2.0f));
            float mag;
            switch (type)
            {
                case 0: mag = 1.0f / std::sqrt (1.0f + std::pow (x, 4.0f)) * peak; break;
                case 1: case 2: mag = 1.0f / std::sqrt (1.0f + std::pow (x, 8.0f)) * peak; break;
                case 3: mag = 1.0f / std::sqrt (1.0f + std::pow (1.0f / x, 4.0f)) * peak; break;
                case 4: mag = 1.0f / std::sqrt (1.0f + std::pow (1.0f / x, 8.0f)) * peak; break;
                case 5: mag = 1.0f / std::sqrt (1.0f + std::pow ((x - 1.0f / x) * (1.6f - reso * 1.4f), 2.0f)); break;
                case 6: mag = std::abs (1.0f - x * x) / std::sqrt (std::pow (1.0f - x * x, 2.0f) + std::pow (x * (1.2f - reso), 2.0f)); break;
                case 7: mag = 1.0f + reso * 3.0f / (1.0f + std::pow ((x - 1.0f / x) * 3.0f, 2.0f)); break;
                case 8: mag = 1.0f + reso * 0.8f * std::cos (twoPi * x); break;
                default: mag = 0.3f + 0.7f * std::exp (-std::pow (std::log2 (x) * 1.5f, 2.0f)) * (1.0f + reso); break;
            }
            const float db = jlimit (-30.0f, 18.0f, 20.0f * std::log10 (jmax (1.0e-4f, mag)));
            const float y = area.getY() + area.getHeight() * (1.0f - (db + 30.0f) / 48.0f);
            const float px = area.getX() + area.getWidth() * t;
            if (i == 0) p.startNewSubPath (px, y); else p.lineTo (px, y);
        }
        const Colour c = f == 0 ? Colours_::red : Colours_::cyan;
        Path fill (p); fill.lineTo (area.getRight(), area.getBottom()); fill.lineTo (area.getX(), area.getBottom()); fill.closeSubPath();
        g.setColour (c.withAlpha (0.12f)); g.fillPath (fill);
        g.setColour (c); g.strokePath (p, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
    }
}

// ================================================================== EnvDisplay / LfoDisplay
void EnvDisplay::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours_::screen); g.fillRoundedRectangle (r, 10.0f);
    g.setColour (Colours_::panelEdge); g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
    auto raw = [this] (const char* s) { return proc.apvts.getRawParameterValue (envId (env, s))->load(); };
    const float d = raw ("delay"), a = raw ("attack"), h = raw ("hold"), dc = raw ("decay"), s = raw ("sustain"), rl = raw ("release"), cv = raw ("curve");
    const float susT = 250.0f;
    auto tw = [] (float ms) { return std::sqrt (ms + 1.0f); };
    const float total = tw (d) + tw (a) + tw (h) + tw (dc) + tw (susT) + tw (rl);
    auto area = r.reduced (8.0f, 8.0f);
    const float pw = cv >= 0.0f ? 1.0f + cv * 5.0f : 1.0f / (1.0f - cv * 3.0f);
    auto shape = [pw] (float x) { return 1.0f - std::pow (1.0f - x, pw); };
    Path p;
    float x = area.getX();
    const float W = area.getWidth() / total;
    auto Y = [&] (float lvl) { return area.getBottom() - lvl * area.getHeight(); };
    p.startNewSubPath (x, Y (0));
    x += tw (d) * W; p.lineTo (x, Y (0));
    for (int i = 1; i <= 20; ++i) p.lineTo (x + tw (a) * W * (float) i / 20.0f, Y (shape ((float) i / 20.0f)));
    x += tw (a) * W; x += tw (h) * W; p.lineTo (x, Y (1));
    for (int i = 1; i <= 20; ++i) p.lineTo (x + tw (dc) * W * (float) i / 20.0f, Y (s + (1 - s) * (1 - shape ((float) i / 20.0f))));
    x += tw (dc) * W; x += tw (susT) * W; p.lineTo (x, Y (s));
    for (int i = 1; i <= 20; ++i) p.lineTo (x + tw (rl) * W * (float) i / 20.0f, Y (s * (1 - shape ((float) i / 20.0f))));
    Path fill (p); fill.lineTo (area.getRight(), area.getBottom()); fill.closeSubPath();
    g.setColour (Colours_::red.withAlpha (0.12f)); g.fillPath (fill);
    g.setColour (Colours_::red); g.strokePath (p, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
    const float live = proc.engine.uiEnv[env].load();
    g.setColour (Colours_::cyan);
    g.fillEllipse (area.getRight() - 6, Y (live) - 4, 8, 8);
}

void LfoDisplay::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours_::screen); g.fillRoundedRectangle (r, 10.0f);
    g.setColour (Colours_::panelEdge); g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
    const int shape = (int) proc.apvts.getRawParameterValue (lfoId (lfo, "shape"))->load();
    auto area = r.reduced (8.0f, 10.0f).withTrimmedRight (14.0f);
    Path p;
    Random rr (7);
    float held = 0.0f; float prevHeld = 0.0f;
    for (int i = 0; i <= 120; ++i)
    {
        const float x = (float) i / 120.0f * 2.0f, ph = x - std::floor (x);
        if (i % 60 == 0) { prevHeld = held; held = rr.nextFloat() * 2.0f - 1.0f; }
        float hv = held;
        if (shape == 6) hv = prevHeld + (held - prevHeld) * (0.5f - 0.5f * std::cos (pi * ph));
        const float v = lfoShape (shape, ph, hv);
        const float px = area.getX() + area.getWidth() * (float) i / 120.0f, py = area.getCentreY() - v * area.getHeight() * 0.48f;
        if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
    }
    g.setColour (Colours_::cyan); g.strokePath (p, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
    const float live = jlimit (-1.0f, 1.0f, proc.engine.uiLfo[lfo].load());
    const float y = area.getCentreY() - live * area.getHeight() * 0.48f;
    g.setColour (Colours_::red);
    g.fillEllipse (r.getRight() - 15, y - 5, 10, 10);
}

// ================================================================== Scope
void Scope::pull()
{
    auto& f = proc.scopeFifo;
    int s1, n1, s2, n2;
    const int ready = f.getNumReady();
    f.prepareToRead (ready, s1, n1, s2, n2);
    auto push = [this] (const float* d, int n) { for (int i = 0; i < n; ++i) { ring[(size_t) head] = d[i]; head = (head + 1) % (int) ring.size(); } };
    if (n1 > 0) push (proc.scopeData.data() + s1, n1);
    if (n2 > 0) push (proc.scopeData.data() + s2, n2);
    f.finishedRead (n1 + n2);

    std::vector<float> re (2048), im (2048, 0.0f);
    for (int i = 0; i < 2048; ++i)
    {
        const float w = 0.5f - 0.5f * std::cos (twoPi * (float) i / 2047.0f);
        re[(size_t) i] = ring[(size_t) ((head - 2048 + i + (int) ring.size()) % (int) ring.size())] * w;
    }
    fft (re, im, false);
    const double srate = proc.getSampleRateNow();
    for (int b = 0; b < 256; ++b)
    {
        const float hz = 30.0f * std::pow (600.0f, (float) b / 255.0f);
        const int bin = jlimit (1, 1023, (int) (hz / srate * 2048.0));
        const float mag = std::sqrt (re[(size_t) bin] * re[(size_t) bin] + im[(size_t) bin] * im[(size_t) bin]) / 512.0f;
        const float db = 20.0f * std::log10 (mag + 1.0e-6f);
        spectrum[(size_t) b] = std::max (db, spectrum[(size_t) b] - 3.0f);
    }
    repaint();
}

void Scope::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto left = r.removeFromLeft (r.getWidth() * 0.5f - 4.0f);
    r.removeFromLeft (8.0f);
    for (auto a : { left, r })
    {
        g.setColour (Colours_::screen); g.fillRoundedRectangle (a, 10.0f);
        g.setColour (Colours_::panelEdge); g.drawRoundedRectangle (a.reduced (0.5f), 10.0f, 1.0f);
    }
    // oscilloscope with a rising-edge trigger
    auto sa = left.reduced (8.0f, 8.0f);
    const int N = (int) ring.size(), window = 600;
    int start = (head - window * 2 + N) % N;
    for (int k = 0; k < window; ++k)
    {
        const int i0 = (start + k) % N, i1 = (start + k + 1) % N;
        if (ring[(size_t) i0] <= 0.0f && ring[(size_t) i1] > 0.0f) { start = i1; break; }
    }
    Path p;
    for (int k = 0; k < window; ++k)
    {
        const float v = jlimit (-1.0f, 1.0f, ring[(size_t) ((start + k) % N)] * 1.5f);
        const float x = sa.getX() + sa.getWidth() * (float) k / (float) (window - 1), y = sa.getCentreY() - v * sa.getHeight() * 0.48f;
        if (k == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    g.setColour (Colours_::red); g.strokePath (p, PathStrokeType (1.6f));
    // spectrum
    auto fa = r.reduced (8.0f, 8.0f);
    Path s;
    for (int b = 0; b < 256; ++b)
    {
        const float x = fa.getX() + fa.getWidth() * (float) b / 255.0f;
        const float y = fa.getBottom() - jlimit (0.0f, 1.0f, (spectrum[(size_t) b] + 84.0f) / 84.0f) * fa.getHeight();
        if (b == 0) s.startNewSubPath (x, y); else s.lineTo (x, y);
    }
    Path fill (s); fill.lineTo (fa.getRight(), fa.getBottom()); fill.lineTo (fa.getX(), fa.getBottom()); fill.closeSubPath();
    g.setColour (Colours_::cyan.withAlpha (0.2f)); g.fillPath (fill);
    g.setColour (Colours_::cyan); g.strokePath (s, PathStrokeType (1.6f));
    g.setFont (LnF::font (10.0f, true)); g.setColour (Colours_::textDim);
    g.drawText ("SCOPE", left.reduced (10, 6).toNearestInt(), Justification::topLeft);
    g.drawText ("SPECTRUM", r.reduced (10, 6).toNearestInt(), Justification::topLeft);
}

// ================================================================== StepGrid
void StepGrid::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const int steps = (int) proc.apvts.getRawParameterValue ("arp_steps")->load();
    const int cur = proc.engine.arpStep.load();
    const bool running = proc.apvts.getRawParameterValue ("arp_on")->load() > 0.5f;
    const float cw = r.getWidth() / (float) numSteps;
    for (int s = 0; s < numSteps; ++s)
    {
        auto col = Rectangle<float> (r.getX() + (float) s * cw, r.getY(), cw, r.getHeight()).reduced (3.0f, 0.0f);
        const bool inRange = s < steps;
        const bool on = proc.apvts.getRawParameterValue (stepId (s, "on"))->load() > 0.5f;
        const float vel = proc.apvts.getRawParameterValue (stepId (s, "vel"))->load();
        const int tr = (int) proc.apvts.getRawParameterValue (stepId (s, "tr"))->load();
        const float a = inRange ? 1.0f : 0.35f;
        auto top = col.removeFromTop (26.0f);
        g.setColour ((on ? Colours_::red : Colours_::track).withMultipliedAlpha (a));
        g.fillRoundedRectangle (top, 7.0f);
        if (running && s == cur) { g.setColour (Colours_::deep); g.drawRoundedRectangle (top.expanded (1.5f), 8.0f, 2.0f); }
        g.setColour (on ? Colours_::onAccent : Colours_::textDim);
        g.setFont (LnF::font (11.0f, true));
        g.drawText (String (s + 1), top, Justification::centred);
        col.removeFromTop (6.0f);
        auto num = col.removeFromBottom (24.0f);
        auto bar = col;
        g.setColour (Colours_::track.withMultipliedAlpha (a)); g.fillRoundedRectangle (bar, 6.0f);
        g.setColour ((on ? Colours_::red.withAlpha (0.75f) : Colours_::textDim.withAlpha (0.4f)).withMultipliedAlpha (a));
        g.fillRoundedRectangle (bar.withTop (bar.getBottom() - bar.getHeight() * vel), 6.0f);
        g.setColour (Colours_::text.withAlpha (a));
        g.setFont (LnF::font (12.0f, true));
        g.drawText ((tr > 0 ? "+" : "") + String (tr), num, Justification::centred);
    }
    g.setFont (LnF::font (10.0f, true));
    g.setColour (Colours_::textDim);
}

void StepGrid::edit (const MouseEvent& e, bool first)
{
    const float cw = (float) getWidth() / (float) numSteps;
    const int s = jlimit (0, numSteps - 1, (int) ((float) e.x / cw));
    const int y = e.y;
    auto setP = [this] (const String& id, float v) { if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v)); };
    if (first)
    {
        dragStep = s;
        dragRow = y < 30 ? 0 : (y > getHeight() - 26 ? 2 : 1);
        dragStartY = y;
        proc.undo.beginNewTransaction ("Edit steps");
        if (dragRow == 0) { setP (stepId (s, "on"), proc.apvts.getRawParameterValue (stepId (s, "on"))->load() > 0.5f ? 0.0f : 1.0f); return; }
        if (dragRow == 2) { dragStartVal = proc.apvts.getRawParameterValue (stepId (s, "tr"))->load(); return; }
    }
    if (dragRow == 1)
    {
        const float top = 32.0f, bottom = (float) getHeight() - 26.0f;
        setP (stepId (s, "vel"), jlimit (0.0f, 1.0f, 1.0f - ((float) y - top) / (bottom - top)));
    }
    else if (dragRow == 2 && ! first)
        setP (stepId (dragStep, "tr"), jlimit (-24.0f, 24.0f, std::round (dragStartVal + (float) (dragStartY - y) / 6.0f)));
    repaint();
}

void StepGrid::mouseDown (const MouseEvent& e) { edit (e, true); }
void StepGrid::mouseDrag (const MouseEvent& e) { edit (e, false); }

// ================================================================== FX rack list
int FxRackList::rowAt (int y) const { return jlimit (0, numFx - 1, y / jmax (1, getHeight() / numFx)); }

void FxRackList::paint (Graphics& g)
{
    const auto order = proc.getFxOrder();
    const int rowH = getHeight() / numFx;
    static const char* onIds[] = { "dist_on", "ch_on", "ph_on", "fl_on", "dly_on", "rev_on", "eq_on", "comp_on", "tape_on" };
    for (int k = 0; k < numFx; ++k)
    {
        const int fxI = order[(size_t) k];
        auto row = Rectangle<float> (0.0f, (float) (k * rowH), (float) getWidth(), (float) rowH).reduced (2.0f, 3.0f);
        const bool on = proc.apvts.getRawParameterValue (onIds[fxI])->load() > 0.5f;
        g.setColour (k == dragFrom ? Colours_::redDim : Colours_::knobBody);
        g.fillPath (LnF::chamfer (row, 8.0f));
        g.setColour (on ? Colours_::red : Colours_::panelEdge);
        g.strokePath (LnF::chamfer (row, 8.0f), PathStrokeType (on ? 1.6f : 1.0f));
        auto dot = Rectangle<float> (14, 14).withCentre ({ row.getX() + 18, row.getCentreY() });
        g.setColour (on ? Colours_::red : Colours_::track);
        g.fillEllipse (dot);
        g.setColour (Colours_::textDim);
        g.setFont (LnF::font (11.0f, true));
        g.drawText (String (k + 1), row.withTrimmedLeft (32).withWidth (16), Justification::centredLeft);
        g.setColour (on ? Colours_::text : Colours_::textDim);
        g.setFont (LnF::title (14.0f));
        g.drawText (Choice::fxNames[fxI], row.withTrimmedLeft (50), Justification::centredLeft);
        g.setColour (Colours_::textDim);
        for (int ln = 0; ln < 3; ++ln) g.fillRect (row.getRight() - 24, row.getCentreY() - 5 + (float) ln * 4, 12.0f, 1.5f);
    }
}

void FxRackList::mouseDown (const MouseEvent& e)
{
    const int k = rowAt (e.y);
    static const char* onIds[] = { "dist_on", "ch_on", "ph_on", "fl_on", "dly_on", "rev_on", "eq_on", "comp_on", "tape_on" };
    if (e.x < 36)
    {
        const int fxI = proc.getFxOrder()[(size_t) k];
        if (auto* p = proc.apvts.getParameter (onIds[fxI])) { proc.undo.beginNewTransaction ("Toggle effect"); p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f); }
        repaint();
        return;
    }
    dragFrom = k;
    repaint();
}

void FxRackList::mouseDrag (const MouseEvent& e)
{
    if (dragFrom < 0) return;
    const int to = rowAt (e.y);
    if (to != dragFrom)
    {
        auto order = proc.getFxOrder();
        const int moving = order[(size_t) dragFrom];
        std::vector<int> v (order.begin(), order.end());
        v.erase (v.begin() + dragFrom);
        v.insert (v.begin() + to, moving);
        for (int i = 0; i < numFx; ++i) order[(size_t) i] = v[(size_t) i];
        proc.setFxOrder (order);
        dragFrom = to;
    }
    repaint();
}
