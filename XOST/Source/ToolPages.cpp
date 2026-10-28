#include "ToolPages.h"
#include "Presets.h"

using namespace juce;
using namespace xo;
using ui::LnF;
using S = Knob::Style;
using TScope = XOSTProcessor::Scope;

namespace
{
    String laneName (int l) { static const char* n[] = { "VOLUME", "FILTER", "PAN", "TIME" }; return n[l]; }
    String toolFolder (TScope s) { return s == TScope::matrix ? "Matrix" : "Minirack"; }
    String toolExt (TScope s) { return s == TScope::matrix ? "*.xostmatrix" : "*.xostrack"; }

    void drawScrew (Graphics& g, Point<float> c)
    {
        g.setColour (Colour (0xff2e3746)); g.fillEllipse (c.x - 5, c.y - 5, 10, 10);
        g.setColour (Colour (0xff090b10)); g.drawLine (c.x - 3, c.y - 3, c.x + 3, c.y + 3, 1.6f);
    }
    void drawJack (Graphics& g, Point<float> c, bool lit)
    {
        g.setColour (Colour (0xff97b3e1)); g.fillEllipse (c.x - 9, c.y - 9, 18, 18);
        g.setColour (Colour (0xff040407)); g.fillEllipse (c.x - 5.5f, c.y - 5.5f, 11, 11);
        if (lit) { g.setColour (Colours_::red.withAlpha (0.9f)); g.drawEllipse (c.x - 11.5f, c.y - 11.5f, 23, 23, 1.0f); }
    }
}

// ================================================================== tool preset bar (Matrix / Minirack)
ToolPresetBar::ToolPresetBar (XOSTProcessor& p, TScope s) : proc (p), scope (s)
{
    for (auto* b : { &nameButton, &prev, &next, &save }) addAndMakeVisible (*b);
    nameButton.onClick = [this] { showMenu(); };
    prev.onClick = [this] { step (-1); };
    next.onClick = [this] { step (1); };
    save.onClick = [this]
    {
        auto dir = XOSTProcessor::getUserFolder (toolFolder (scope));
        chooser = std::make_unique<FileChooser> ("Save " + toolFolder (scope) + " preset", dir.getChildFile ("My Preset"), toolExt (scope));
        chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting,
                              [this] (const FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f != File()) proc.saveToolPreset (scope, f, "User");
            repaint();
        });
    };
    nameButton.setTooltip ("Factory and saved " + toolFolder (scope) + " presets");
}

Array<File> ToolPresetBar::userFiles() const
{
    auto files = XOSTProcessor::getUserFolder (toolFolder (scope)).findChildFiles (File::findFiles, true, toolExt (scope));
    files.sort();
    return files;
}

void ToolPresetBar::step (int d)
{
    const auto& bank = scope == TScope::matrix ? Presets::matrixBank() : Presets::rackBank();
    if (bank.empty()) return;
    index = (index + d + (int) bank.size()) % (int) bank.size();
    proc.loadToolFactory (scope, index);
    repaint();
}

void ToolPresetBar::showMenu()
{
    const auto& bank = scope == TScope::matrix ? Presets::matrixBank() : Presets::rackBank();
    PopupMenu m;
    StringArray cats;
    for (const auto& e : bank) cats.addIfNotAlreadyThere (e.category);
    for (const auto& c : cats)
    {
        PopupMenu sub;
        for (int i = 0; i < (int) bank.size(); ++i)
            if (bank[(size_t) i].category == c) sub.addItem (1 + i, bank[(size_t) i].name);
        m.addSubMenu (c, sub);
    }
    const auto files = userFiles();
    PopupMenu user;
    for (int i = 0; i < files.size(); ++i) user.addItem (10000 + i, files[i].getFileNameWithoutExtension());
    m.addSeparator();
    m.addSubMenu ("Your presets", user, files.size() > 0);
    m.addItem (20000, "Open preset folder");
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&nameButton), [this, files] (int r)
    {
        if (r >= 1 && r < 10000) { index = r - 1; proc.loadToolFactory (scope, index); }
        else if (r >= 10000 && r < 20000) proc.loadToolPreset (scope, files[r - 10000]);
        else if (r == 20000) XOSTProcessor::getUserFolder (toolFolder (scope)).startAsProcess();
        repaint();
    });
}

void ToolPresetBar::resized()
{
    auto r = getLocalBounds();
    save.setBounds (r.removeFromRight (56)); r.removeFromRight (4);
    next.setBounds (r.removeFromRight (26)); r.removeFromRight (2);
    prev.setBounds (r.removeFromLeft (26)); r.removeFromLeft (2);
    nameButton.setBounds (r);
}

void ToolPresetBar::paint (Graphics&)
{
    nameButton.setButtonText (scope == TScope::matrix ? proc.currentMatrixName : proc.currentRackName);
}

// ================================================================== lane editor
int LaneEditor::cells() const
{
    const int len = (int) proc.apvts.getRawParameterValue (String ("mx_") + mxLaneIds[lane] + "_len")->load();
    const double beats = Choice::mxLenBeats[jlimit (0, 4, len)];
    return jmax (1, (int) std::round ((double) grid * beats / 4.0));
}

Point<float> LaneEditor::toPt (const Breakpoint& b) const
{
    const auto a = area();
    return { a.getX() + b.x * a.getWidth(), a.getBottom() - b.y * a.getHeight() };
}

Breakpoint LaneEditor::fromPos (Point<float> p) const
{
    const auto a = area();
    return { jlimit (0.0f, 0.9999f, (p.x - a.getX()) / a.getWidth()), jlimit (0.0f, 1.0f, (a.getBottom() - p.y) / a.getHeight()), 0.0f };
}

int LaneEditor::hit (Point<float> p) const
{
    for (int i = 0; i < (int) work.size(); ++i)
        if (toPt (work[(size_t) i]).getDistanceFrom (p) < 8.0f) return i;
    return -1;
}

