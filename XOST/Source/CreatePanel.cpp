#include "CreatePanel.h"
#include "SampleTools.h"

using namespace juce;
using LnF = XOLookAndFeel;

namespace
{
    Colour chipColour (const xost::Chip& c)
    {
        if (c.unknown) return Colours_::chipUnknown;
        if (c.type == xost::EntryType::cat || c.type == xost::EntryType::kind) return Colours_::chipSound;
        if (c.type == xost::EntryType::style) return Colours_::chipStyle;
        if (c.type == xost::EntryType::chr) return Colours_::chipChar;
        if (c.type == xost::EntryType::fx) return Colours_::chipFx;
        return Colours_::chipAmount;
    }

    void panelBox (Graphics& g, Rectangle<float> r)
    {
        g.setColour (Colours_::panel); g.fillRoundedRectangle (r, 5.0f);
        g.setColour (Colours_::panelEdge); g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
    }

    void caption (Graphics& g, const String& t, Rectangle<int> r)
    {
        g.setFont (LnF::font (10.0f, true)); g.setColour (Colours_::textDim);
        g.drawText (t, r, Justification::centredLeft);
    }
}

// ================================================================== chips
void ChipRow::paint (Graphics& g)
{
    float x = 0.0f;
    const float h = (float) getHeight();
    g.setFont (LnF::font (11.0f, true));
    for (auto& c : chips)
    {
        auto text = c.text.startsWithChar ('~') ? c.text.substring (1) + "?" : c.text;
        if (c.unknown) text = c.text;
        const float w = GlyphArrangement::getStringWidth (g.getCurrentFont(), text) + 20.0f;
        if (x + w > (float) getWidth() - 4) break;
        auto r = Rectangle<float> (x, 2.0f, w, h - 4.0f);
        const auto col = chipColour (c);
        g.setColour (col.withAlpha (c.unknown ? 0.12f : 0.16f)); g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (col.withAlpha (c.unknown ? 0.5f : 0.9f)); g.drawRoundedRectangle (r.reduced (0.5f), r.getHeight() * 0.5f, 1.0f);
        g.setColour (c.unknown ? Colours_::textDim : col.brighter (0.4f));
        if (c.negated) g.setColour (col.withAlpha (0.7f));
        g.drawText (text, r, Justification::centred);
        if (c.unknown) g.drawLine (r.getX() + 8, r.getCentreY(), r.getRight() - 8, r.getCentreY(), 1.0f);
        x += w + 6.0f;
    }
    auto msgArea = getLocalBounds().toFloat().withTrimmedLeft (x + 6.0f);
    g.setFont (LnF::font (11.0f));
    if (busy)
    {
        g.setColour (Colours_::red);
        String dots; for (int i = 0; i < (frame / 6) % 4; ++i) dots << ".";
        g.drawText ("GENERATING" + dots, msgArea, Justification::centredLeft);
    }
    else if (message.isNotEmpty())
    {
        g.setColour (Colours_::textDim);
        g.drawFittedText (message, msgArea.toNearestInt(), Justification::centredLeft, 1);
    }
    else if (chips.empty())
    {
        g.setColour (Colours_::textDim.withAlpha (0.8f));
        g.drawText ("Words XOST understands light up here: SOUND  STYLE  CHARACTER  EFFECT", msgArea, Justification::centredLeft);
    }
}

