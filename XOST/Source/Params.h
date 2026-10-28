#pragma once

// XO parameter definitions. Every automatable parameter lives here.

#include <juce_audio_processors/juce_audio_processors.h>

namespace xo
{
    constexpr int numOsc = 3, numEnv = 3, numLfo = 4, numMacros = 8, numSlots = 16, numSteps = 16, numFx = 9;
    constexpr int maxVoices = 32, maxUnison = 8;
    constexpr int numMxLanes = 4, mxTableSize = 1024, numRack = 6;
    inline const char* mxLaneIds[numMxLanes] = { "vol", "flt", "pan", "time" };
    inline juce::String rackId (int m, const char* s) { return "rk" + juce::String (m + 1) + "_" + s; }

    inline juce::String oscId (int o, const char* s)   { return juce::String ("o") + juce::String::charToString ((juce::juce_wchar) ('a' + o)) + "_" + s; }
    inline juce::String envId (int e, const char* s)   { return "e" + juce::String (e + 1) + "_" + s; }
    inline juce::String lfoId (int l, const char* s)   { return "l" + juce::String (l + 1) + "_" + s; }
    inline juce::String fltId (int f, const char* s)   { return "f" + juce::String (f + 1) + "_" + s; }
    inline juce::String macroId (int m)                { return "macro" + juce::String (m + 1); }
    inline juce::String slotId (int s, const char* w)  { return "mm" + juce::String (s + 1) + "_" + w; }
    inline juce::String stepId (int s, const char* w)  { return "st" + juce::String (s + 1) + "_" + w; }

    namespace Choice
    {
        inline const juce::StringArray tables { "Basic Shapes", "PWM", "Harmonic Sweep", "Bell FM", "Glass", "Vocal", "Digital",
                                                "Hollow", "Bubble", "Super Saw", "Organ", "Growl", "Chiptune", "Flute", "Pluck",
                                                "Sine Stack", "User" };
        inline const juce::StringArray warp    { "Off", "Sync", "Bend", "Mirror", "PWM", "Quantize", "Self FM" };
        inline const juce::StringArray oscMod  { "Off", "FM", "Phase Mod", "Ring Mod", "AM", "Hard Sync" };
        inline const juce::StringArray modFrom { "Osc A", "Osc B", "Osc C", "Sub", "Noise" };
        inline const juce::StringArray route   { "Filter 1", "Filter 2", "Both", "Direct" };
        inline const juce::StringArray subShape { "Sine", "Triangle", "Square", "Saw" };
        inline const juce::StringArray subOct  { "-2 Oct", "-1 Oct", "0 Oct" };
        inline const juce::StringArray noise   { "White", "Pink", "Crackle", "Vinyl" };
        inline const juce::StringArray smpMode { "One Shot", "Loop", "Granular" };
        inline const juce::StringArray filter  { "Low Pass 12", "Low Pass 24", "Ladder LP", "High Pass 12", "High Pass 24",
                                                "Band Pass", "Notch", "Peak", "Comb", "Formant" };
        inline const juce::StringArray fltRouting { "Serial", "Parallel" };
        inline const juce::StringArray shaper  { "Soft", "Hard", "Tube", "Fold", "Bitcrush" };
        inline const juce::StringArray shaperPos { "Pre Filter", "Post Filter" };
        inline const juce::StringArray lfoShape { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H", "Smooth Rnd",
                                                  "Bounce", "Stairs", "Pump" };
        inline const juce::StringArray lfoMode { "Free", "Retrigger", "Envelope" };
        inline const juce::StringArray divs    { "8 Bars", "4 Bars", "2 Bars", "1 Bar", "1/2", "1/2T", "1/4.", "1/4", "1/4T",
                                                 "1/8.", "1/8", "1/8T", "1/16.", "1/16", "1/16T", "1/32", "1/64" };
        inline const double divBeats[] { 32, 16, 8, 4, 2, 4.0 / 3.0, 1.5, 1, 2.0 / 3.0, 0.75, 0.5, 1.0 / 3.0, 0.375, 0.25, 1.0 / 6.0, 0.125, 0.0625 };
        inline const juce::StringArray voiceMode { "Poly", "Mono", "Legato" };
        inline const juce::StringArray glideMode { "Always", "Legato Only" };
        inline const juce::StringArray quality { "Eco", "Normal", "High (2x)", "Ultra (4x)" };
        inline const juce::StringArray onOff   { "Off", "On" };
        inline const juce::StringArray arpMode { "Up", "Down", "Up/Down", "Random", "As Played", "Chord", "Sequence" };
        inline const juce::StringArray distType { "Overdrive", "Tube", "Hard Clip", "Bitcrush", "Fold", "Rectify" };
        inline const juce::StringArray fxNames { "Distortion", "Chorus", "Phaser", "Flanger", "Delay", "Reverb", "EQ", "Compressor", "Tape" };
        inline const juce::StringArray chordType { "Off", "Minor", "Major", "Minor 7", "Major 7", "Minor 9", "Major 9", "Minor add9",
                                                   "Sus2", "Sus4", "Power", "Octaves", "Diminished", "Phrygian" };
        inline const juce::StringArray chordVoicing { "Close", "Open", "Wide" };
        // Matrix (gate / shaper / time tool)
        inline const juce::StringArray mxLen   { "1/4", "1/2", "1 Bar", "2 Bars", "4 Bars" };
        inline const double mxLenBeats[] { 1, 2, 4, 8, 16 };
        inline const juce::StringArray mxFilter { "Low Pass", "High Pass", "Band Pass" };
        inline const juce::StringArray mxTimeRange { "1/16", "1/8", "1/4", "1/2", "1 Bar" };
        inline const double mxTimeBeats[] { 0.25, 0.5, 1, 2, 4 };
        // Minirack modules
        inline const juce::StringArray rackType { "Empty", "LFO", "Sample & Hold", "Drunk Walk", "Turing Machine", "Chaos",
                                                  "Sequencer", "Bouncing Ball", "Follower" };
    }

