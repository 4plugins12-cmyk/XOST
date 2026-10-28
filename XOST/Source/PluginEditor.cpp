#include "PluginEditor.h"
#include "Presets.h"

using namespace juce;
using namespace xo;
using ui::LnF;
using S = Knob::Style;

// ================================================================== Page base
Knob& Page::knob (const String& id, const String& l, Knob::Style s)
{
    knobs.push_back (std::make_unique<Knob> (proc, id, l, s));
    addAndMakeVisible (*knobs.back());
    return *knobs.back();
}
ChoiceBox& Page::choice (const String& id, const String& caption)
{
    choices.push_back (std::make_unique<ChoiceBox> (proc, id, caption));
    addAndMakeVisible (*choices.back());
    return *choices.back();
}
PowerButton& Page::power (const String& id, const String& text)
{
    powers.push_back (std::make_unique<PowerButton> (proc, id, text));
    addAndMakeVisible (*powers.back());
    return *powers.back();
}
Knob* Page::find (const String& id) { for (auto& k : knobs) if (k->paramID == id) return k.get(); jassertfalse; return nullptr; }

Rectangle<int> Page::panel (const String& title, Rectangle<int> r, bool titleSpace)
{
    panels.push_back ({ title, r });
    return r.reduced (12).withTrimmedTop (titleSpace ? 24 : 0);
}

void Page::row (Rectangle<int> area, std::initializer_list<Component*> comps, int cellW, int cellH)
{
    const int n = (int) comps.size();
    const float slot = (float) area.getWidth() / (float) n;
    int i = 0;
    for (auto* c : comps)
    {
        const int cx = area.getX() + (int) (slot * ((float) i + 0.5f));
        c->setBounds (cx - cellW / 2, area.getY(), cellW, cellH);
        ++i;
    }
}

void Page::paint (Graphics& g)
{
    for (auto& p : panels)
    {
        auto r = p.r.toFloat();
        auto shape = LnF::chamfer (r, 12.0f);
        g.setGradientFill (ColourGradient (Colour (0xff11141b), 0, r.getY(), Colours_::panel, 0, r.getY() + 80.0f, false));
        g.fillPath (shape);
        g.setColour (Colours_::panelEdge);
        g.strokePath (shape, PathStrokeType (1.0f));
        if (p.title.isNotEmpty())
        {
            // violet tab marker + spaced title
            g.setColour (Colours_::red);
            g.fillRect (r.getX() + 12.0f, r.getY() + 11.0f, 3.0f, 16.0f);
            g.setColour (Colours_::red.withAlpha (0.25f));
            g.fillRect (r.getX() + 12.0f, r.getY() + 11.0f, 9.0f, 16.0f);
            g.setFont (LnF::title (12.5f));
            g.setColour (Colours_::text);
            g.drawText (p.title, (int) r.getX() + 28, (int) r.getY() + 9, 260, 20, Justification::centredLeft);
        }
    }
    g.setFont (LnF::font (10.5f, true));
    g.setColour (Colours_::textDim);
    for (auto& [t, r] : labels) g.drawFittedText (t, r, Justification::centredLeft, 2);
}

// ================================================================== OSC page
OscPage::OscPage (XOSTProcessor& p) : Page (p)
{
    for (int o = 0; o < numOsc; ++o)
    {
        power (oscId (o, "on"));
        choice (oscId (o, "table"));
        make<WaveDisplay> (proc, o);
        for (auto [id, l] : { std::pair { "pos", "WT Pos" }, { "level", "Level" }, { "pan", "Pan" }, { "oct", "Octave" }, { "semi", "Semi" },
                              { "fine", "Fine" }, { "uni", "Unison" }, { "detune", "Detune" }, { "width", "Stereo" }, { "phase", "Phase" },
                              { "rand", "Rand" }, { "warpamt", "Warp" }, { "modamt", "Mod Amt" } })
            knob (oscId (o, id), l);
        choice (oscId (o, "warp"), "Warp Mode"); choice (oscId (o, "mod"), "Mod Type");
        choice (oscId (o, "modfrom"), "Mod From"); choice (oscId (o, "route"), "Route");
    }
    power ("sub_on", "SUB"); choice ("sub_shape", "Shape"); choice ("sub_oct", "Octave"); knob ("sub_level", "Level"); choice ("sub_route", "Route");
    power ("noise_on", "NOISE"); choice ("noise_type", "Type"); knob ("noise_color", "Color"); knob ("noise_level", "Level"); choice ("noise_route", "Route");
    power ("smp_on"); choice ("smp_mode"); power ("smp_keytrack", "KEY"); choice ("smp_route");
    make<SampleDisplay> (proc);
    for (auto [id, l] : { std::pair { "smp_level", "Level" }, { "smp_pan", "Pan" }, { "smp_start", "Start" }, { "smp_root", "Root" },
                          { "smp_gpitch", "Pitch Rnd" }, { "smp_gpos", "Grain Pos" }, { "smp_gsize", "Size" }, { "smp_gdens", "Density" }, { "smp_gspray", "Spray" } })
        knob (id, l);
    for (int f = 0; f < 2; ++f)
    {
        power (fltId (f, "on"), "F" + String (f + 1));
        choice (fltId (f, "type"));
        for (auto [id, l] : { std::pair { "cutoff", "Cutoff" }, { "reso", "Reso" }, { "drive", "Drive" }, { "key", "Key" }, { "env", "Env 2" }, { "mix", "Mix" } })
            knob (fltId (f, id), l);
    }
    choice ("flt_routing"); power ("shp_on", "SHAPER"); choice ("shp_type"); choice ("shp_pos");
    knob ("shp_drive", "Shp Drive"); knob ("shp_mix", "Shp Mix");
    make<FilterDisplay> (proc);
}

