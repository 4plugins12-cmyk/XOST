#pragma once

#include "Page.h"
#include "PresetBrowser.h"
#include "ToolPages.h"
#include "CreatePanel.h"

class OscPage : public Page   { public: explicit OscPage (XOSTProcessor&); void resized() override; void tick() override; };
class ModPage : public Page   { public: explicit ModPage (XOSTProcessor&); void resized() override; void tick() override; };
class RoutingPage : public Page{ public: explicit RoutingPage (XOSTProcessor&); void resized() override; };
class FxPage : public Page    { public: explicit FxPage (XOSTProcessor&); void resized() override; void tick() override; };
class ArpPage : public Page   { public: explicit ArpPage (XOSTProcessor&); void resized() override; void tick() override; };

class MacroStrip : public Page
{
public:
    explicit MacroStrip (XOSTProcessor&);
    void resized() override;
    void tick() override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
private:
    juce::TextEditor nameEditor;
    int editing = -1;
};

class BounceChip : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    juce::File file;
};

class Content : public juce::Component, public juce::DragAndDropContainer, private juce::Timer
{
public:
    explicit Content (XOSTProcessor&);
    ~Content() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    static constexpr int W = 1600, H = 934;
private:
    void timerCallback() override;
    void showPage (int);
    void stepPreset (int);
    void showRandomMenu();
    void showBounceMenu();
    void showHistory();
    void updatePresetLabel();

    XOSTProcessor& proc;
    PresetLibrary library;
    OscPage oscPage { proc }; ModPage modPage { proc }; RoutingPage routingPage { proc }; FxPage fxPage { proc }; ArpPage arpPage { proc };
    MatrixPage matrixPage { proc }; RackPage rackPage { proc }; MutatePage mutatePage { proc, library };
    MacroStrip macros { proc };
    Scope scope { proc };
    PromptBar promptBar { proc };
    OneShotBox oneShotBox { proc };
    CreatePanel createPanel { proc, library, promptBar };
    ToneList& toneList = createPanel.toneList;
    juce::MidiKeyboardComponent keyboard { proc.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard };
    static constexpr int numPages = 8;
    std::array<Page*, numPages> pages { &oscPage, &modPage, &routingPage, &fxPage, &arpPage, &matrixPage, &rackPage, &mutatePage };
    juce::OwnedArray<juce::TextButton> tabs;
    int page = 0;

    juce::TextButton presetButton, prevButton { "<" }, nextButton { ">" }, browseButton { "BROWSE" }, saveButton { "SAVE" },
                     initButton { "INIT" }, diceButton { "RANDOM" }, lockButton { "LOCKS" }, undoButton { "UNDO" }, redoButton { "REDO" },
                     historyButton { "HISTORY" }, bounceButton { "BOUNCE" }, scaleButton { "100%" };
    std::unique_ptr<ChoiceBox> quality;
    std::unique_ptr<Knob> master;
    BounceChip bounceChip;
    PresetBrowser browser { proc, library };
    juce::String presetCategory;
    int shownCounter = -1, frame = 0;
    float matrixHash = -1.0f;
};

class XOSTEditor : public juce::AudioProcessorEditor
{
public:
    explicit XOSTEditor (XOSTProcessor&);
    ~XOSTEditor() override;
    void resized() override;
    void setScale (double s);
private:
    XOSTProcessor& proc;
    XOLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 700 };
    Content content { proc };
};
