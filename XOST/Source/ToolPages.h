#pragma once
// XO's extra pages: MATRIX (drawable gate / filter / pan / time lanes), MINIRACK (eurorack-style modulators),
// MUTATE (Synplant-style gene field, breeding and morphing), plus the always-visible TONE LIST.

#include "Page.h"
#include "PresetBrowser.h"

// ------------------------------------------------------------------ small helpers
class ToolPresetBar : public juce::Component
{
public:
    ToolPresetBar (XOSTProcessor&, XOSTProcessor::Scope);
    void paint (juce::Graphics&) override;
    void resized() override;
    void step (int d);
private:
    void showMenu();
    juce::Array<juce::File> userFiles() const;
    XOSTProcessor& proc;
    XOSTProcessor::Scope scope;
    juce::TextButton nameButton, prev { "<" }, next { ">" }, save { "SAVE" };
    int index = -1;
    std::unique_ptr<juce::FileChooser> chooser;
};

// ------------------------------------------------------------------ MATRIX
class LaneEditor : public juce::Component
{
public:
    explicit LaneEditor (XOSTProcessor& p) : proc (p) {}
    void setLane (int l) { lane = l; repaint(); }
    int getLane() const { return lane; }
    void setGrid (int cellsPerBar) { grid = cellsPerBar; repaint(); }
    void setDrawMode (bool d) { drawMode = d; }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    int cells() const;
private:
    juce::Rectangle<float> area() const { return getLocalBounds().toFloat().reduced (10.0f, 12.0f); }
    juce::Point<float> toPt (const xo::Breakpoint& b) const;
    xo::Breakpoint fromPos (juce::Point<float>) const;
    int hit (juce::Point<float>) const;
    void paintCell (juce::Point<float>);
    XOSTProcessor& proc;
    int lane = 0, grid = 16, dragIndex = -1, hover = -1, curveSeg = -1;
    bool drawMode = false;
    float curveStartY = 0, curveStartVal = 0;
    std::vector<xo::Breakpoint> before, work;
};

class MatrixPage : public Page
{
public:
    explicit MatrixPage (XOSTProcessor&);
    void resized() override;
    void tick() override;
    void paint (juce::Graphics&) override;
private:
    void selectLane (int l);
    void applyShape (int shape);
    juce::OwnedArray<juce::TextButton> laneTabs;
    LaneEditor editor { proc };
    juce::ComboBox gridBox;
    juce::TextButton drawButton { "DRAW" }, shapesButton { "SHAPES" }, invertButton { "INVERT" }, shiftButton { "SHIFT >" },
                     clearButton { "CLEAR" }, mirrorButton { "REVERSE" };
    ToolPresetBar presets { proc, XOSTProcessor::Scope::matrix };
    int lastCounter = -1;
};

// ------------------------------------------------------------------ MINIRACK
class RackModule : public juce::Component
{
public:
    RackModule (XOSTProcessor&, int index);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();
    juce::Point<float> jackPos (int out) const;   // in module coordinates
    int index;
private:
    void updateLabels();
    XOSTProcessor& proc;
    std::unique_ptr<ChoiceBox> type, div, dst1, dst2;
    std::unique_ptr<Knob> rate, a, b, amt1, amt2;
    std::unique_ptr<PowerButton> sync;
    std::array<float, 96> history {};
    int head = 0, lastType = -1;
};

class RackPage : public Page
{
public:
    explicit RackPage (XOSTProcessor&);
    void resized() override;
    void tick() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
private:
    juce::OwnedArray<RackModule> modules;
    ToolPresetBar presets { proc, XOSTProcessor::Scope::rack };
    juce::Rectangle<int> bay;
};

// ------------------------------------------------------------------ MUTATE
class GeneField : public juce::Component, private juce::Timer
{
public:
    explicit GeneField (XOSTProcessor&);
    ~GeneField() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void newSeed();
    void plant();
    void recentre();
    void setDepth (float d) { depth = d; }
    float getDepth() const  { return depth; }
private:
    struct Gene { juce::RangedAudioParameter* param; bool discrete; int numSteps; int section; juce::RangedAudioParameter* gate = nullptr; };
    struct Branch { std::vector<float> delta; std::vector<float> jump; float curl; };
    void timerCallback() override;
    void collectGenes();
    void captureBase();
    bool userChangedSomething() const;
    void applyAt (juce::Point<float> p);
    juce::Point<float> toField (juce::Point<float> local) const;
    juce::Point<float> toLocal (juce::Point<float> field) const;
    void buildBranchPaths();
    XOSTProcessor& proc;
    std::vector<Gene> genes;
    std::vector<float> base, lastApplied;
    std::vector<Branch> branches;
    std::vector<juce::Path> branchPaths;
    uint32_t seed = 1;
    float depth = 0.6f;
    juce::Point<float> pos, hover;
    bool hovering = false, dragging = false, hasBase = false;
    std::array<juce::Point<float>, 24> trail {};
    int trailHead = 0, lastPresetCounter = -1;
    float pulse = 0.0f;
    juce::Rectangle<float> field;
};

class MutatePage : public Page
{
public:
    MutatePage (XOSTProcessor&, PresetLibrary&);
    void resized() override;
    void tick() override;
    void paint (juce::Graphics&) override;
private:
    void choosePartner();
    void setPartner (int factoryIndex);
    void randomPartner();
    void updateLocks();
    PresetLibrary& lib;
    GeneField gene { proc };
    juce::TextButton plantButton { "PLANT" }, centreButton { "CENTRE" }, seedButton { "NEW SEED" };
    juce::Slider depthSlider, mixSlider;
    juce::TextButton subtle { "SUBTLE" }, medium { "MEDIUM" }, wild { "WILD" };
    juce::TextButton partnerButton { "CHOOSE PARTNER..." }, randomPartnerButton { "RANDOM" }, breedButton { "BREED" }, morphButton { "LIVE MORPH" };
    juce::ComboBox categoryBox;
    juce::TextButton fromCategoryButton { "LOAD + MUTATE" }, backButton { "BACK" }, forwardButton { "FORWARD" };
    juce::OwnedArray<juce::TextButton> lockChips;
    int partner = -1;
    juce::String baseValues;
};

// ------------------------------------------------------------------ TONE LIST (always visible, left)
class ToneList : public juce::Component, private juce::ListBoxModel
{
public:
    ToneList (XOSTProcessor&, PresetLibrary&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    void syncSelection();
    std::function<void()> onLoaded;
private:
    int getNumRows() override { return items.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void returnKeyPressed (int row) override { selectedRowsChanged (row); }
    void selectedRowsChanged (int row) override;
    XOSTProcessor& proc;
    PresetLibrary& lib;
    juce::TextEditor search;
    juce::ComboBox category, vibe;
    juce::ListBox list { "tones", this };
    juce::Array<int> items;
    bool loading = false;
};
