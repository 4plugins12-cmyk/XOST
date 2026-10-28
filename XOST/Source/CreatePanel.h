#pragma once
// The text-to-sound UI: the prompt bar across the top, the one-shot exporter, and the CREATE column on the left
// (variations, quick tweaks, history - with the preset list on its second tab).
#include "Widgets.h"
#include "ToolPages.h"
#include "WordEngine.h"

class ChipRow : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void setChips (std::vector<xost::Chip> c) { chips = std::move (c); repaint(); }
    juce::String message;
    bool busy = false;
    int frame = 0;
private:
    std::vector<xost::Chip> chips;
};

class PromptBar : public juce::Component, private juce::TextEditor::Listener
{
public:
    explicit PromptBar (XOSTProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void generate (bool again = false, const juce::String& preferKind = {});
    void tweak (const juce::String& words);
    void surprise();
    void tick (int frame);
    void syncFromProcessor();
    juce::TextEditor text;
    ChipRow chips;
    std::function<void (const xost::Generator::Result&)> onResult;
private:
    void textEditorTextChanged (juce::TextEditor&) override;
    void textEditorReturnKeyPressed (juce::TextEditor&) override { generate(); }
    void submit (const juce::String& prompt, bool refine, const juce::String& preferKind);
    XOSTProcessor& proc;
    juce::TextButton generateButton { "GENERATE" }, surpriseButton { "SURPRISE" }, tweakToggle { "TWEAK" }, helpButton { "?" };
    juce::uint32 seed = 1;
    int pending = 0;
    juce::Random rng;
};

class OneShotBox : public juce::Component
{
public:
    explicit OneShotBox (XOSTProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDrag (const juce::MouseEvent&) override;
    void exportNow();
private:
    void showOptions();
    XOSTProcessor& proc;
    juce::TextButton exportButton { "EXPORT ONE-SHOT" }, optionsButton { "OPTIONS" }, folderButton { "FOLDER" };
    juce::Rectangle<int> chipArea;
    juce::String shownRoot;
};

class VariationCard : public juce::Component
{
public:
    VariationCard (XOSTProcessor& p, int i) : proc (p), index (i) {}
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    std::function<void (int, bool audition)> onPick;
private:
    XOSTProcessor& proc;
    const int index;
};

class CreatePanel : public juce::Component, private juce::ListBoxModel
{
public:
    CreatePanel (XOSTProcessor&, PresetLibrary&, PromptBar&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();
    void showTab (int);
    void pick (int index, bool audition);
    ToneList toneList;
private:
    int getNumRows() override { return (int) proc.create.history.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void audition();
    XOSTProcessor& proc;
    PromptBar& prompt;
    juce::TextButton createTab { "CREATE" }, presetsTab { "PRESETS" }, againButton { "GENERATE AGAIN" }, moreButton { "MORE LIKE THIS" };
    juce::OwnedArray<VariationCard> cards;
    juce::OwnedArray<juce::TextButton> tweaks;
    juce::ListBox history { "history", this };
    int tab = 0, shownCounter = -1, auditionNote = -1;
    juce::uint32 auditionUntil = 0;
};
