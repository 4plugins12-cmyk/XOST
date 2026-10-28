#include "WordEngine.h"
#include "PluginProcessor.h"
#include "Presets.h"
#include "BinaryData.h"
#include <cmath>

using namespace juce;

namespace xost
{
// ================================================================== tables mirrored from tools/xo_lib.py
namespace
{
    const StringArray divNames { "8bar", "4bar", "2bar", "1bar", "1/2", "1/2t", "1/4.", "1/4", "1/4t", "1/8.", "1/8", "1/8t",
                                 "1/16.", "1/16", "1/16t", "1/32", "1/64" };
    const StringArray lfoShapes { "sine", "tri", "sawup", "sawdn", "square", "sh", "smooth", "bounce", "stairs", "pump" };
    const StringArray arpModes { "up", "down", "updown", "random", "played", "chord", "seq" };
    const StringArray chordNames { "off", "minor", "major", "min7", "maj7", "min9", "maj9", "madd9", "sus2", "sus4", "power", "oct", "dim", "phryg" };
    const StringArray dstShort { "none", "apitch", "apos", "alevel", "apan", "adetune", "awarp", "amod", "bpitch", "bpos", "blevel", "bpan", "bdetune",
                                 "bwarp", "bmod", "cpitch", "cpos", "clevel", "cpan", "cdetune", "cwarp", "cmod", "sub", "noise", "noisecol", "smplevel",
                                 "smppos", "grainsize", "cut1", "res1", "drive1", "cut2", "res2", "shpdrive", "amp", "pan", "lfo1rate", "lfo2rate",
                                 "lfo3rate", "lfo4rate", "distdrive", "distmix", "chorusmix", "phasermix", "delaymix", "delayfb", "revmix", "revsize", "master" };
    constexpr int srcEnv3 = 3, srcLfo1 = 4;

    // starting points when a word switches an effect on (so "a little reverb" really is a little)
    const std::map<String, std::vector<std::pair<const char*, float>>>& freshFx()
    {
        static const std::map<String, std::vector<std::pair<const char*, float>>> m {
            { "rev",  { { "rev_mix", 0.08f }, { "rev_size", 0.55f }, { "rev_decay", 2.0f } } },
            { "dly",  { { "dly_mix", 0.06f }, { "dly_fb", 0.3f }, { "dly_l", 9.0f }, { "dly_r", 10.0f } } },
            { "ch",   { { "ch_mix", 0.15f } } },
            { "ph",   { { "ph_mix", 0.15f } } },
            { "fl",   { { "fl_mix", 0.15f } } },
            { "dist", { { "dist_drive", 0.15f }, { "dist_mix", 0.3f } } },
            { "tape", { { "tape_wow", 0.1f }, { "tape_flutter", 0.08f }, { "tape_sat", 0.15f }, { "tape_hiss", 0.02f }, { "tape_age", 0.15f }, { "tape_mix", 1.0f } } },
            { "comp", { { "comp_ratio", 2.0f }, { "comp_gain", 1.0f } } },
            { "eq",   {} },
            { "noise", { { "noise_level", 0.03f } } },
            { "sub",  { { "sub_level", 0.2f }, { "sub_oct", 1.0f } } },
            { "shp",  { { "shp_drive", 0.1f } } },
        };
        return m;
    }

    String prefixOf (const String& id) { return id.upToFirstOccurrenceOf ("_", false, false); }

    String titleCase (const String& s)
    {
        StringArray w; w.addTokens (s.toLowerCase(), " ", "");
        for (auto& x : w) if (x.isNotEmpty()) x = x.substring (0, 1).toUpperCase() + x.substring (1);
        return w.joinIntoString (" ");
    }

    int levenshtein (const String& a, const String& b)
    {
        const int n = a.length(), m = b.length();
        std::vector<int> prev ((size_t) m + 1), cur ((size_t) m + 1);
        for (int j = 0; j <= m; ++j) prev[(size_t) j] = j;
        for (int i = 1; i <= n; ++i)
        {
            cur[0] = i;
            for (int j = 1; j <= m; ++j)
                cur[(size_t) j] = std::min ({ prev[(size_t) j] + 1, cur[(size_t) j - 1] + 1, prev[(size_t) j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1) });
            std::swap (prev, cur);
        }
        return prev[(size_t) m];
    }

    std::vector<Weighted> parseWeighted (const String& s)
    {
        std::vector<Weighted> out;
        for (auto& t : StringArray::fromTokens (s, " ", ""))
        {
            if (t.isEmpty()) continue;
            Weighted w;
            w.key = t.upToFirstOccurrenceOf ("*", false, false);
            w.w = t.containsChar ('*') ? t.fromFirstOccurrenceOf ("*", false, false).getFloatValue() : 1.0f;
            out.push_back (w);
        }
        return out;
    }