// ================================================================== prompt bar
PromptBar::PromptBar (XOSTProcessor& p) : proc (p)
{
    text.setFont (LnF::font (17.0f));
    text.setIndents (14, 9);
    text.setTextToShowWhenEmpty ("Describe a sound...   e.g.  dark glassy pluck like pierre bourne with tape", Colours_::textDim);
    text.setSelectAllWhenFocused (false);
    text.addListener (this);
    text.setColour (TextEditor::backgroundColourId, Colours_::screen);
    addAndMakeVisible (text);
    addAndMakeVisible (chips);
    for (auto* b : { &generateButton, &surpriseButton, &tweakToggle, &helpButton }) addAndMakeVisible (*b);
    generateButton.getProperties().set ("accent", true);
    generateButton.setColour (TextButton::buttonColourId, Colours_::red);
    generateButton.setColour (TextButton::textColourOffId, Colours_::onAccent);
    generateButton.onClick = [this] { generate(); };
    surpriseButton.onClick = [this] { surprise(); };
    tweakToggle.setClickingTogglesState (true);
    tweakToggle.setTooltip ("TWEAK: words change the current sound instead of making new ones (\"darker\", \"more reverb\", \"no chorus\").\n"
                            "Words like 'more', 'less' and 'darker' switch to tweaking automatically.");
    surpriseButton.setTooltip ("Writes a random description and generates it - a quick way to see what words XOST knows");
    generateButton.setTooltip ("Make 4 sounds from your description (Enter). Press again for 4 new ones.");
    helpButton.setTooltip ("Type sound words (pluck, 808, bells, piano, pad, flute, cowbell...), styles (pierre bourne, yeat, phonk, jerk, drill, pluggnb, "
                           "drain gang...), character (dark, glassy, warm, wide, lofi, haunted...), effects (reverb, tape, sidechain, wobble, chords...) "
                           "and amounts (a little, very, no). XOST is keyword based: it ignores grammar, so short descriptions work best.");
    helpButton.onClick = [this]
    {
        PopupMenu m;
        m.addSectionHeader ("Try one of these");
        const char* examples[] = { "dark glassy pluck like pierre bourne with tape", "yeat bells", "phonk cowbell very distorted",
                                   "pluggnb keys with chords", "uk drill sliding 808", "jerk lead", "drain gang trance pad", "metro boomin dark piano",
                                   "warm lofi rhodes a little wobble", "sigilkore crushed pad", "jersey club vocal chop", "haunted choir no reverb",
                                   "rage supersaw lead wide", "emo guitar sad", "detroit piano" };
        for (int i = 0; i < (int) std::size (examples); ++i) m.addItem (i + 1, examples[i]);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&helpButton), [this, examples] (int r)
        {
            if (r > 0) { text.setText (examples[r - 1], true); generate(); }
        });
    };
    syncFromProcessor();
}

void PromptBar::syncFromProcessor()
{
    if (text.getText() != proc.create.prompt) text.setText (proc.create.prompt, false);
    chips.setChips (xost::understand (text.getText()).chips);
}

void PromptBar::paint (Graphics& g)
{
    panelBox (g, getLocalBounds().toFloat());
}

void PromptBar::resized()
{
    auto r = getLocalBounds().reduced (8, 8);
    auto top = r.removeFromTop (38);
    helpButton.setBounds (top.removeFromRight (34));
    top.removeFromRight (6);
    tweakToggle.setBounds (top.removeFromRight (70));
    top.removeFromRight (6);
    surpriseButton.setBounds (top.removeFromRight (96));
    top.removeFromRight (6);
    generateButton.setBounds (top.removeFromRight (130));
    top.removeFromRight (8);
    text.setBounds (top);
    r.removeFromTop (6);
    chips.setBounds (r.removeFromTop (24));
}

void PromptBar::textEditorTextChanged (TextEditor&)
{
    chips.message.clear();
    chips.setChips (xost::understand (text.getText()).chips);
}

void PromptBar::submit (const String& prompt, bool refine, const String& preferKind)
{
    xost::Generator::Request r;
    r.prompt = prompt;
    r.refine = refine;
    r.preferKind = preferKind;
    r.currentValues = proc.captureValues (XOSTProcessor::Scope::all);
    r.currentName = proc.getCurrentPresetName();
    r.seed = seed++ + (juce::uint32) Time::getMillisecondCounter();
    pending = proc.generator().submit (r);
    chips.busy = true;
    chips.repaint();
}

void PromptBar::generate (bool again, const String& preferKind)
{
    auto prompt = text.getText().trim();
    if (again && prompt.isEmpty()) prompt = proc.create.prompt;
    proc.create.prompt = prompt;
    submit (prompt, tweakToggle.getToggleState() && ! again && preferKind.isEmpty(), preferKind);
}

void PromptBar::tweak (const String& words) { submit (words, true, {}); }

void PromptBar::surprise()
{
    text.setText (xost::randomPrompt (rng), true);
    tweakToggle.setToggleState (false, dontSendNotification);
    generate();
}