    // ---------------------------------------------------------------- modulation sources
    enum Src { srcNone, srcEnv1, srcEnv2, srcEnv3, srcLfo1, srcLfo2, srcLfo3, srcLfo4, srcVelocity, srcNote, srcPressure,
               srcModWheel, srcBend, srcSlide, srcRandom, srcMacro1, numSources = srcMacro1 + numMacros };
    inline const juce::StringArray srcNames { "None", "Env 1", "Env 2", "Env 3", "LFO 1", "LFO 2", "LFO 3", "LFO 4",
                                              "Velocity", "Note", "Aftertouch", "Mod Wheel", "Pitch Bend", "Slide (MPE)",
                                              "Random", "Macro 1", "Macro 2", "Macro 3", "Macro 4", "Macro 5", "Macro 6",
                                              "Macro 7", "Macro 8" };

    // ---------------------------------------------------------------- modulation destinations
    enum Dst
    {
        dstNone,
        dstAPitch, dstAPos, dstALevel, dstAPan, dstADetune, dstAWarp, dstAMod,
        dstBPitch, dstBPos, dstBLevel, dstBPan, dstBDetune, dstBWarp, dstBMod,
        dstCPitch, dstCPos, dstCLevel, dstCPan, dstCDetune, dstCWarp, dstCMod,
        dstSubLevel, dstNoiseLevel, dstNoiseColor, dstSmpLevel, dstSmpPos, dstGrainSize,
        dstF1Cutoff, dstF1Reso, dstF1Drive, dstF2Cutoff, dstF2Reso, dstShaperDrive,
        dstAmp, dstVoicePan, dstLfo1Rate, dstLfo2Rate, dstLfo3Rate, dstLfo4Rate,
        // global (applied once for the whole synth, using the newest voice's values)
        dstDistDrive, dstDistMix, dstChorusMix, dstPhaserMix, dstDelayMix, dstDelayFb, dstReverbMix, dstReverbSize, dstMaster,
        numDests,
        firstGlobalDest = dstDistDrive
    };
    inline const juce::StringArray dstNames {
        "None",
        "A Pitch", "A WT Pos", "A Level", "A Pan", "A Detune", "A Warp", "A Mod Amt",
        "B Pitch", "B WT Pos", "B Level", "B Pan", "B Detune", "B Warp", "B Mod Amt",
        "C Pitch", "C WT Pos", "C Level", "C Pan", "C Detune", "C Warp", "C Mod Amt",
        "Sub Level", "Noise Level", "Noise Color", "Sample Level", "Sample Pos", "Grain Size",
        "F1 Cutoff", "F1 Reso", "F1 Drive", "F2 Cutoff", "F2 Reso", "Shaper Drive",
        "Amp", "Pan", "LFO 1 Rate", "LFO 2 Rate", "LFO 3 Rate", "LFO 4 Rate",
        "Dist Drive", "Dist Mix", "Chorus Mix", "Phaser Mix", "Delay Mix", "Delay Feedback", "Reverb Mix", "Reverb Size", "Master" };