    std::vector<Op> parseOps (const String& s, StringArray& problems)
    {
        std::vector<Op> out;
        // tokens are separated by spaces, except inside @fn(...)
        StringArray toks; String cur; int depth = 0;
        for (auto c : s)
        {
            if (c == '(') ++depth;
            if (c == ')') --depth;
            if (c == ' ' && depth == 0) { if (cur.isNotEmpty()) toks.add (cur); cur.clear(); }
            else cur << String::charToString (c);
        }
        if (cur.isNotEmpty()) toks.add (cur);
        for (auto& t : toks)
        {
            Op op;
            if (t.startsWithChar ('@'))
            {
                op.type = '@';
                op.id = t.substring (1).upToFirstOccurrenceOf ("(", false, false);
                op.args.addTokens (t.fromFirstOccurrenceOf ("(", false, false).upToLastOccurrenceOf (")", false, false), ",", "");
                op.args.trim();
                op.args.removeEmptyStrings();
                out.push_back (op);
                continue;
            }
            String head, rest = t;
            if (t.startsWith ("o*_") || t.startsWith ("f*_")) { head = t.substring (0, 3); rest = t.substring (3); }
            const int pos = rest.indexOfAnyOf ("*+=~-");
            if (pos <= 0) { problems.add ("bad op: " + t); continue; }
            op.id = head + rest.substring (0, pos);
            const auto c = rest[pos];
            op.value = rest.substring (pos + 1).getFloatValue();
            op.type = c == '-' ? '+' : (char) c;
            if (c == '-') op.value = -op.value;
            out.push_back (op);
        }
        return out;
    }
}

// ================================================================== Lexicon
String Lexicon::normalise (const String& in)
{
    String s = in.toLowerCase().replace ("’", "'");
    String out;
    for (auto c : s)
    {
        if (CharacterFunctions::isLetterOrDigit (c) || c == '\'' || c == '&' || c == '/' || c == '!') out << String::charToString (c);
        else if (c == ',' || c == '.' || c == ';') out << " , ";
        else out << " ";
    }
    return out.replace ("!", "").trim();
}

const Lexicon& Lexicon::get()
{
    static const Lexicon lex (String::fromUTF8 (BinaryData::Lexicon_txt, BinaryData::Lexicon_txtSize));
    return lex;
}

Lexicon::Lexicon (const String& text)
{
    StringArray lines; lines.addLines (text);
    for (auto line : lines)
    {
        line = line.trim();
        if (line.isEmpty() || line.startsWithChar ('#')) continue;
        StringArray parts; parts.addTokens (line, "|", "");
        parts.trim();
        if (parts.size() < 2) { problems.add ("bad line: " + line); continue; }
        LexEntry e;
        const auto t = parts[0];
        if (t == "cat") e.type = EntryType::cat;
        else if (t == "kind") e.type = EntryType::kind;
        else if (t == "style") e.type = EntryType::style;
        else if (t == "char") e.type = EntryType::chr;
        else if (t == "fx") e.type = EntryType::fx;
        else if (t == "amount") e.type = EntryType::amount;
        else if (t == "neg") e.type = EntryType::neg;
        else if (t == "refine") e.type = EntryType::refine;
        else if (t == "stop") e.type = EntryType::stop;
        else { problems.add ("bad type: " + line); continue; }

        StringArray ph; ph.addTokens (parts[1], ",", ""); ph.trim(); ph.removeEmptyStrings();
        const String payload = parts.size() > 2 ? parts[2] : String();
        if (e.type == EntryType::amount || e.type == EntryType::refine) e.amount = payload.getFloatValue();
        else
        {
            for (auto f : StringArray::fromTokens (payload, ";", ""))
            {
                f = f.trim();
                const auto key = f.upToFirstOccurrenceOf ("=", false, false).trim();
                const auto val = f.fromFirstOccurrenceOf ("=", false, false).trim();
                if (key == "cat") e.category = val;
                else if (key == "label") e.label = val;
                else if (key == "tags") e.tags = parseWeighted (val);
                else if (key == "kinds") e.kinds = parseWeighted (val);
                else if (key == "ops") e.ops = parseOps (val, problems);
                else if (key == "neg") e.neg = parseOps (val, problems);
                else if (key == "use")
                {
                    for (auto& u : StringArray::fromTokens (val, " ", ""))
                        if (u.isNotEmpty()) e.uses.push_back ({ u.upToFirstOccurrenceOf (":", false, false), u.fromFirstOccurrenceOf (":", false, false).getFloatValue() });
                }
                else if (f.isNotEmpty()) problems.add ("bad field '" + f + "' in: " + line);
            }
        }
        if (e.type == EntryType::kind && ! e.kinds.empty()) e.category = e.kinds[0].key.upToFirstOccurrenceOf (".", false, false);
        if (e.label.isEmpty() && ! ph.isEmpty()) e.label = ph[0].trimCharactersAtEnd ("+").toUpperCase();

        const int idx = (int) list.size();
        list.push_back (e);
        for (auto p : ph)
        {
            const bool hint = p.endsWithChar ('+');
            p = normalise (p.trimCharactersAtEnd ("+"));
            if (p.isEmpty()) continue;
            auto it = phrases.find (p);
            if (it != phrases.end() && it->second.first != idx) problems.add ("duplicate phrase: '" + p + "'");
            phrases[p] = { idx, hint };
            const int words = StringArray::fromTokens (p, " ", "").size();
            maxWords = std::max (maxWords, words);
            if (words == 1 && p.length() >= 4 && e.type != EntryType::stop) singles.add (p);
        }
    }
}

int Lexicon::find (const String& phrase, bool& hint) const
{
    auto it = phrases.find (phrase);
    if (it == phrases.end()) return -1;
    hint = it->second.second;
    return it->second.first;
}

int Lexicon::fuzzy (const String& w) const
{
    if (w.length() < 4) return -1;
    const int allowed = w.length() >= 7 ? 2 : 1;
    int best = -1, bestD = 99;
    for (auto& s : singles)
    {
        if (std::abs (s.length() - w.length()) > allowed) continue;
        const int d = levenshtein (w, s);
        if (d < bestD) { bestD = d; best = phrases.at (s).first; }
    }
    return bestD <= allowed ? best : -1;
}

int Lexicon::findLabel (const String& name) const
{
    bool h = false;
    return find (normalise (name), h);
}

StringArray Lexicon::phrasesOf (EntryType t) const
{
    StringArray out;
    for (auto& [p, v] : phrases) if (list[(size_t) v.first].type == t) out.add (p);
    return out;
}

// ================================================================== understanding a prompt
Understanding understand (const String& text)
{
    const auto& lex = Lexicon::get();
    Understanding u;
    StringArray words; words.addTokens (Lexicon::normalise (text), " ", "");
    words.removeEmptyStrings();

    float amount = 1.0f, refineK = 0.0f;
    bool neg = false, negCarry = false, lastConj = false;
    String prefix;
    const auto& L = lex.entries();

    for (int i = 0; i < words.size();)
    {
        if (words[i] == ",") { lastConj = negCarry; ++i; continue; }
        int idx = -1, used = 1; bool hint = false;
        String typed;
        for (int n = std::min (lex.maxWords, words.size() - i); n >= 1 && idx < 0; --n)
        {
            StringArray sub; for (int k = 0; k < n; ++k) sub.add (words[i + k]);
            if (sub.contains (",")) continue;
            idx = lex.find (sub.joinIntoString (" "), hint);
            if (idx >= 0) { used = n; typed = sub.joinIntoString (" ").toUpperCase(); }
        }
        bool corrected = false;
        const String word = words[i];
        if (idx < 0 && word.endsWithChar ('s')) { idx = lex.find (word.dropLastCharacters (1), hint); typed = word.toUpperCase(); }
        if (idx < 0) { idx = lex.fuzzy (word); corrected = idx >= 0; }
        if (corrected || typed.isEmpty()) typed = idx >= 0 ? L[(size_t) idx].label : String();
        i += used;
        if (idx < 0)
        {
            if (word.length() > 1 && ! word.containsOnly ("0123456789"))
            {
                u.unknown.add (word);
                Chip c; c.text = word; c.unknown = true; u.chips.push_back (c);
            }
            lastConj = false;
            continue;
        }
        const auto& e = L[(size_t) idx];
        if (e.type == EntryType::stop)
        {
            if (word == "and" || word == "or" || word == "nor" || word == "&") lastConj = negCarry;
            else if (word == "with" || word == "but") { neg = negCarry = lastConj = false; }
            continue;
        }
        if (e.type == EntryType::amount) { amount *= e.amount; prefix = e.amount < 1 ? "A LITTLE " : (e.amount > 2 ? "EXTREMELY " : "VERY "); continue; }
        if (e.type == EntryType::neg) { neg = true; continue; }
        if (e.type == EntryType::refine) { refineK = e.amount; u.refineHint = true; prefix = e.amount > 0 ? "MORE " : "LESS "; continue; }

        ++u.recognised;
        Chip chip; chip.type = e.type; chip.amount = amount;
        if (corrected) chip.text = "~";
        const bool negated = (neg || (negCarry && lastConj)) && (e.type == EntryType::chr || e.type == EntryType::fx);
        if (e.type == EntryType::cat)
        {
            u.cats[e.category] += 1.0f; u.hasSound = true;
            chip.text << typed;
            neg = negCarry = false;
        }
        else if (e.type == EntryType::kind)
        {
            for (auto& w : e.kinds) u.kinds[w.key] += w.w;
            u.hasSound = true;
            chip.text << typed;
            neg = negCarry = false;
        }
        else if (e.type == EntryType::style)
        {
            for (auto& w : e.tags) u.tags[w.key] += w.w * amount;
            for (auto& w : e.kinds) u.prefKinds[w.key] += w.w * amount;
            u.hasStyle = true;
            u.styleLabels.addIfNotAlreadyThere (e.label);
            u.applied.push_back ({ idx, amount, false });
            chip.text << prefix << e.label << " STYLE";
            neg = negCarry = false;
        }
        else   // character / effect
        {
            float k = amount * (std::abs (refineK) > 1.0e-6f ? refineK : 1.0f);
            if (negated) k = -1.0f;
            for (auto& w : e.tags) u.tags[w.key] += negated ? -w.w : w.w * std::min (1.5f, std::max (-1.0f, k));
            if (! negated && k > 0) for (auto& w : e.kinds) u.prefKinds[w.key] += 0.5f * w.w;
            if (! negated && k > 0 && e.type == EntryType::chr) u.adjectives.addIfNotAlreadyThere (titleCase (e.label));
            if (hint) u.refineHint = true;
            u.applied.push_back ({ idx, k, negated });
            chip.negated = negated;
            chip.text << (negated ? "NO " : prefix) << typed;
            if (negated) u.refineHint = true;
            negCarry = negated;
            neg = false;
        }
        lastConj = false;
        amount = 1.0f; refineK = 0.0f; prefix.clear();
        u.chips.push_back (chip);
    }
    return u;
}

// ================================================================== patch values
ParamTable makeParamTable (AudioProcessor& p)
{
    ParamTable t;
    for (auto* ap : p.getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (ap))
        {
            ParamInfo i;
            i.range = rp->getNormalisableRange();
            i.def = rp->convertFrom0to1 (rp->getDefaultValue());
            i.discrete = rp->isDiscrete() || dynamic_cast<AudioParameterChoice*> (rp) != nullptr || dynamic_cast<AudioParameterInt*> (rp) != nullptr;
            i.steps = rp->getNumSteps();
            t[rp->getParameterID()] = i;
        }
    return t;
}

void PatchValues::load (const String& values, const ParamTable& t)
{
    v.clear();
    for (auto& [id, info] : t) v[id] = info.def;
    for (int l = 0; l < 4; ++l) lanes[l].clear();
    for (const auto& tok : StringArray::fromTokens (values, " ", ""))
    {
        const int eq = tok.indexOfChar ('=');
        if (eq <= 0) continue;
        const auto id = tok.substring (0, eq);
        if (id.length() == 3 && id.startsWith ("mx") && CharacterFunctions::isDigit (id[2])) { lanes[id.getTrailingIntValue() & 3] = tok.substring (eq + 1); continue; }
        if (t.count (id)) v[id] = tok.substring (eq + 1).getFloatValue();
    }
}

String PatchValues::toString (const ParamTable& t, bool onlyNonDefault) const
{
    StringArray parts;
    for (auto& [id, x] : v)
    {
        auto it = t.find (id);
        if (it == t.end() || id == "quality") continue;
        if (onlyNonDefault && std::abs (x - it->second.def) < 1.0e-5f) continue;
        String num = it->second.discrete ? String (roundToInt (x)) : String (x, 4);
        if (num.containsChar ('.')) num = num.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
        if (num == "-0") num = "0";
        parts.add (id + "=" + num);
    }
    for (int l = 0; l < 4; ++l) if (lanes[l].isNotEmpty()) parts.add ("mx" + String (l) + "=" + lanes[l]);
    return parts.joinIntoString (" ");
}

void sanitise (PatchValues& pv, const ParamTable& t)
{
    for (auto& [id, x] : pv.v)
    {
        auto it = t.find (id);
        if (it == t.end()) continue;
        const auto& r = it->second.range;
        if (! std::isfinite (x)) x = it->second.def;
        x = jlimit (r.start, r.end, x);
        if (it->second.discrete) x = (float) roundToInt (x);
    }
}

namespace
{
    bool oscOn (const PatchValues& pv, int o) { return pv.get (String ("o") + String::charToString ((juce_wchar) ('a' + o)) + "_on") > 0.5f; }