void PromptBar::tick (int frame)
{
    chips.frame = frame;
    if (chips.busy) chips.repaint();
    if (pending == 0) return;
    xost::Generator::Result res;
    if (! proc.generator().take (res)) return;
    if (res.id != pending) return;
    pending = 0;
    chips.busy = false;
    chips.message = res.message;
    if (res.request.prompt == text.getText().trim()) chips.setChips (res.understood.chips);
    if (res.variations.empty()) { chips.repaint(); return; }
    const bool tweaked = res.variations.size() == 1 && res.variations[0].kind.isEmpty();
    if (! tweaked)
    {
        const ScopedLock sl (proc.createLock);
        proc.create.variations = res.variations;
        proc.create.selected = 0;
    }
    proc.loadVariation (res.variations[0], res.request.prompt);
    proc.addHistory (res.request.prompt, res.variations[0]);
    if (onResult) onResult (res);
    chips.repaint();
}

// ================================================================== one-shot exporter
OneShotBox::OneShotBox (XOSTProcessor& p) : proc (p)
{
    for (auto* b : { &exportButton, &optionsButton, &folderButton }) addAndMakeVisible (*b);
    exportButton.onClick = [this] { exportNow(); };
    optionsButton.onClick = [this] { showOptions(); };
    folderButton.onClick = [] { XOSTProcessor::getUserFolder ("One Shots").startAsProcess(); };
    exportButton.setTooltip ("Render the current sound as a trimmed one-shot WAV (named with its root note), ready to drag into a sampler or FL's playlist");
    folderButton.setTooltip ("Open Documents/XOST/One Shots");
}

void OneShotBox::paint (Graphics& g)
{
    panelBox (g, getLocalBounds().toFloat());
    caption (g, "ONE-SHOT", { 12, 6, 120, 14 });
    auto c = chipArea.toFloat();
    const bool has = proc.lastOneShot.existsAsFile();
    g.setColour (has ? Colours_::red.withAlpha (0.14f) : Colours_::knobBody); g.fillRoundedRectangle (c, 6.0f);
    g.setColour (has ? Colours_::red : Colours_::panelEdge); g.drawRoundedRectangle (c.reduced (0.5f), 6.0f, has ? 1.4f : 1.0f);
    g.setFont (LnF::font (11.0f, true));
    g.setColour (has ? Colours_::text : Colours_::textDim);
    if (has)
    {
        g.drawFittedText ("DRAG  " + proc.lastOneShot.getFileNameWithoutExtension(), chipArea.reduced (10, 0), Justification::centredLeft, 1);
    }
    else g.drawFittedText ("Export, then drag the WAV from here", chipArea.reduced (10, 0), Justification::centredLeft, 1);
}

void OneShotBox::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    r.removeFromTop (16);
    auto row = r.removeFromTop (30);
    exportButton.setBounds (row.removeFromLeft (160));
    row.removeFromLeft (6);
    optionsButton.setBounds (row.removeFromLeft (row.getWidth() / 2 - 3));
    row.removeFromLeft (6);
    folderButton.setBounds (row);
    r.removeFromTop (6);
    chipArea = r.removeFromTop (28);
}

void OneShotBox::exportNow()
{
    String root;
    auto f = proc.exportOneShot (proc.oneShot, &root);
    shownRoot = root;
    repaint();
    if (! f.existsAsFile())
        AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "XOST", "Couldn't write the one-shot (is the sound silent?)");
}