void OscPage::resized()
{
    panels.clear(); labels.clear();
    int ki = 0, ci = 0, pwi = 0, oi = 0;
    auto nextKnob = [&] { return knobs[(size_t) ki++].get(); };
    auto nextChoice = [&] { return choices[(size_t) ci++].get(); };
    auto nextPower = [&] { return powers[(size_t) pwi++].get(); };
    auto nextOther = [&] { return others[(size_t) oi++].get(); };
    const int kw = 52, kh = 56;

    for (int o = 0; o < numOsc; ++o)
    {
        auto pr = Rectangle<int> (o * 422, 0, 412, 256);
        auto c = panel ("OSC " + String::charToString ((juce_wchar) ('A' + o)), pr);
        nextPower()->setBounds (pr.getX() + 84, pr.getY() + 8, 26, 24);
        nextChoice()->setBounds (pr.getRight() - 150, pr.getY() + 8, 138, 24);
        nextOther()->setBounds (c.removeFromTop (54));
        c.removeFromTop (4);
        auto r1 = c.removeFromTop (kh);
        std::vector<Component*> a, b;
        for (int k = 0; k < 6; ++k) a.push_back (nextKnob());
        for (int k = 0; k < 7; ++k) b.push_back (nextKnob());
        const float s1 = (float) r1.getWidth() / 6.0f;
        for (int k = 0; k < 6; ++k) a[(size_t) k]->setBounds (r1.getX() + (int) (s1 * (float) k + s1 / 2) - kw / 2, r1.getY(), kw, kh);
        c.removeFromTop (2);
        auto r2 = c.removeFromTop (kh);
        const float s2 = (float) r2.getWidth() / 7.0f;
        for (int k = 0; k < 7; ++k) b[(size_t) k]->setBounds (r2.getX() + (int) (s2 * (float) k + s2 / 2) - kw / 2, r2.getY(), kw, kh);
        c.removeFromTop (4);
        auto cr = c.removeFromTop (36);
        const int cw = (cr.getWidth() - 18) / 4;
        for (int k = 0; k < 4; ++k) { nextChoice()->setBounds (cr.removeFromLeft (cw)); cr.removeFromLeft (6); }
    }

    // sub + noise
    {
        auto c = panel ("SUB & NOISE", { 0, 266, 296, 246 });
        for (int sct = 0; sct < 2; ++sct)
        {
            auto blk = c.removeFromTop (96);
            auto line = blk.removeFromTop (36);
            nextPower()->setBounds (line.removeFromLeft (80).withTrimmedTop (12));
            nextChoice()->setBounds (line.removeFromLeft (92)); line.removeFromLeft (8);
            if (sct == 0) { nextChoice()->setBounds (line); auto l2 = blk.withTrimmedTop (4); nextKnob()->setBounds (l2.removeFromLeft (70).withHeight (kh)); l2.removeFromLeft (10); nextChoice()->setBounds (l2.withHeight (36).withWidth (110).translated (0, 10)); }
            else { auto l2 = blk.withTrimmedTop (4); nextKnob()->setBounds (l2.removeFromLeft (70).withHeight (kh)); nextKnob()->setBounds (l2.removeFromLeft (70).withHeight (kh)); l2.removeFromLeft (10); nextChoice()->setBounds (l2.withHeight (36).withWidth (110).translated (0, 10)); }
            c.removeFromTop (4);
        }
    }
    // sampler
    {
        auto pr = Rectangle<int> (306, 266, 380, 246);
        auto c = panel ("SAMPLER", pr);
        nextPower()->setBounds (pr.getX() + 108, pr.getY() + 8, 26, 24);
        nextChoice()->setBounds (pr.getX() + 138, pr.getY() + 8, 96, 24);
        nextPower()->setBounds (pr.getX() + 236, pr.getY() + 8, 60, 24);
        nextChoice()->setBounds (pr.getRight() - 82, pr.getY() + 8, 72, 24);
        nextOther()->setBounds (c.removeFromTop (64));
        c.removeFromTop (4);
        auto r1 = c.removeFromTop (kh); c.removeFromTop (4);
        auto r2 = c.removeFromTop (kh);
        std::vector<Component*> a, b;
        for (int k = 0; k < 5; ++k) a.push_back (nextKnob());
        for (int k = 0; k < 4; ++k) b.push_back (nextKnob());
        const float s1 = (float) r1.getWidth() / 5.0f, s2 = (float) r2.getWidth() / 4.0f;
        for (int k = 0; k < 5; ++k) a[(size_t) k]->setBounds (r1.getX() + (int) (s1 * (float) k + s1 / 2) - kw / 2, r1.getY(), kw, kh);
        for (int k = 0; k < 4; ++k) b[(size_t) k]->setBounds (r2.getX() + (int) (s2 * (float) k + s2 / 2) - kw / 2, r2.getY(), kw, kh);
    }
    // filters
    {
        auto pr = Rectangle<int> (696, 266, 560, 246);
        auto c = panel ("FILTERS", pr);
        auto right = c.removeFromRight (160);
        for (int f = 0; f < 2; ++f)
        {
            auto blk = c.removeFromTop (78);
            auto left = blk.removeFromLeft (104);
            nextPower()->setBounds (left.removeFromTop (26));
            nextChoice()->setBounds (left.removeFromTop (26).withTrimmedRight (6));
            const float s = (float) blk.getWidth() / 6.0f;
            for (int k = 0; k < 6; ++k) nextKnob()->setBounds (blk.getX() + (int) (s * (float) k + s / 2) - 22, blk.getY() + 4, 44, kh);
            c.removeFromTop (2);
        }
        auto bottom = c.removeFromTop (36).withTrimmedTop (4);
        nextChoice()->setBounds (bottom.removeFromLeft (84)); bottom.removeFromLeft (8);
        nextPower()->setBounds (bottom.removeFromLeft (86));
        nextChoice()->setBounds (bottom.removeFromLeft (70)); bottom.removeFromLeft (4);
        nextChoice()->setBounds (bottom.removeFromLeft (86)); bottom.removeFromLeft (4);
        right = right.withTrimmedLeft (8);
        auto kr = right.removeFromBottom (56);
        nextKnob()->setBounds (kr.removeFromLeft (kr.getWidth() / 2).withSizeKeepingCentre (50, 56));
        nextKnob()->setBounds (kr.withSizeKeepingCentre (50, 56));
        nextOther()->setBounds (right.withTrimmedBottom (4));
    }
}

