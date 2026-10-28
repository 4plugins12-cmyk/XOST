#include <set>
// Offline test harness: renders every preset with a bouncy MIDI pattern, checks levels/CPU/NaNs,
// exercises randomize / undo / state / bounce / resample, and snapshots every page of the editor.
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "Presets.h"
#include "PluginEditor.h"
#include <chrono>

using namespace juce;

struct TestPlayHead : AudioPlayHead
{
    double bpm = 150.0, ppq = 0.0;
    Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p; p.setBpm (bpm); p.setPpqPosition (ppq); p.setIsPlaying (true);
        p.setTimeSignature (TimeSignature { 4, 4 });
        return p;
    }
};

// 150 bpm: 1/8 note = 0.2 s.  Bars 1-2: bouncy line. Then a held Cm9 chord, release, tail.
static MidiBuffer makePattern (double sr, int len)
{
    MidiBuffer m;
    const int line[] = { 60, 67, 72, 70, 67, 63, 65, 67,   60, 72, 70, 67, 75, 74, 70, 67 };
    const double e = 0.2;
    for (int i = 0; i < 16; ++i)
    {
        const int on = (int) (sr * e * i), off = (int) (sr * (e * i + 0.14));
        const float vel = (i % 4 == 0) ? 1.0f : (i % 2 ? 0.7f : 0.85f);
        m.addEvent (MidiMessage::noteOn (1, line[i], vel), on);
        m.addEvent (MidiMessage::noteOff (1, line[i]), off);
    }
    const int cStart = (int) (sr * 3.2), cEnd = (int) (sr * 4.8);
    for (int n : { 48, 55, 58, 62, 63 })
    {
        m.addEvent (MidiMessage::noteOn (1, n, 0.85f), cStart);
        m.addEvent (MidiMessage::noteOff (1, n), std::min (len - 1, cEnd));
    }
    return m;
}

static void render (XOSTProcessor& proc, AudioBuffer<float>& out, const MidiBuffer& pattern, int block, double sr, TestPlayHead* ph)
{
    const int len = out.getNumSamples();
    out.clear();
    for (int pos = 0; pos < len; pos += block)
    {
        const int n = std::min (block, len - pos);
        MidiBuffer midi;
        for (const auto meta : pattern)
            if (meta.samplePosition >= pos && meta.samplePosition < pos + n) midi.addEvent (meta.getMessage(), meta.samplePosition - pos);
        AudioBuffer<float> view (out.getArrayOfWritePointers(), 2, pos, n);
        if (ph != nullptr) ph->ppq = (double) pos / sr * ph->bpm / 60.0;
        proc.processBlock (view, midi);
    }
}

static void stats (const AudioBuffer<float>& b, float& peak, double& rms, bool& finite)
{
    peak = 0; rms = 0; finite = true;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float v = b.getSample (ch, i);
            if (! std::isfinite (v)) finite = false;
            peak = std::max (peak, std::abs (v));
            rms += (double) v * v;
        }
    rms = std::sqrt (rms / (2.0 * b.getNumSamples()));
}
// loudest 400 ms window (short-term level), fair between plucks and pads
static double shortTerm (const AudioBuffer<float>& b, double sr)
{
    const int w = (int) (sr * 0.4); double best = 0;
    for (int s = 0; s + w <= b.getNumSamples(); s += w / 4)
    {
        double acc = 0;
        for (int ch = 0; ch < 2; ++ch) for (int i = s; i < s + w; ++i) { const double v = b.getSample (ch, i); acc += v * v; }
        best = std::max (best, std::sqrt (acc / (2.0 * w)));
    }
    return best;
}