void LaneEditor::paint (Graphics& g)
{
    const auto a = area();
    g.setColour (Colours_::screen);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 5.0f);
    // grid
    const int n = cells();
    for (int i = 0; i <= n; ++i)
    {
        const float x = a.getX() + a.getWidth() * (float) i / (float) n;
        const bool beat = (grid % 4 == 0) && i % jmax (1, grid / 4) == 0;
        g.setColour (beat ? Colour (0xff242b38) : Colour (0xff13161e));
        g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
    }
    for (int i = 0; i <= 4; ++i)
    {
        g.setColour (i == 2 && lane == 2 ? Colour (0xff2e3746) : Colour (0xff13161e));
        g.drawHorizontalLine ((int) (a.getY() + a.getHeight() * (float) i / 4.0f), a.getX(), a.getRight());
    }
    // curve
    const auto pts = work.empty() ? proc.getLane (lane) : work;
    float table[mxTableSize];
    renderLane (pts, table);
    Path line, fill;
    for (int i = 0; i <= mxTableSize; ++i)
    {
        const float x = a.getX() + a.getWidth() * (float) i / (float) mxTableSize;
        const float y = a.getBottom() - table[i % mxTableSize] * a.getHeight();
        if (i == 0) { line.startNewSubPath (x, y); fill.startNewSubPath (x, a.getBottom()); }
        line.lineTo (x, y); fill.lineTo (x, y);
    }
    fill.lineTo (a.getRight(), a.getBottom()); fill.closeSubPath();
    g.setGradientFill (ColourGradient (Colours_::red.withAlpha (0.22f), 0, a.getY(), Colours_::red.withAlpha (0.02f), 0, a.getBottom(), false));
    g.fillPath (fill);
    g.setColour (Colours_::red);
    g.strokePath (line, PathStrokeType (1.8f));
    // points
    const auto& show = work.empty() ? pts : work;
    for (int i = 0; i < (int) show.size(); ++i)
    {
        const auto p = toPt (show[(size_t) i]);
        const float s = i == hover || i == dragIndex ? 9.0f : 6.0f;
        g.setColour (Colours_::bg); g.fillRect (p.x - s * 0.5f - 1, p.y - s * 0.5f - 1, s + 2, s + 2);
        g.setColour (Colours_::red); g.fillRect (p.x - s * 0.5f, p.y - s * 0.5f, s, s);
    }
    // playhead
    if (proc.apvts.getRawParameterValue ("mx_on")->load() > 0.5f)
    {
        const float ph = proc.engine.matrix.uiLanePhase[lane].load();
        const float x = a.getX() + a.getWidth() * ph;
        g.setColour (Colours_::red.withAlpha (0.6f));
        g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
        g.fillEllipse (x - 3.5f, a.getBottom() - table[(int) (ph * mxTableSize) & (mxTableSize - 1)] * a.getHeight() - 3.5f, 7, 7);
    }
    // axis labels
    g.setFont (LnF::font (10.5f, true));
    g.setColour (Colours_::textDim);
    static const char* top[] = { "100%", "OPEN", "R", "MAX" };
    static const char* bot[] = { "0%", "CLOSED", "L", "0" };
    g.drawText (top[lane], getLocalBounds().removeFromTop (14).withTrimmedLeft (12), Justification::centredLeft);
    g.drawText (bot[lane], getLocalBounds().removeFromBottom (13).withTrimmedLeft (12), Justification::centredLeft);
    g.drawText (drawMode ? "DRAW: drag to paint steps" : "Click: add point   Drag: move   Right-click: delete   Ctrl+drag: bend   Shift: no snap",
                getLocalBounds().removeFromBottom (13).withTrimmedRight (12), Justification::centredRight);
}

void LaneEditor::mouseMove (const MouseEvent& e)
{
    work = proc.getLane (lane);
    const int h = hit (e.position);
    work.clear();
    if (h != hover) { hover = h; repaint(); }
}

void LaneEditor::paintCell (Point<float> p)
{
    const int n = cells();
    const auto b = fromPos (p);
    const int c = jlimit (0, n - 1, (int) (b.x * (float) n));
    const float x0 = (float) c / (float) n, x1 = (float) (c + 1) / (float) n - 0.002f;
    work.erase (std::remove_if (work.begin(), work.end(), [&] (const Breakpoint& q) { return q.x >= x0 - 1.0e-4f && q.x < x1 + 0.0019f; }), work.end());
    work.push_back ({ x0, b.y, 0.0f });
    work.push_back ({ x1, b.y, 0.0f });
    std::sort (work.begin(), work.end(), [] (const Breakpoint& a, const Breakpoint& q) { return a.x < q.x; });
    proc.setLane (lane, work, false);
}

void LaneEditor::mouseDown (const MouseEvent& e)
{
    before = proc.getLane (lane);
    work = before;
    dragIndex = -1; curveSeg = -1;
    if (drawMode && ! e.mods.isPopupMenu()) { paintCell (e.position); repaint(); return; }
    const int idx = hit (e.position);
    if (idx >= 0 && (e.mods.isPopupMenu() || e.mods.isAltDown()))
    {
        if (work.size() > 1) { work.erase (work.begin() + idx); proc.setLane (lane, work, false); }
        repaint();
        return;
    }
    if (idx >= 0) { dragIndex = idx; return; }
    if (e.mods.isPopupMenu()) return;
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        const auto b = fromPos (e.position);
        curveSeg = 0;
        for (int k = 0; k < (int) work.size(); ++k) if (work[(size_t) k].x <= b.x) curveSeg = k;
        curveStartY = e.position.y;
        curveStartVal = work[(size_t) curveSeg].curve;
        return;
    }
    auto b = fromPos (e.position);
    if (! e.mods.isShiftDown()) { const int n = cells(); b.x = jlimit (0.0f, 0.9999f, std::round (b.x * (float) n) / (float) n); }
    work.push_back (b);
    std::sort (work.begin(), work.end(), [] (const Breakpoint& a, const Breakpoint& q) { return a.x < q.x; });
    for (int k = 0; k < (int) work.size(); ++k) if (std::abs (work[(size_t) k].x - b.x) < 1.0e-6f && std::abs (work[(size_t) k].y - b.y) < 1.0e-6f) dragIndex = k;
    proc.setLane (lane, work, false);
    repaint();
}

void LaneEditor::mouseDrag (const MouseEvent& e)
{
    if (drawMode && curveSeg < 0 && dragIndex < 0) { paintCell (e.position); repaint(); return; }
    if (curveSeg >= 0)
    {
        work[(size_t) curveSeg].curve = jlimit (-1.0f, 1.0f, curveStartVal - (e.position.y - curveStartY) / 120.0f);
        proc.setLane (lane, work, false);
    }
    else if (dragIndex >= 0)
    {
        auto b = fromPos (e.position);
        if (! e.mods.isShiftDown()) { const int n = cells(); b.x = std::round (b.x * (float) n) / (float) n; }
        const float lo = dragIndex > 0 ? work[(size_t) dragIndex - 1].x + 0.001f : 0.0f;
        const float hi = dragIndex + 1 < (int) work.size() ? work[(size_t) dragIndex + 1].x - 0.001f : 0.9999f;
        work[(size_t) dragIndex].x = jlimit (lo, jmax (lo, hi), b.x);
        work[(size_t) dragIndex].y = b.y;
        proc.setLane (lane, work, false);
    }
    repaint();
}

void LaneEditor::mouseUp (const MouseEvent&)
{
    if (! work.empty())
    {
        bool changed = work.size() != before.size();
        for (size_t i = 0; ! changed && i < work.size(); ++i)
            changed = std::abs (work[i].x - before[i].x) > 1.0e-6f || std::abs (work[i].y - before[i].y) > 1.0e-6f || std::abs (work[i].curve - before[i].curve) > 1.0e-6f;
        if (changed) proc.commitLane (lane, before);
    }
    work.clear(); dragIndex = -1; curveSeg = -1;
    repaint();
}

void LaneEditor::mouseDoubleClick (const MouseEvent& e)
{
    work = proc.getLane (lane);
    const int idx = hit (e.position);
    if (idx >= 0) { before = work; work[(size_t) idx].curve = 0.0f; proc.setLane (lane, work, false); proc.commitLane (lane, before); }
    work.clear();
    repaint();
}