void OscPage::tick() { for (auto& o : others) o->repaint(); }

// ================================================================== MOD page
ModPage::ModPage (XOSTProcessor& p) : Page (p)
{
    for (int e = 0; e < numEnv; ++e)
    {
        make<ModSourceChip> (proc, srcEnv1 + e, "Env " + String (e + 1));
        make<EnvDisplay> (proc, e);
        for (auto [id, l] : { std::pair { "delay", "Delay" }, { "attack", "Attack" }, { "hold", "Hold" }, { "decay", "Decay" },
                              { "sustain", "Sustain" }, { "release", "Release" }, { "curve", "Curve" } })
            knob (envId (e, id), l);
    }
    for (int l = 0; l < numLfo; ++l)
    {
        make<ModSourceChip> (proc, srcLfo1 + l, "LFO " + String (l + 1));
        make<LfoDisplay> (proc, l);
        choice (lfoId (l, "shape"), "Shape"); choice (lfoId (l, "mode"), "Mode"); choice (lfoId (l, "div"), "Sync Rate");
        power (lfoId (l, "sync"), "SYNC");
        knob (lfoId (l, "rate"), "Rate"); knob (lfoId (l, "phase"), "Phase"); knob (lfoId (l, "smooth"), "Smooth");
    }
    choice ("voice_mode", "Voices"); choice ("glide_mode", "Glide Mode");
    for (auto [id, l] : { std::pair { "glide", "Glide" }, { "polyphony", "Poly" }, { "bend_range", "Bend" }, { "vel_sens", "Velocity" },
                          { "amp_level", "Amp" }, { "amp_pan", "Pan" } })
        knob (id, l);
    power ("mpe", "MPE");
    const int src[] = { srcVelocity, srcNote, srcPressure, srcModWheel, srcBend, srcSlide, srcRandom };
    const char* names[] = { "Velocity", "Note", "Press", "Wheel", "Bend", "Slide", "Random" };
    for (int i = 0; i < 7; ++i) make<ModSourceChip> (proc, src[i], names[i]);
}

void ModPage::resized()
{
    panels.clear(); labels.clear();
    int ki = 0, ci = 0, pwi = 0, oi = 0;
    auto K = [&] { return knobs[(size_t) ki++].get(); };
    auto C = [&] { return choices[(size_t) ci++].get(); };
    auto Pw = [&] { return powers[(size_t) pwi++].get(); };
    auto O = [&] { return others[(size_t) oi++].get(); };
    const int kh = 56;
    for (int e = 0; e < numEnv; ++e)
    {
        auto pr = Rectangle<int> (e * 336, 0, 326, 250);
        auto c = panel (e == 0 ? "ENV 1  (AMP)" : "ENV " + String (e + 1), pr);
        O()->setBounds (pr.getRight() - 90, pr.getY() + 9, 78, 22);
        O()->setBounds (c.removeFromTop (112));
        c.removeFromTop (8);
        auto r = c.removeFromTop (kh);
        const float s = (float) r.getWidth() / 7.0f;
        for (int k = 0; k < 7; ++k) K()->setBounds (r.getX() + (int) (s * (float) k + s / 2) - 22, r.getY(), 44, kh);
    }
    for (int l = 0; l < numLfo; ++l)
    {
        auto pr = Rectangle<int> (l * 316, 260, 306, 252);
        auto c = panel ("LFO " + String (l + 1), pr);
        O()->setBounds (pr.getRight() - 90, pr.getY() + 9, 78, 22);
        O()->setBounds (c.removeFromTop (74));
        c.removeFromTop (6);
        auto cr = c.removeFromTop (38);
        const int cw = (cr.getWidth() - 12) / 3;
        for (int k = 0; k < 3; ++k) { C()->setBounds (cr.removeFromLeft (cw)); cr.removeFromLeft (6); }
        c.removeFromTop (6);
        auto r = c.removeFromTop (kh);
        Pw()->setBounds (r.removeFromLeft (72).withSizeKeepingCentre (72, 24));
        const float s = (float) r.getWidth() / 3.0f;
        for (int k = 0; k < 3; ++k) K()->setBounds (r.getX() + (int) (s * (float) k + s / 2) - 26, r.getY(), 52, kh);
    }
    {
        auto c = panel ("VOICE", { 1008, 0, 248, 250 });
        auto cr = c.removeFromTop (38);
        C()->setBounds (cr.removeFromLeft (cr.getWidth() / 2 - 3)); cr.removeFromLeft (6); C()->setBounds (cr);
        c.removeFromTop (4);
        auto r = c.removeFromTop (kh);
        row (r, { K(), K(), K(), K() }, 52, kh);
        c.removeFromTop (2);
        auto r2 = c.removeFromTop (kh);
        auto amp = K(); auto pan = K();
        amp->setBounds (r2.removeFromLeft (56)); pan->setBounds (r2.removeFromLeft (56));
        Pw()->setBounds (r2.withSizeKeepingCentre (80, 24));
        (void) r2;
    }
    // the extra source chips live under the LFO row's right edge, in the voice panel space
    {
        auto area = Rectangle<int> (1008 + 12, 196, 224, 48);
        for (int i = 0; i < 7; ++i)
        {
            auto* chip = O();
            chip->setBounds (area.getX() + (i % 4) * 57, area.getY() + (i / 4) * 25, 54, 22);
        }
    }
}