// ================================================================== word engine tests
static int wordTests (const String& outDir, std::function<void (const AudioBuffer<float>&, const String&, int)> writeWav, bool writeAudio)
{
    int fails = 0;
    const auto& lex = xost::Lexicon::get();
    for (auto& p : lex.problems) { printf ("  !! lexicon: %s\n", p.toRawUTF8()); ++fails; }
    // every sound family the vocabulary points at must exist in the library, every op must hit a real parameter
    std::set<String> kinds;
    for (auto& s : Presets::seeds()) kinds.insert (s.kind);
    XOSTProcessor probe (true);
    const auto table = xost::makeParamTable (probe);
    int nEntries = 0;
    for (auto& e : lex.entries())
    {
        ++nEntries;
        for (auto& k : e.kinds) if (! kinds.count (k.key)) { printf ("  !! unknown kind %s (%s)\n", k.key.toRawUTF8(), e.label.toRawUTF8()); ++fails; }
        for (auto* ops : { &e.ops, &e.neg })
            for (auto& op : *ops)
            {
                if (op.type == '@') continue;
                auto id = op.id.replace ("o*_", "oa_").replace ("f*_", "f1_");
                if (! table.count (id)) { printf ("  !! unknown param %s (%s)\n", id.toRawUTF8(), e.label.toRawUTF8()); ++fails; }
            }
        for (auto& [u, f] : e.uses) if (lex.findLabel (u) < 0) { printf ("  !! unknown use %s (%s)\n", u.toRawUTF8(), e.label.toRawUTF8()); ++fails; }
    }
    printf ("lexicon: %d entries, %d sound families in %d library sounds\n", nEntries, (int) kinds.size(), (int) Presets::seeds().size());

    struct Case { const char* prompt; const char* cat; const char* kind; const char* check; };
    const Case cases[] = {
        { "dark glassy pluck like pierre bourne with tape", "Pluck", "", "tape_on=1" },
        { "yeat bells", "Bell", "Bell.toll", "" },
        { "phonk cowbell very distorted", "Bell", "Bell.cowbell", "dist_on=1" },
        { "uk drill sliding 808", "Bass", "Bass.drill", "" },
        { "pluggnb keys with chords", "Keys", "", "chord_type>0" },
        { "jerk lead", "Lead", "Lead.jerk", "" },
        { "drain gang trance pad", "Pad", "Pad.trance", "" },
        { "metro boomin dark piano", "Keys", "", "" },
        { "warm lofi rhodes with a little wobble", "Keys", "", "tape_on=1" },
        { "sigilkore crushed pad", "Pad", "Pad.sigil", "dist_type=3" },
        { "jersey club vocal chop", "Vox", "Vox.jersey", "" },
        { "haunted choir no reverb", "", "", "rev_on=0" },
        { "rage supersaw lead wide", "Lead", "Lead.rage", "" },
        { "sad emo guitar", "", "Pluck.emo", "" },
        { "detroit piano", "Keys", "Keys.detroit", "" },
        { "kordhell type beat cowbel", "Bell", "Bell.cowbell", "" },
        { "808", "Bass", "", "" },
        { "very bright arp with sidechain", "Arp", "", "mx_on=1" },
        { "cash cobain keys", "Keys", "Keys.rnb", "" },
        { "sematary horror lead", "Lead", "Lead.horror", "" },
        { "sophie plastic pluck", "Pluck", "Pluck.plastic", "" },
        { "ambient", "", "", "" },
        { "flute like future", "", "Lead.flute", "" },
        { "a little dark music box with no tape", "", "Bell.musicbox", "tape_on=0" },
        { "blorp zzzt", "", "", "" },
    };
    xost::Generator gen;
    double totalMs = 0; int nGen = 0;
    juce::String firstValues, firstName;
    for (auto& c : cases)
    {
        xost::Generator::Request r; r.prompt = c.prompt; r.seed = 11;
        const auto t0 = Time::getMillisecondCounterHiRes();
        auto res = gen.runNow (r);
        totalMs += Time::getMillisecondCounterHiRes() - t0; ++nGen;
        StringArray chips; for (auto& ch : res.understood.chips) chips.add ((ch.unknown ? "?" : "") + ch.text);
        printf ("\n\"%s\"  ->  [%s]\n", c.prompt, chips.joinIntoString (" | ").toRawUTF8());
        bool kindHit = String (c.kind).isEmpty();
        if ((int) res.variations.size() != 4) { printf ("  !! %d variations\n", (int) res.variations.size()); ++fails; }
        for (size_t i = 0; i < res.variations.size(); ++i)
        {
            const auto& v = res.variations[i];
            xost::PatchValues pv; pv.load (v.values, table);
            bool ok = String (c.cat).isEmpty() || v.category == c.cat;
            if (String (c.kind) == v.kind) kindHit = true;
            const String chk (c.check);
            if (chk.isNotEmpty())
            {
                const bool gt = chk.containsChar ('>');
                const auto id = chk.upToFirstOccurrenceOf (gt ? ">" : "=", false, false);
                const float want = chk.fromFirstOccurrenceOf (gt ? ">" : "=", false, false).getFloatValue();
                const float got = pv.get (id);
                if (gt ? ! (got > want) : std::abs (got - want) > 1.0e-3f) { ok = false; printf ("  !! %s: %s = %g\n", v.name.toRawUTF8(), id.toRawUTF8(), got); }
            }
            // re-measure in a fresh instance: everything should come out at the target level
            probe.applyValues (v.values, XOSTProcessor::Scope::all);
            float peak = 0; const double st = xost::measureLoudness (probe, 48000.0, &peak);
            if (std::abs (st + 15.0) > 1.5 || ! std::isfinite (peak) || peak > 4.0f) { ok = false; printf ("  !! %s level %.1f dB peak %.1f dB  (master %.1f comp %g ratio %g gain %g)\n", v.name.toRawUTF8(), st, Decibels::gainToDecibels (peak), pv.get ("master_vol"), pv.get ("comp_on"), pv.get ("comp_ratio"), pv.get ("comp_gain")); }
            printf ("  %c %-30s %-10s %-20s %5.1f dB   %s\n", 'A' + (int) i, v.name.toRawUTF8(), v.category.toRawUTF8(), v.kind.toRawUTF8(), st, ok ? "" : "FAIL");
            if (! ok) ++fails;
            if (writeAudio && i == 0)
            {
                AudioBuffer<float> out (2, (int) (48000 * 6.0)); MidiBuffer m; xost::makeTestPattern (m, 48000.0, out.getNumSamples());
                probe.renderOffline (out, m, 48000.0, 150.0);
                writeWav (out, "words_" + String (c.prompt).replaceCharacters (" '", "__").substring (0, 40), 16);
            }
            if (firstValues.isEmpty()) { firstValues = v.values; firstName = v.name; }
        }
        if (! kindHit) { printf ("  !! none of the results is %s\n", c.kind); ++fails; }
        printf ("  %s\n", res.message.toRawUTF8());
    }
    printf ("\ngenerate: %.0f ms average for 4 sounds\n", totalMs / nGen);

    // ---- tweaks on an existing sound
    struct T { const char* words; const char* id; int dir; };
    const T tweaks[] = { { "darker", "f1_cutoff", -1 }, { "more reverb", "rev_mix", 1 }, { "wider", "out_width", 1 }, { "a lot less reverb", "rev_mix", -1 },
                         { "no chorus", "ch_on", 0 }, { "more distortion", "dist_mix", 1 }, { "brighter", "f1_cutoff", 1 }, { "longer", "e1_release", 1 },
                         { "more tape", "tape_wow", 1 }, { "less wobble", "", 0 } };
    juce::String cur = firstValues;
    for (auto& t : tweaks)
    {
        xost::Generator::Request r; r.prompt = t.words; r.currentValues = cur; r.currentName = firstName; r.refine = false; r.seed = 3;
        auto res = gen.runNow (r);
        const bool single = res.variations.size() == 1;
        xost::PatchValues a, b; a.load (cur, table);
        if (single) b.load (res.variations[0].values, table);
        bool ok = single;
        if (single && String (t.id).isNotEmpty())
        {
            const float x0 = a.get (t.id), x1 = b.get (t.id);
            ok = t.dir > 0 ? x1 > x0 : (t.dir < 0 ? x1 < x0 : x1 < 0.5f);
            printf ("tweak \"%s\": %s %g -> %g  %s  (%s)\n", t.words, t.id, x0, x1, ok ? "OK" : "FAIL", single ? res.variations[0].name.toRawUTF8() : "");
        }
        else printf ("tweak \"%s\": %s\n", t.words, ok ? "OK" : "FAIL (not treated as a tweak)");
        if (! ok) ++fails;
        if (single && String (t.words) != "a lot less reverb") cur = res.variations[0].values;
    }
    // refine toggle with a sound word makes fresh sounds
    {
        xost::Generator::Request r; r.prompt = "piano"; r.currentValues = cur; r.refine = true; r.seed = 5;
        auto res = gen.runNow (r);
        printf ("tweak with a sound word -> %d new sounds %s\n", (int) res.variations.size(), res.variations.size() == 4 ? "OK" : "FAIL");
        if (res.variations.size() != 4) ++fails;
    }
    // randomness: the same prompt again gives different sounds
    {
        xost::Generator::Request r; r.prompt = "dark pluck"; r.seed = 100;
        auto a = gen.runNow (r); r.seed = 101; auto b = gen.runNow (r);
        int same = 0; for (size_t i = 0; i < 4; ++i) if (a.variations[i].values == b.variations[i].values) ++same;
        printf ("generate again: %d/4 identical %s\n", same, same == 0 ? "OK" : "FAIL");
        if (same) ++fails;
    }
    // surprise prompts always parse and produce sounds
    {
        Random rr (42); int bad = 0;
        for (int i = 0; i < 12; ++i)
        {
            const auto p = xost::randomPrompt (rr);
            xost::Generator::Request r; r.prompt = p; r.seed = (juce::uint32) i;
            auto res = gen.runNow (r);
            if (res.variations.size() != 4 || res.understood.isEmpty()) ++bad;
            printf ("surprise: \"%s\" -> %s\n", p.toRawUTF8(), res.variations.empty() ? "-" : res.variations[0].name.toRawUTF8());
        }
        fails += bad;
    }
    return fails;
}

