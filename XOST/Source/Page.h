#pragma once

#include "Widgets.h"

// A page of the synth: owns its widgets and draws its panels
class Page : public juce::Component
{
public:
    explicit Page (XOSTProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    virtual void tick() {}
    std::vector<Knob*> allKnobs() const { std::vector<Knob*> v; for (auto& k : knobs) v.push_back (k.get()); return v; }
protected:
    Knob& knob (const juce::String& id, const juce::String& label, Knob::Style s = Knob::Style::mini);
    ChoiceBox& choice (const juce::String& id, const juce::String& caption = {});
    PowerButton& power (const juce::String& id, const juce::String& text = {});
    template <class T, class... A> T& make (A&&... a) { auto c = std::make_unique<T> (std::forward<A> (a)...); auto& r = *c; addAndMakeVisible (r); others.push_back (std::move (c)); return r; }
    Knob* find (const juce::String& id);
    juce::Rectangle<int> panel (const juce::String& title, juce::Rectangle<int> r, bool titleSpace = true);
    static void row (juce::Rectangle<int> area, std::initializer_list<juce::Component*> comps, int cellW, int cellH);
    void label (const juce::String& text, juce::Rectangle<int> r) { labels.push_back ({ text, r }); }

    XOSTProcessor& proc;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<ChoiceBox>> choices;
    std::vector<std::unique_ptr<PowerButton>> powers;
    std::vector<std::unique_ptr<juce::Component>> others;
    struct P { juce::String title; juce::Rectangle<int> r; };
    std::vector<P> panels;
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> labels;
};