void ModPage::tick() { for (auto& o : others) o->repaint(); }

// ================================================================== MATRIX page
RoutingPage::RoutingPage (XOSTProcessor& p) : Page (p)
{
    for (int s = 0; s < numSlots; ++s)
    {
        choice (slotId (s, "src")); choice (slotId (s, "dst"));
        knob (slotId (s, "amt"), {}, S::compact);
        auto& clear = make<TextButton> ("x");
        clear.setTooltip ("Clear this slot");
        clear.onClick = [this, s]
        {
            proc.undo.beginNewTransaction ("Clear mod slot");
            for (auto* w : { "src", "dst", "amt" }) if (auto* q = proc.apvts.getParameter (slotId (s, w))) q->setValueNotifyingHost (q->getDefaultValue());
        };
    }
}

void RoutingPage::resized()
{
    panels.clear(); labels.clear();
    for (int col = 0; col < 2; ++col)
    {
        auto c = panel (col == 0 ? "MOD ROUTING  1-8" : "MOD ROUTING  9-16", { col * 633, 0, 623, 512 });
        auto hdr = c.removeFromTop (18);
        label ("SOURCE", hdr.withTrimmedLeft (30).withWidth (180));
        label ("DESTINATION", hdr.withTrimmedLeft (240).withWidth (200));
        label ("AMOUNT", hdr.withTrimmedLeft (450).withWidth (100));
        c.removeFromTop (6);
        for (int k = 0; k < 8; ++k)
        {
            const int s = col * 8 + k;
            auto r = c.removeFromTop (54).reduced (0, 9);
            label (String (s + 1), r.removeFromLeft (26));
            choices[(size_t) s * 2]->setBounds (r.removeFromLeft (190)); r.removeFromLeft (20);
            choices[(size_t) s * 2 + 1]->setBounds (r.removeFromLeft (200)); r.removeFromLeft (14);
            knobs[(size_t) s]->setBounds (r.removeFromLeft (120));
            others[(size_t) s]->setBounds (r.removeFromLeft (30).withSizeKeepingCentre (26, 26));
        }
        label ("Tip: drag any Env / LFO / Macro / source chip onto a knob to route it here automatically.", c.withTrimmedTop (2).withHeight (20));
    }
}

// ================================================================== FX page
FxPage::FxPage (XOSTProcessor& p) : Page (p)
{
    make<FxRackList> (proc);
    power ("dist_on"); choice ("dist_type", "Type"); knob ("dist_drive", "Drive", S::normal); knob ("dist_tone", "Tone", S::normal); knob ("dist_mix", "Mix", S::normal);
    power ("ch_on"); knob ("ch_rate", "Rate", S::normal); knob ("ch_depth", "Depth", S::normal); knob ("ch_mix", "Mix", S::normal);
    power ("ph_on"); knob ("ph_rate", "Rate", S::normal); knob ("ph_depth", "Depth", S::normal); knob ("ph_fb", "Feedback", S::normal); knob ("ph_mix", "Mix", S::normal);
    power ("fl_on"); knob ("fl_rate", "Rate", S::normal); knob ("fl_depth", "Depth", S::normal); knob ("fl_fb", "Feedback", S::normal); knob ("fl_mix", "Mix", S::normal);
    power ("dly_on"); choice ("dly_l", "Left"); choice ("dly_r", "Right"); knob ("dly_fb", "Feedback", S::normal); knob ("dly_ping", "Ping Pong", S::normal);
    knob ("dly_tone", "Tone", S::normal); knob ("dly_mix", "Mix", S::normal);
    power ("rev_on"); knob ("rev_size", "Size", S::normal); knob ("rev_decay", "Decay", S::normal); knob ("rev_damp", "Damping", S::normal);
    knob ("rev_pre", "Pre-Delay", S::normal); knob ("rev_width", "Width", S::normal); knob ("rev_mix", "Mix", S::normal);
    power ("eq_on"); knob ("eq_low", "Low", S::normal); knob ("eq_mid", "Mid", S::normal); knob ("eq_midfreq", "Mid Freq", S::normal); knob ("eq_high", "High", S::normal);
    power ("comp_on"); knob ("comp_thresh", "Threshold", S::normal); knob ("comp_ratio", "Ratio", S::normal); knob ("comp_attack", "Attack", S::normal);
    knob ("comp_release", "Release", S::normal); knob ("comp_gain", "Makeup", S::normal); knob ("comp_mix", "Mix", S::normal);
    power ("tape_on"); knob ("tape_wow", "Wow", S::normal); knob ("tape_flutter", "Flutter", S::normal); knob ("tape_sat", "Saturate", S::normal);
    knob ("tape_hiss", "Hiss", S::normal); knob ("tape_age", "Age", S::normal); knob ("tape_mix", "Mix", S::normal);
    knob ("out_width", "Width", S::normal); knob ("drift", "Drift", S::normal);
}

