#include "PresetBrowser.h"
#include "Presets.h"
#include "XOLookAndFeel.h"

using namespace juce;
using LnF = XOLookAndFeel;

// ================================================================== library
PresetLibrary::PresetLibrary() { loadFavourites(); refresh(); }

const StringArray& PresetLibrary::genreTags()
{
    static const StringArray t { "Trap", "Rage", "Hyperpop", "Ambient", "Plugg", "Pluggnb", "Dark", "Bouncy", "Melodic", "Phonk", "Jerk",
                                 "Drill", "Detroit", "Jersey_Club", "Cloud", "Digicore", "Sigilkore", "Emo", "RnB" };
    return t;
}
const StringArray& PresetLibrary::typeBeatTags()
{
    static const StringArray t { "Carti", "WLR", "Ken_Carson", "Destroy_Lonely", "Osamason", "Che", "Future", "Uzi", "Bleood",
                                 "Jetsonmade", "TM88", "Outtatown", "Starboy", "Richie_Souf", "Pierre_Bourne", "Clayco",
                                 "Loesoe", "Ginseng", "Yeat", "F1lthy", "Summrs", "Xaviersobased", "Luci4", "Glaive", "Drain_Gang",
                                 "Clams_Casino", "Kordhell", "Cash_Cobain", "Metro_Boomin", "Lil_Peep", "Sematary", "Sophie" };
    return t;
}
const StringArray& PresetLibrary::characterTags()
{
    static const StringArray t { "Bright", "Soft", "Gritty", "Clean", "Wide", "Detuned", "Mono", "Glassy", "Haunted",
                                 "Dreamy", "LoFi", "Chords", "Arp", "Moving", "Retro" };
    return t;
}
const StringArray& PresetLibrary::allTags()
{
    static const StringArray t = [] { StringArray a (genreTags()); a.addArray (characterTags()); return a; }();
    return t;
}
int PresetLibrary::countInCategory (const String& c) const
{
    int n = 0;
    for (auto& e : entries) if (e.category == c) ++n;
    return n;
}

void PresetLibrary::refresh()
{
    entries.clear();
    for (int i = 0; i < Presets::count(); ++i)
    {
        Entry e;
        e.name = Presets::name (i);
        e.category = Presets::category (i);
        e.description = Presets::description (i);
        e.tags = Presets::tags (i);
        e.author = "Factory";
        e.factoryIndex = i;
        entries.add (e);
    }
    auto files = XOSTProcessor::getUserFolder ("Presets").findChildFiles (File::findFiles, true, "*.xostpreset");
    files.sort();
    for (auto& f : files)
    {
        Entry e;
        e.name = f.getFileNameWithoutExtension();
        e.file = f;
        e.author = "User";
        e.category = "User";
        if (auto xml = XmlDocument::parse (f))
        {
            e.category = xml->getStringAttribute ("category", "User");
            e.description = xml->getStringAttribute ("description");
            e.tags.addTokens (xml->getStringAttribute ("tags"), " ", "");
            e.tags.removeEmptyStrings();
        }
        entries.add (e);
    }
}

StringArray PresetLibrary::getCategories() const
{
    StringArray cats;
    for (auto& e : entries) cats.addIfNotAlreadyThere (e.category);
    cats.removeString ("Init");
    return cats;
}

void PresetLibrary::toggleFavourite (const String& name)
{
    if (favourites.contains (name)) favourites.removeString (name);
    else favourites.add (name);
    saveFavourites();
}

void PresetLibrary::loadFavourites()
{
    auto f = XOSTProcessor::getUserFolder ("Presets").getParentDirectory().getChildFile ("favourites.txt");
    favourites.clear();
    if (f.existsAsFile()) favourites.addLines (f.loadFileAsString());
    favourites.removeEmptyStrings();
}

void PresetLibrary::saveFavourites() const
{
    XOSTProcessor::getUserFolder ("Presets").getParentDirectory().getChildFile ("favourites.txt")
        .replaceWithText (favourites.joinIntoString ("\n"));
}

// ================================================================== grid
void PresetGrid::updateSize()
{
    const int rows = (items.size() + columns - 1) / columns;
    setSize (viewWidth, jmax (1, rows * rowH + 8));
}

