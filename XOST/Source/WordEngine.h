#pragma once
// XOST's text-to-sound engine. No AI and no internet: a vocabulary of words (Resources/Lexicon.txt) is matched against
// what you type, and the matches choose a sound family from a library of ~2000 synth recipes (SeedPatches.txt), then
// reshape it with parameter moves for every character / effect word. Each result is loudness-matched by rendering it
// offline on a background thread.
#include <juce_audio_processors/juce_audio_processors.h>
#include <map>
#include <vector>

class XOSTProcessor;

namespace xost
{
    enum class EntryType { cat, kind, style, chr, fx, amount, neg, refine, stop };

    struct Op
    {
        char type = '+';               // '*', '+', '=', '~', '@'
        juce::String id;               // parameter id (may contain o*_ / f*_ wildcards) or function name for '@'
        float value = 0.0f;
        juce::StringArray args;
    };

    struct Weighted { juce::String key; float w = 1.0f; };

    struct LexEntry
    {
        EntryType type = EntryType::stop;
        juce::String label, category;
        std::vector<Weighted> kinds, tags;
        std::vector<Op> ops, neg;
        std::vector<std::pair<juce::String, float>> uses;   // other words whose ops are applied too
        float amount = 1.0f;
    };

    class Lexicon
    {
    public:
        static const Lexicon& get();
        explicit Lexicon (const juce::String& text);
        const std::vector<LexEntry>& entries() const { return list; }
        int find (const juce::String& phrase, bool& impliesRefine) const;   // -1 if unknown
        int fuzzy (const juce::String& word) const;                       // closest single word within 1-2 edits
        int findLabel (const juce::String& label) const;
        juce::StringArray phrasesOf (EntryType) const;
        juce::StringArray problems;                                       // load warnings (tested)
        int maxWords = 1;
        static juce::String normalise (const juce::String&);
    private:
        std::vector<LexEntry> list;
        std::map<juce::String, std::pair<int, bool>> phrases;
        juce::StringArray singles;
    };

    struct Chip
    {
        juce::String text;
        EntryType type = EntryType::stop;
        bool unknown = false, negated = false;
        float amount = 1.0f;
    };

    struct Applied { int entry = -1; float k = 1.0f; bool negated = false; };

    struct Understanding
    {
        std::vector<Chip> chips;
        std::vector<Applied> applied;                  // character / fx / style entries + intensity, in typed order
        std::map<juce::String, float> cats, kinds;     // explicit sound words
        std::map<juce::String, float> prefKinds, tags; // soft preferences from styles / characters
        juce::StringArray adjectives, styleLabels, unknown;
        bool refineHint = false, hasSound = false, hasStyle = false;
        int recognised = 0;
        bool isEmpty() const { return recognised == 0; }
    };

    Understanding understand (const juce::String& text);

    // ---- a patch as a map of real parameter values + Matrix lanes
    struct ParamInfo { juce::NormalisableRange<float> range; float def = 0.0f; bool discrete = false; int steps = 0; };
    using ParamTable = std::map<juce::String, ParamInfo>;
    ParamTable makeParamTable (juce::AudioProcessor&);

    struct PatchValues
    {
        std::map<juce::String, float> v;
        juce::String lanes[4];
        void load (const juce::String& values, const ParamTable&);
        juce::String toString (const ParamTable&, bool onlyNonDefault) const;
        float get (const juce::String& id) const { auto it = v.find (id); return it != v.end() ? it->second : 0.0f; }
    };

    struct Variation
    {
        juce::String name, values, kind, category, seedName, summary;
        float level = 0.0f;
    };

    struct Settings { float targetDb = -15.0f; };

    // offline loudness of the patch currently loaded in 'proc' (loudest 400 ms window of a short melody + chord)
    double measureLoudness (XOSTProcessor& proc, double sampleRate = 48000.0, float* peakOut = nullptr);
    void makeTestPattern (juce::MidiBuffer&, double sampleRate, int length);

    // ---- the background worker
    class Generator : private juce::Thread
    {
    public:
        Generator();
        ~Generator() override;

        struct Request
        {
            juce::String prompt, currentValues, currentName;
            juce::String preferKind;          // "more like this": stay in this sound family
            bool refine = false;
            int count = 4;
            juce::uint32 seed = 1;
        };
        struct Result
        {
            int id = 0;
            Request request;
            Understanding understood;
            std::vector<Variation> variations;
            juce::String message;
        };

        int submit (const Request&);           // returns the job id
        bool isBusy() const { return busy.load(); }
        bool take (Result&);                   // message thread: collects a finished job

        // synchronous version (used by the tests)
        Result runNow (const Request&);

    private:
        void run() override;
        Result process (const Request&, int id);
        bool ensureScratch();
        Variation finish (PatchValues&, const juce::String& name, const juce::String& kind, const juce::String& cat,
                          const juce::String& seedName, juce::Random&);

        std::unique_ptr<XOSTProcessor> scratch;
        ParamTable table;
        juce::CriticalSection lock;
        std::vector<std::pair<int, Request>> queue;
        std::vector<Result> done;
        std::atomic<bool> busy { false };
        int nextId = 1;
        juce::WaitableEvent wake;
    };

    // helpers shared with the tests
    void applyOps (PatchValues&, const std::vector<Op>&, float k, const ParamTable&, juce::Random&);
    void applyEntry (PatchValues&, const LexEntry&, float k, bool negated, const ParamTable&, juce::Random&, int depth = 0);
    void sanitise (PatchValues&, const ParamTable&);
    juce::String randomPrompt (juce::Random&);
}