void FxPage::resized()
{
    panels.clear(); labels.clear();
    auto rc = panel ("FX CHAIN", { 0, 0, 196, 512 });
    label ("Drag to reorder, click the dot to switch on/off. Audio flows top to bottom.", rc.removeFromBottom (32));
    others[0]->setBounds (rc);
    const char* names[] = { "DISTORTION", "CHORUS", "PHASER", "FLANGER", "DELAY", "REVERB", "EQ", "COMPRESSOR", "TAPE", "CHARACTER" };
    const int knobCounts[] = { 3, 3, 4, 4, 4, 6, 4, 6, 6, 2 };
    const int choiceCounts[] = { 1, 0, 0, 0, 2, 0, 0, 0, 0, 0 };
    int ki = 0, ci = 0;
    const int kw = 58, kh = 68;
    for (int f = 0; f < numFx + 1; ++f)
    {
        auto pr = Rectangle<int> (206 + (f % 5) * 210, (f / 5) * 260, 202, 252);
        auto c = panel (names[f], pr);
        if (f < numFx) powers[(size_t) f]->setBounds (pr.getRight() - 38, pr.getY() + 8, 26, 24);
        if (choiceCounts[f] > 0)
        {
            auto cr = c.removeFromTop (38);
            const int cw = (cr.getWidth() - 6 * (choiceCounts[f] - 1)) / choiceCounts[f];
            for (int k = 0; k < choiceCounts[f]; ++k) { choices[(size_t) ci++]->setBounds (cr.removeFromLeft (cw)); cr.removeFromLeft (6); }
            c.removeFromTop (8);
        }
        const int n = knobCounts[f];
        const int perRow = 3;
        for (int k = 0; k < n; ++k)
        {
            const int rowI = k / perRow, colI = k % perRow;
            const int inRow = std::min (perRow, n - rowI * perRow);
            const float sl = (float) c.getWidth() / (float) inRow;
            knobs[(size_t) ki++]->setBounds (c.getX() + (int) (sl * (float) colI + sl / 2) - kw / 2, c.getY() + rowI * (kh + 10), kw, kh);
        }
        if (f == numFx)
            label ("Drift: slow analog pitch and filter wander per voice. Width: stereo spread of the whole synth (0 = mono).",
                   c.withTrimmedTop (kh + 14).withHeight (60));
    }
}

void FxPage::tick() { others[0]->repaint(); }

// ================================================================== ARP page
ArpPage::ArpPage (XOSTProcessor& p) : Page (p)
{
    power ("arp_on", "ARP ON"); power ("arp_latch", "LATCH");
    choice ("arp_mode", "Mode"); choice ("arp_rate", "Rate");
    knob ("arp_oct", "Octaves", S::normal); knob ("arp_gate", "Gate", S::normal); knob ("arp_swing", "Swing", S::normal); knob ("arp_steps", "Steps", S::normal);
    make<StepGrid> (proc);
    choice ("chord_type", "Chord"); choice ("chord_voicing", "Voicing");
    knob ("chord_inv", "Inversion", S::normal); knob ("chord_strum", "Strum", S::normal);
}

void ArpPage::resized()
{
    panels.clear(); labels.clear();
    auto c = panel ("ARPEGGIATOR", { 0, 0, 330, 300 });
    auto top = c.removeFromTop (28);
    powers[0]->setBounds (top.removeFromLeft (140)); powers[1]->setBounds (top.removeFromLeft (120));
    c.removeFromTop (8);
    auto cr = c.removeFromTop (40);
    choices[0]->setBounds (cr.removeFromLeft (cr.getWidth() / 2 - 4)); cr.removeFromLeft (8); choices[1]->setBounds (cr);
    c.removeFromTop (12);
    row (c.removeFromTop (74), { knobs[0].get(), knobs[1].get(), knobs[2].get(), knobs[3].get() }, 66, 74);
    label ("Sequence mode plays the step grid from the lowest held note. Chord plays every held note on each step.", c.withTrimmedTop (8).withHeight (40));

    auto h = panel ("CHORD MEMORY", { 0, 310, 330, 202 });
    auto hr = h.removeFromTop (40);
    choices[2]->setBounds (hr.removeFromLeft (hr.getWidth() / 2 - 4)); hr.removeFromLeft (8); choices[3]->setBounds (hr);
    h.removeFromTop (8);
    row (h.removeFromTop (74), { knobs[4].get(), knobs[5].get() }, 70, 74);
    label ("One key plays a whole chord. Strum rolls the notes in like a guitar or harp.", h.withTrimmedTop (4).withHeight (36));

    auto g = panel ("STEPS", { 340, 0, 916, 512 });
    label ("Top: step on/off.  Bars: velocity (drag).  Numbers: transpose in semitones (drag up/down).", g.removeFromBottom (24));
    others[0]->setBounds (g.withTrimmedTop (8));
}

void ArpPage::tick() { others[0]->repaint(); }

// ================================================================== Macro strip
MacroStrip::MacroStrip (XOSTProcessor& p) : Page (p)
{
    for (int m = 0; m < numMacros; ++m)
    {
        knob (macroId (m), {}, S::normal);
        make<ModSourceChip> (proc, srcMacro1 + m, "Drag");
    }
    nameEditor.setFont (LnF::font (13.0f, true));
    nameEditor.setJustification (Justification::centred);
    nameEditor.onReturnKey = nameEditor.onFocusLost = [this]
    {
        if (editing >= 0) { proc.macroNames.set (editing, nameEditor.getText().substring (0, 14)); editing = -1; }
        nameEditor.setVisible (false);
        resized(); repaint();
    };
    addChildComponent (nameEditor);
}