// ================================================================== MATRIX page
MatrixPage::MatrixPage (XOSTProcessor& p) : Page (p)
{
    for (int l = 0; l < numMxLanes; ++l)
    {
        auto* t = laneTabs.add (new TextButton (laneName (l)));
        t->setClickingTogglesState (true);
        t->setRadioGroupId (41);
        t->onClick = [this, l] { selectLane (l); };
        addAndMakeVisible (t);
    }
    laneTabs[0]->setToggleState (true, dontSendNotification);
    addAndMakeVisible (editor);
    gridBox.addItemList ({ "Grid 1/4", "Grid 1/8", "Grid 1/16", "Grid 1/32", "Grid 1/8T", "Grid 1/16T" }, 1);
    gridBox.setSelectedId (3, dontSendNotification);
    gridBox.onChange = [this] { const int g[] = { 4, 8, 16, 32, 12, 24 }; editor.setGrid (g[gridBox.getSelectedId() - 1]); };
    addAndMakeVisible (gridBox);
    drawButton.setClickingTogglesState (true);
    drawButton.onClick = [this] { editor.setDrawMode (drawButton.getToggleState()); editor.repaint(); };
    for (auto* b : { &drawButton, &shapesButton, &invertButton, &shiftButton, &clearButton, &mirrorButton }) addAndMakeVisible (*b);
    shapesButton.onClick = [this]
    {
        PopupMenu m;
        const char* names[] = { "Gate 1/8", "Gate 1/16", "Gate 3-3-2", "Gate 1/8 triplet", "Sidechain pump 1/4", "Sidechain pump 1/8",
                                "Ramp up", "Ramp down", "Sine", "Triangle", "Square 1/4", "Random steps", "Stutter (time)",
                                "Tape stop (time)", "Half-time (time)", "Full (flat top)", "Empty (flat bottom)" };
        for (int i = 0; i < 17; ++i) m.addItem (i + 1, names[i]);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&shapesButton), [this] (int r) { if (r > 0) applyShape (r - 1); });
    };
    invertButton.onClick = [this] { auto pts = proc.getLane (editor.getLane()); proc.undo.beginNewTransaction ("Invert lane"); for (auto& q : pts) q.y = 1.0f - q.y; proc.setLane (editor.getLane(), pts); editor.repaint(); };
    mirrorButton.onClick = [this]
    {
        auto pts = proc.getLane (editor.getLane());
        proc.undo.beginNewTransaction ("Reverse lane");
        for (auto& q : pts) q.x = jlimit (0.0f, 0.9999f, 0.9999f - q.x);
        proc.setLane (editor.getLane(), pts); editor.repaint();
    };
    shiftButton.onClick = [this]
    {
        auto pts = proc.getLane (editor.getLane());
        const float d = 1.0f / (float) editor.cells();
        proc.undo.beginNewTransaction ("Shift lane");
        for (auto& q : pts) { q.x += d; if (q.x >= 1.0f) q.x -= 1.0f; }
        proc.setLane (editor.getLane(), pts); editor.repaint();
    };
    clearButton.onClick = [this] { applyShape (editor.getLane() == 3 || editor.getLane() == 1 ? 16 : 15); };
    addAndMakeVisible (presets);

    power ("mx_on", "MATRIX"); knob ("mx_mix", "Mix", S::normal); knob ("mx_smooth", "Smooth", S::normal);
    for (int l = 0; l < numMxLanes; ++l)
    {
        const String id = String ("mx_") + mxLaneIds[l] + "_";
        power (id + "on", laneName (l)); knob (id + "depth", "Depth", S::compact); choice (id + "len");
    }
    choice ("mx_flt_type", "Filter"); knob ("mx_flt_reso", "Reso", S::normal); choice ("mx_time_range", "Time Range");
}

void MatrixPage::selectLane (int l) { editor.setLane (l); for (int i = 0; i < numMxLanes; ++i) laneTabs[i]->setToggleState (i == l, dontSendNotification); }

void MatrixPage::applyShape (int shape)
{
    std::vector<Breakpoint> p;
    auto steps = [&p] (std::initializer_list<float> v)
    {
        const int n = (int) v.size(); int k = 0;
        for (float y : v) { p.push_back ({ (float) k / (float) n, y, 0.0f }); p.push_back ({ (float) (k + 1) / (float) n - 0.003f, y, 0.0f }); ++k; }
    };
    auto gate = [&p] (int n, float duty)
    {
        for (int k = 0; k < n; ++k)
        {
            const float x = (float) k / (float) n;
            p.push_back ({ x, 1.0f, 0.0f }); p.push_back ({ x + duty / (float) n, 1.0f, 0.0f }); p.push_back ({ x + duty / (float) n + 0.004f, 0.0f, 0.0f });
        }
    };
    Random r;
    switch (shape)
    {
        case 0: gate (8, 0.5f); break;
        case 1: gate (16, 0.55f); break;
        case 2: steps ({ 1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 0 }); break;
        case 3: gate (12, 0.5f); break;
        case 4: for (int k = 0; k < 4; ++k) { const float x = (float) k / 4.0f; p.push_back ({ x, 0.0f, 0.0f }); p.push_back ({ x + 0.02f, 0.05f, -0.6f }); p.push_back ({ x + 0.2f, 1.0f, 0.0f }); } break;
        case 5: for (int k = 0; k < 8; ++k) { const float x = (float) k / 8.0f; p.push_back ({ x, 0.0f, 0.0f }); p.push_back ({ x + 0.01f, 0.1f, -0.6f }); p.push_back ({ x + 0.1f, 1.0f, 0.0f }); } break;
        case 6: p = { { 0.0f, 0.0f, 0.0f }, { 0.995f, 1.0f, 0.0f } }; break;
        case 7: p = { { 0.0f, 1.0f, 0.0f }, { 0.995f, 0.0f, 0.0f } }; break;
        case 8: for (int k = 0; k < 16; ++k) p.push_back ({ (float) k / 16.0f, 0.5f + 0.5f * std::sin ((float) k / 16.0f * MathConstants<float>::twoPi), 0.0f }); break;
        case 9: p = { { 0.0f, 0.0f, 0.0f }, { 0.5f, 1.0f, 0.0f } }; break;
        case 10: gate (4, 0.5f); break;
        case 11: { std::vector<float> v; for (int k = 0; k < 16; ++k) v.push_back (r.nextFloat()); for (int k = 0; k < 16; ++k) { p.push_back ({ (float) k / 16.0f, v[(size_t) k], 0.0f }); p.push_back ({ (float) (k + 1) / 16.0f - 0.003f, v[(size_t) k], 0.0f }); } break; }
        case 12: steps ({ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.25f, 0.25f, 0.5f, 0.5f }); break;
        case 13: p = { { 0.0f, 0.0f, 0.0f }, { 0.5f, 0.0f, 0.6f }, { 0.995f, 1.0f, 0.0f } }; break;
        case 14: p = { { 0.0f, 0.0f, 0.0f }, { 0.995f, 0.5f, 0.0f } }; break;
        case 15: p = { { 0.0f, 1.0f, 0.0f } }; break;
        default: p = { { 0.0f, 0.0f, 0.0f } }; break;
    }
    for (auto& q : p) q.x = jlimit (0.0f, 0.9999f, q.x);
    proc.undo.beginNewTransaction ("Matrix shape");
    proc.setLane (editor.getLane(), p);
    editor.repaint();
}

void MatrixPage::resized()
{
    panels.clear(); labels.clear();
    auto c = panel ("MATRIX  -  GATE / FILTER / PAN / TIME", { 0, 0, 900, 512 });
    auto tabs = c.removeFromTop (30);
    for (int l = 0; l < numMxLanes; ++l) laneTabs[l]->setBounds (tabs.removeFromLeft (96).withTrimmedRight (6));
    c.removeFromTop (6);
    auto tools = c.removeFromTop (28);
    gridBox.setBounds (tools.removeFromLeft (110)); tools.removeFromLeft (6);
    for (auto* b : { &drawButton, &shapesButton, &invertButton, &mirrorButton, &shiftButton, &clearButton }) { b->setBounds (tools.removeFromLeft (84)); tools.removeFromLeft (6); }
    c.removeFromTop (8);
    editor.setBounds (c);

    auto s = panel ("SETTINGS", { 910, 0, 346, 512 });
    presets.setBounds (s.removeFromTop (28));
    s.removeFromTop (10);
    auto top = s.removeFromTop (70);
    powers[0]->setBounds (top.removeFromLeft (110).withSizeKeepingCentre (110, 26));
    knobs[0]->setBounds (top.removeFromLeft (100).withSizeKeepingCentre (64, 70));
    knobs[1]->setBounds (top.removeFromLeft (100).withSizeKeepingCentre (64, 70));
    s.removeFromTop (8);
    { auto hdr = s.removeFromTop (14); label ("LANE", hdr.withWidth (100)); label ("DEPTH", hdr.withTrimmedLeft (110).withWidth (100)); label ("LENGTH", hdr.withTrimmedLeft (220)); }
    for (int l = 0; l < numMxLanes; ++l)
    {
        auto r = s.removeFromTop (44).reduced (0, 5);
        powers[(size_t) l + 1]->setBounds (r.removeFromLeft (110));
        knobs[(size_t) l + 2]->setBounds (r.removeFromLeft (110).withSizeKeepingCentre (110, 30));
        choices[(size_t) l]->setBounds (r.withSizeKeepingCentre (r.getWidth(), 26));
    }
    s.removeFromTop (8);
    auto f = s.removeFromTop (74);
    choices[4]->setBounds (f.removeFromLeft (150).withSizeKeepingCentre (150, 40));
    knobs[6]->setBounds (f.removeFromLeft (90).withSizeKeepingCentre (64, 70));
    s.removeFromTop (4);
    choices[5]->setBounds (s.removeFromTop (40).withWidth (150));
    label ("Matrix runs on the whole synth after the effects. TIME re-reads the last bar like a turntable (Gross Beat style).", s.withTrimmedTop (6).withHeight (40));
}