Rectangle<int> PresetGrid::boundsForItem (int i) const
{
    // fill down each column first, like a directory listing
    const int rows = jmax (1, (items.size() + columns - 1) / columns);
    const int col = i / rows, row = i % rows;
    const int colW = getWidth() / columns;
    return { col * colW + 4, row * rowH + 4, colW - 8, rowH };
}

int PresetGrid::indexAt (Point<int> p) const
{
    for (int i = 0; i < items.size(); ++i)
        if (boundsForItem (i).contains (p)) return i;
    return -1;
}

void PresetGrid::paint (Graphics& g)
{
    const auto& entries = lib.getEntries();
    for (int i = 0; i < items.size(); ++i)
    {
        const auto& e = entries.getReference (items[i]);
        auto r = boundsForItem (i);
        const bool isCurrent = e.name == current;
        if (isCurrent || i == hoverIndex)
        {
            g.setColour (isCurrent ? Colours_::red.withAlpha (0.16f) : Colours_::red.withAlpha (0.06f));
            g.fillRect (r);
            if (isCurrent) { g.setColour (Colours_::red); g.fillRect (r.getX(), r.getY(), 2, r.getHeight()); }
        }
        auto text = r.reduced (8, 0);
        if (lib.isFavourite (e.name))
        {
            g.setColour (Colours_::red);
            g.setFont (LnF::font (12.0f, true));
            g.drawText (CharPointer_UTF8 ("\xe2\x98\x85"), text.removeFromRight (14), Justification::centred);
        }
        if (e.isUser())
        {
            g.setColour (Colours_::cyan);
            g.fillEllipse ((float) text.getX(), (float) text.getCentreY() - 2.5f, 5.0f, 5.0f);
            text.removeFromLeft (10);
        }
        g.setColour (isCurrent ? Colours_::deep : Colours_::text.withAlpha (0.88f));
        g.setFont (LnF::font (14.0f, isCurrent));
        g.drawText (e.name, text, Justification::centredLeft, true);
    }
    if (items.isEmpty())
    {
        g.setColour (Colours_::textDim);
        g.setFont (LnF::font (15.0f));
        g.drawText ("No presets match these filters", getLocalBounds().withHeight (60), Justification::centred);
    }
}

void PresetGrid::mouseDown (const MouseEvent& e)
{
    const int i = indexAt (e.getPosition());
    if (i >= 0 && onClick) onClick (items[i]);
}

void PresetGrid::mouseDoubleClick (const MouseEvent& e)
{
    const int i = indexAt (e.getPosition());
    if (i >= 0 && onDoubleClick) onDoubleClick (items[i]);
}

// ================================================================== save dialog
SavePresetDialog::SavePresetDialog (const StringArray& categories)
{
    for (auto* te : { &nameBox, &descBox })
    {
        te->setFont (LnF::font (15.0f));
        te->setIndents (8, 6);
        addAndMakeVisible (*te);
    }
    descBox.setMultiLine (true, true);
    descBox.setReturnKeyStartsNewLine (true);
    nameBox.setTextToShowWhenEmpty ("Preset name", Colours_::textDim);
    descBox.setTextToShowWhenEmpty ("Describe the sound (optional)", Colours_::textDim);

    StringArray cats (categories);
    cats.addIfNotAlreadyThere ("User");
    categoryBox.addItemList (cats, 1);
    addAndMakeVisible (categoryBox);

    for (auto& t : PresetLibrary::allTags())
    {
        auto* b = tagButtons.add (new TextButton (t));
        b->setClickingTogglesState (true);
        addAndMakeVisible (b);
    }
    for (auto* b : { &saveButton, &cancelButton }) addAndMakeVisible (*b);
    saveButton.onClick = [this]
    {
        auto name = File::createLegalFileName (nameBox.getText().trim());
        if (name.isEmpty()) { nameBox.grabKeyboardFocus(); return; }
        StringArray tags;
        for (auto* b : tagButtons) if (b->getToggleState()) tags.add (b->getButtonText());
        setVisible (false);
        if (onSave) onSave (name, categoryBox.getText(), tags.joinIntoString (" "), descBox.getText().trim());
    };
    cancelButton.onClick = [this] { setVisible (false); };
}