    StringArray expand (const String& id, const PatchValues& pv)
    {
        StringArray out;
        if (id.startsWith ("o*_"))
        {
            for (int o = 0; o < 3; ++o) if (oscOn (pv, o)) out.add (String ("o") + String::charToString ((juce_wchar) ('a' + o)) + id.substring (2));
        }
        else if (id.startsWith ("f*_"))
        {
            for (int f = 1; f <= 2; ++f) if (pv.get ("f" + String (f) + "_on") > 0.5f) out.add ("f" + String (f) + id.substring (2));
        }
        else out.add (id);
        return out;
    }

    void ensureOn (PatchValues& pv, const String& id)
    {
        if (id.endsWith ("_on")) return;
        const auto pre = prefixOf (id);
        auto& fresh = freshFx();
        auto it = fresh.find (pre);
        if (it == fresh.end()) return;
        const auto onId = pre + "_on";
        if (pv.get (onId) > 0.5f) return;
        pv.v[onId] = 1.0f;
        for (auto& [k, x] : it->second) pv.v[k] = x;
        if (pre == "eq") { pv.v["eq_low"] = 0; pv.v["eq_mid"] = 0; pv.v["eq_high"] = 0; }
    }

    void set (PatchValues& pv, const String& id, float x, const ParamTable& t)
    {
        auto it = t.find (id);
        if (it == t.end()) return;
        pv.v[id] = jlimit (it->second.range.start, it->second.range.end, x);
    }