void OneShotBox::showOptions()
{
    auto& o = proc.oneShot;
    PopupMenu m, note, len, names;
    note.addItem (100, "Auto (C for most sounds, low C for bass)", true, o.note < 0);
    for (int n : { 36, 48, 60, 72 }) note.addItem (100 + n, uno::noteName (n, o.middleCOctave), true, o.note == n);
    len.addItem (200, "Auto (by sound type)", true, o.length < 0);
    len.addItem (201, "Short hit", true, o.length == 0);
    len.addItem (202, "Medium (1 s)", true, o.length == 1);
    len.addItem (203, "Long (2.5 s)", true, o.length == 2);
    names.addItem (305, "FL Studio (middle C = C5)", true, o.middleCOctave == 5);
    names.addItem (304, "Logic / most (middle C = C4)", true, o.middleCOctave == 4);
    names.addItem (303, "Ableton (middle C = C3)", true, o.middleCOctave == 3);
    m.addSubMenu ("Note", note);
    m.addSubMenu ("Length", len);
    m.addSubMenu ("Note names", names);
    m.addItem (400, "Normalise to -0.3 dB", true, o.normalise);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&optionsButton), [this] (int r)
    {
        auto& op = proc.oneShot;
        if (r == 100) op.note = -1;
        else if (r > 100 && r < 200) op.note = r - 100;
        else if (r >= 200 && r < 300) op.length = r - 201;
        else if (r >= 300 && r < 400) op.middleCOctave = r - 300;
        else if (r == 400) op.normalise = ! op.normalise;
    });
}

void OneShotBox::mouseDrag (const MouseEvent& e)
{
    if (chipArea.contains (e.getMouseDownPosition()) && e.getDistanceFromDragStart() > 4 && proc.lastOneShot.existsAsFile())
        DragAndDropContainer::performExternalDragDropOfFiles ({ proc.lastOneShot.getFullPathName() }, false, this);
}

// ================================================================== variation cards
void VariationCard::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const auto& vars = proc.create.variations;
    const bool has = index < (int) vars.size();
    const bool sel = has && proc.create.selected == index;
    const bool over = isMouseOver (true);
    g.setColour (sel ? Colours_::red.withAlpha (0.12f) : Colours_::knobBody); g.fillRoundedRectangle (r, 5.0f);
    g.setColour (sel ? Colours_::red : (over && has ? Colours_::cyan : Colours_::panelEdge)); g.drawRoundedRectangle (r, 5.0f, sel ? 1.6f : 1.0f);
    // letter badge
    auto badge = Rectangle<float> (r.getX() + 7, r.getY() + 7, 18, 18);
    g.setColour (sel ? Colours_::red : Colours_::track); g.fillRoundedRectangle (badge, 4.0f);
    g.setColour (sel ? Colours_::onAccent : Colours_::textDim);
    g.setFont (LnF::font (11.0f, true));
    g.drawText (String::charToString ((juce_wchar) ('A' + index)), badge, Justification::centred);
    // audition triangle
    auto play = Rectangle<float> (r.getRight() - 22, r.getY() + 7, 15, 15);
    if (has)
    {
        Path tri; tri.addTriangle (play.getX() + 3, play.getY() + 2, play.getX() + 3, play.getBottom() - 2, play.getRight() - 1, play.getCentreY());
        g.setColour (over ? Colours_::red : Colours_::textDim); g.fillPath (tri);
    }
    if (! has)
    {
        g.setFont (LnF::font (11.0f)); g.setColour (Colours_::textDim.withAlpha (0.6f));
        g.drawText ("-", r, Justification::centred);
        return;
    }
    const auto& v = vars[(size_t) index];
    g.setFont (LnF::font (10.0f, true)); g.setColour (Colours_::red.withAlpha (0.85f));
    g.drawText (v.category.toUpperCase(), Rectangle<float> (r.getX() + 31, r.getY() + 7, r.getWidth() - 58, 18), Justification::centredLeft);
    g.setFont (LnF::font (12.5f, true)); g.setColour (Colours_::text);
    g.drawFittedText (v.name, Rectangle<int> ((int) r.getX() + 8, (int) r.getY() + 29, (int) r.getWidth() - 14, 18), Justification::centredLeft, 1, 0.75f);
    g.setFont (LnF::font (10.0f)); g.setColour (Colours_::textDim);
    g.drawFittedText (v.summary, Rectangle<int> ((int) r.getX() + 8, (int) r.getY() + 49, (int) r.getWidth() - 14, (int) r.getHeight() - 52), Justification::topLeft, 3, 0.9f);
}

void VariationCard::mouseUp (const MouseEvent& e)
{
    if (index >= (int) proc.create.variations.size() || ! e.mouseWasClicked()) return;
    const bool play = e.getPosition().x > getWidth() - 28 && e.getPosition().y < 28;
    if (onPick) onPick (index, play);
}

