#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace Colours_
{
    const juce::Colour bg        { 0xff04060a };
    const juce::Colour panel     { 0xff0c1016 };
    const juce::Colour panelEdge { 0xff1f2a36 };
    const juce::Colour knobBody  { 0xff131922 };   // fields, buttons, knob bodies
    const juce::Colour track     { 0xff222c38 };
    const juce::Colour red       { 0xff7fd0f7 };   // accent: Zenology-style light blue (named "red" for shared UI code)
    const juce::Colour redDim    { 0xff1c3a4c };
    const juce::Colour cyan      { 0xff8fa7bb };   // modulation: steel blue-grey
    const juce::Colour text      { 0xffe8f4fb };
    const juce::Colour textDim   { 0xff6f8394 };
    const juce::Colour deep      { 0xffc9ecff };
    const juce::Colour onAccent  { 0xff03111a };   // text drawn on top of the blue accent
    const juce::Colour screen    { 0xff070b10 };   // display backgrounds
    const juce::Colour chipSound { 0xff7fd0f7 }, chipStyle { 0xffb79cff }, chipChar { 0xff7ff0c8 },
                       chipFx { 0xffffc46b }, chipAmount { 0xff9aa9b6 }, chipUnknown { 0xff4a5866 };
}

class XOLookAndFeel : public juce::LookAndFeel_V4
{
public:
    XOLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool isDown, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getLabelFont (juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                        int thumbStart, int thumbSize, bool over, bool down) override;
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;

    // Inter for UI text, Space Grotesk for titles
    static juce::Font font (float size, bool bold = false);
    static juce::Font title (float size);

    // panel shape: tight rounded corners (clean, studio-hardware look)
    static juce::Path chamfer (juce::Rectangle<float> r, float cut);
};