    int freeSlot (const PatchValues& pv)
    {
        for (int s = 1; s <= 16; ++s) if (pv.get ("mm" + String (s) + "_src") < 0.5f) return s;
        return -1;
    }

    void applyBankEntry (PatchValues& pv, const Presets::Entry& e, const String& prefix, const ParamTable& t)
    {
        for (auto& [id, info] : t) if (id.startsWith (prefix)) pv.v[id] = info.def;
        if (prefix == "mx") for (int l = 0; l < 4; ++l) pv.lanes[l].clear();
        for (const auto& tok : StringArray::fromTokens (e.values, " ", ""))
        {
            const int eq = tok.indexOfChar ('=');
            if (eq <= 0) continue;
            const auto id = tok.substring (0, eq);
            if (prefix == "mx" && id.length() == 3 && id.startsWith ("mx") && CharacterFunctions::isDigit (id[2])) { pv.lanes[id.getTrailingIntValue() & 3] = tok.substring (eq + 1); continue; }
            if (id.startsWith (prefix) && t.count (id)) pv.v[id] = tok.substring (eq + 1).getFloatValue();
        }
    }

    void applyFunction (PatchValues& pv, const Op& op, float k, const ParamTable& t, Random& rng)
    {
        const auto& a = op.args;
        const auto fn = op.id;
        if (fn == "lfo" && a.size() >= 3)
        {
            const int dst = dstShort.indexOf (a[0]);
            if (dst <= 0) return;
            if (k < 0)
            {
                for (int s = 1; s <= 16; ++s)
                {
                    const auto p = "mm" + String (s) + "_";
                    const int src = (int) pv.get (p + "src");
                    if (src >= srcLfo1 && src < srcLfo1 + 4 && (int) pv.get (p + "dst") == dst) pv.v[p + "amt"] *= std::max (0.0f, 1.0f + k);
                }
                return;
            }
            int lfo = -1;
            for (int l : { 3, 2, 1, 0 })
            {
                bool usedL = false;
                for (int s = 1; s <= 16; ++s) if ((int) pv.get ("mm" + String (s) + "_src") == srcLfo1 + l) usedL = true;
                if (! usedL) { lfo = l; break; }
            }
            const int slot = freeSlot (pv);
            if (lfo < 0 || slot < 0) return;
            const auto L = "l" + String (lfo + 1) + "_";
            const int shape = a.size() > 3 ? jmax (0, lfoShapes.indexOf (a[3])) : 0;
            pv.v[L + "shape"] = (float) shape;
            pv.v[L + "mode"] = 1.0f;
            if (a[2].startsWith ("free")) { pv.v[L + "sync"] = 0.0f; set (pv, L + "rate", a[2].substring (4).getFloatValue(), t); }
            else { pv.v[L + "sync"] = 1.0f; pv.v[L + "div"] = (float) jmax (0, divNames.indexOf (a[2].toLowerCase())); }
            const auto S = "mm" + String (slot) + "_";
            pv.v[S + "src"] = (float) (srcLfo1 + lfo);
            pv.v[S + "dst"] = (float) dst;
            pv.v[S + "amt"] = jlimit (-1.0f, 1.0f, a[1].getFloatValue() * std::min (k, 2.2f));
        }
        else if ((fn == "mx" || fn == "rk") && a.size() >= 1)
        {
            const bool mx = fn == "mx";
            if (k < 0) { pv.v[mx ? "mx_on" : "rk_on"] = 0.0f; return; }
            const auto& bank = mx ? Presets::matrixBank() : Presets::rackBank();
            std::vector<const Presets::Entry*> pool;
            for (auto& e : bank) if (e.category.equalsIgnoreCase (a[0])) pool.push_back (&e);
            if (pool.empty()) return;
            applyBankEntry (pv, *pool[(size_t) rng.nextInt ((int) pool.size())], mx ? "mx" : "rk", t);
            if (mx) { pv.v["mx_on"] = 1.0f; pv.v["mx_mix"] = jlimit (0.3f, 1.0f, 0.55f + 0.3f * k); }
            else    { pv.v["rk_on"] = 1.0f; pv.v["rk_depth"] = jlimit (0.3f, 1.0f, 0.5f + 0.4f * k); }
        }
        else if (fn == "chord" && a.size() >= 1)
        {
            if (k < 0) { pv.v["chord_type"] = 0.0f; return; }
            pv.v["chord_type"] = (float) jmax (0, chordNames.indexOf (a[0]));
            if (a.size() > 1) pv.v["chord_voicing"] = (float) a[1].getIntValue();
            if (a.size() > 2) set (pv, "chord_strum", a[2].getFloatValue(), t);
            pv.v["polyphony"] = std::max (pv.get ("polyphony"), 24.0f);
            if (pv.get ("voice_mode") > 0.5f) pv.v["voice_mode"] = 0.0f;
        }
        else if (fn == "arp" && a.size() >= 1)
        {
            if (k < 0) { pv.v["arp_on"] = 0.0f; return; }
            pv.v["arp_on"] = 1.0f;
            pv.v["arp_mode"] = (float) jmax (0, arpModes.indexOf (a[0]));
            if (a.size() > 1) pv.v["arp_rate"] = (float) jmax (0, divNames.indexOf (a[1]));
            if (a.size() > 2) pv.v["arp_oct"] = (float) a[2].getIntValue();
        }
        else if (fn == "oct" && a.size() >= 1)
        {
            const int n = a[0].getIntValue() * (k < 0 ? -1 : 1) * (std::abs (k) > 1.8f ? 2 : 1);
            for (int o = 0; o < 3; ++o)
                if (oscOn (pv, o))
                {
                    const auto id = String ("o") + String::charToString ((juce_wchar) ('a' + o)) + "_oct";
                    pv.v[id] = (float) jlimit (-3, 3, (int) pv.get (id) + n);
                }
        }
        else if (fn == "mono")
        {
            if (k < 0) { pv.v["voice_mode"] = 0.0f; pv.v["glide"] = 0.0f; return; }
            pv.v["voice_mode"] = 2.0f;
            const float g = a.size() > 0 ? a[0].getFloatValue() : 0.0f;
            if (g > 0) set (pv, "glide", std::max (pv.get ("glide"), g * std::min (k, 2.0f)), t);
        }
        else if (fn == "poly")
        {
            if (k > 0) pv.v["voice_mode"] = 0.0f;
        }
        else if (fn == "uni" && a.size() >= 1)
        {
            const int n = roundToInt (a[0].getFloatValue() * k);
            for (int o = 0; o < 3; ++o)
                if (oscOn (pv, o))
                {
                    const auto id = String ("o") + String::charToString ((juce_wchar) ('a' + o)) + "_uni";
                    pv.v[id] = (float) jlimit (1, xo::maxUnison, (int) pv.get (id) + n);
                }
        }
        else if (fn == "pitchdrop" && a.size() >= 2)
        {
            int slot = -1;
            for (int s = 1; s <= 16; ++s)
                if ((int) pv.get ("mm" + String (s) + "_src") == srcEnv3 && (int) pv.get ("mm" + String (s) + "_dst") == 1) slot = s;
            if (k < 0) { if (slot > 0) pv.v["mm" + String (slot) + "_amt"] = 0.0f; return; }
            if (slot < 0)
            {
                slot = freeSlot (pv);
                if (slot < 0) return;
                pv.v["e3_attack"] = 0.5f; pv.v["e3_decay"] = a[1].getFloatValue(); pv.v["e3_sustain"] = 0.0f; pv.v["e3_release"] = 50.0f; pv.v["e3_curve"] = 0.6f;
                pv.v["mm" + String (slot) + "_src"] = (float) srcEnv3; pv.v["mm" + String (slot) + "_dst"] = 1.0f;
                pv.v["mm" + String (slot) + "_amt"] = 0.0f;
            }
            auto& amt = pv.v["mm" + String (slot) + "_amt"];
            amt = jlimit (-1.0f, 1.0f, amt + a[0].getFloatValue() * std::min (k, 2.0f));
        }
    }
}

void applyOps (PatchValues& pv, const std::vector<Op>& ops, float k, const ParamTable& t, Random& rng)
{
    for (auto& op : ops)
    {
        if (op.type == '@') { applyFunction (pv, op, k, t, rng); continue; }
        for (auto& id : expand (op.id, pv))
        {
            auto it = t.find (id);
            if (it == t.end()) continue;
            const bool boosting = (op.type == '+' && op.value * k > 0) || (op.type == '*' && std::pow (op.value, k) > 1.0f) || op.type == '=' || op.type == '~';
            if (boosting && k > 0) ensureOn (pv, id);
            float x = pv.get (id);
            switch (op.type)
            {
                case '*': x *= std::pow (op.value, k); break;
                case '+': x += op.value * k; break;
                case '=': if (k > 0) x = op.value; break;
                case '~': x = k > 0 ? x + (op.value - x) * std::min (1.0f, k) : x - (op.value - x) * std::min (1.0f, -k) * 0.5f; break;
                default: break;
            }
            set (pv, id, x, t);
        }
    }
}

void applyEntry (PatchValues& pv, const LexEntry& e, float k, bool negated, const ParamTable& t, Random& rng, int depth)
{
    if (negated && ! e.neg.empty()) applyOps (pv, e.neg, 1.0f, t, rng);
    else applyOps (pv, e.ops, k, t, rng);
    if (depth > 2 || negated) return;
    const auto& lex = Lexicon::get();
    for (auto& [name, f] : e.uses)
    {
        const int idx = lex.findLabel (name);
        if (idx >= 0) applyEntry (pv, lex.entries()[(size_t) idx], k * f, false, t, rng, depth + 1);
    }
}

// ================================================================== loudness
void makeTestPattern (MidiBuffer& m, double sr, int len)
{
    // 150 bpm: a bouncy line, then a held Cm9 chord (the same pattern the preset calibration uses)
    const int line[] = { 60, 67, 72, 70, 67, 63, 65, 67, 60, 72, 70, 67, 75, 74, 70, 67 };
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
}

double measureLoudness (XOSTProcessor& proc, double sr, float* peakOut)
{
    const int len = (int) (sr * 6.0);
    AudioBuffer<float> out (2, len);
    MidiBuffer m;
    makeTestPattern (m, sr, len);
    proc.renderOffline (out, m, sr, 150.0);
    const int w = (int) (sr * 0.4);
    double best = 0;
    for (int s = 0; s + w <= len; s += w / 4)
    {
        double acc = 0;
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = out.getReadPointer (ch);
            for (int i = s; i < s + w; ++i) acc += (double) d[i] * d[i];
        }
        best = std::max (best, std::sqrt (acc / (2.0 * w)));
    }
    if (peakOut != nullptr) *peakOut = out.getMagnitude (0, len);
    if (! std::isfinite (best)) return -200.0;
    return best > 1.0e-9 ? 20.0 * std::log10 (best) : -200.0;
}