// ================================================================== CREATE column
CreatePanel::CreatePanel (XOSTProcessor& p, PresetLibrary& lib, PromptBar& pb) : toneList (p, lib), proc (p), prompt (pb)
{
    for (auto* t : { &createTab, &presetsTab })
    {
        t->setClickingTogglesState (true);
        t->setRadioGroupId (77);
        addAndMakeVisible (*t);
    }
    createTab.onClick = [this] { showTab (0); };
    presetsTab.onClick = [this] { showTab (1); };
    for (int i = 0; i < 4; ++i)
    {
        auto* c = cards.add (new VariationCard (proc, i));
        c->onPick = [this] (int idx, bool play) { pick (idx, play); };
        addAndMakeVisible (c);
    }
    addAndMakeVisible (againButton);
    addAndMakeVisible (moreButton);
    againButton.onClick = [this] { prompt.generate (true); };
    moreButton.onClick = [this]
    {
        const auto& v = proc.create.variations;
        const int s = proc.create.selected;
        if (s >= 0 && s < (int) v.size() && v[(size_t) s].kind.isNotEmpty()) prompt.generate (true, v[(size_t) s].kind);
        else prompt.generate (true);
    };
    againButton.setTooltip ("Same description, four new sounds");
    moreButton.setTooltip ("Four new sounds from the same family as the selected one");

    const char* names[][2] = { { "DARKER", "darker" }, { "BRIGHTER", "brighter" }, { "WIDER", "wider" },
                               { "SOFTER", "softer" }, { "PUNCHIER", "punchier" }, { "GRITTIER", "grittier" },
                               { "MORE SPACE", "more reverb" }, { "DRIER", "drier" }, { "LO-FI", "lofi tape" },
                               { "MOVEMENT", "moving" }, { "SHORTER", "shorter" }, { "LONGER", "longer" } };
    for (auto& n : names)
    {
        auto* b = tweaks.add (new TextButton (n[0]));
        const String words (n[1]);
        b->onClick = [this, words] { prompt.tweak (words); };
        b->setTooltip ("Tweak the current sound: \"" + words + "\"");
        addAndMakeVisible (b);
    }
    history.setRowHeight (30);
    history.setColour (ListBox::backgroundColourId, Colours::transparentBlack);
    addAndMakeVisible (history);
    addChildComponent (toneList);
    showTab (0);
}

void CreatePanel::showTab (int t)
{
    tab = t;
    createTab.setToggleState (t == 0, dontSendNotification);
    presetsTab.setToggleState (t == 1, dontSendNotification);
    toneList.setVisible (t == 1);
    for (auto* c : cards) c->setVisible (t == 0);
    for (auto* b : tweaks) b->setVisible (t == 0);
    for (auto* b : { (Component*) &againButton, (Component*) &moreButton, (Component*) &history }) b->setVisible (t == 0);
    if (t == 1) { toneList.refresh(); toneList.syncSelection(); }
    repaint();
}

void CreatePanel::paint (Graphics& g)
{
    panelBox (g, getLocalBounds().toFloat());
    if (tab != 0) return;
    caption (g, "VARIATIONS", { 12, 40, 200, 14 });
    caption (g, "QUICK TWEAKS", { 12, 288, 200, 14 });
    caption (g, "HISTORY", { 12, 430, 200, 14 });
    if (proc.create.history.empty())
    {
        g.setFont (LnF::font (11.0f)); g.setColour (Colours_::textDim.withAlpha (0.7f));
        g.drawFittedText ("Sounds you generate are kept here, with the words you used.", Rectangle<int> (14, 454, getWidth() - 28, 40), Justification::topLeft, 3);
    }
}

void CreatePanel::resized()
{
    auto r = getLocalBounds().reduced (8, 6);
    auto top = r.removeFromTop (28);
    createTab.setBounds (top.removeFromLeft (top.getWidth() / 2).reduced (1, 0));
    presetsTab.setBounds (top.reduced (1, 0));
    toneList.setBounds (getLocalBounds().withTrimmedTop (40).reduced (2, 2));
    const int cw = (getWidth() - 16 - 6) / 2, ch = 92;
    for (int i = 0; i < 4; ++i) cards[i]->setBounds (8 + (i % 2) * (cw + 6), 58 + (i / 2) * (ch + 6), cw, ch);
    againButton.setBounds (8, 254, cw, 26);
    moreButton.setBounds (8 + cw + 6, 254, cw, 26);
    const int bw = (getWidth() - 16 - 8) / 3;
    for (int i = 0; i < tweaks.size(); ++i) tweaks[i]->setBounds (8 + (i % 3) * (bw + 4), 304 + (i / 3) * 30, bw, 26);
    history.setBounds (6, 448, getWidth() - 12, getHeight() - 454);
}