    // parameter that a destination "belongs" to, for drawing modulation rings on knobs and drag-and-drop
    inline juce::String paramForDest (int d)
    {
        static const char* oscParts[] = { "pitch", "pos", "level", "pan", "detune", "warpamt", "modamt" };
        if (d >= dstAPitch && d <= dstCMod)
        {
            const int o = (d - dstAPitch) / 7, part = (d - dstAPitch) % 7;
            return part == 0 ? oscId (o, "semi") : oscId (o, oscParts[part]);
        }
        switch (d)
        {
            case dstSubLevel: return "sub_level";      case dstNoiseLevel: return "noise_level";
            case dstNoiseColor: return "noise_color";  case dstSmpLevel: return "smp_level";
            case dstSmpPos: return "smp_gpos";         case dstGrainSize: return "smp_gsize";
            case dstF1Cutoff: return "f1_cutoff";      case dstF1Reso: return "f1_reso";      case dstF1Drive: return "f1_drive";
            case dstF2Cutoff: return "f2_cutoff";      case dstF2Reso: return "f2_reso";      case dstShaperDrive: return "shp_drive";
            case dstAmp: return "amp_level";           case dstVoicePan: return "amp_pan";
            case dstLfo1Rate: return "l1_rate";        case dstLfo2Rate: return "l2_rate";
            case dstLfo3Rate: return "l3_rate";        case dstLfo4Rate: return "l4_rate";
            case dstDistDrive: return "dist_drive";    case dstDistMix: return "dist_mix";
            case dstChorusMix: return "ch_mix";        case dstPhaserMix: return "ph_mix";
            case dstDelayMix: return "dly_mix";        case dstDelayFb: return "dly_fb";
            case dstReverbMix: return "rev_mix";       case dstReverbSize: return "rev_size";
            case dstMaster: return "master_vol";
            default: return {};
        }
    }
    inline int destForParam (const juce::String& id)
    {
        for (int d = 1; d < numDests; ++d) if (paramForDest (d) == id) return d;
        return 0;
    }

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        AudioProcessorValueTreeState::ParameterLayout L;
        auto pct = [] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; };
        auto bip = [] (float v, int) { return (v > 0 ? "+" : "") + String (roundToInt (v * 100.0f)) + "%"; };
        auto db  = [] (float v, int) { return String (v, 1) + " dB"; };
        auto hz  = [] (float v, int) { return v < 1000.0f ? String (v, v < 10.0f ? 2 : 0) + " Hz" : String (v / 1000.0f, 2) + " kHz"; };
        auto ms  = [] (float v, int) { return v < 1000.0f ? String (v, v < 10.0f ? 1 : 0) + " ms" : String (v / 1000.0f, 2) + " s"; };
        auto st  = [] (float v, int) { return (v > 0 ? "+" : "") + String (roundToInt (v)) + " st"; };
        auto ct  = [] (float v, int) { return (v > 0 ? "+" : "") + String (roundToInt (v)) + " ct"; };
        auto F = [&] (const String& id, const String& name, NormalisableRange<float> r, float def, std::function<String (float, int)> f)
        { L.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, AudioParameterFloatAttributes().withStringFromValueFunction (f))); };
        auto C = [&] (const String& id, const String& name, const StringArray& c, int def)
        { L.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, c, def)); };
        auto I = [&] (const String& id, const String& name, int lo, int hi, int def)
        { L.add (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def)); };
        auto skew = [] (float lo, float hi, float centre) { NormalisableRange<float> r (lo, hi); r.setSkewForCentre (centre); return r; };

        // ---- global / voice
        F ("master_vol", "Master", { -36.0f, 6.0f, 0.01f }, -6.0f, db);
        C ("quality", "Quality", Choice::quality, 1);
        I ("polyphony", "Polyphony", 1, maxVoices, 12);
        C ("voice_mode", "Voice Mode", Choice::voiceMode, 0);
        F ("glide", "Glide", skew (0.0f, 2000.0f, 150.0f), 0.0f, ms);
        C ("glide_mode", "Glide Mode", Choice::glideMode, 0);
        I ("bend_range", "Bend Range", 1, 48, 2);
        C ("mpe", "MPE", Choice::onOff, 0);
        F ("vel_sens", "Velocity", { 0.0f, 1.0f }, 0.6f, pct);
        F ("amp_level", "Amp", { 0.0f, 1.0f }, 0.8f, pct);
        F ("amp_pan", "Pan", { -1.0f, 1.0f }, 0.0f, bip);

        // ---- oscillators
        for (int o = 0; o < numOsc; ++o)
        {
            const String n = String ("Osc ") + String::charToString ((juce_wchar) ('A' + o)) + " ";
            C (oscId (o, "on"), n + "On", Choice::onOff, o == 0 ? 1 : 0);
            C (oscId (o, "table"), n + "Table", Choice::tables, o == 0 ? 0 : (o == 1 ? 9 : 1));
            F (oscId (o, "pos"), n + "WT Pos", { 0.0f, 1.0f }, o == 0 ? 0.66f : 0.0f, pct);
            I (oscId (o, "oct"), n + "Octave", -3, 3, 0);
            F (oscId (o, "semi"), n + "Semi", { -24.0f, 24.0f, 1.0f }, 0.0f, st);
            F (oscId (o, "fine"), n + "Fine", { -100.0f, 100.0f, 0.1f }, 0.0f, ct);
            F (oscId (o, "level"), n + "Level", { 0.0f, 1.0f }, 0.75f, pct);
            F (oscId (o, "pan"), n + "Pan", { -1.0f, 1.0f }, 0.0f, bip);
            I (oscId (o, "uni"), n + "Unison", 1, maxUnison, 1);
            F (oscId (o, "detune"), n + "Detune", { 0.0f, 1.0f }, 0.2f, pct);
            F (oscId (o, "width"), n + "Stereo", { 0.0f, 1.0f }, 0.7f, pct);
            F (oscId (o, "phase"), n + "Phase", { 0.0f, 1.0f }, 0.0f, pct);
            F (oscId (o, "rand"), n + "Phase Rand", { 0.0f, 1.0f }, 1.0f, pct);
            C (oscId (o, "warp"), n + "Warp", Choice::warp, 0);
            F (oscId (o, "warpamt"), n + "Warp Amount", { 0.0f, 1.0f }, 0.0f, pct);
            C (oscId (o, "mod"), n + "Mod Type", Choice::oscMod, 0);
            C (oscId (o, "modfrom"), n + "Mod From", Choice::modFrom, o == 0 ? 2 : o - 1);
            F (oscId (o, "modamt"), n + "Mod Amount", { 0.0f, 1.0f }, 0.0f, pct);
            C (oscId (o, "route"), n + "Route", Choice::route, 0);
        }
        // ---- sub / noise / sampler
        C ("sub_on", "Sub On", Choice::onOff, 0);
        C ("sub_shape", "Sub Shape", Choice::subShape, 0);
        C ("sub_oct", "Sub Octave", Choice::subOct, 1);
        F ("sub_level", "Sub Level", { 0.0f, 1.0f }, 0.5f, pct);
        C ("sub_route", "Sub Route", Choice::route, 3);
        C ("noise_on", "Noise On", Choice::onOff, 0);
        C ("noise_type", "Noise Type", Choice::noise, 0);
        F ("noise_color", "Noise Color", { 0.0f, 1.0f }, 0.6f, pct);
        F ("noise_level", "Noise Level", { 0.0f, 1.0f }, 0.3f, pct);
        C ("noise_route", "Noise Route", Choice::route, 0);
        C ("smp_on", "Sampler On", Choice::onOff, 0);
        C ("smp_mode", "Sampler Mode", Choice::smpMode, 0);
        I ("smp_root", "Sample Root", 0, 127, 60);
        C ("smp_keytrack", "Sample Keytrack", Choice::onOff, 1);
        F ("smp_start", "Sample Start", { 0.0f, 1.0f }, 0.0f, pct);
        F ("smp_level", "Sample Level", { 0.0f, 1.0f }, 0.7f, pct);
        F ("smp_pan", "Sample Pan", { -1.0f, 1.0f }, 0.0f, bip);
        F ("smp_gpos", "Grain Position", { 0.0f, 1.0f }, 0.3f, pct);
        F ("smp_gsize", "Grain Size", skew (5.0f, 500.0f, 60.0f), 80.0f, ms);
        F ("smp_gdens", "Grain Density", skew (2.0f, 120.0f, 25.0f), 30.0f, hz);
        F ("smp_gspray", "Grain Spray", { 0.0f, 1.0f }, 0.2f, pct);
        F ("smp_gpitch", "Grain Pitch Rand", { 0.0f, 1.0f }, 0.0f, pct);
        C ("smp_route", "Sample Route", Choice::route, 0);

        // ---- filters + shaper
        for (int f = 0; f < 2; ++f)
        {
            const String n = "Filter " + String (f + 1) + " ";
            C (fltId (f, "on"), n + "On", Choice::onOff, f == 0 ? 1 : 0);
            C (fltId (f, "type"), n + "Type", Choice::filter, f == 0 ? 1 : 3);
            F (fltId (f, "cutoff"), n + "Cutoff", skew (20.0f, 20000.0f, 1200.0f), f == 0 ? 8000.0f : 200.0f, hz);
            F (fltId (f, "reso"), n + "Resonance", { 0.0f, 1.0f }, 0.15f, pct);
            F (fltId (f, "drive"), n + "Drive", { 0.0f, 1.0f }, 0.0f, pct);
            F (fltId (f, "key"), n + "Keytrack", { 0.0f, 1.0f }, 0.0f, pct);
            F (fltId (f, "env"), n + "Env 2 Amount", { -1.0f, 1.0f }, 0.0f, bip);
            F (fltId (f, "mix"), n + "Mix", { 0.0f, 1.0f }, 1.0f, pct);
        }
        C ("flt_routing", "Filter Routing", Choice::fltRouting, 0);
        C ("shp_on", "Shaper On", Choice::onOff, 0);
        C ("shp_type", "Shaper Type", Choice::shaper, 0);
        F ("shp_drive", "Shaper Drive", { 0.0f, 1.0f }, 0.3f, pct);
        C ("shp_pos", "Shaper Position", Choice::shaperPos, 0);
        F ("shp_mix", "Shaper Mix", { 0.0f, 1.0f }, 1.0f, pct);

        // ---- envelopes (DAHDSR with curve)
        for (int e = 0; e < numEnv; ++e)
        {
            const String n = "Env " + String (e + 1) + " ";
            F (envId (e, "delay"), n + "Delay", skew (0.0f, 2000.0f, 200.0f), 0.0f, ms);
            F (envId (e, "attack"), n + "Attack", skew (0.0f, 10000.0f, 400.0f), e == 0 ? 2.0f : 1.0f, ms);
            F (envId (e, "hold"), n + "Hold", skew (0.0f, 2000.0f, 200.0f), 0.0f, ms);
            F (envId (e, "decay"), n + "Decay", skew (1.0f, 15000.0f, 600.0f), e == 0 ? 800.0f : 400.0f, ms);
            F (envId (e, "sustain"), n + "Sustain", { 0.0f, 1.0f }, e == 0 ? 0.8f : 0.0f, pct);
            F (envId (e, "release"), n + "Release", skew (1.0f, 15000.0f, 600.0f), e == 0 ? 150.0f : 300.0f, ms);
            F (envId (e, "curve"), n + "Curve", { -1.0f, 1.0f }, 0.3f, bip);
        }
        // ---- LFOs
        for (int l = 0; l < numLfo; ++l)
        {
            const String n = "LFO " + String (l + 1) + " ";
            C (lfoId (l, "shape"), n + "Shape", Choice::lfoShape, l == 1 ? 7 : 0);
            C (lfoId (l, "sync"), n + "Sync", Choice::onOff, 1);
            F (lfoId (l, "rate"), n + "Rate", skew (0.01f, 40.0f, 2.0f), 2.0f, hz);
            C (lfoId (l, "div"), n + "Division", Choice::divs, 10);
            F (lfoId (l, "phase"), n + "Phase", { 0.0f, 1.0f }, 0.0f, pct);
            C (lfoId (l, "mode"), n + "Mode", Choice::lfoMode, 1);
            F (lfoId (l, "smooth"), n + "Smooth", { 0.0f, 1.0f }, 0.0f, pct);
        }
        for (int m = 0; m < numMacros; ++m) F (macroId (m), "Macro " + String (m + 1), { 0.0f, 1.0f }, 0.0f, pct);
        for (int s = 0; s < numSlots; ++s)
        {
            const String n = "Mod " + String (s + 1) + " ";
            L.add (std::make_unique<AudioParameterChoice> (ParameterID { slotId (s, "src"), 1 }, n + "Source", srcNames, 0));
            L.add (std::make_unique<AudioParameterChoice> (ParameterID { slotId (s, "dst"), 1 }, n + "Destination", dstNames, 0));
            F (slotId (s, "amt"), n + "Amount", { -1.0f, 1.0f }, 0.0f, bip);
        }

        // ---- arpeggiator / step sequencer
        C ("arp_on", "Arp On", Choice::onOff, 0);
        C ("arp_mode", "Arp Mode", Choice::arpMode, 0);
        C ("arp_rate", "Arp Rate", Choice::divs, 13);
        I ("arp_oct", "Arp Octaves", 1, 4, 1);
        F ("arp_gate", "Arp Gate", { 0.05f, 1.0f }, 0.6f, pct);
        F ("arp_swing", "Arp Swing", { 0.0f, 0.5f }, 0.0f, pct);
        C ("arp_latch", "Arp Latch", Choice::onOff, 0);
        I ("arp_steps", "Arp Steps", 1, numSteps, 8);
        for (int s = 0; s < numSteps; ++s)
        {
            C (stepId (s, "on"), "Step " + String (s + 1) + " On", Choice::onOff, 1);
            F (stepId (s, "tr"), "Step " + String (s + 1) + " Transpose", { -24.0f, 24.0f, 1.0f }, 0.0f, st);
            F (stepId (s, "vel"), "Step " + String (s + 1) + " Velocity", { 0.0f, 1.0f }, 0.8f, pct);
        }

        // ---- effects
        C ("dist_on", "Distortion On", Choice::onOff, 0);
        C ("dist_type", "Distortion Type", Choice::distType, 0);
        F ("dist_drive", "Dist Drive", { 0.0f, 1.0f }, 0.3f, pct);
        F ("dist_tone", "Dist Tone", { 0.0f, 1.0f }, 0.6f, pct);
        F ("dist_mix", "Dist Mix", { 0.0f, 1.0f }, 1.0f, pct);
        C ("ch_on", "Chorus On", Choice::onOff, 0);
        F ("ch_rate", "Chorus Rate", skew (0.05f, 8.0f, 0.8f), 0.6f, hz);
        F ("ch_depth", "Chorus Depth", { 0.0f, 1.0f }, 0.5f, pct);
        F ("ch_mix", "Chorus Mix", { 0.0f, 1.0f }, 0.4f, pct);
        C ("ph_on", "Phaser On", Choice::onOff, 0);
        F ("ph_rate", "Phaser Rate", skew (0.02f, 8.0f, 0.5f), 0.3f, hz);
        F ("ph_depth", "Phaser Depth", { 0.0f, 1.0f }, 0.6f, pct);
        F ("ph_fb", "Phaser Feedback", { 0.0f, 0.95f }, 0.4f, pct);
        F ("ph_mix", "Phaser Mix", { 0.0f, 1.0f }, 0.5f, pct);
        C ("fl_on", "Flanger On", Choice::onOff, 0);
        F ("fl_rate", "Flanger Rate", skew (0.02f, 8.0f, 0.5f), 0.2f, hz);
        F ("fl_depth", "Flanger Depth", { 0.0f, 1.0f }, 0.5f, pct);
        F ("fl_fb", "Flanger Feedback", { -0.95f, 0.95f }, 0.5f, bip);
        F ("fl_mix", "Flanger Mix", { 0.0f, 1.0f }, 0.5f, pct);
        C ("dly_on", "Delay On", Choice::onOff, 0);
        C ("dly_l", "Delay Left", Choice::divs, 10);
        C ("dly_r", "Delay Right", Choice::divs, 9);
        F ("dly_fb", "Delay Feedback", { 0.0f, 0.95f }, 0.35f, pct);
        F ("dly_ping", "Delay Ping Pong", { 0.0f, 1.0f }, 0.6f, pct);
        F ("dly_tone", "Delay Tone", { 0.0f, 1.0f }, 0.5f, pct);
        F ("dly_mix", "Delay Mix", { 0.0f, 1.0f }, 0.25f, pct);
        C ("rev_on", "Reverb On", Choice::onOff, 0);
        F ("rev_size", "Reverb Size", { 0.0f, 1.0f }, 0.6f, pct);
        F ("rev_decay", "Reverb Decay", skew (0.2f, 20.0f, 2.5f), 2.5f, [] (float v, int) { return String (v, 1) + " s"; });
        F ("rev_damp", "Reverb Damping", { 0.0f, 1.0f }, 0.4f, pct);
        F ("rev_pre", "Reverb Pre-Delay", { 0.0f, 200.0f }, 10.0f, ms);
        F ("rev_width", "Reverb Width", { 0.0f, 1.0f }, 1.0f, pct);
        F ("rev_mix", "Reverb Mix", { 0.0f, 1.0f }, 0.25f, pct);
        C ("eq_on", "EQ On", Choice::onOff, 0);
        F ("eq_low", "EQ Low", { -15.0f, 15.0f }, 0.0f, db);
        F ("eq_mid", "EQ Mid", { -15.0f, 15.0f }, 0.0f, db);
        F ("eq_midfreq", "EQ Mid Freq", skew (200.0f, 8000.0f, 1200.0f), 1200.0f, hz);
        F ("eq_high", "EQ High", { -15.0f, 15.0f }, 0.0f, db);
        C ("comp_on", "Compressor On", Choice::onOff, 0);
        F ("comp_thresh", "Comp Threshold", { -48.0f, 0.0f }, -18.0f, db);
        F ("comp_ratio", "Comp Ratio", skew (1.0f, 20.0f, 4.0f), 4.0f, [] (float v, int) { return String (v, 1) + ":1"; });
        F ("comp_attack", "Comp Attack", skew (0.1f, 100.0f, 10.0f), 5.0f, ms);
        F ("comp_release", "Comp Release", skew (10.0f, 1000.0f, 150.0f), 120.0f, ms);
        F ("comp_gain", "Comp Makeup", { 0.0f, 24.0f }, 4.0f, db);
        F ("comp_mix", "Comp Mix", { 0.0f, 1.0f }, 1.0f, pct);
        // ---- tape (new in XO)
        C ("tape_on", "Tape On", Choice::onOff, 0);
        F ("tape_wow", "Tape Wow", { 0.0f, 1.0f }, 0.3f, pct);
        F ("tape_flutter", "Tape Flutter", { 0.0f, 1.0f }, 0.2f, pct);
        F ("tape_sat", "Tape Saturation", { 0.0f, 1.0f }, 0.3f, pct);
        F ("tape_hiss", "Tape Hiss", { 0.0f, 1.0f }, 0.1f, pct);
        F ("tape_age", "Tape Age", { 0.0f, 1.0f }, 0.3f, pct);
        F ("tape_mix", "Tape Mix", { 0.0f, 1.0f }, 1.0f, pct);
        // ---- output / character (new in XO)
        F ("out_width", "Stereo Width", { 0.0f, 2.0f }, 1.0f, pct);
        F ("drift", "Analog Drift", { 0.0f, 1.0f }, 0.0f, pct);
        C ("chord_type", "Chord", Choice::chordType, 0);
        I ("chord_inv", "Chord Inversion", 0, 3, 0);
        C ("chord_voicing", "Chord Voicing", Choice::chordVoicing, 0);
        F ("chord_strum", "Chord Strum", skew (0.0f, 250.0f, 40.0f), 0.0f, ms);
        // ---- Matrix: tempo-synced drawable lanes (volume gate, filter, pan, time) on the output
        C ("mx_on", "Matrix On", Choice::onOff, 0);
        F ("mx_mix", "Matrix Mix", { 0.0f, 1.0f }, 1.0f, pct);
        F ("mx_smooth", "Matrix Smooth", { 0.0f, 1.0f }, 0.2f, pct);
        for (int l = 0; l < numMxLanes; ++l)
        {
            static const char* names[] = { "Volume", "Filter", "Pan", "Time" };
            const String id = String ("mx_") + mxLaneIds[l] + "_", n = String ("Matrix ") + names[l] + " ";
            C (id + "on", n + "On", Choice::onOff, l == 0 ? 1 : 0);
            F (id + "depth", n + "Depth", { 0.0f, 1.0f }, 1.0f, pct);
            C (id + "len", n + "Length", Choice::mxLen, 2);
        }
        C ("mx_flt_type", "Matrix Filter Type", Choice::mxFilter, 0);
        F ("mx_flt_reso", "Matrix Filter Resonance", { 0.0f, 1.0f }, 0.3f, pct);
        C ("mx_time_range", "Matrix Time Range", Choice::mxTimeRange, 2);
        // ---- Minirack: six modulation modules patched to any destination
        C ("rk_on", "Minirack On", Choice::onOff, 0);
        F ("rk_depth", "Minirack Depth", { 0.0f, 1.0f }, 1.0f, pct);
        for (int m = 0; m < numRack; ++m)
        {
            const String n = "Module " + String (m + 1) + " ";
            C (rackId (m, "type"), n + "Type", Choice::rackType, 0);
            C (rackId (m, "sync"), n + "Sync", Choice::onOff, 1);
            F (rackId (m, "rate"), n + "Rate", skew (0.01f, 30.0f, 1.5f), 1.0f, hz);
            C (rackId (m, "div"), n + "Division", Choice::divs, 7);
            F (rackId (m, "a"), n + "Shape", { 0.0f, 1.0f }, 0.0f, pct);
            F (rackId (m, "b"), n + "Variation", { 0.0f, 1.0f }, 0.3f, pct);
            L.add (std::make_unique<AudioParameterChoice> (ParameterID { rackId (m, "dst1"), 1 }, n + "Destination 1", dstNames, 0));
            F (rackId (m, "amt1"), n + "Amount 1", { -1.0f, 1.0f }, 0.0f, bip);
            L.add (std::make_unique<AudioParameterChoice> (ParameterID { rackId (m, "dst2"), 1 }, n + "Destination 2", dstNames, 0));
            F (rackId (m, "amt2"), n + "Amount 2", { -1.0f, 1.0f }, 0.0f, bip);
        }
        return L;
    }
}