// ================================================================== one-shot export tests
static int oneShotTests()
{
    int fails = 0;
    XOSTProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    AudioFormatManager fm; fm.registerBasicFormats();
    StringArray cats { "Pluck", "Bass", "Pad", "Bell", "Keys", "Lead" };
    for (auto& cat : cats)
    {
        int idx = -1;
        for (int p = 1; p < Presets::count(); ++p) if (Presets::category (p) == cat) { idx = p; break; }
        if (idx < 0) continue;
        proc.setCurrentProgram (idx);
        String root;
        auto f = proc.exportOneShot (proc.oneShot, &root);
        std::unique_ptr<AudioFormatReader> rd (fm.createReaderFor (f));
        bool ok = rd != nullptr && rd->lengthInSamples > 1000;
        float startDb = -100, peakDb = -100; double secs = 0;
        if (ok)
        {
            AudioBuffer<float> b (2, (int) rd->lengthInSamples);
            rd->read (&b, 0, b.getNumSamples(), 0, true, true);
            const float pk = b.getMagnitude (0, b.getNumSamples());
            peakDb = Decibels::gainToDecibels (pk);
            startDb = Decibels::gainToDecibels (b.getMagnitude (0, std::min (b.getNumSamples(), 96)) / std::max (1.0e-9f, pk));   // first 2 ms
            secs = b.getNumSamples() / rd->sampleRate;
            ok = peakDb > -1.0f && peakDb < 0.0f && startDb > -50.0f && f.getFileName().contains ("(" + root + ")");
        }
        printf ("one-shot %-6s %-28s -> %-36s %5.2f s  peak %5.1f dB  start %5.1f dB  %s\n", cat.toRawUTF8(), Presets::name (idx).toRawUTF8(),
                f.getFileName().toRawUTF8(), secs, peakDb, startDb, ok ? "OK" : "FAIL");
        if (! ok) ++fails;
        f.deleteFile();
    }
    // the root note follows the oscillator octave: an octave-up patch exported at C5 must be named C6
    proc.setCurrentProgram (0);
    proc.apvts.getParameter ("oa_oct")->setValueNotifyingHost (proc.apvts.getParameter ("oa_oct")->convertTo0to1 (1.0f));
    String root; auto f = proc.exportOneShot (proc.oneShot, &root);
    printf ("one-shot root follows octave: %s %s\n", root.toRawUTF8(), root == "C6" ? "OK" : "FAIL");
    if (root != "C6") ++fails;
    f.deleteFile();
    return fails;
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const String outDir = argc > 1 ? argv[1] : "/tmp/xo_test";
    const bool onlyUi = argc > 2 && String (argv[2]) == "ui";
    File (outDir).createDirectory();

    const double sr = 48000.0;
    const int block = 512;
    const int len = (int) (sr * 6.0);
    const auto pattern = makePattern (sr, len);
    WavAudioFormat wav;
    auto writeWav = [&] (const AudioBuffer<float>& b, const String& name, int bits = 24)
    {
        auto f = File (outDir).getChildFile (name + ".wav");
        f.deleteFile();
        std::unique_ptr<OutputStream> os = std::make_unique<FileOutputStream> (f);
        auto w = wav.createWriterFor (os, AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (2).withBitsPerSample (bits));
        if (w) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    };

    int failures = 0;
    TestPlayHead ph;
    if (argc > 2 && String (argv[2]) == "defaults")   // print every parameter's default (used by the preset generator)
    {
        XOSTProcessor proc;
        for (auto* p : proc.getParameters())
            if (auto* rp = dynamic_cast<RangedAudioParameter*> (p))
                printf ("%s=%g\n", rp->getParameterID().toRawUTF8(), rp->convertFrom0to1 (rp->getDefaultValue()));
        return 0;
    }
    if (argc > 4 && String (argv[2]) == "demo")   // XOTest <dir> demo "<preset>" melody|chords|arp|bass
    {
        const double beat = 60.0 / 150.0;
        const int dlen = (int) (sr * (16 * beat + 2.0));
        MidiBuffer m;
        auto note = [&] (double startBeats, double lenBeats, int n, float vel)
        {
            m.addEvent (MidiMessage::noteOn (1, n, vel), (int) (sr * startBeats * beat));
            m.addEvent (MidiMessage::noteOff (1, n), (int) (sr * (startBeats + lenBeats) * beat));
        };
        const int chords[4][5] = { { 48, 55, 58, 62, 63 }, { 44, 51, 55, 60, 63 }, { 51, 55, 58, 62, 65 }, { 46, 53, 58, 62, 67 } };
        const String kind = argv[4];
        if (kind == "chords" || kind == "arp")
            for (int b = 0; b < 4; ++b) for (int k = 0; k < 5; ++k) note (b * 4.0, 3.9, chords[b][k] + (kind == "arp" ? 12 : 0) - (k == 0 && kind == "arp" ? 12 : 0), 0.85f);
        else if (kind == "roots")   // one key per bar: for chord-memory presets
        {
            const int roots[] = { 60, 56, 63, 58 };
            for (int b = 0; b < 4; ++b) { note (b * 4.0, 1.4, roots[b], 0.9f); note (b * 4.0 + 1.5, 1.0, roots[b], 0.7f); note (b * 4.0 + 2.75, 1.1, roots[b] + (b == 3 ? 2 : 0), 0.8f); }
        }
        else if (kind == "bass")
        {
            const double t[] = { 0, 0.75, 1.5, 2.5, 3.0, 4, 4.75, 5.5, 6.5, 7.25, 8, 8.75, 9.5, 10.5, 11, 12, 12.75, 13.5, 14, 14.5, 15 };
            const int n[] = { 36, 36, 43, 36, 39, 32, 32, 39, 32, 44, 39, 39, 46, 39, 41, 34, 34, 41, 46, 34, 43 };
            for (int i = 0; i < 21; ++i) note (t[i], (i + 1 < 21 ? t[i + 1] - t[i] : 1.0) * 0.95, n[i], 0.9f);
        }
        else
        {
            // bouncy 2-bar hook played twice, ending on a held note
            const double t[] = { 0, 0.5, 1, 1.5, 1.75, 2.5, 3, 3.5,   4, 4.5, 5, 5.5, 6, 6.5, 7, 7.25 };
            const int n[]   = { 72, 75, 79, 77, 75, 72, 70, 72,      75, 79, 82, 79, 77, 75, 74, 75 };
            for (int rep = 0; rep < 2; ++rep)
                for (int i = 0; i < 16; ++i)
                {
                    const double len = rep == 1 && i == 15 ? 0.7 : std::min (0.45, (i + 1 < 16 ? t[i + 1] - t[i] : 0.5) * 0.85);
                    note (rep * 8.0 + t[i], len, n[i] - (kind == "low" ? 12 : 0), i % 2 ? 0.75f : 1.0f);
                }
        }
        XOSTProcessor proc;
        proc.setPlayConfigDetails (0, 2, sr, block);
        proc.setPlayHead (&ph);
        for (int p = 0; p < Presets::count(); ++p) if (Presets::name (p) == argv[3]) proc.setCurrentProgram (p);
        if (argc > 5 && String (argv[5]) == "prompt")   // demo of a generated sound: XOSTTest <dir> demo "<words>" kind prompt [variation]
        {
            xost::Generator gen; xost::Generator::Request r; r.prompt = argv[3]; r.seed = 11;
            auto res = gen.runNow (r);
            const int vi = argc > 6 ? jlimit (0, 3, String (argv[6]).getIntValue()) : 0;
            if (! res.variations.empty()) proc.loadVariation (res.variations[(size_t) vi], r.prompt);
            printf ("%s\n", proc.getCurrentPresetName().toRawUTF8());
        }
        proc.prepareToPlay (sr, block);
        AudioBuffer<float> out (2, dlen);
        render (proc, out, m, block, sr, &ph);
        writeWav (out, String (argv[3]).replaceCharacter (' ', '_'));
        return 0;
    }
    if (argc > 3 && String (argv[2]) == "one")   // debug: XOTest <dir> one "<preset>" [id=value ...]
    {
        XOSTProcessor proc;
        proc.setPlayConfigDetails (0, 2, sr, block);
        proc.setPlayHead (&ph);
        for (int p = 0; p < Presets::count(); ++p) if (Presets::name (p) == argv[3]) proc.setCurrentProgram (p);
        for (int a = 4; a < argc; ++a)
        {
            auto kv = StringArray::fromTokens (argv[a], "=", "");
            if (auto* q = proc.apvts.getParameter (kv[0])) q->setValueNotifyingHost (q->convertTo0to1 (kv[1].getFloatValue()));
        }
        proc.prepareToPlay (sr, block);
        AudioBuffer<float> out (2, len);
        render (proc, out, pattern, block, sr, &ph);
        writeWav (out, "debug");
        const int w = (int) (sr * 0.005);
        for (int i = 0; i < 20; ++i) printf ("%.2f ", out.getMagnitude (0, i * w, w));
        printf ("\n");
        return 0;
    }
    if (argc > 2 && String (argv[2]) == "words")
    {
        const int f = wordTests (outDir, writeWav, true);
        printf ("%d failures\n", f);
        return f == 0 ? 0 : 1;
    }
    if (argc > 3 && String (argv[2]) == "lvl")
    {
        xost::Generator gen; xost::Generator::Request r; r.prompt = argv[3]; r.seed = 11;
        auto res = gen.runNow (r);
        XOSTProcessor a (true), b (true);
        for (auto& v : res.variations)
        {
            a.applyValues (v.values, XOSTProcessor::Scope::all);
            b.applyValues (v.values, XOSTProcessor::Scope::all);
            printf ("%s: %.2f %.2f %.2f\n", v.name.toRawUTF8(), xost::measureLoudness (a), xost::measureLoudness (a), xost::measureLoudness (b));
        }
        return 0;
    }
    if (argc > 2 && String (argv[2]) == "oneshot")
    {
        const int f = oneShotTests();
        printf ("%d failures\n", f);
        return f == 0 ? 0 : 1;
    }
    double worstCpu = 0; String worstName;
    if (! onlyUi)
    {
        XOSTProcessor proc;
        proc.setPlayConfigDetails (0, 2, sr, block);
        proc.setPlayHead (&ph);
        proc.prepareToPlay (sr, block);
        for (int p = 0; p < Presets::count(); ++p)
        {
            proc.setCurrentProgram (p);
            proc.prepareToPlay (sr, block);    // resets voices + FX tails between presets
            AudioBuffer<float> out (2, len);
            const auto t0 = std::chrono::high_resolution_clock::now();
            render (proc, out, pattern, block, sr, &ph);
            const double secs = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
            float peak; double rms; bool finite;
            stats (out, peak, rms, finite);
            const bool bad = ! finite || peak > 1.5f || rms < 1.0e-3;
            failures += bad ? 1 : 0;
            const double cpu = 100.0 * secs / 6.0;
            if (cpu > worstCpu) { worstCpu = cpu; worstName = Presets::name (p); }
            printf ("%s %3d %-8s %-26s peak %6.2f dB  rms %6.1f dB  st %6.1f dB  cpu %5.2f%%%s\n", bad ? "!!" : "  ", p + 1,
                    Presets::category (p).toRawUTF8(), Presets::name (p).toRawUTF8(),
                    Decibels::gainToDecibels (peak), Decibels::gainToDecibels ((float) rms),
                    xost::measureLoudness (proc), cpu, finite ? "" : "  NaN!");
            writeWav (out, String (p + 1).paddedLeft ('0', 4) + "_" + Presets::name (p).replaceCharacters (" /'", "___"), 16);
        }
        printf ("worst cpu %.2f%% (%s)\n", worstCpu, worstName.toRawUTF8());
        failures += wordTests (outDir, writeWav, false);
        failures += oneShotTests();

        // ---- quality modes on a heavy patch
        for (int q = 0; q < 4; ++q)
        {
            proc.setCurrentProgram (std::min (5, Presets::count() - 1));
            proc.apvts.getParameter ("quality")->setValueNotifyingHost (proc.apvts.getParameter ("quality")->convertTo0to1 ((float) q));
            proc.prepareToPlay (sr, block);
            AudioBuffer<float> out (2, len);
            const auto t0 = std::chrono::high_resolution_clock::now();
            render (proc, out, pattern, block, sr, &ph);
            const double secs = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
            float peak; double rms; bool finite; stats (out, peak, rms, finite);
            printf ("quality %d: cpu %.2f%%  rms %.1f dB %s\n", q, 100.0 * secs / 6.0, Decibels::gainToDecibels ((float) rms), finite ? "" : "NaN!");
            if (! finite) ++failures;
        }
        proc.apvts.getParameter ("quality")->setValueNotifyingHost (proc.apvts.getParameter ("quality")->convertTo0to1 (1.0f));

        // ---- randomize robustness
        int randBad = 0;
        for (int r = 0; r < 60; ++r)
        {
            proc.randomize (0);
            proc.prepareToPlay (sr, block);
            AudioBuffer<float> out (2, len / 2);
            render (proc, out, pattern, block, sr, &ph);
            float peak; double rms; bool finite; stats (out, peak, rms, finite);
            if (! finite || peak > 4.0f) { ++randBad; printf ("random %d: peak %.1f dB %s\n", r, Decibels::gainToDecibels (peak), finite ? "" : "NaN"); }
            if (r < 3) writeWav (out, "zz_random_" + String (r));
        }
        printf ("randomize: %d/60 bad\n", randBad);
        failures += randBad;

        // ---- Matrix + Minirack factory banks, mutate / breed / morph, tool preset save + load
        {
            int bad = 0;
            auto check = [&] (const String& what, int seconds)
            {
                proc.prepareToPlay (sr, block);
                AudioBuffer<float> out (2, (int) sr * seconds);
                render (proc, out, pattern, block, sr, &ph);
                float peak; double rms; bool finite; stats (out, peak, rms, finite);
                if (! finite || peak > 4.0f || rms < 1.0e-4) { ++bad; printf ("  !! %s: peak %.1f dB rms %.1f dB %s\n", what.toRawUTF8(), Decibels::gainToDecibels (peak), Decibels::gainToDecibels ((float) rms), finite ? "" : "NaN"); }
            };
            for (int i = 0; i < (int) Presets::matrixBank().size(); ++i)
            {
                proc.setCurrentProgram (1 + (i * 37) % (Presets::count() - 1));
                proc.loadToolFactory (XOSTProcessor::Scope::matrix, i);
                check ("matrix " + Presets::matrixBank()[(size_t) i].name, 3);
            }
            for (int i = 0; i < (int) Presets::rackBank().size(); ++i)
            {
                proc.setCurrentProgram (1 + (i * 53) % (Presets::count() - 1));
                proc.loadToolFactory (XOSTProcessor::Scope::rack, i);
                check ("rack " + Presets::rackBank()[(size_t) i].name, 3);
            }
            for (int i = 0; i < 30; ++i)
            {
                proc.setCurrentProgram (1 + (i * 97) % (Presets::count() - 1));
                proc.mutate (i % 3 == 0 ? 0.65f : 0.3f, 0);
                if (i % 2) proc.blendWith (Presets::factory()[(size_t) (1 + (i * 131) % (Presets::count() - 1))].values, 0.5f, 0, i % 4 == 1, "test");
                check ("mutate " + String (i), 2);
            }
            proc.setCurrentProgram (5);
            proc.loadToolFactory (XOSTProcessor::Scope::matrix, 3);
            const auto beforeMx = proc.captureValues (XOSTProcessor::Scope::matrix);
            auto tmp = File::getSpecialLocation (File::tempDirectory).getChildFile ("xo_test.xostmatrix");
            proc.saveToolPreset (XOSTProcessor::Scope::matrix, tmp, "Test");
            proc.loadToolFactory (XOSTProcessor::Scope::matrix, 10);
            proc.loadToolPreset (XOSTProcessor::Scope::matrix, tmp);
            const bool same = proc.captureValues (XOSTProcessor::Scope::matrix) == beforeMx;
            printf ("tools: %d bad renders, matrix preset roundtrip %s\n", bad, same ? "OK" : "DIFFERENT");
            failures += bad + (same ? 0 : 1);
        }

        // ---- undo
        {
            proc.initPatch();
            proc.undo.clearUndoHistory();
            auto* prm = proc.apvts.getParameter ("f1_cutoff");
            const float before = prm->getValue();
            proc.undo.beginNewTransaction ("cutoff");
            prm->setValueNotifyingHost (0.2f);
            MessageManager::getInstance()->runDispatchLoopUntil (200);
            proc.undo.beginNewTransaction();
            const bool undone = proc.undo.undo();
            MessageManager::getInstance()->runDispatchLoopUntil (200);
            const float after = prm->getValue();
            printf ("undo: %s (%.3f -> 0.2 -> %.3f)\n", undone && std::abs (after - before) < 1.0e-4f ? "OK" : "FAILED", before, after);
            if (std::abs (after - before) > 1.0e-4f) ++failures;
        }

        // ---- state roundtrip incl. user wavetable + fx order
        {
            proc.setCurrentProgram (std::min (10, Presets::count() - 1));
            proc.resampleToWavetable (1, 48);
            proc.setFxOrder ({ 8, 7, 6, 5, 4, 3, 2, 1, 0 });
            MemoryBlock mb; proc.getStateInformation (mb);
            XOSTProcessor other;
            other.setStateInformation (mb.getData(), (int) mb.getSize());
            int diffs = 0;
            for (auto* p : proc.getParameters())
                if (auto* rp = dynamic_cast<RangedAudioParameter*> (p))
                    if (std::abs (rp->getValue() - other.apvts.getParameter (rp->paramID)->getValue()) > 1.0e-4f) ++diffs;
            const bool tableOk = other.getUserWavetable (1) != nullptr;
            const bool orderOk = other.getFxOrder()[0] == 8;
            printf ("state roundtrip: %d param diffs, user table %s, fx order %s\n", diffs, tableOk ? "OK" : "MISSING", orderOk ? "OK" : "WRONG");
            if (diffs || ! tableOk || ! orderOk) ++failures;
        }

        // ---- bounce
        {
            proc.setCurrentProgram (0);
            auto f = proc.bounceToFile (60, 2.0);
            printf ("bounce: %s (%lld bytes)\n", f.getFullPathName().toRawUTF8(), (long long) f.getSize());
            if (f.getSize() < 10000) ++failures;
        }
        // ---- sampler: load the bounce as a sample and play it in each mode
        {
            auto f = XOSTProcessor::getUserFolder ("Bounces").findChildFiles (File::findFiles, false, "*.wav");
            if (! f.isEmpty())
            {
                for (int mode = 0; mode < 3; ++mode)
                {
                    proc.initPatch();
                    proc.loadSampleFile (f[0]);
                    auto set = [&] (const char* id, float v) { auto* q = proc.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); };
                    set ("oa_on", 0); set ("smp_on", 1); set ("smp_mode", (float) mode);
                    proc.prepareToPlay (sr, block);
                    AudioBuffer<float> out (2, len);
                    render (proc, out, pattern, block, sr, &ph);
                    float peak; double rms; bool finite; stats (out, peak, rms, finite);
                    printf ("sampler mode %d: rms %.1f dB %s\n", mode, Decibels::gainToDecibels ((float) rms), finite && rms > 1.0e-3 ? "OK" : "FAIL");
                    if (! finite || rms < 1.0e-3) ++failures;
                    writeWav (out, "zz_sampler_mode" + String (mode));
                }
            }
            else { printf ("sampler: could not load bounce\n"); ++failures; }
        }
    }

    // ---- editor snapshots
    {
        XOSTProcessor proc;
        proc.setPlayConfigDetails (0, 2, sr, block);
        proc.setPlayHead (&ph);
        proc.setCurrentProgram (std::min (3, Presets::count() - 1));
        proc.prepareToPlay (sr, block);
        std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
        ed->setVisible (true);
        std::function<Component* (Component*, std::function<bool (Component*)>)> findComp;
        findComp = [&] (Component* c, std::function<bool (Component*)> pred) -> Component*
        {
            for (auto* ch : c->getChildren()) { if (pred (ch)) return ch; if (auto* r = findComp (ch, pred)) return r; }
            return nullptr;
        };
        AudioBuffer<float> out (2, (int) sr);
        auto feed = [&]
        {
            MidiBuffer pat = makePattern (sr, (int) sr);
            render (proc, out, pat, block, sr, &ph);
            MessageManager::getInstance()->runDispatchLoopUntil (120);
            if (auto* sc = dynamic_cast<Scope*> (findComp (ed.get(), [] (Component* c) { return dynamic_cast<Scope*> (c) != nullptr; }))) sc->pull();
        };
        auto snap = [&] (const String& name)
        {
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            auto f = File (outDir).getChildFile (name);
            f.deleteFile();
            FileOutputStream fos (f);
            PNGImageFormat().writeImageToStream (img, fos);
            printf ("UI snapshot %s %dx%d\n", name.toRawUTF8(), img.getWidth(), img.getHeight());
        };
        auto button = [&] (const String& text) { return dynamic_cast<TextButton*> (findComp (ed.get(), [&] (Component* c) {
            auto* b = dynamic_cast<TextButton*> (c); return b != nullptr && b->getButtonText() == text; })); };

        // a couple of routings so the mod rings show
        ui::addModulation (proc, xo::srcLfo1, xo::dstAPos);
        ui::addModulation (proc, xo::srcEnv2, xo::dstF1Cutoff);
        feed();
        snap ("ui_osc.png");
        // ---- text-to-sound: type a description, generate, export a one-shot
        if (auto* te = dynamic_cast<TextEditor*> (findComp (ed.get(), [] (Component* c) { return dynamic_cast<TextEditor*> (c) != nullptr && dynamic_cast<PromptBar*> (c->getParentComponent()) != nullptr; })))
        {
            te->setText ("dark glassy pluck like pierre bourne with tape", true);
            if (auto* g = button ("GENERATE")) g->onClick();
            for (int k = 0; k < 600 && proc.create.variations.empty(); ++k) MessageManager::getInstance()->runDispatchLoopUntil (50);
            if (auto* b = button ("EXPORT ONE-SHOT")) b->onClick();
            for (int k = 0; k < 4; ++k) feed();
            snap ("ui_create.png");
            printf ("UI generate: %d variations, preset now \"%s\", one-shot %s\n", (int) proc.create.variations.size(),
                    proc.getCurrentPresetName().toRawUTF8(), proc.lastOneShot.getFileName().toRawUTF8());
            if (proc.create.variations.size() != 4 || ! proc.lastOneShot.existsAsFile()) ++failures;
            te->setText ("phonk cowbell very distorted", true);
            for (int k = 0; k < 3; ++k) feed();
            snap ("ui_typing.png");
            if (auto* b = button ("PRESETS")) { b->setToggleState (true, dontSendNotification); b->onClick(); feed(); snap ("ui_presets.png"); }
            if (auto* b = button ("CREATE")) { b->setToggleState (true, dontSendNotification); b->onClick(); }
        }
        for (auto t : { "MOD", "ROUTING", "FX", "ARP", "MATRIX", "MINIRACK", "MUTATE" })
            if (auto* b = button (t))
            {
                if (String (t) == "MATRIX") proc.loadToolFactory (XOSTProcessor::Scope::matrix, 10);
                if (String (t) == "MINIRACK") proc.loadToolFactory (XOSTProcessor::Scope::rack, 27);
                b->setToggleState (true, dontSendNotification); b->onClick(); for (int k = 0; k < 6; ++k) feed(); snap (String ("ui_") + String (t).toLowerCase() + ".png");
            }
        if (auto* b = button ("BROWSE")) { b->onClick(); feed(); snap ("ui_browser.png"); }
        static_cast<XOSTEditor*> (ed.get())->setScale (0.75);
        feed(); snap ("ui_scaled75.png");
    }

    printf ("%d failures\n", failures);
    return failures == 0 ? 0 : 1;
}