void MacroStrip::resized()
{
    panels.clear(); labels.clear();
    auto c = panel ("MACROS", getLocalBounds());
    const int w = c.getWidth() / numMacros;
    for (int m = 0; m < numMacros; ++m)
    {
        auto col = Rectangle<int> (c.getX() + m * w, c.getY() - 6, w, c.getHeight() + 6);
        label (proc.macroNames[m].toUpperCase(), col.removeFromTop (16).withTrimmedLeft (w / 2 - 34).withWidth (80));
        knobs[(size_t) m]->setBounds (col.removeFromTop (54).withSizeKeepingCentre (54, 54));
        others[(size_t) m]->setBounds (col.removeFromTop (22).withSizeKeepingCentre (60, 20));
    }
}

void MacroStrip::tick() { for (auto& o : others) o->repaint(); }

void MacroStrip::mouseDoubleClick (const MouseEvent& e)
{
    for (int m = 0; m < numMacros; ++m)
    {
        auto r = labels[(size_t) m].second;
        if (r.expanded (8, 4).contains (e.getPosition()))
        {
            editing = m;
            nameEditor.setText (proc.macroNames[m], false);
            nameEditor.setBounds (r.expanded (10, 3));
            nameEditor.setVisible (true);
            nameEditor.grabKeyboardFocus();
            nameEditor.selectAll();
            return;
        }
    }
}

// ================================================================== Bounce chip
void BounceChip::paint (Graphics& g)
{
    if (! file.existsAsFile()) return;
    auto r = getLocalBounds().toFloat().reduced (1);
    g.setColour (Colours_::cyan.withAlpha (0.15f)); g.fillRoundedRectangle (r, 10);
    g.setColour (Colours_::cyan); g.drawRoundedRectangle (r, 10, 1.4f);
    g.setFont (LnF::font (11.0f, true)); g.setColour (Colours_::text);
    g.drawFittedText ("DRAG WAV", getLocalBounds(), Justification::centred, 1);
}

void BounceChip::mouseDrag (const MouseEvent& e)
{
    if (e.getDistanceFromDragStart() > 4 && file.existsAsFile())
        DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
}

// ================================================================== Content
Content::Content (XOSTProcessor& p) : proc (p)
{
    setWantsKeyboardFocus (true);
    const char* tabNames[] = { "OSC", "MOD", "ROUTING", "FX", "ARP", "MATRIX", "MINIRACK", "MUTATE" };
    for (int i = 0; i < numPages; ++i)
    {
        auto* t = tabs.add (new TextButton (String (tabNames[i])));
        t->setClickingTogglesState (true);
        t->setRadioGroupId (5);
        t->setTooltip (i == 5 ? "Gate / filter / pan / time shaper (like Gross Beat or ShaperBox)" : i == 6 ? "Virtual eurorack: six modulators you patch into the synth" : i == 7 ? "Gene field, breeding and morphing to make new sounds from presets" : String());
        t->onClick = [this, i] { showPage (i); };
        addAndMakeVisible (t);
    }
    for (auto* pg : pages) addChildComponent (pg);
    addAndMakeVisible (macros);
    addAndMakeVisible (scope);
    addAndMakeVisible (createPanel);
    addAndMakeVisible (promptBar);
    addAndMakeVisible (oneShotBox);
    keyboard.setColour (MidiKeyboardComponent::keyDownOverlayColourId, Colours_::red.withAlpha (0.6f));
    keyboard.setColour (MidiKeyboardComponent::mouseOverKeyOverlayColourId, Colours_::red.withAlpha (0.2f));
    keyboard.setColour (MidiKeyboardComponent::whiteNoteColourId, Colour (0xffd1e2f1));
    keyboard.setColour (MidiKeyboardComponent::blackNoteColourId, Colour (0xff08090d));
    keyboard.setColour (MidiKeyboardComponent::keyDownOverlayColourId, Colour (0xff617391).withAlpha (0.8f));
    keyboard.setColour (MidiKeyboardComponent::mouseOverKeyOverlayColourId, Colour (0xff7b92b7).withAlpha (0.35f));
    keyboard.setColour (MidiKeyboardComponent::textLabelColourId, Colours_::textDim);
    keyboard.setColour (MidiKeyboardComponent::keySeparatorLineColourId, Colours_::panelEdge);
    keyboard.setColour (MidiKeyboardComponent::shadowColourId, Colours_::red.withAlpha (0.1f));
    keyboard.setLowestVisibleKey (36);
    keyboard.setKeyWidth (22.0f);
    addAndMakeVisible (keyboard);

    quality = std::make_unique<ChoiceBox> (proc, "quality", "");
    master = std::make_unique<Knob> (proc, "master_vol", "Master", Knob::Style::compact);
    addAndMakeVisible (*quality); addAndMakeVisible (*master);
    quality->box.setTooltip ("Quality: Eco (8 voices, 4 unison), Normal, High (2x oversampling), Ultra (4x)");

    for (auto* b : { &presetButton, &prevButton, &nextButton, &browseButton, &saveButton, &initButton, &diceButton, &lockButton,
                     &undoButton, &redoButton, &historyButton, &bounceButton, &scaleButton })
        addAndMakeVisible (*b);
    addChildComponent (bounceChip);
    presetButton.onClick = browseButton.onClick = [this] { browser.openBrowser(); };
    saveButton.onClick = [this] { browser.openBrowser(); browser.openSaveDialog(); };
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    initButton.onClick = [this] { proc.initPatch(); };
    diceButton.onClick = [this] { proc.randomize (proc.randomLocks.load()); };
    lockButton.onClick = [this] { showRandomMenu(); };
    undoButton.onClick = [this] { proc.undo.undo(); };
    redoButton.onClick = [this] { proc.undo.redo(); };
    historyButton.onClick = [this] { showHistory(); };
    bounceButton.onClick = [this] { showBounceMenu(); };
    scaleButton.onClick = [this]
    {
        PopupMenu m;
        const double scales[] = { 0.6, 0.75, 0.9, 1.0, 1.15, 1.3, 1.5 };
        for (int i = 0; i < 7; ++i) m.addItem (i + 1, String (roundToInt (scales[i] * 100)) + "%", true, std::abs (proc.uiScale - scales[i]) < 0.01);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&scaleButton), [this, scales] (int r)
        {
            if (r > 0) if (auto* ed = findParentComponentOfClass<XOSTEditor>()) ed->setScale (scales[r - 1]);
        });
    };
    diceButton.setTooltip ("Randomize the patch (sections set in LOCKS are left alone)");
    lockButton.setTooltip ("Choose which sections RANDOM is not allowed to change");
    bounceButton.setTooltip ("Bounce the sound to a WAV you can drag into your DAW, or resample it into a wavetable");
    historyButton.setTooltip ("Jump back through the undo history");
    browser.onClose = [this] { browser.setVisible (false); updatePresetLabel(); toneList.syncSelection(); };
    addChildComponent (browser);

    tabs[0]->setToggleState (true, dontSendNotification);
    showPage (0);
    setSize (W, H);
    startTimerHz (30);
}