void MatrixPage::paint (Graphics& g) { Page::paint (g); }

void MatrixPage::tick()
{
    editor.repaint();
    if (proc.getLaneCounter() != lastCounter) { lastCounter = proc.getLaneCounter(); presets.repaint(); }
}

// ================================================================== MINIRACK
RackModule::RackModule (XOSTProcessor& p, int i) : index (i), proc (p)
{
    type = std::make_unique<ChoiceBox> (proc, rackId (i, "type"), "");
    div = std::make_unique<ChoiceBox> (proc, rackId (i, "div"), "");
    dst1 = std::make_unique<ChoiceBox> (proc, rackId (i, "dst1"), "");
    dst2 = std::make_unique<ChoiceBox> (proc, rackId (i, "dst2"), "");
    rate = std::make_unique<Knob> (proc, rackId (i, "rate"), "Rate", S::mini);
    a = std::make_unique<Knob> (proc, rackId (i, "a"), "Shape", S::mini);
    b = std::make_unique<Knob> (proc, rackId (i, "b"), "Var", S::mini);
    amt1 = std::make_unique<Knob> (proc, rackId (i, "amt1"), "Amt", S::mini);
    amt2 = std::make_unique<Knob> (proc, rackId (i, "amt2"), "Amt", S::mini);
    sync = std::make_unique<PowerButton> (proc, rackId (i, "sync"), "SYNC");
    for (Component* c : std::initializer_list<Component*> { type.get(), div.get(), dst1.get(), dst2.get(), rate.get(), a.get(), b.get(), amt1.get(), amt2.get(), sync.get() })
        addAndMakeVisible (c);
    updateLabels();
}

void RackModule::updateLabels()
{
    const int t = (int) proc.apvts.getRawParameterValue (rackId (index, "type"))->load();
    static const char* A[] = { "Shape", "Shape", "Chance", "Step", "Length", "Output", "Pattern", "Bounces", "Speed" };
    static const char* B[] = { "Var", "Skew", "Glide", "Pull", "Mutate", "Rho", "Glide", "Damping", "Sens" };
    a->setLabel (A[jlimit (0, 8, t)]); b->setLabel (B[jlimit (0, 8, t)]);
    lastType = t;
}

Point<float> RackModule::jackPos (int out) const { return { 22.0f, out == 0 ? 266.0f : 332.0f }; }

void RackModule::resized()
{
    const int w = getWidth();
    type->setBounds (12, 36, w - 24, 24);
    rate->setBounds (8, 128, 58, 58); a->setBounds (w / 2 - 29, 128, 58, 58); b->setBounds (w - 66, 128, 58, 58);
    sync->setBounds (10, 196, 80, 24); div->setBounds (94, 196, w - 106, 24);
    dst1->setBounds (40, 254, w - 104, 24); amt1->setBounds (w - 60, 238, 52, 54);
    dst2->setBounds (40, 320, w - 104, 24); amt2->setBounds (w - 60, 304, 52, 54);
}

void RackModule::tick()
{
    history[(size_t) head] = proc.engine.rack.uiOut[index].load();
    head = (head + 1) % (int) history.size();
    if ((int) proc.apvts.getRawParameterValue (rackId (index, "type"))->load() != lastType) updateLabels();
    repaint (12, 66, getWidth() - 24, 56);
    repaint (0, 240, 50, 120);
}