// ================================================================== random prompts (SURPRISE)
String randomPrompt (Random& r)
{
    const auto& lex = Lexicon::get();
    auto pick = [&] (EntryType t)
    {
        StringArray p = lex.phrasesOf (t);
        // prefer the first (main) phrase of each entry so prompts read naturally
        StringArray mains;
        for (auto& e : lex.entries()) if (e.type == t) mains.add (e.label.toLowerCase());
        auto& src = (t == EntryType::style || t == EntryType::chr || t == EntryType::fx) ? mains : p;
        return src.isEmpty() ? String() : src[r.nextInt (src.size())];
    };
    StringArray parts;
    if (r.nextFloat() < 0.35f) parts.add (r.nextBool() ? "very" : "a little");
    parts.add (pick (EntryType::chr));
    if (r.nextFloat() < 0.4f) parts.add (pick (EntryType::chr));
    parts.add (r.nextFloat() < 0.5f ? pick (EntryType::kind) : pick (EntryType::cat));
    if (r.nextFloat() < 0.65f) parts.add ("like " + pick (EntryType::style));
    if (r.nextFloat() < 0.45f) parts.add ("with " + pick (EntryType::fx));
    parts.removeEmptyStrings();
    return parts.joinIntoString (" ");
}

// ================================================================== the generator
Generator::Generator() : Thread ("XOST word engine")
{
    ensureScratch();   // built on the calling (message) thread
    startThread (Thread::Priority::low);
}