void SavePresetDialog::open (const String& suggestedName, const String& category)
{
    nameBox.setText (suggestedName, false);
    descBox.clear();
    for (auto* b : tagButtons) b->setToggleState (false, dontSendNotification);
    const int idx = categoryBox.indexOfItemId (1);
    juce::ignoreUnused (idx);
    for (int i = 0; i < categoryBox.getNumItems(); ++i)
        if (categoryBox.getItemText (i) == category) categoryBox.setSelectedItemIndex (i, dontSendNotification);
    if (categoryBox.getSelectedId() == 0) categoryBox.setText ("User", dontSendNotification);
    setVisible (true);
    toFront (true);
    nameBox.grabKeyboardFocus();
    nameBox.selectAll();
}

void SavePresetDialog::resized()
{
    card = getLocalBounds().withSizeKeepingCentre (460, 400);
    auto r = card.reduced (24).withTrimmedTop (40);
    nameBox.setBounds (r.removeFromTop (32));
    r.removeFromTop (22);
    categoryBox.setBounds (r.removeFromTop (28).withWidth (200));
    r.removeFromTop (24);
    auto tagArea = r.removeFromTop (96);
    int x = tagArea.getX(), y = tagArea.getY();
    for (auto* b : tagButtons)
    {
        const int w = (int) GlyphArrangement::getStringWidth (LnF::font (13.5f, true), b->getButtonText()) + 22;
        if (x + w > tagArea.getRight()) { x = tagArea.getX(); y += 30; }
        b->setBounds (x, y, w, 24);
        x += w + 6;
    }
    r.removeFromTop (6);
    descBox.setBounds (r.removeFromTop (60));
    r.removeFromTop (14);
    auto buttons = r.removeFromTop (32);
    saveButton.setBounds (buttons.removeFromRight (140));
    buttons.removeFromRight (8);
    cancelButton.setBounds (buttons.removeFromRight (100));
}

void SavePresetDialog::paint (Graphics& g)
{
    g.fillAll (Colours::black.withAlpha (0.6f));
    auto shape = LnF::chamfer (card.toFloat(), 10.0f);
    g.setColour (Colours_::panel);
    g.fillPath (shape);
    g.setColour (Colours_::red);
    g.strokePath (shape, PathStrokeType (1.2f));
    g.setFont (LnF::title (15.0f));
    g.setColour (Colours_::text);
    g.drawText ("SAVE PRESET", card.getX() + 24, card.getY() + 18, 300, 22, Justification::centredLeft);
    g.setFont (LnF::font (11.5f, true));
    g.setColour (Colours_::textDim);
    g.drawText ("CATEGORY", nameBox.getX(), nameBox.getBottom() + 5, 200, 16, Justification::centredLeft);
    g.drawText ("TAGS", categoryBox.getX(), categoryBox.getBottom() + 6, 200, 16, Justification::centredLeft);
}