void RackModule::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    // faceplate
    g.setGradientFill (ColourGradient (Colour (0xff171b24), 0, 0, Colour (0xff0d1016), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (Colour (0xff2a3240)); g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
    for (auto c : { Point<float> (12, 10), Point<float> (r.getRight() - 12, 10), Point<float> (12, r.getBottom() - 10), Point<float> (r.getRight() - 12, r.getBottom() - 10) })
        drawScrew (g, c);
    const int t = (int) proc.apvts.getRawParameterValue (rackId (index, "type"))->load();
    g.setFont (LnF::title (13.0f));
    g.setColour (Colours_::text);
    g.drawText (t == 0 ? String ("EMPTY SLOT") : Choice::rackType[t].toUpperCase(), 24, 8, getWidth() - 48, 22, Justification::centred);
    g.setFont (LnF::font (9.5f, true)); g.setColour (Colours_::textDim);
    g.drawText ("XO-" + String (index + 1), 24, getHeight() - 18, getWidth() - 48, 12, Justification::centred);

    // scope of the module's output
    auto sc = Rectangle<float> (12, 66, (float) getWidth() - 24, 56);
    g.setColour (Colours_::screen); g.fillRoundedRectangle (sc, 3.0f);
    g.setColour (Colour (0xff1b202a)); g.drawHorizontalLine ((int) sc.getCentreY(), sc.getX(), sc.getRight());
    if (t != 0)
    {
        Path p;
        for (int i = 0; i < (int) history.size(); ++i)
        {
            const float v = history[(size_t) ((head + i) % (int) history.size())];
            const float x = sc.getX() + sc.getWidth() * (float) i / (float) (history.size() - 1);
            const float y = sc.getCentreY() - v * (sc.getHeight() * 0.45f);
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (Colours_::red); g.strokePath (p, PathStrokeType (1.4f));
    }
    // LED
    const float out = proc.engine.rack.uiOut[index].load();
    g.setColour (Colours_::red.withAlpha (0.15f + 0.85f * std::abs (out)));
    g.fillEllipse ((float) getWidth() - 22, 70, 7, 7);

    // outputs
    g.setFont (LnF::font (10.0f, true)); g.setColour (Colours_::textDim);
    g.drawText ("OUT 1  >  DESTINATION", 10, 238, 150, 12, Justification::centredLeft);
    g.drawText ("OUT 2  >  DESTINATION", 10, 304, 150, 12, Justification::centredLeft);
    const bool p1 = (int) proc.apvts.getRawParameterValue (rackId (index, "dst1"))->load() > 0;
    const bool p2 = (int) proc.apvts.getRawParameterValue (rackId (index, "dst2"))->load() > 0;
    drawJack (g, jackPos (0), p1 && t != 0);
    drawJack (g, jackPos (1), p2 && t != 0);
}

RackPage::RackPage (XOSTProcessor& p) : Page (p)
{
    power ("rk_on", "MINIRACK"); knob ("rk_depth", "Depth", S::compact);
    addAndMakeVisible (presets);
    for (int m = 0; m < numRack; ++m) addAndMakeVisible (modules.add (new RackModule (proc, m)));
}

void RackPage::resized()
{
    panels.clear(); labels.clear();
    auto top = Rectangle<int> (0, 0, 1256, 34);
    powers[0]->setBounds (top.removeFromLeft (130).reduced (0, 4));
    knobs[0]->setBounds (top.removeFromLeft (150).reduced (0, 2));
    presets.setBounds (top.removeFromRight (420).reduced (0, 3));
    const int w = 202;
    for (int m = 0; m < numRack; ++m) modules[m]->setBounds (m * (w + 8) + 2, 42, w, 388);
    bay = { 0, 438, 1256, 74 };
}

void RackPage::paint (Graphics& g)
{
    // rails
    g.setColour (Colour (0xff141820));
    g.fillRect (0, 38, 1256, 3); g.fillRect (0, 431, 1256, 3);
    // patch bay
    auto b = bay.toFloat();
    g.setColour (Colours_::panel); g.fillRoundedRectangle (b, 4.0f);
    g.setColour (Colours_::panelEdge); g.drawRoundedRectangle (b.reduced (0.5f), 4.0f, 1.0f);
    g.setFont (LnF::title (11.0f)); g.setColour (Colours_::textDim);
    g.drawText ("PATCH BAY", bay.withTrimmedLeft (10).removeFromTop (16), Justification::centredLeft);
    for (int m = 0; m < numRack; ++m)
        for (int o = 0; o < 2; ++o)
        {
            const int d = (int) proc.apvts.getRawParameterValue (rackId (m, o == 0 ? "dst1" : "dst2"))->load();
            const int t = (int) proc.apvts.getRawParameterValue (rackId (m, "type"))->load();
            const auto c = Point<float> ((float) modules[m]->getX() + 60.0f + (float) o * 90.0f, b.getY() + 34.0f);
            drawJack (g, c, d > 0 && t > 0);
            g.setFont (LnF::font (10.0f, true));
            g.setColour (d > 0 && t > 0 ? Colours_::text : Colours_::textDim.withAlpha (0.5f));
            g.drawText (d > 0 ? dstNames[d].toUpperCase() : String ("-"), (int) c.x - 44, (int) c.y + 12, 88, 14, Justification::centred);
        }
}

void RackPage::paintOverChildren (Graphics& g)
{
    // patch cables: module output jack -> patch bay
    for (int m = 0; m < numRack; ++m)
        for (int o = 0; o < 2; ++o)
        {
            const int d = (int) proc.apvts.getRawParameterValue (rackId (m, o == 0 ? "dst1" : "dst2"))->load();
            const int t = (int) proc.apvts.getRawParameterValue (rackId (m, "type"))->load();
            if (d <= 0 || t <= 0) continue;
            const auto from = modules[m]->getPosition().toFloat() + modules[m]->jackPos (o);
            const auto to = Point<float> ((float) modules[m]->getX() + 60.0f + (float) o * 90.0f, (float) bay.getY() + 34.0f);
            Path cable;
            cable.startNewSubPath (from);
            const float sag = 40.0f + 15.0f * (float) o;
            cable.cubicTo (from.x - 30.0f, from.y + sag, to.x, to.y + sag, to.x, to.y);
            g.setColour (Colours::black.withAlpha (0.6f));
            g.strokePath (cable, PathStrokeType (6.0f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (o == 0 ? Colours_::red : Colours_::cyan);
            g.strokePath (cable, PathStrokeType (3.5f, PathStrokeType::curved, PathStrokeType::rounded));
            for (auto pt : { from, to }) { g.fillEllipse (pt.x - 6, pt.y - 6, 12, 12); g.setColour (Colours::black); g.fillEllipse (pt.x - 2, pt.y - 2, 4, 4); g.setColour (o == 0 ? Colours_::red : Colours_::cyan); }
        }
}

void RackPage::tick() { for (auto* m : modules) m->tick(); repaint (bay); presets.repaint(); }

// ================================================================== Gene field
namespace
{
    constexpr int numBranches = 10;
    float smoothStep (float t) { return t * t * (3.0f - 2.0f * t); }
}

GeneField::GeneField (XOSTProcessor& p) : proc (p)
{
    collectGenes();
    seed = (uint32_t) Time::currentTimeMillis();
    newSeed();
    startTimerHz (30);
    setMouseCursor (MouseCursor::CrosshairCursor);
}

GeneField::~GeneField() { stopTimer(); }

void GeneField::collectGenes()
{
    for (auto* ap : proc.getParameters())
    {
        auto* rp = dynamic_cast<RangedAudioParameter*> (ap);
        if (rp == nullptr) continue;
        const auto id = rp->getParameterID();
        const int sec = XOSTProcessor::sectionFor (id);
        if (sec < 0 || id.endsWith ("_src") || id.contains ("_dst") || id.endsWith ("_on") || id == "polyphony" || id == "voice_mode") continue;
        const bool isChoice = dynamic_cast<AudioParameterChoice*> (rp) != nullptr;
        // choices that make musical sense to jump between
        const bool discrete = isChoice && (id.endsWith ("_table") || id.endsWith ("_type") || id.endsWith ("_shape") || id.endsWith ("_warp") || id.endsWith ("_mode"));
        if (isChoice && ! discrete) continue;
        RangedAudioParameter* gate = nullptr;
        if (id.startsWith ("mm") && id.endsWith ("_amt")) gate = proc.apvts.getParameter (id.replace ("_amt", "_src"));
        genes.push_back ({ rp, discrete, discrete ? rp->getNumSteps() : 0, sec, gate });
    }
    base.resize (genes.size());
    lastApplied.resize (genes.size());
}

void GeneField::newSeed()
{
    Random r ((int64) seed);
    seed = seed * 1664525u + 1013904223u;
    branches.clear();
    for (int b = 0; b < numBranches; ++b)
    {
        Branch br;
        br.curl = r.nextFloat() * 1.2f - 0.6f;
        for (size_t g = 0; g < genes.size(); ++g)
        {
            const bool strong = r.nextFloat() < 0.4f;
            br.delta.push_back ((r.nextFloat() * 2.0f - 1.0f) * (strong ? 0.3f + 0.6f * r.nextFloat() : 0.07f));
            br.jump.push_back (r.nextFloat() < 0.35f ? r.nextFloat() : -1.0f);
        }
        branches.push_back (std::move (br));
    }
    buildBranchPaths();
    if (hasBase && (std::abs (pos.x) > 0.001f || std::abs (pos.y) > 0.001f)) applyAt (pos);
    repaint();
}

void GeneField::captureBase()
{
    for (size_t g = 0; g < genes.size(); ++g) base[g] = lastApplied[g] = genes[g].param->getValue();
    hasBase = true;
}

bool GeneField::userChangedSomething() const
{
    for (size_t g = 0; g < genes.size(); ++g)
        if (std::abs (genes[g].param->getValue() - lastApplied[g]) > 1.0e-3f) return true;
    return false;
}

void GeneField::plant() { captureBase(); pos = {}; for (auto& t : trail) t = {}; repaint(); }
void GeneField::recentre() { if (! hasBase) return; proc.undo.beginNewTransaction ("Gene field"); applyAt ({}); pos = {}; repaint(); }

void GeneField::applyAt (Point<float> p)
{
    const uint32_t locks = proc.randomLocks.load();
    const float r = jlimit (0.0f, 1.0f, p.getDistanceFromOrigin());
    float theta = std::atan2 (p.y, p.x);
    if (theta < 0) theta += MathConstants<float>::twoPi;
    const float slot = theta / MathConstants<float>::twoPi * (float) numBranches;
    const int b0 = (int) slot % numBranches, b1 = (b0 + 1) % numBranches;
    const float w = smoothStep (slot - std::floor (slot));
    const float amount = depth * std::pow (r, 1.35f);
    for (size_t g = 0; g < genes.size(); ++g)
    {
        const auto& gn = genes[g];
        if ((locks & (1u << gn.section)) != 0) { lastApplied[g] = gn.param->getValue(); continue; }
        if (gn.gate != nullptr && gn.gate->getValue() < 0.001f) { lastApplied[g] = gn.param->getValue(); continue; }
        float v = base[g];
        if (gn.discrete)
        {
            const auto& br = w < 0.5f ? branches[(size_t) b0] : branches[(size_t) b1];
            if (br.jump[g] >= 0.0f && amount > 0.42f) v = br.jump[g];
            if (gn.numSteps > 1) v = std::round (v * (float) (gn.numSteps - 1)) / (float) (gn.numSteps - 1);
        }
        else
        {
            const float d = branches[(size_t) b0].delta[g] * (1.0f - w) + branches[(size_t) b1].delta[g] * w;
            v = jlimit (0.0f, 1.0f, base[g] + d * amount);
        }
        if (std::abs (v - gn.param->getValue()) > 1.0e-5f) gn.param->setValueNotifyingHost (v);
        lastApplied[g] = gn.param->getValue();
    }
}

Point<float> GeneField::toField (Point<float> l) const
{
    const auto c = field.getCentre();
    Point<float> p ((l.x - c.x) / (field.getWidth() * 0.5f), (l.y - c.y) / (field.getHeight() * 0.5f));
    if (p.getDistanceFromOrigin() > 1.0f) p /= p.getDistanceFromOrigin();
    return p;
}

Point<float> GeneField::toLocal (Point<float> f) const
{
    const auto c = field.getCentre();
    return { c.x + f.x * field.getWidth() * 0.5f, c.y + f.y * field.getHeight() * 0.5f };
}

void GeneField::resized()
{
    const float s = (float) jmin (getWidth(), getHeight()) - 24.0f;
    field = Rectangle<float> (s, s).withCentre (getLocalBounds().toFloat().getCentre().translated (0, -6));
    buildBranchPaths();
}

void GeneField::buildBranchPaths()
{
    branchPaths.clear();
    if (field.isEmpty()) return;
    for (int b = 0; b < numBranches && b < (int) branches.size(); ++b)
    {
        const float a = (float) b / (float) numBranches * MathConstants<float>::twoPi;
        const float curl = branches[(size_t) b].curl;
        Path p;
        p.startNewSubPath (toLocal ({}));
        for (int s = 1; s <= 24; ++s)
        {
            const float t = (float) s / 24.0f;
            const float aa = a + curl * t * t + 0.06f * std::sin (t * 9.0f + (float) b);
            p.lineTo (toLocal ({ std::cos (aa) * t * 0.97f, std::sin (aa) * t * 0.97f }));
        }
        branchPaths.push_back (p);
    }
}

void GeneField::paint (Graphics& g)
{
    const auto c = field.getCentre();
    const float R = field.getWidth() * 0.5f;
    g.setGradientFill (ColourGradient (Colour (0xff101319), c.x, c.y, Colour (0xff040407), c.x, c.y + R, true));
    g.fillEllipse (field);
    g.setColour (Colours_::red.withAlpha (0.05f));
    for (int i = 1; i <= 4; ++i) { const float rr = R * (float) i / 4.0f; g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 1.0f); }
    {
        const float a = pulse * MathConstants<float>::twoPi;
        Path wedge; wedge.addPieSegment (field, a, a + 0.5f, 0.0f);
        g.setGradientFill (ColourGradient (Colours_::red.withAlpha (0.07f), c.x, c.y, Colours::transparentBlack, c.x + R, c.y, true));
        g.fillPath (wedge);
    }
    for (size_t b = 0; b < branchPaths.size(); ++b)
    {
        g.setColour (Colours_::red.withAlpha (0.08f));
        g.strokePath (branchPaths[b], PathStrokeType (5.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (Colours_::red.withAlpha (0.5f));
        g.strokePath (branchPaths[b], PathStrokeType (1.2f, PathStrokeType::curved, PathStrokeType::rounded));
        for (float t : { 0.33f, 0.66f, 0.97f })
        {
            const auto pt = branchPaths[b].getPointAlongPath (branchPaths[b].getLength() * t);
            const float glow = 0.5f + 0.5f * std::sin (pulse * 12.0f + (float) b + t * 5.0f);
            g.setColour (Colours_::cyan.withAlpha (0.15f + 0.2f * glow)); g.fillEllipse (pt.x - 5, pt.y - 5, 10, 10);
            g.setColour (Colours_::red.withAlpha (0.85f)); g.fillEllipse (pt.x - 2, pt.y - 2, 4, 4);
        }
    }
    g.setColour (Colours_::red.withAlpha (0.3f)); g.drawEllipse (field.reduced (1.0f), 1.5f);
    g.setColour (Colours_::red.withAlpha (0.8f)); g.fillEllipse (c.x - 5, c.y - 5, 10, 10);
    for (int i = 0; i < (int) trail.size(); ++i)
    {
        const auto& t = trail[(size_t) ((trailHead + i) % (int) trail.size())];
        const float a = (float) i / (float) trail.size();
        const auto p = toLocal (t);
        g.setColour (Colours_::red.withAlpha (a * 0.5f));
        g.fillEllipse (p.x - 2.5f * a, p.y - 2.5f * a, 5.0f * a, 5.0f * a);
    }
    const auto pl = toLocal (pos);
    g.setColour (Colours_::red.withAlpha (0.25f)); g.drawLine (c.x, c.y, pl.x, pl.y, 1.0f);
    g.fillEllipse (pl.x - 14, pl.y - 14, 28, 28);
    g.setColour (Colours_::red); g.fillEllipse (pl.x - 6, pl.y - 6, 12, 12);
    g.setColour (Colours::black); g.fillEllipse (pl.x - 2.5f, pl.y - 2.5f, 5, 5);
    if (hovering && ! dragging) { const auto hl = toLocal (hover); g.setColour (Colours_::cyan.withAlpha (0.5f)); g.drawEllipse (hl.x - 9, hl.y - 9, 18, 18, 1.0f); }
    g.setFont (LnF::font (11.5f, true));
    g.setColour (Colours_::textDim);
    g.drawText ("MUTATION " + String (roundToInt (pos.getDistanceFromOrigin() * depth * 100.0f)) + "%", getLocalBounds().removeFromBottom (16).removeFromLeft (160), Justification::centredLeft);
    g.drawText ("DOUBLE-CLICK TO PLANT", getLocalBounds().removeFromBottom (16).removeFromRight (180), Justification::centredRight);
}

void GeneField::mouseDown (const MouseEvent& e)
{
    if (! hasBase || userChangedSomething()) { captureBase(); pos = {}; }
    dragging = true;
    proc.undo.beginNewTransaction ("Gene field");
    for (auto& gn : genes) gn.param->beginChangeGesture();
    mouseDrag (e);
}

void GeneField::mouseDrag (const MouseEvent& e)
{
    pos = toField (e.position);
    trail[(size_t) trailHead] = pos;
    trailHead = (trailHead + 1) % (int) trail.size();
    applyAt (pos);
    repaint();
}

void GeneField::mouseUp (const MouseEvent&) { dragging = false; for (auto& gn : genes) gn.param->endChangeGesture(); }
void GeneField::mouseDoubleClick (const MouseEvent&) { plant(); }
void GeneField::mouseMove (const MouseEvent& e) { hover = toField (e.position); hovering = true; }
void GeneField::mouseExit (const MouseEvent&) { hovering = false; repaint(); }

void GeneField::timerCallback()
{
    if (! isShowing()) return;
    pulse += 0.004f;
    if (pulse > 1.0f) pulse -= 1.0f;
    if (proc.getPresetChangeCounter() != lastPresetCounter)
    {
        lastPresetCounter = proc.getPresetChangeCounter();
        captureBase();
        pos = {};
        for (auto& t : trail) t = {};
    }
    repaint();
}

// ================================================================== MUTATE page
MutatePage::MutatePage (XOSTProcessor& p, PresetLibrary& l) : Page (p), lib (l)
{
    addAndMakeVisible (gene);
    for (auto* b : { &plantButton, &centreButton, &seedButton, &subtle, &medium, &wild, &partnerButton, &randomPartnerButton,
                     &breedButton, &morphButton, &fromCategoryButton, &backButton, &forwardButton })
        addAndMakeVisible (*b);
    plantButton.onClick = [this] { gene.plant(); };
    centreButton.onClick = [this] { gene.recentre(); };
    seedButton.onClick = [this] { gene.newSeed(); };
    depthSlider.setSliderStyle (Slider::LinearHorizontal);
    depthSlider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    depthSlider.setRange (0.1, 1.0);
    depthSlider.setValue (0.6, dontSendNotification);
    depthSlider.onValueChange = [this] { gene.setDepth ((float) depthSlider.getValue()); };
    addAndMakeVisible (depthSlider);

    subtle.onClick = [this] { proc.mutate (0.12f, proc.randomLocks.load()); };
    medium.onClick = [this] { proc.mutate (0.3f, proc.randomLocks.load()); };
    wild.onClick = [this] { proc.mutate (0.65f, proc.randomLocks.load()); };
    subtle.setTooltip ("Nudge the current sound a little (locked sections stay put)");
    wild.setTooltip ("Big changes - expect something very different");

    partnerButton.onClick = [this] { choosePartner(); };
    randomPartnerButton.onClick = [this] { randomPartner(); };
    mixSlider.setSliderStyle (Slider::LinearHorizontal);
    mixSlider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    mixSlider.setRange (0.0, 1.0);
    mixSlider.setValue (0.5, dontSendNotification);
    mixSlider.onDragStart = [this] { if (morphButton.getToggleState() && partner >= 0) proc.undo.beginNewTransaction ("Morph"); };
    mixSlider.onValueChange = [this]
    {
        if (! morphButton.getToggleState() || partner < 0 || baseValues.isEmpty()) return;
        proc.applyValues (baseValues, XOSTProcessor::Scope::all);
        proc.blendWith (Presets::factory()[(size_t) partner].values, (float) mixSlider.getValue(), proc.randomLocks.load(), true,
                        "Morph " + String (roundToInt (mixSlider.getValue() * 100)) + "%", false);
    };
    addAndMakeVisible (mixSlider);
    morphButton.setClickingTogglesState (true);
    morphButton.onClick = [this] { if (morphButton.getToggleState()) baseValues = proc.captureValues (XOSTProcessor::Scope::all); };
    morphButton.setTooltip ("When on, the slider blends smoothly from your sound (left) to the partner (right)");
    breedButton.onClick = [this]
    {
        if (partner < 0) randomPartner();
        proc.blendWith (Presets::factory()[(size_t) partner].values, (float) mixSlider.getValue(), proc.randomLocks.load(), false,
                        "Child of " + Presets::name (partner));
    };
    breedButton.setTooltip ("Mix whole sections of your sound and the partner (slider = how much comes from the partner), plus a small mutation");

    categoryBox.addItem ("Any category", 1);
    int id = 2;
    for (auto& c : lib.getCategories()) categoryBox.addItem (c, id++);
    categoryBox.setSelectedId (1, dontSendNotification);
    addAndMakeVisible (categoryBox);
    fromCategoryButton.onClick = [this]
    {
        const auto cat = categoryBox.getSelectedId() > 1 ? categoryBox.getText() : String();
        Array<int> pool;
        for (int i = 1; i < Presets::count(); ++i) if (cat.isEmpty() || Presets::category (i) == cat) pool.add (i);
        if (pool.isEmpty()) return;
        proc.setCurrentProgram (pool[Random::getSystemRandom().nextInt (pool.size())]);
        proc.mutate (0.25f, proc.randomLocks.load());
    };
    backButton.onClick = [this] { proc.undo.undo(); };
    forwardButton.onClick = [this] { proc.undo.redo(); };

    const auto& names = XOSTProcessor::sectionNames();
    for (int i = 0; i < names.size(); ++i)
    {
        auto* b = lockChips.add (new TextButton (names[i]));
        b->setClickingTogglesState (true);
        b->onClick = [this, i]
        {
            uint32_t lk = proc.randomLocks.load();
            lk = lockChips[i]->getToggleState() ? (lk | (1u << i)) : (lk & ~(1u << i));
            proc.randomLocks = lk;
        };
        addAndMakeVisible (b);
    }
    updateLocks();
}

void MutatePage::updateLocks()
{
    const uint32_t l = proc.randomLocks.load();
    for (int i = 0; i < lockChips.size(); ++i) lockChips[i]->setToggleState ((l >> i) & 1u, dontSendNotification);
}

void MutatePage::setPartner (int idx)
{
    partner = idx;
    partnerButton.setButtonText (idx >= 0 ? "PARTNER: " + Presets::name (idx) : String ("CHOOSE PARTNER..."));
    if (morphButton.getToggleState()) baseValues = proc.captureValues (XOSTProcessor::Scope::all);
}

void MutatePage::randomPartner()
{
    const int n = Presets::count();
    if (n > 1) setPartner (1 + Random::getSystemRandom().nextInt (n - 1));
}

void MutatePage::choosePartner()
{
    PopupMenu m;
    auto cats = lib.getCategories();
    for (auto& c : cats)
    {
        PopupMenu sub;
        int added = 0;
        for (int i = 1; i < Presets::count(); ++i)
            if (Presets::category (i) == c) { sub.addItem (1 + i, Presets::name (i)); ++added; }
        if (added > 0) m.addSubMenu (c, sub);
    }
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&partnerButton).withMaximumNumColumns (6),
                     [this] (int r) { if (r > 0) setPartner (r - 1); });
}

void MutatePage::resized()
{
    panels.clear(); labels.clear();
    auto f = panel ("GENE FIELD  -  DRAG TO GROW NEW SOUNDS", { 0, 0, 560, 512 });
    auto fb = f.removeFromBottom (34);
    plantButton.setBounds (fb.removeFromLeft (86)); fb.removeFromLeft (6);
    centreButton.setBounds (fb.removeFromLeft (86)); fb.removeFromLeft (6);
    seedButton.setBounds (fb.removeFromLeft (96)); fb.removeFromLeft (14);
    label ("DEPTH", fb.removeFromLeft (44));
    depthSlider.setBounds (fb);
    gene.setBounds (f.withTrimmedBottom (6));

    auto m = panel ("MUTATE", { 570, 0, 330, 160 });
    label ("Change the current sound. Locked sections (right) are never touched.", m.removeFromTop (30));
    auto mr = m.removeFromTop (36);
    const int bw = (mr.getWidth() - 12) / 3;
    subtle.setBounds (mr.removeFromLeft (bw)); mr.removeFromLeft (6); medium.setBounds (mr.removeFromLeft (bw)); mr.removeFromLeft (6); wild.setBounds (mr);
    m.removeFromTop (10);
    auto hist = m.removeFromTop (30);
    backButton.setBounds (hist.removeFromLeft (hist.getWidth() / 2 - 3)); hist.removeFromLeft (6); forwardButton.setBounds (hist);

    auto bpanel = panel ("BREED + MORPH", { 570, 170, 330, 342 });
    partnerButton.setBounds (bpanel.removeFromTop (30));
    bpanel.removeFromTop (6);
    randomPartnerButton.setBounds (bpanel.removeFromTop (28).withWidth (120));
    bpanel.removeFromTop (12);
    auto lr = bpanel.removeFromTop (16);
    label ("YOUR SOUND", lr.removeFromLeft (lr.getWidth() / 2));
    label ("                       PARTNER", lr);
    mixSlider.setBounds (bpanel.removeFromTop (30));
    bpanel.removeFromTop (8);
    auto br = bpanel.removeFromTop (34);
    breedButton.setBounds (br.removeFromLeft (br.getWidth() / 2 - 3)); br.removeFromLeft (6); morphButton.setBounds (br);
    bpanel.removeFromTop (14);
    label ("RANDOM PRESET FROM", bpanel.removeFromTop (16));
    categoryBox.setBounds (bpanel.removeFromTop (28));
    bpanel.removeFromTop (6);
    fromCategoryButton.setBounds (bpanel.removeFromTop (30));
    label ("Breed swaps whole sections between the two sounds. Live Morph blends every knob as you move the slider.", bpanel.withTrimmedTop (8).withHeight (44));

    auto lk = panel ("LOCKS", { 910, 0, 346, 512 });
    label ("Locked sections stay as they are for Random, Mutate, Breed, Morph and the Gene Field.", lk.removeFromTop (34));
    for (int i = 0; i < lockChips.size(); ++i)
    {
        const int col = i % 2, rowI = i / 2;
        lockChips[i]->setBounds (lk.getX() + col * (lk.getWidth() / 2 + 2), lk.getY() + rowI * 36, lk.getWidth() / 2 - 4, 30);
    }
}

void MutatePage::paint (Graphics& g) { Page::paint (g); }
void MutatePage::tick() { updateLocks(); }

// ================================================================== TONE LIST
ToneList::ToneList (XOSTProcessor& p, PresetLibrary& l) : proc (p), lib (l)
{
    search.setTextToShowWhenEmpty ("Search...", Colours_::textDim);
    search.setFont (LnF::font (13.5f));
    search.setIndents (8, 5);
    search.onTextChange = [this] { refresh(); };
    addAndMakeVisible (search);
    addAndMakeVisible (category);
    addAndMakeVisible (vibe);
    category.onChange = [this] { refresh(); };
    vibe.onChange = [this] { refresh(); };
    list.setRowHeight (21);
    list.setColour (ListBox::backgroundColourId, Colours::transparentBlack);
    list.setOutlineThickness (0);
    addAndMakeVisible (list);
    category.addItem ("All categories", 1);
    int id = 2;
    for (auto& c : lib.getCategories()) category.addItem (c, id++);
    category.addItem ("Favourites", 900);
    category.setSelectedId (1, dontSendNotification);
    vibe.addItem ("Any style", 1);
    id = 2;
    for (auto& t : PresetLibrary::genreTags()) vibe.addItem (t, id++);
    vibe.addSeparator();
    for (auto& t : PresetLibrary::typeBeatTags()) vibe.addItem (t.replaceCharacter ('_', ' ') + " type", id++);
    vibe.setSelectedId (1, dontSendNotification);
    refresh();
}

void ToneList::refresh()
{
    const auto& entries = lib.getEntries();
    const auto q = search.getText().trim().toLowerCase();
    const auto cat = category.getSelectedId() > 1 && category.getSelectedId() < 900 ? category.getText() : String();
    const bool favs = category.getSelectedId() == 900;
    String tag;
    if (vibe.getSelectedId() > 1)
    {
        const int i = vibe.getSelectedId() - 2;
        const auto& g = PresetLibrary::genreTags();
        tag = i < g.size() ? g[i] : PresetLibrary::typeBeatTags()[i - g.size()];
    }
    items.clearQuick();
    for (int i = 0; i < entries.size(); ++i)
    {
        const auto& e = entries.getReference (i);
        if (cat.isNotEmpty() && e.category != cat) continue;
        if (favs && ! lib.isFavourite (e.name)) continue;
        if (tag.isNotEmpty() && ! e.tags.contains (tag)) continue;
        if (q.isNotEmpty() && ! (e.name + " " + e.category + " " + e.tags.joinIntoString (" ")).toLowerCase().contains (q)) continue;
        items.add (i);
    }
    list.updateContent();
    syncSelection();
    repaint();
}

void ToneList::syncSelection()
{
    const auto& entries = lib.getEntries();
    for (int r = 0; r < items.size(); ++r)
        if (entries.getReference (items[r]).name == proc.getCurrentPresetName())
        {
            loading = true;
            list.selectRow (r, false, true);
            loading = false;
            return;
        }
    loading = true; list.deselectAllRows(); loading = false;
    list.repaint();
}

void ToneList::paintListBoxItem (int row, Graphics& g, int w, int h, bool selected)
{
    if (! isPositiveAndBelow (row, items.size())) return;
    const auto& e = lib.getEntries().getReference (items[row]);
    if (selected) { g.setColour (Colours_::red); g.fillRect (0, 0, w, h); }
    else if (row % 2) { g.setColour (Colours_::red.withAlpha (0.025f)); g.fillRect (0, 0, w, h); }
    g.setFont (LnF::font (10.0f, true));
    g.setColour (selected ? Colours_::onAccent.withAlpha (0.6f) : Colours_::textDim);
    g.drawText (String (items[row]).paddedLeft ('0', 4), 6, 0, 34, h, Justification::centredLeft);
    g.setFont (LnF::font (13.0f, selected));
    g.setColour (selected ? Colours_::onAccent : Colours_::text);
    g.drawText (e.name, 40, 0, w - 110, h, Justification::centredLeft, true);
    g.setFont (LnF::font (10.0f, true));
    g.setColour (selected ? Colours_::onAccent.withAlpha (0.6f) : Colours_::textDim);
    g.drawText (e.category.toUpperCase(), w - 72, 0, 66, h, Justification::centredRight);
}

void ToneList::listBoxItemClicked (int row, const MouseEvent&) { selectedRowsChanged (row); }

void ToneList::selectedRowsChanged (int row)
{
    if (loading || ! isPositiveAndBelow (row, items.size())) return;
    const auto& e = lib.getEntries().getReference (items[row]);
    if (e.name == proc.getCurrentPresetName()) return;
    if (e.isUser()) proc.loadUserPreset (e.file); else proc.setCurrentProgram (e.factoryIndex);
    if (onLoaded) onLoaded();
}

void ToneList::resized()
{
    auto r = getLocalBounds().reduced (8);
    r.removeFromTop (22);
    search.setBounds (r.removeFromTop (28)); r.removeFromTop (6);
    category.setBounds (r.removeFromTop (26)); r.removeFromTop (4);
    vibe.setBounds (r.removeFromTop (26)); r.removeFromTop (6);
    list.setBounds (r);
}

void ToneList::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours_::panel); g.fillRoundedRectangle (r, 5.0f);
    g.setColour (Colours_::panelEdge); g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
    g.setFont (LnF::title (11.5f)); g.setColour (Colours_::text);
    g.drawText ("TONE LIST", 12, 8, 120, 16, Justification::centredLeft);
    g.setFont (LnF::font (10.5f, true)); g.setColour (Colours_::textDim);
    g.drawText (String (items.size()) + " / " + String (lib.getEntries().size()), getWidth() - 110, 8, 98, 16, Justification::centredRight);
}