Generator::~Generator()
{
    signalThreadShouldExit();
    wake.signal();
    stopThread (10000);
}

int Generator::submit (const Request& r)
{
    const ScopedLock sl (lock);
    const int id = nextId++;
    queue.clear();                      // only the newest request matters
    queue.push_back ({ id, r });
    busy = true;
    wake.signal();
    return id;
}

bool Generator::take (Result& out)
{
    const ScopedLock sl (lock);
    if (done.empty()) return false;
    out = std::move (done.back());
    done.clear();
    return true;
}

void Generator::run()
{
    while (! threadShouldExit())
    {
        wake.wait (500);
        std::pair<int, Request> job { 0, {} };
        {
            const ScopedLock sl (lock);
            if (queue.empty()) { busy = false; continue; }
            job = queue.back();
            queue.clear();
        }
        auto res = process (job.second, job.first);
        const ScopedLock sl (lock);
        done.push_back (std::move (res));
        busy = ! queue.empty();
    }
}

Generator::Result Generator::runNow (const Request& r) { return process (r, 0); }

bool Generator::ensureScratch()
{
    if (scratch == nullptr)
    {
        scratch = std::make_unique<XOSTProcessor> (true);
        table = makeParamTable (*scratch);
    }
    return scratch != nullptr;
}