// ================================================================== browser
PresetBrowser::PresetBrowser (XOSTProcessor& p, PresetLibrary& l)
    : proc (p), lib (l), grid (l), saveDialog (l.getCategories())
{
    setWantsKeyboardFocus (true);
    search.setTextToShowWhenEmpty ("Search presets, tags, descriptions...", Colours_::textDim);
    search.setFont (LnF::font (15.0f));
    search.setIndents (10, 6);
    search.onTextChange = [this] { refreshList(); };
    search.onEscapeKey = [this] { search.clear(); refreshList(); };
    addAndMakeVisible (search);

    for (auto& s : { "ALL", "FAVOURITES", "FACTORY", "MINE" })
    {
        auto* b = makeChip (s, showChips);
        b->setRadioGroupId (77);
    }
    showChips[0]->setToggleState (true, dontSendNotification);
    for (auto& c : lib.getCategories()) makeChip (c, catChips)->setTooltip (String (lib.countInCategory (c)) + " presets");
    for (auto& t : PresetLibrary::genreTags()) makeChip (t.replaceCharacter ('_', ' '), genreChips);
    for (auto& t : PresetLibrary::typeBeatTags()) makeChip (t.replaceCharacter ('_', ' '), typeChips)->setTooltip ("Sounds for " + t.replaceCharacter ('_', ' ') + "-style beats (our interpretation)");
    for (auto& t : PresetLibrary::characterTags()) makeChip (t, tagChips);
    sortBox.addItemList ({ "Sort: Category", "Sort: Name A-Z", "Sort: Name Z-A", "Sort: Shuffle" }, 1);
    sortBox.setSelectedId (1, dontSendNotification);
    sortBox.onChange = [this] { refreshList(); };
    addAndMakeVisible (sortBox);
    auditionButton.setClickingTogglesState (true);
    auditionButton.setToggleState (true, dontSendNotification);
    auditionButton.setTooltip ("Play a short note whenever you click a preset");
    randomButton.setTooltip ("Load a random preset from the current list");
    randomButton.onClick = [this]
    {
        const auto& items = grid.getItems();
        if (! items.isEmpty()) loadEntry (items[Random::getSystemRandom().nextInt (items.size())]);
    };
    addAndMakeVisible (auditionButton);
    addAndMakeVisible (randomButton);

    viewport.setViewedComponent (&grid, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);
    grid.onClick = [this] (int i) { loadEntry (i); };
    grid.onDoubleClick = [this] (int i) { loadEntry (i); if (onClose) onClose(); };

    for (auto* b : { &favButton, &saveButton, &deleteButton, &restoreButton, &closeButton, &refreshButton, &folderButton })
        addAndMakeVisible (*b);
    favButton.setClickingTogglesState (false);
    favButton.onClick = [this]
    {
        if (! isPositiveAndBelow (selected, lib.getEntries().size())) return;
        lib.toggleFavourite (lib.getEntries()[selected].name);
        updateInfo();
        refreshList();
    };
    saveButton.onClick = [this]
    {
        const auto& entries = lib.getEntries();
        const String cat = isPositiveAndBelow (selected, entries.size()) ? entries[selected].category : String ("User");
        saveDialog.open (proc.getCurrentPresetName(), cat);
    };
    deleteButton.onClick = [this]
    {
        if (! isPositiveAndBelow (selected, lib.getEntries().size())) return;
        auto e = lib.getEntries()[selected];
        if (! e.isUser()) return;
        SafePointer<PresetBrowser> safe (this);
        AlertWindow::showOkCancelBox (MessageBoxIconType::QuestionIcon, "Delete preset",
                                      "Move \"" + e.name + "\" to the trash?", "Delete", "Cancel", this,
                                      ModalCallbackFunction::create ([safe, e] (int result)
        {
            if (safe == nullptr || result == 0) return;
            e.file.moveToTrash();
            safe->lib.refresh();
            safe->selected = -1;
            safe->refreshList();
            safe->updateInfo();
        }));
    };
    restoreButton.onClick = [this] { restoreSnapshot(); };
    closeButton.onClick = [this] { if (onClose) onClose(); };
    refreshButton.onClick = [this] { lib.refresh(); refreshList(); };
    folderButton.onClick = [] { XOSTProcessor::getUserFolder ("Presets").startAsProcess(); };
    restoreButton.setTooltip ("Go back to the sound you had when you opened the browser");

    saveDialog.onSave = [this] (const String& name, const String& cat, const String& tags, const String& desc)
    {
        auto file = XOSTProcessor::getUserFolder ("Presets").getChildFile (name + ".xostpreset");
        if (proc.saveUserPreset (file, cat, tags, desc))
        {
            lib.refresh();
            refreshList();
            for (int i = 0; i < lib.getEntries().size(); ++i)
                if (lib.getEntries()[i].file == file) selected = i;
            updateInfo();
        }
        else
            AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "XOST", "Couldn't save the preset to:\n" + file.getFullPathName());
    };
    addChildComponent (saveDialog);
    refreshList();
}

TextButton* PresetBrowser::makeChip (const String& text, OwnedArray<TextButton>& group)
{
    auto* b = group.add (new TextButton (text));
    b->setClickingTogglesState (true);
    b->onClick = [this] { refreshList(); };
    addAndMakeVisible (b);
    return b;
}

void PresetBrowser::openBrowser()
{
    proc.getStateInformation (snapshot);
    snapshotName = proc.getCurrentPresetName();
    lib.refresh();
    refreshList();
    const auto& entries = lib.getEntries();
    selected = -1;
    for (int i = 0; i < entries.size(); ++i)
        if (entries[i].name == proc.getCurrentPresetName()) selected = i;
    updateInfo();
    setVisible (true);
    toFront (true);
    grabKeyboardFocus();
}