Content::~Content() { stopTimer(); }

void Content::showPage (int i)
{
    page = i;
    for (int k = 0; k < numPages; ++k) pages[(size_t) k]->setVisible (k == i);
    for (int k = 0; k < numPages; ++k) tabs[k]->setToggleState (k == i, dontSendNotification);
}

void Content::stepPreset (int d)
{
    library.refresh();
    const auto& e = library.getEntries();
    if (e.isEmpty()) return;
    int idx = 0;
    for (int i = 0; i < e.size(); ++i) if (e[i].name == proc.getCurrentPresetName()) idx = i;
    idx = (idx + d + e.size()) % e.size();
    if (e[idx].isUser()) proc.loadUserPreset (e[idx].file); else proc.setCurrentProgram (e[idx].factoryIndex);
}

void Content::updatePresetLabel()
{
    presetCategory = proc.currentCategory().isNotEmpty() ? proc.currentCategory() : juce::String ("User");
    for (auto& e : library.getEntries()) if (e.name == proc.getCurrentPresetName()) { presetCategory = e.category; break; }
    presetButton.setButtonText (presetCategory.toUpperCase() + "   " + proc.getCurrentPresetName());
}

void Content::showRandomMenu()
{
    PopupMenu m;
    m.addSectionHeader ("Locked sections are kept when you press RANDOM");
    const auto& names = XOSTProcessor::sectionNames();
    const uint32_t locks = proc.randomLocks.load();
    for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, names[i], true, (locks >> i) & 1u);
    m.addSeparator();
    m.addItem (100, "Unlock all"); m.addItem (101, "Lock all but oscillators");
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&lockButton), [this] (int r)
    {
        if (r == 0) return;
        uint32_t l = proc.randomLocks.load();
        if (r == 100) l = 0;
        else if (r == 101) l = ~0u & ~7u;
        else l ^= (1u << (r - 1));
        proc.randomLocks = l;
        showRandomMenu();
    });
}

void Content::showBounceMenu()
{
    PopupMenu m;
    m.addSectionHeader ("Bounce to audio");
    m.addItem (1, "Bounce C2 (2 s)"); m.addItem (2, "Bounce C3 (2 s)"); m.addItem (3, "Bounce C4 (2 s)"); m.addItem (4, "Bounce C3 (6 s)");
    m.addSectionHeader ("Resample into a wavetable");
    m.addItem (11, "Resample into Osc A"); m.addItem (12, "Resample into Osc B"); m.addItem (13, "Resample into Osc C");
    m.addSeparator();
    m.addItem (20, "Open bounce folder");
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&bounceButton), [this] (int r)
    {
        const int notes[] = { 0, 36, 48, 60, 48 };
        if (r >= 1 && r <= 4)
        {
            bounceChip.file = proc.bounceToFile (notes[r], r == 4 ? 6.0 : 2.0);
            bounceChip.setVisible (bounceChip.file.existsAsFile());
            bounceChip.repaint();
        }
        else if (r >= 11 && r <= 13) proc.resampleToWavetable (r - 11, 48);
        else if (r == 20) XOSTProcessor::getUserFolder ("Bounces").startAsProcess();
    });
}

void Content::showHistory()
{
    PopupMenu m;
    const auto list = proc.undo.getUndoDescriptions();
    m.addSectionHeader ("Undo history (newest first)");
    for (int i = 0; i < std::min (25, list.size()); ++i) m.addItem (i + 1, list[i].isEmpty() ? String ("Change") : list[i]);
    if (list.isEmpty()) m.addItem (-1, "Nothing to undo yet", false);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&historyButton), [this] (int r)
    {
        for (int i = 0; i < r; ++i) proc.undo.undo();
    });
}

bool Content::keyPressed (const KeyPress& k)
{
    const bool cmd = k.getModifiers().isCommandDown();
    if (cmd && k.getKeyCode() == 'Z') { if (k.getModifiers().isShiftDown()) proc.undo.redo(); else proc.undo.undo(); return true; }
    if (cmd && k.getKeyCode() == 'Y') { proc.undo.redo(); return true; }
    return false;
}