Variation Generator::finish (PatchValues& pv, const String& name, const String& kind, const String& cat, const String& seedName, Random&)
{
    sanitise (pv, table);
    Variation v;
    v.name = name; v.kind = kind; v.category = cat; v.seedName = seedName;
    // loudness-match: render it, then move the master (and, past +6 dB, the compressor make-up) to the target level.
    // The output stage limits very loud patches, so a second pass corrects what the first one could not see.
    const float target = -15.0f;
    v.level = -200.0f;
    for (int pass = 0; pass < 3; ++pass)
    {
        scratch->applyValues (pv.toString (table, false), XOSTProcessor::Scope::all);
        const double st = measureLoudness (*scratch, pass == 0 ? 24000.0 : 48000.0);   // quick first look, then the real measurement
        v.level = (float) st;
        if (st < -90.0 || (pass > 0 && std::abs (st - target) < 0.4)) break;
        float mv = pv.get ("master_vol") + (float) (target - st);
        if (mv > 6.0f)
        {
            const float extra = mv - 6.0f;
            mv = 6.0f;
            if (pv.get ("comp_on") < 0.5f) { pv.v["comp_on"] = 1.0f; pv.v["comp_ratio"] = 1.0f; pv.v["comp_gain"] = 0.0f; pv.v["comp_mix"] = 1.0f; }
            pv.v["comp_gain"] = std::min (24.0f, pv.get ("comp_gain") + extra);
        }
        else if (pv.get ("comp_on") > 0.5f && std::abs (pv.get ("comp_ratio") - 1.0f) < 1.0e-3f && pv.get ("comp_gain") > 0.0f && mv < 6.0f)
        {
            // a make-up-only compressor from an earlier pass: take gain back out of it first
            const float take = std::min (pv.get ("comp_gain"), 6.0f - mv);
            pv.v["comp_gain"] -= take; mv += take;
        }
        pv.v["master_vol"] = std::max (-36.0f, mv);
        if (pass == 2) v.level = target;
    }
    sanitise (pv, table);
    v.values = pv.toString (table, true);
    return v;
}

namespace
{
    String nounOf (const String& seedName)
    {
        // seed names are "Adjective Noun [Noun]" - keep the noun part
        auto rest = seedName.fromFirstOccurrenceOf (" ", false, false).trim();
        return rest.isEmpty() ? seedName : rest;
    }

    void microMutate (PatchValues& pv, Random& r, float amount)
    {
        auto jig = [&] (const String& id, float rel) { auto it = pv.v.find (id); if (it != pv.v.end()) it->second *= 1.0f + rel * amount * (r.nextFloat() * 2.0f - 1.0f); };
        for (int o = 0; o < 3; ++o)
        {
            if (! oscOn (pv, o)) continue;
            const auto p = String ("o") + String::charToString ((juce_wchar) ('a' + o)) + "_";
            pv.v[p + "pos"] = jlimit (0.0f, 1.0f, pv.get (p + "pos") + amount * 0.06f * (r.nextFloat() * 2.0f - 1.0f));
            jig (p + "detune", 0.15f);
        }
        jig ("f1_cutoff", 0.1f); jig ("e1_decay", 0.12f); jig ("e1_release", 0.12f); jig ("rev_decay", 0.1f); jig ("e2_decay", 0.12f);
    }

    void blend (PatchValues& a, const PatchValues& b, float t, const ParamTable& table)
    {
        for (auto& [id, info] : table)
        {
            if (info.discrete) continue;
            const bool sound = id.startsWith ("oa_") || id.startsWith ("ob_") || id.startsWith ("oc_") || id.startsWith ("f1_") || id.startsWith ("f2_")
                               || id.startsWith ("e1_") || id.startsWith ("e2_") || id.startsWith ("e3_");
            if (! sound) continue;
            const auto sec = prefixOf (id);
            auto differs = [&] (const String& k) { return std::abs (a.get (k) - b.get (k)) > 1.0e-4f; };
            if (differs (sec + "_on") && ! sec.startsWithChar ('e')) continue;
            if (sec.startsWithChar ('o') && differs (sec + "_table")) continue;
            if (sec.startsWithChar ('f') && differs (sec + "_type")) continue;
            const float na = info.range.convertTo0to1 (jlimit (info.range.start, info.range.end, a.get (id)));
            const float nb = info.range.convertTo0to1 (jlimit (info.range.start, info.range.end, b.get (id)));
            a.v[id] = info.range.convertFrom0to1 (na + (nb - na) * t);
        }
    }
}

