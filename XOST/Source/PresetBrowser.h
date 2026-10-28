#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// ---------------------------------------------------------------- library of factory + user presets
class PresetLibrary
{
public:
    struct Entry
    {
        juce::String name, category, description, author;
        juce::StringArray tags;
        int factoryIndex = -1;   // >= 0 for factory presets
        juce::File file;         // for user presets
        bool isUser() const { return factoryIndex < 0; }
    };

    PresetLibrary();
    void refresh();
    const juce::Array<Entry>& getEntries() const { return entries; }
    juce::StringArray getCategories() const;
    static const juce::StringArray& allTags();          // every tag (save dialog)
    static const juce::StringArray& genreTags();        // Trap, Rage, Hyperpop ...
    static const juce::StringArray& typeBeatTags();     // artist-style families (underscores shown as spaces)
    static const juce::StringArray& characterTags();    // Bright, Dark, Gritty ...
    int countInCategory (const juce::String& c) const;

    bool isFavourite (const juce::String& name) const { return favourites.contains (name); }
    void toggleFavourite (const juce::String& name);

private:
    void loadFavourites();
    void saveFavourites() const;
    juce::Array<Entry> entries;
    juce::StringArray favourites;
};

// ---------------------------------------------------------------- the multi-column preset list
class PresetGrid : public juce::Component
{
public:
    PresetGrid (PresetLibrary& l) : lib (l) {}
    void setItems (const juce::Array<int>& idx) { items = idx; updateSize(); repaint(); }
    void setCurrentName (const juce::String& n) { current = n; repaint(); }
    void setViewWidth (int w) { viewWidth = w; updateSize(); }
    const juce::Array<int>& getItems() const { return items; }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override { hoverIndex = indexAt (getMouseXYRelative()); repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hoverIndex = -1; repaint(); }
    std::function<void (int entryIndex)> onClick, onDoubleClick;
    static constexpr int rowH = 22, columns = 4;
    juce::Rectangle<int> boundsForItem (int i) const;

private:
    int indexAt (juce::Point<int>) const;
    void updateSize();
    PresetLibrary& lib;
    juce::Array<int> items;
    juce::String current;
    int viewWidth = 400, hoverIndex = -1;
};

// ---------------------------------------------------------------- save dialog
class SavePresetDialog : public juce::Component
{
public:
    SavePresetDialog (const juce::StringArray& categories);
    void open (const juce::String& suggestedName, const juce::String& category);
    void paint (juce::Graphics&) override;
    void resized() override;
    std::function<void (const juce::String& name, const juce::String& category,
                        const juce::String& tags, const juce::String& description)> onSave;
private:
    juce::TextEditor nameBox, descBox;
    juce::ComboBox categoryBox;
    juce::OwnedArray<juce::TextButton> tagButtons;
    juce::TextButton saveButton { "SAVE PRESET" }, cancelButton { "CANCEL" };
    juce::Rectangle<int> card;
};

// ---------------------------------------------------------------- browser overlay
class PresetBrowser : public juce::Component
{
public:
    PresetBrowser (XOSTProcessor&, PresetLibrary&);
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void openBrowser();
    void openSaveDialog() { saveButton.onClick(); }
    void refreshList();

    std::function<void()> onClose;

private:
    void loadEntry (int entryIndex);
    void updateInfo();
    juce::TextButton* makeChip (const juce::String& text, juce::OwnedArray<juce::TextButton>& group);
    void restoreSnapshot();

    XOSTProcessor& proc;
    PresetLibrary& lib;
    juce::TextEditor search;
    juce::OwnedArray<juce::TextButton> catChips, tagChips, showChips, genreChips, typeChips;
    juce::ComboBox sortBox;
    juce::TextButton auditionButton { "AUDITION" }, randomButton { "RANDOM" };
    void audition();
    juce::Viewport viewport;
    PresetGrid grid;
    juce::TextButton favButton { "FAVOURITE" }, saveButton { "SAVE AS" }, deleteButton { "DELETE" },
                     restoreButton { "RESTORE" }, closeButton { "CLOSE" }, refreshButton { "RESCAN" }, folderButton { "FOLDER" };
    SavePresetDialog saveDialog;
    juce::MemoryBlock snapshot;
    juce::String snapshotName;
    int selected = -1;
    juce::Rectangle<int> sidebar, listArea, infoArea, catLabel, tagLabel, showLabel, headerArea, genreLabel, typeLabel;
};