void Content::resized()
{
    // header
    prevButton.setBounds (430, 14, 28, 34);
    presetButton.setBounds (462, 12, 360, 38);
    nextButton.setBounds (826, 14, 28, 34);
    browseButton.setBounds (864, 14, 80, 34);
    saveButton.setBounds (948, 14, 62, 34);
    initButton.setBounds (1020, 14, 54, 34);
    diceButton.setBounds (1078, 14, 80, 34);
    lockButton.setBounds (1162, 14, 64, 34);
    undoButton.setBounds (1236, 14, 58, 34);
    redoButton.setBounds (1298, 14, 58, 34);
    historyButton.setBounds (1360, 14, 76, 34);
    bounceButton.setBounds (1446, 14, 70, 34);
    bounceChip.setBounds (1520, 14, 68, 34);

    // prompt bar + one-shot exporter (below the header)
    promptBar.setBounds (12, 70, 1230, 82);
    oneShotBox.setBounds (1250, 70, 338, 82);

    // tab bar (Serum style, across the top of the page area)
    const int dy = 88;
    for (int i = 0; i < numPages; ++i) tabs[i]->setBounds (300 + i * 116, 70 + dy, 112, 30);
    quality->setBounds (1240, 72 + dy, 96, 26);
    scaleButton.setBounds (1342, 70 + dy, 60, 30);
    master->setBounds (1412, 70 + dy, 176, 30);

    // CREATE / PRESETS column on the left
    createPanel.setBounds (12, 70 + dy, 276, 684);
    for (auto* pg : pages) pg->setBounds (316, 108 + dy, 1256, 512);
    macros.setBounds (300, 628 + dy, 790, 126);
    scope.setBounds (1108, 642 + dy, 472, 104);
    keyboard.setBounds (12, 764 + dy, W - 24, 72);
    browser.setBounds (0, 62, W, H - 62);
}

void Content::paint (Graphics& g)
{
    g.fillAll (Colours_::bg);
    // header
    g.setGradientFill (ColourGradient (Colour (0xff0e1117), 0, 0, Colour (0xff05060a), 0, 62, false));
    g.fillRect (0, 0, W, 62);
    g.setColour (Colour (0xff212733)); g.fillRect (0, 61, W, 1);
    // logo: X + O with a crossing line
    g.setFont (LnF::title (40.0f));
    g.setColour (Colours_::red);
    g.drawText ("XOST", 18, 4, 150, 52, Justification::centredLeft);
    g.setColour (Colours_::red.withAlpha (0.5f));
    g.fillRect (18, 52, 118, 2);
    g.setFont (LnF::font (9.5f, true));
    g.setColour (Colours_::textDim);
    g.drawText ("TEXT-TO-SOUND SYNTHESIZER", 150, 14, 190, 14, Justification::centredLeft);
    g.drawText ("VOICES " + String (proc.engine.activeVoices.load()) + "   CPU " + String (roundToInt (proc.cpuLoad.load() * 100.0f)) + "%",
                150, 32, 190, 14, Justification::centredLeft);
    // page frame
    auto pf = Rectangle<float> (300, 104 + 88, 1288, 520);
    g.setColour (Colour (0xff08090d)); g.fillRoundedRectangle (pf, 5.0f);
    g.setColour (Colour (0xff181c25)); g.drawRoundedRectangle (pf.reduced (0.5f), 5.0f, 1.0f);
    // scope panel
    auto sp = Rectangle<float> (1100, 628 + 88, 488, 126);
    g.setColour (Colours_::panel); g.fillRoundedRectangle (sp, 5.0f);
    g.setColour (Colours_::panelEdge); g.drawRoundedRectangle (sp.reduced (0.5f), 5.0f, 1.0f);
}

void Content::timerCallback()
{
    ++frame;
    pages[(size_t) page]->tick();
    macros.tick();

    scope.pull();
    promptBar.tick (frame);
    createPanel.tick();
    if (frame % 15 == 0) oneShotBox.repaint();

    if (proc.getPresetChangeCounter() != shownCounter) { shownCounter = proc.getPresetChangeCounter(); updatePresetLabel(); toneList.syncSelection(); }
    undoButton.setEnabled (proc.undo.canUndo());
    redoButton.setEnabled (proc.undo.canRedo());
    scaleButton.setButtonText (String (roundToInt (proc.uiScale * 100)) + "%");
    if (frame % 10 == 0)
    {
        repaint (150, 30, 190, 18);
        // repaint knobs if the matrix changed so the modulation rings stay current
        float h = 0.0f;
        for (int s = 0; s < numSlots; ++s)
            h += proc.apvts.getRawParameterValue (slotId (s, "src"))->load() * 1.3f + proc.apvts.getRawParameterValue (slotId (s, "dst"))->load() * 7.1f
                 + proc.apvts.getRawParameterValue (slotId (s, "amt"))->load() * 31.0f * (float) (s + 1);
        if (std::abs (h - matrixHash) > 1.0e-4f)
        {
            matrixHash = h;
            for (auto* pg : pages) for (auto* k : pg->allKnobs()) k->repaint();
            for (auto* k : macros.allKnobs()) k->repaint();
        }
    }
}

// ================================================================== Editor
XOSTEditor::XOSTEditor (XOSTProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    const double startScale = juce::jlimit (0.5, 2.0, proc.uiScale);
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    setResizable (true, true);
    setResizeLimits (Content::W / 2, Content::H / 2, Content::W * 2, Content::H * 2);
    getConstrainer()->setFixedAspectRatio ((double) Content::W / (double) Content::H);
    setSize ((int) (Content::W * startScale), (int) (Content::H * startScale));
}

XOSTEditor::~XOSTEditor() { setLookAndFeel (nullptr); }

void XOSTEditor::setScale (double s) { setSize ((int) (Content::W * s), (int) (Content::H * s)); }

void XOSTEditor::resized()
{
    const double s = (double) getWidth() / (double) Content::W;
    proc.uiScale = s;
    content.setTransform (AffineTransform::scale ((float) s));
    content.setBounds (0, 0, Content::W, Content::H);
}