Generator::Result Generator::process (const Request& req, int id)
{
    Result res;
    res.id = id;
    res.request = req;
    if (! ensureScratch()) return res;
    Random rng ((int64) req.seed * 7919 + (int64) req.prompt.hashCode());
    const auto& lex = Lexicon::get();
    res.understood = understand (req.prompt);
    auto& u = res.understood;
    if (req.preferKind.isNotEmpty()) { u.kinds.clear(); u.kinds[req.preferKind] = 2.0f; u.hasSound = true; }

    const bool canRefine = req.currentValues.isNotEmpty() && ! u.hasSound && ! u.hasStyle && ! u.applied.empty();
    const bool refine = canRefine && (req.refine || u.refineHint);

    if (refine)
    {
        PatchValues pv;
        pv.load (req.currentValues, table);
        for (auto& a : u.applied) applyEntry (pv, lex.entries()[(size_t) a.entry], a.k, a.negated, table, rng);
        StringArray what; for (auto& c : u.chips) if (! c.unknown) what.add (c.text.toLowerCase().trimCharactersAtStart ("~"));
        const auto dot = String::fromUTF8 (" \xc2\xb7 ");
        const auto base = req.currentName.upToFirstOccurrenceOf (dot, false, false).upToFirstOccurrenceOf (" *", false, false).trim();
        auto v = finish (pv, base + dot + what.joinIntoString (", "), {}, {}, req.currentName, rng);
        v.summary = "Tweaked: " + what.joinIntoString (", ");
        res.variations.push_back (v);
        res.message = "Tweaked the current sound: " + what.joinIntoString (", ");
        return res;
    }
    if (req.refine && (u.hasSound || u.hasStyle)) res.message = "Sound and style words start a fresh sound, so here are new ideas.";

    // ---- score every sound in the library against what was asked for
    const auto& seeds = Presets::seeds();
    std::map<String, float> explicitCats = u.cats;
    for (auto& [k, w] : u.kinds) { (void) w; explicitCats[k.upToFirstOccurrenceOf (".", false, false)] += 0.0f; }
    struct Cand { int index; float score; };
    std::vector<Cand> cands;
    for (int i = 0; i < (int) seeds.size(); ++i)
    {
        const auto& s = seeds[(size_t) i];
        if (s.category == "Init") continue;
        float score = 0.0f;
        if (! explicitCats.empty() && ! explicitCats.count (s.category)) continue;
        if (! u.kinds.empty())
        {
            auto it = u.kinds.find (s.kind);
            if (it != u.kinds.end()) score += 8.0f + 3.0f * it->second;
            else if (u.cats.empty()) score -= 6.0f;   // only a specific sound was asked for: stay close to it
        }
        if (u.cats.count (s.category)) score += 4.0f;
        if (auto it = u.prefKinds.find (s.kind); it != u.prefKinds.end()) score += 3.0f * it->second;
        for (auto& tag : s.tags) if (auto it = u.tags.find (tag); it != u.tags.end()) score += 1.6f * it->second;
        cands.push_back ({ i, score });
    }
    if (cands.empty())
        for (int i = 1; i < (int) seeds.size(); ++i) cands.push_back ({ i, 0.0f });
    if (u.isEmpty())
        res.message = u.unknown.isEmpty() ? "Type a few words about the sound - here are some random ideas." : "I didn't recognise those words, so here are some random ideas.";

    // ---- pick distinct candidates: a little randomness so GENERATE again gives new results
    std::vector<int> picked;
    std::vector<String> pickedKinds;
    for (int v = 0; v < req.count; ++v)
    {
        float best = -1.0e9f; int bestI = -1;
        for (auto& c : cands)
        {
            const int idx = c.index;
            if (std::find (picked.begin(), picked.end(), idx) != picked.end()) continue;
            float sc = c.score + rng.nextFloat() * 3.0f;
            // spread the four results across different sound families when the prompt allows it
            const auto& k = seeds[(size_t) idx].kind;
            const auto repeats = std::count (pickedKinds.begin(), pickedKinds.end(), k);
            sc -= (float) repeats * (u.kinds.empty() ? 3.0f : 2.0f);
            if (sc > best) { best = sc; bestI = idx; }
        }
        if (bestI < 0) break;
        picked.push_back (bestI);
        pickedKinds.push_back (seeds[(size_t) bestI].kind);
    }

    for (size_t n = 0; n < picked.size(); ++n)
    {
        const auto& s = seeds[(size_t) picked[n]];
        PatchValues pv;
        pv.load (s.values, table);
        // sometimes cross the sound with a sibling from the same family, so results are new every time
        if (rng.nextFloat() < 0.45f)
        {
            std::vector<int> sib;
            for (int i = 0; i < (int) seeds.size(); ++i) if (seeds[(size_t) i].kind == s.kind && i != picked[n]) sib.push_back (i);
            if (! sib.empty())
            {
                PatchValues other;
                other.load (seeds[(size_t) sib[(size_t) rng.nextInt ((int) sib.size())]].values, table);
                blend (pv, other, 0.15f + 0.25f * rng.nextFloat(), table);
            }
        }
        for (auto& a : u.applied) applyEntry (pv, lex.entries()[(size_t) a.entry], a.k, a.negated, table, rng);
        microMutate (pv, rng, 1.0f);

        auto adj = u.adjectives;
        while (adj.size() > 2) adj.remove (adj.size() - 1);
        String lead = adj.joinIntoString (" ");
        if (lead.isEmpty() && ! u.styleLabels.isEmpty())
            lead = u.styleLabels[0].containsAnyOf ("0123456789&") ? u.styleLabels[0] : titleCase (u.styleLabels[0]);
        const auto noun = nounOf (s.name);
        String name = lead.isEmpty() ? s.name : lead + " " + noun;
        // avoid "Drill Drill 808" and repeated names among the four results
        auto lw = StringArray::fromTokens (lead.toLowerCase(), " ", "");
        for (auto& w : lw) if (w.length() > 2 && noun.toLowerCase().contains (w)) { name = s.name; break; }
        auto taken = [&] (const String& nm) { for (auto& r2 : res.variations) if (r2.name == nm) return true; return false; };
        if (taken (name)) name = lead.isEmpty() ? s.name + " II" : lead + " " + s.name.upToFirstOccurrenceOf (" ", false, false) + " " + noun;
        if (taken (name)) name = s.name;
        if (name.length() > 30) name = name.substring (0, 30).trim();

        auto v = finish (pv, name, s.kind, s.category, s.name, rng);
        v.summary = s.description.upToFirstOccurrenceOf (" For ", false, false).upToFirstOccurrenceOf (" Matrix on", false, false);
        res.variations.push_back (v);
    }
    if (res.message.isEmpty())
    {
        StringArray what; for (auto& c : u.chips) if (! c.unknown) what.add (c.text.toLowerCase().trimCharactersAtStart ("~"));
        res.message = String ((int) res.variations.size()) + " sounds for: " + what.joinIntoString (", ");
        if (! u.unknown.isEmpty()) res.message << "   (skipped: " << u.unknown.joinIntoString (", ") << ")";
    }
    return res;
}
}