void PresetBrowser::restoreSnapshot()
{
    if (snapshot.getSize() > 0)
        proc.setStateInformation (snapshot.getData(), (int) snapshot.getSize());
    grid.setCurrentName (proc.getCurrentPresetName());
    updateInfo();
}

void PresetBrowser::refreshList()
{
    const auto& entries = lib.getEntries();
    const auto query = search.getText().trim().toLowerCase();
    StringArray cats, tags, anyOf;
    for (auto* b : catChips) if (b->getToggleState()) cats.add (b->getButtonText());
    for (auto* b : tagChips) if (b->getToggleState()) tags.add (b->getButtonText());
    for (auto* b : genreChips) if (b->getToggleState()) tags.add (b->getButtonText().replaceCharacter (' ', '_'));
    for (auto* b : typeChips) if (b->getToggleState()) anyOf.add (b->getButtonText().replaceCharacter (' ', '_'));
    int show = 0;
    for (int i = 0; i < showChips.size(); ++i) if (showChips[i]->getToggleState()) show = i;

    Array<int> items;
    for (int i = 0; i < entries.size(); ++i)
    {
        const auto& e = entries.getReference (i);
        if (show == 1 && ! lib.isFavourite (e.name)) continue;
        if (show == 2 && e.isUser()) continue;
        if (show == 3 && ! e.isUser()) continue;
        if (! cats.isEmpty() && ! cats.contains (e.category)) continue;
        bool ok = true;
        for (auto& t : tags) ok = ok && e.tags.contains (t);
        if (! anyOf.isEmpty())
        {
            bool hit = false;
            for (auto& t : anyOf) hit = hit || e.tags.contains (t);
            ok = ok && hit;
        }
        if (! ok) continue;
        if (query.isNotEmpty())
        {
            const auto hay = (e.name + " " + e.category + " " + e.tags.joinIntoString (" ") + " " + e.description).toLowerCase();
            StringArray words; words.addTokens (query, " ", "");
            for (auto& w : words) ok = ok && (w.isEmpty() || hay.contains (w));
            if (! ok) continue;
        }
        items.add (i);
    }
    const int sort = sortBox.getSelectedId();
    if (sort == 2 || sort == 3)
    {
        std::sort (items.begin(), items.end(), [&entries, sort] (int a, int b)
        {
            const int c = entries.getReference (a).name.compareIgnoreCase (entries.getReference (b).name);
            return sort == 2 ? c < 0 : c > 0;
        });
    }
    else if (sort == 4)
    {
        Random r;
        for (int i = items.size() - 1; i > 0; --i) items.swap (i, r.nextInt (i + 1));
    }
    grid.setItems (items);
    grid.setCurrentName (proc.getCurrentPresetName());
    repaint (headerArea);
}

void PresetBrowser::loadEntry (int i)
{
    const auto& entries = lib.getEntries();
    if (! isPositiveAndBelow (i, entries.size())) return;
    selected = i;
    const auto& e = entries.getReference (i);
    if (e.isUser()) proc.loadUserPreset (e.file);
    else proc.setCurrentProgram (e.factoryIndex);
    grid.setCurrentName (e.name);
    updateInfo();
    if (auditionButton.getToggleState()) audition();
}

void PresetBrowser::audition()
{
    proc.keyboardState.noteOn (1, 60, 0.85f);
    SafePointer<PresetBrowser> safe (this);
    Timer::callAfterDelay (650, [safe] { if (safe != nullptr) safe->proc.keyboardState.noteOff (1, 60, 0.0f); });
}

void PresetBrowser::updateInfo()
{
    const bool has = isPositiveAndBelow (selected, lib.getEntries().size());
    deleteButton.setEnabled (has && lib.getEntries()[selected].isUser());
    favButton.setEnabled (has);
    favButton.setToggleState (has && lib.isFavourite (lib.getEntries()[selected].name), dontSendNotification);
    favButton.setButtonText (favButton.getToggleState() ? CharPointer_UTF8 ("\xe2\x98\x85 FAVOURITE") : CharPointer_UTF8 ("\xe2\x98\x86 FAVOURITE"));
    repaint (infoArea);
}