void CreatePanel::pick (int index, bool play)
{
    const auto& vars = proc.create.variations;
    if (! isPositiveAndBelow (index, (int) vars.size())) return;
    proc.create.selected = index;
    proc.loadVariation (vars[(size_t) index], proc.create.prompt);
    proc.addHistory (proc.create.prompt, vars[(size_t) index]);
    if (play) audition();
    for (auto* c : cards) c->repaint();
}

void CreatePanel::audition()
{
    if (auditionNote >= 0) proc.keyboardState.noteOff (1, auditionNote, 0.0f);
    auditionNote = proc.currentCategory() == "Bass" ? 36 : 60;
    proc.keyboardState.noteOn (1, auditionNote, 0.8f);
    auditionUntil = Time::getMillisecondCounter() + 700;
}

void CreatePanel::tick()
{
    if (auditionNote >= 0 && Time::getMillisecondCounter() > auditionUntil)
    {
        proc.keyboardState.noteOff (1, auditionNote, 0.0f);
        auditionNote = -1;
    }
    if (shownCounter != proc.create.counter)
    {
        shownCounter = proc.create.counter;
        history.updateContent();
        history.repaint();
        for (auto* c : cards) c->repaint();
        repaint();
    }
}

void CreatePanel::paintListBoxItem (int row, Graphics& g, int w, int h, bool)
{
    if (! isPositiveAndBelow (row, (int) proc.create.history.size())) return;
    const auto& it = proc.create.history[(size_t) row];
    const bool current = it.name == proc.getCurrentPresetName();
    if (current) { g.setColour (Colours_::red.withAlpha (0.12f)); g.fillRoundedRectangle (1, 1, (float) w - 2, (float) h - 2, 4.0f); }
    else if (row % 2) { g.setColour (Colours_::red.withAlpha (0.025f)); g.fillRect (0, 0, w, h); }
    g.setFont (LnF::font (12.0f, true)); g.setColour (current ? Colours_::red : Colours_::text);
    g.drawFittedText (it.name, 8, 2, w - 16, 15, Justification::centredLeft, 1);
    g.setFont (LnF::font (10.0f)); g.setColour (Colours_::textDim);
    g.drawFittedText (it.prompt.isEmpty() ? String ("(random)") : "\"" + it.prompt + "\"", 8, 16, w - 16, 12, Justification::centredLeft, 1);
}

void CreatePanel::listBoxItemClicked (int row, const MouseEvent& e)
{
    if (! isPositiveAndBelow (row, (int) proc.create.history.size())) return;
    const auto item = proc.create.history[(size_t) row];
    if (e.mods.isPopupMenu())
    {
        PopupMenu m;
        m.addItem (1, "Load");
        m.addItem (2, "Put the words back in the box");
        m.addItem (3, "Remove from history");
        m.addItem (4, "Clear history");
        m.showMenuAsync (PopupMenu::Options(), [this, row, item] (int r)
        {
            if (r == 1) { xost::Variation v; v.name = item.name; v.values = item.values; v.category = item.category; proc.loadVariation (v, item.prompt); }
            else if (r == 2) { prompt.text.setText (item.prompt, true); }
            else if (r == 3 && row < (int) proc.create.history.size()) { const ScopedLock sl (proc.createLock); proc.create.history.erase (proc.create.history.begin() + row); ++proc.create.counter; }
            else if (r == 4) { const ScopedLock sl (proc.createLock); proc.create.history.clear(); ++proc.create.counter; }
        });
        return;
    }
    xost::Variation v; v.name = item.name; v.values = item.values; v.category = item.category;
    proc.loadVariation (v, item.prompt);
    history.repaint();
}
