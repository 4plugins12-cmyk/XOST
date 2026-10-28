#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Params.h"
#include "Engine.h"
#include "WordEngine.h"
#include <map>

class XOSTProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    explicit XOSTProcessor (bool offlineScratch = false);   // scratch = silent copy the word engine renders with
    ~XOSTProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "XOST"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    bool supportsMPE() const override { return true; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::UndoManager undo { 30000, 60 };
    juce::AudioProcessorValueTreeState apvts;

    // ---- presets
    static juce::File getUserFolder (const juce::String& sub);
    bool saveUserPreset (const juce::File&, const juce::String& category, const juce::String& tags, const juce::String& description);
    bool loadUserPreset (const juce::File&);
    juce::String getCurrentPresetName() const { return currentPresetName; }
    int getPresetChangeCounter() const { return presetChangeCounter; }
    void initPatch();
    void randomize (uint32_t lockMask);            // bit per section (see sectionNames)
    static const juce::StringArray& sectionNames();

    // ---- wavetables & sampler (message thread)
    bool loadWavetableFile (int osc, const juce::File&);
    void setUserWavetable (int osc, std::unique_ptr<xo::Wavetable>);
    const xo::Wavetable* getUserWavetable (int osc) const { return userLive[osc].load(); }
    bool loadSampleFile (const juce::File&);
    void clearSample();
    const xo::SampleData* getSample() const { return sampleLive.load(); }

    // ---- bounce / resample / one-shots
    juce::File bounceToFile (int note, double seconds);
    void renderOffline (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, double rate, double bpm);
    struct OneShotOptions { int note = -1; int length = -1; bool normalise = true; int middleCOctave = 5; };   // -1 = auto
    juce::File exportOneShot (const OneShotOptions&, juce::String* rootOut = nullptr);
    OneShotOptions oneShot;
    juce::File lastOneShot;

    // ---- text-to-sound (the CREATE panel)
    xost::Generator& generator();
    void loadVariation (const xost::Variation&, const juce::String& prompt);
    juce::String compactValues() const;
    struct HistoryItem { juce::String prompt, name, values, category; };
    struct CreateState
    {
        juce::String prompt;
        std::vector<xost::Variation> variations;
        int selected = -1;
        std::vector<HistoryItem> history;     // newest first
        int counter = 0;                      // bumps whenever any of this changes (UI refresh)
    };
    CreateState create;
    juce::CriticalSection createLock;     // guards 'create' against hosts saving state from another thread
    void addHistory (const juce::String& prompt, const xost::Variation&);
    juce::String currentCategory() const { return currentCategoryName; }
    bool resampleToWavetable (int osc, int note);

    // ---- FX order
    void setFxOrder (const std::array<int, xo::numFx>&);
    std::array<int, xo::numFx> getFxOrder() const;
    static std::array<int, xo::numFx> defaultFxOrder() { return { 0, 8, 1, 2, 3, 4, 5, 6, 7 }; }

    // ---- value strings: factory presets and the Matrix / Minirack banks are stored as "id=value ..." strings
    enum class Scope { all, matrix, rack };
    static bool inScope (const juce::String& id, Scope);
    void applyValues (const juce::String& values, Scope scope);
    juce::String captureValues (Scope scope) const;
    std::map<juce::String, float> normalisedFromValues (const juce::String& values) const;   // without touching the synth

    // ---- Matrix lanes (message thread)
    std::vector<xo::Breakpoint> getLane (int lane) const { return lanes[(size_t) lane]; }
    void setLane (int lane, std::vector<xo::Breakpoint> pts, bool undoable = true);
    void commitLane (int lane, std::vector<xo::Breakpoint> before);   // makes a finished live edit undoable
    static std::vector<xo::Breakpoint> defaultLane (int lane);
    static juce::String laneToString (const std::vector<xo::Breakpoint>&);
    static std::vector<xo::Breakpoint> laneFromString (const juce::String&);
    int getLaneCounter() const { return laneCounter; }

    // ---- Matrix / Minirack presets
    bool saveToolPreset (Scope, const juce::File&, const juce::String& category);
    bool loadToolPreset (Scope, const juce::File&);
    void loadToolFactory (Scope, int index);
    juce::String currentMatrixName { "Init" }, currentRackName { "Init" };

    // ---- Mutate page
    static int sectionFor (const juce::String& id);         // -1 = never mutated
    void mutate (float amount, uint32_t lockMask);
    void mutateInternal (float amount, uint32_t lockMask);
    void blendWith (const juce::String& otherValues, float mix, uint32_t lockMask, bool morph, const juce::String& label, bool newTransaction = true);
    void loadFactoryPreset (int index) { setCurrentProgram (index); }

    juce::MidiKeyboardState keyboardState;
    std::atomic<uint32_t> randomLocks { (1u << 10) | (1u << 11) | (1u << 12) };
    static juce::StringArray defaultMacroNames() { return { "Bright", "Space", "Grit", "Motion", "Macro 5", "Macro 6", "Macro 7", "Macro 8" }; }
    juce::StringArray macroNames = defaultMacroNames();
    double uiScale = 1.0;

    // ---- UI feeds
    xo::Engine engine { apvts };
    juce::AbstractFifo scopeFifo { 4096 };
    std::vector<float> scopeData = std::vector<float> (4096, 0.0f);
    std::atomic<float> cpuLoad { 0.0f };
    double getSampleRateNow() const { return sr; }

private:
    void timerCallback() override;
    void pushScope (const float* data, int n);
    void restoreExtras (const juce::ValueTree&);
    void storeExtras (juce::ValueTree&) const;

    double sr = 44100.0;
    int blockSize = 512;
    int currentProgram = 0, presetChangeCounter = 0;
    juce::String currentPresetName, currentCategoryName { "Init" };
    const bool scratchMode;
    std::unique_ptr<xost::Generator> gen;
    const xost::ParamTable& paramTable();
    std::unique_ptr<xost::ParamTable> table;

    // resources swapped to the audio thread through atomics; old copies freed later on the message thread
    std::unique_ptr<xo::Wavetable> userOwned[xo::numOsc];
    std::atomic<const xo::Wavetable*> userLive[xo::numOsc] {};
    std::unique_ptr<xo::SampleData> sampleOwned;
    std::atomic<const xo::SampleData*> sampleLive { nullptr };
    struct Retired { std::unique_ptr<xo::Wavetable> wt; std::unique_ptr<xo::SampleData> smp; juce::uint32 time; std::unique_ptr<xo::MatrixShapes> shapes; };
    std::vector<xo::Breakpoint> lanes[xo::numMxLanes];
    std::unique_ptr<xo::MatrixShapes> shapesOwned;
    int laneCounter = 0;
    void rebuildShapes();
    friend struct LaneAction;
    std::vector<Retired> graveyard;

    juce::AudioFormatManager formats;
    juce::Random random;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (XOSTProcessor)
};