bool PresetBrowser::keyPressed (const KeyPress& k)
{
    const auto& items = grid.getItems();
    if (items.isEmpty()) return false;
    int pos = items.indexOf (selected);
    const int rows = jmax (1, (items.size() + PresetGrid::columns - 1) / PresetGrid::columns);
    if (k == KeyPress::downKey)       pos = jmin (items.size() - 1, pos + 1);
    else if (k == KeyPress::upKey)    pos = jmax (0, pos - 1);
    else if (k == KeyPress::rightKey) pos = jmin (items.size() - 1, pos + rows);
    else if (k == KeyPress::leftKey)  pos = jmax (0, pos - rows);
    else if (k == KeyPress::returnKey || k == KeyPress::escapeKey) { if (onClose) onClose(); return true; }
    else return false;
    loadEntry (items[jmax (0, pos)]);
    viewport.setViewPosition (0, jmax (0, grid.boundsForItem (jmax (0, pos)).getY() - viewport.getHeight() / 2));
    return true;
}

void PresetBrowser::resized()
{
    auto r = getLocalBounds().reduced (12);
    headerArea = r.removeFromTop (34);
    closeButton.setBounds (headerArea.getRight() - 90, headerArea.getY() + 3, 90, 28);
    randomButton.setBounds (headerArea.getRight() - 190, headerArea.getY() + 3, 90, 28);
    auditionButton.setBounds (headerArea.getRight() - 296, headerArea.getY() + 3, 100, 28);
    sortBox.setBounds (headerArea.getRight() - 452, headerArea.getY() + 3, 150, 28);
    r.removeFromTop (8);

    sidebar = r.removeFromLeft (300);
    infoArea = r.removeFromRight (270);
    r.removeFromLeft (12); r.removeFromRight (12);
    listArea = r;

    auto s = sidebar.reduced (12);
    search.setBounds (s.removeFromTop (32));
    s.removeFromTop (14);
    auto flow = [&s] (OwnedArray<TextButton>& chips, Rectangle<int>& label)
    {
        label = s.removeFromTop (18);
        s.removeFromTop (4);
        int x = s.getX(), y = s.getY();
        for (auto* b : chips)
        {
            const int w = (int) GlyphArrangement::getStringWidth (LnF::font (12.5f, true), b->getButtonText()) + 14;
            if (x + w > s.getRight()) { x = s.getX(); y += 24; }
            b->setBounds (x, y, w, 21);
            x += w + 4;
        }
        const int used = y + 24 - s.getY();
        s.removeFromTop (used + 6);
    };
    flow (showChips, showLabel);
    flow (catChips, catLabel);
    flow (genreChips, genreLabel);
    flow (typeChips, typeLabel);
    flow (tagChips, tagLabel);

    viewport.setBounds (listArea.reduced (2));
    grid.setViewWidth (viewport.getWidth() - viewport.getScrollBarThickness());

    auto info = infoArea.reduced (16);
    auto buttons = info.removeFromBottom (32 * 3 + 16);
    auto row1 = buttons.removeFromTop (32);
    favButton.setBounds (row1.removeFromLeft (row1.getWidth() / 2 - 3)); row1.removeFromLeft (6); saveButton.setBounds (row1);
    buttons.removeFromTop (8);
    auto row2 = buttons.removeFromTop (32);
    restoreButton.setBounds (row2.removeFromLeft (row2.getWidth() / 2 - 3)); row2.removeFromLeft (6); deleteButton.setBounds (row2);
    buttons.removeFromTop (8);
    auto row3 = buttons.removeFromTop (32);
    refreshButton.setBounds (row3.removeFromLeft (row3.getWidth() / 2 - 3)); row3.removeFromLeft (6); folderButton.setBounds (row3);

    saveDialog.setBounds (getLocalBounds());
}

