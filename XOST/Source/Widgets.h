#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "XOLookAndFeel.h"

// ------------------------------------------------------------------ helpers
namespace ui
{
    using LnF = XOLookAndFeel;
    // adds a modulation routing in the first free matrix slot
    void addModulation (XOSTProcessor&, int source, int dest);
    float modAmountFor (XOSTProcessor&, int dest);   // sum of amounts routed to dest
}

class LearnSlider : public juce::Slider
{
public:
    std::function<void()> onPopup, onGestureStart;
    void mouseDown (const juce::MouseEvent& e) override
    {
        popupClick = e.mods.isPopupMenu();
        if (popupClick) { if (onPopup) onPopup(); return; }
        if (onGestureStart) onGestureStart();
        Slider::mouseDown (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override { if (! popupClick) Slider::mouseDrag (e); }
    void mouseUp (const juce::MouseEvent& e) override   { if (! popupClick) Slider::mouseUp (e); popupClick = false; }
private:
    bool popupClick = false;
};

// Knob with label, value readout, modulation ring and drag-and-drop modulation target
class Knob : public juce::Component, public juce::DragAndDropTarget
{
public:
    enum class Style { normal, mini, big, compact };
    Knob (XOSTProcessor&, const juce::String& paramID, const juce::String& label, Style = Style::normal);
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { dragOver = true; repaint(); }
    void itemDragExit (const SourceDetails&) override  { dragOver = false; repaint(); }
    void itemDropped (const SourceDetails&) override;

    LearnSlider slider;
    void setLabel (const juce::String& l) { label = l; repaint(); }
    const juce::String paramID;
    const int dest;
private:
    void showMenu();
    XOSTProcessor& proc;
    juce::String label;
    Style style;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    bool dragOver = false;
};

class ChoiceBox : public juce::Component
{
public:
    ChoiceBox (XOSTProcessor&, const juce::String& paramID, const juce::String& caption);
    ~ChoiceBox() override { box.removeMouseListener (beginListener.get()); }
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::ComboBox box;
    const juce::String paramID;
private:
    juce::String caption;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    std::unique_ptr<juce::MouseListener> beginListener;
};

class PowerButton : public juce::ToggleButton
{
public:
    PowerButton (XOSTProcessor&, const juce::String& paramID, const juce::String& text = {});
    ~PowerButton() override { removeMouseListener (beginListener.get()); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    std::unique_ptr<juce::MouseListener> beginListener;
};

// draggable modulation source (drop it on any knob)
class ModSourceChip : public juce::Component
{
public:
    ModSourceChip (XOSTProcessor& p, int source, juce::String text) : proc (p), src (source), label (std::move (text))
    { setMouseCursor (juce::MouseCursor::DraggingHandCursor); setTooltip ("Drag onto a knob to modulate it"); }
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void setTooltip (const juce::String& t) { tip = t; }
    juce::String getTooltip() { return tip; }
private:
    XOSTProcessor& proc;
    int src;
    juce::String label, tip;
};

// oscillator wave view; drop audio files here to make a wavetable
class WaveDisplay : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    WaveDisplay (XOSTProcessor& p, int o) : proc (p), osc (o) {}
    void paint (juce::Graphics&) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray& files, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { hover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { hover = false; repaint(); }
private:
    XOSTProcessor& proc;
    int osc;
    bool hover = false;
    std::vector<float> frame;
};

class SampleDisplay : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    explicit SampleDisplay (XOSTProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray& files, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { hover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { hover = false; repaint(); }
private:
    XOSTProcessor& proc;
    bool hover = false;
    std::unique_ptr<juce::FileChooser> chooser;
    const xo::SampleData* cachedFor = nullptr;
    std::vector<float> overview;
};

class FilterDisplay : public juce::Component
{
public:
    explicit FilterDisplay (XOSTProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    XOSTProcessor& proc;
};

class EnvDisplay : public juce::Component
{
public:
    EnvDisplay (XOSTProcessor& p, int e) : proc (p), env (e) {}
    void paint (juce::Graphics&) override;
private:
    XOSTProcessor& proc;
    int env;
};

class LfoDisplay : public juce::Component
{
public:
    LfoDisplay (XOSTProcessor& p, int l) : proc (p), lfo (l) {}
    void paint (juce::Graphics&) override;
private:
    XOSTProcessor& proc;
    int lfo;
};

class Scope : public juce::Component
{
public:
    explicit Scope (XOSTProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void pull();
private:
    XOSTProcessor& proc;
    std::vector<float> ring = std::vector<float> (4096, 0.0f);
    int head = 0;
    std::vector<float> spectrum = std::vector<float> (256, -100.0f);
};

class StepGrid : public juce::Component
{
public:
    explicit StepGrid (XOSTProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
private:
    void edit (const juce::MouseEvent&, bool first);
    XOSTProcessor& proc;
    int dragStep = -1, dragRow = -1;
    float dragStartVal = 0; int dragStartY = 0;
};

class FxRackList : public juce::Component
{
public:
    explicit FxRackList (XOSTProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragFrom = -1; repaint(); }
private:
    int rowAt (int y) const;
    XOSTProcessor& proc;
    int dragFrom = -1;
};