void PresetBrowser::paint (Graphics& g)
{
    g.fillAll (Colours_::bg);

    // header
    g.setFont (LnF::title (16.0f));
    g.setColour (Colours_::text);
    g.drawText ("PRESET LIBRARY", headerArea.withTrimmedLeft (4), Justification::centredLeft);
    g.setFont (LnF::font (14.0f, true));
    g.setColour (Colours_::cyan);
    g.drawText (String (grid.getItems().size()) + " / " + String (lib.getEntries().size()) + " PRESETS",
                headerArea.withTrimmedLeft (230), Justification::centredLeft);
    g.setColour (Colours_::red);
    g.fillRect (headerArea.getX(), headerArea.getBottom() + 2, headerArea.getWidth(), 1);

    for (auto area : { sidebar, listArea, infoArea })
    {
        auto shape = LnF::chamfer (area.toFloat(), 8.0f);
        g.setColour (Colours_::panel);
        g.fillPath (shape);
        g.setColour (Colours_::panelEdge);
        g.strokePath (shape, PathStrokeType (1.0f));
    }

    g.setFont (LnF::font (12.0f, true));
    g.setColour (Colours_::textDim);
    g.drawText ("SHOW", showLabel, Justification::centredLeft);
    g.drawText ("CATEGORIES", catLabel, Justification::centredLeft);
    g.drawText ("CHARACTER", tagLabel, Justification::centredLeft);
    g.drawText ("GENRE", genreLabel, Justification::centredLeft);
    g.drawText ("TYPE BEAT", typeLabel, Justification::centredLeft);
    for (auto lbl : { showLabel, catLabel, tagLabel, genreLabel, typeLabel })
    {
        g.setColour (Colours_::panelEdge);
        g.fillRect (lbl.getX() + 80, lbl.getCentreY(), lbl.getWidth() - 80, 1);
    }

    // info panel
    auto info = infoArea.reduced (16);
    g.setFont (LnF::font (12.0f, true));
    g.setColour (Colours_::textDim);
    g.drawText ("PRESET INFO", info.removeFromTop (18), Justification::centredLeft);
    info.removeFromTop (8);
    if (isPositiveAndBelow (selected, lib.getEntries().size()))
    {
        const auto& e = lib.getEntries().getReference (selected);
        g.setFont (LnF::title (15.0f));
        g.setColour (Colours_::red);
        g.drawFittedText (e.name.toUpperCase(), info.removeFromTop (40), Justification::topLeft, 2);
        g.setFont (LnF::font (14.0f, true));
        g.setColour (Colours_::text);
        g.drawText ("in " + e.category, info.removeFromTop (20), Justification::centredLeft);
        g.drawText ("by " + e.author, info.removeFromTop (20), Justification::centredLeft);
        info.removeFromTop (14);
        g.setColour (Colours_::textDim);
        g.setFont (LnF::font (12.0f, true));
        g.drawText ("DESCRIPTION", info.removeFromTop (16), Justification::centredLeft);
        g.setColour (Colours_::text.withAlpha (0.85f));
        g.setFont (LnF::font (14.0f));
        g.drawFittedText (e.description.isEmpty() ? String ("-") : e.description, info.removeFromTop (92), Justification::topLeft, 6);
        info.removeFromTop (8);
        g.setColour (Colours_::textDim);
        g.setFont (LnF::font (12.0f, true));
        g.drawText ("CHARACTER", info.removeFromTop (16), Justification::centredLeft);
        int x = info.getX(), y = info.getY() + 4;
        g.setFont (LnF::font (12.5f, true));
        for (auto& t : e.tags)
        {
            const int w = (int) GlyphArrangement::getStringWidth (g.getCurrentFont(), t.replaceCharacter ('_', ' ')) + 14;
            if (x + w > info.getRight()) { x = info.getX(); y += 24; }
            auto chip = Rectangle<float> ((float) x, (float) y, (float) w, 19.0f);
            g.setColour (Colours_::cyan.withAlpha (0.12f)); g.fillRoundedRectangle (chip, 3.0f);
            g.setColour (Colours_::cyan); g.drawRoundedRectangle (chip, 3.0f, 1.0f);
            g.drawText (t.replaceCharacter ('_', ' '), chip.toNearestInt(), Justification::centred);
            x += w + 5;
        }
    }
    else
    {
        g.setColour (Colours_::textDim);
        g.setFont (LnF::font (14.0f));
        g.drawFittedText ("Click a preset to hear it. Arrow keys step through the list, Enter closes the browser.",
                          info.removeFromTop (80), Justification::topLeft, 4);
    }
}
