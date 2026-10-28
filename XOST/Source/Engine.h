#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Params.h"
#include "SynthDSP.h"
#include "Wavetables.h"
#include "FX.h"
#include "Modules.h"

namespace xo
{
    // ---------------------------------------------------------------- loaded sample (immutable once shared)
    struct SampleData
    {
        std::vector<float> L, R;
        double sampleRate = 44100.0;
        juce::String name, path;
        int length() const { return (int) L.size(); }
    };

    // ---------------------------------------------------------------- per-block parameter snapshot
    struct OscP
    {
        bool on; int table, oct, uni, warp, mod, modFrom, route;
        float pos, semi, fine, level, pan, detune, width, phase, rand, warpAmt, modAmt;
    };
    struct BlockP
    {
        OscP osc[numOsc];
        bool subOn; int subShape, subOct, subRoute; float subLevel;
        bool noiseOn; int noiseType, noiseRoute; float noiseColor, noiseLevel;
        bool smpOn; int smpMode, smpRoot, smpRoute; bool smpKey; float smpStart, smpLevel, smpPan, gPos, gSize, gDens, gSpray, gPitch;
        bool fOn[2]; int fType[2]; float fCut[2], fReso[2], fDrive[2], fKey[2], fEnv[2], fMix[2]; bool parallel;
        bool shpOn; int shpType; bool shpPost; float shpDrive, shpMix;
        float env[numEnv][7];
        int lfoShape[numLfo], lfoMode[numLfo]; bool lfoSync[numLfo]; float lfoRate[numLfo], lfoPhase[numLfo], lfoSmooth[numLfo], lfoBeats[numLfo];
        float macro[numMacros];
        int slotSrc[numSlots], slotDst[numSlots]; float slotAmt[numSlots];
        float ampLevel, ampPan, velSens, glideMs; int voiceMode, glideMode, polyphony, bendRange; bool mpe;
        int quality;
        float drift = 0.0f;
        float rackMod[numDests] {};
    };

    // ---------------------------------------------------------------- shared resources handed to voices
    struct Resources
    {
        const Wavetable* userTables[numOsc] {};
        const SampleData* sample = nullptr;
    };

    struct ChannelState { float bend = 0, pressure = 0, slide = 0; };

    class Voice
    {
    public:
        void prepare (double sampleRate, int maxBlock);
        void start (int note, float velocity, int channel, float startPitch, bool legatoRetrigger, const BlockP&, const Resources&, Rng&);
        void glideTo (int note, float velocity, bool retrigger, const BlockP&);
        void release();
        void steal();   // fast fade so the slot can be reused
        void reseed (uint32_t v) { rng.seed (v); }
        bool isActive() const { return active; }
        bool isReleasing() const { return releasing; }
        int  getNote() const { return note; }
        int  getChannel() const { return channel; }
        uint32_t age = 0;
        float polyPressure = 0.0f;

        // render n samples (at the internal, possibly oversampled rate) and ADD into L/R
        void render (float* L, float* R, int n, const BlockP&, const Resources&, const ChannelState* chans,
                     float modWheel, double beatPos, float bpm, const float* globalLfoPhase);
        // last control-rate source values (for global modulation / UI)
        float srcVals[numSources] {};
        float lastEnv1 = 0.0f;

    private:
        void controlUpdate (int n, const BlockP&, const ChannelState* chans, float modWheel, double beatPos, float bpm, const float* gPhase);
        float filterSample (int f, int ch, float x, const BlockP&);

        float sr = 44100.0f;
        bool active = false, releasing = false, stealing = false;
        int note = 60, channel = 1;
        float velocity = 1.0f, randomVal = 0.0f;
        float pitch = 60.0f, targetPitch = 60.0f;
        float driftWalk = 0.0f, driftTarget = 0.0f, driftNote = 0.0f, driftCut = 0.0f; int driftTimer = 0;
        float stealGain = 1.0f;

        // oscillators
        float phases[numOsc][maxUnison] {};
        float initPhase[numOsc][maxUnison] {};
        float oscOut[5] {}, oscPrev[5] {};
        bool  oscWrapped[numOsc] {};
        float subPhase = 0.0f;
        Rng rng;
        float pinkB[2][3] {};
        OnePole noiseLp[2];

        // sampler
        double smpPos = 0.0;
        struct Grain { bool on = false; double pos = 0, inc = 1; float age = 0, len = 1, pan = 0; };
        Grain grains[12];
        float grainTimer = 0.0f;

        // envelopes / LFOs
        Envelope env[numEnv];
        float lfoPh[numLfo] {}, lfoHeld[numLfo] {}, lfoPrevHeld[numLfo] {}, lfoOut[numLfo] {};
        bool lfoDone[numLfo] {};

        // filters
        SVF svfA[2][2], svfB[2][2];
        Ladder ladder[2][2];
        SVF formant[2][2][3];
        Delay comb[2][2];
        float combFb[2][2] {};
        DcBlocker dc[2];

        // control-rate values (smoothed linearly across each control block)
        float mod[numDests] {};
        float ampGain = 0.0f, ampGainTarget = 0.0f, panL = 1, panR = 1;
        float oscLevel[numOsc] {}, oscLevelTarget[numOsc] {};
        float oscInc[numOsc][maxUnison] {};
        float oscPanL[numOsc][maxUnison] {}, oscPanR[numOsc][maxUnison] {};
        int   oscMip[numOsc] {};
        float oscPos[numOsc] {}, oscWarp[numOsc] {}, oscModAmt[numOsc] {};
        float subInc = 0, subGain = 0, noiseGain = 0, smpGain = 0, smpInc = 1.0f, grainPos = 0, grainSize = 0.05f;
        float cutHz[2] { 1000, 1000 }, resoV[2] {}, driveV[2] {}, shaperDrive = 0;
    };

    // ---------------------------------------------------------------- arpeggiator / step sequencer
    class Arp
    {
    public:
        void reset() { numHeld = 0; step = 0; playing = -1; nextTime = 0; noteOffAt = -1; latched = false; dirUp = true; numOrdered = 0; }
        void noteOn (int n, float v);
        void noteOff (int n);
        // generate events for this block into out (sample offsets)
        void process (juce::MidiBuffer& out, int numSamples, double sr, double bpm, double ppq, bool hostPlaying,
                      const std::atomic<float>* const* p, int stepsOnBase);
        void allOff (juce::MidiBuffer& out) { if (playingCount > 0) { for (int i = 0; i < playingCount; ++i) out.addEvent (juce::MidiMessage::noteOff (1, playingNotes[i]), 0); playingCount = 0; } }
        int currentStep = 0;
    private:
        int held[32] {}; float heldVel[32] {}; int numHeld = 0;
        int ordered[32] {}; int numOrdered = 0;
        int step = 0, playing = -1; double nextTime = 0; double noteOffAt = -1;
        bool latched = false, dirUp = true;
        int playingNotes[16] {}; int playingCount = 0;
        double lastStepBeat = -1.0;
        Rng rng;
    };

    // ---------------------------------------------------------------- the engine
    class Engine
    {
    public:
        explicit Engine (juce::AudioProcessorValueTreeState&);
        void prepare (double sampleRate, int maxBlock);
        void setResources (const Resources& r) { res = r; }
        void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, double bpm, double ppq, bool hostPlaying);
        void allNotesOff();
        int getLatency() const { return latency; }

        // UI feeds
        std::atomic<int> activeVoices { 0 };
        std::atomic<int> arpStep { 0 };
        float uiSrc[numSources] {};
        std::atomic<float> uiLfo[numLfo] {}, uiEnv[numEnv] {};

        FxRack fx;
        MatrixFx matrix;
        Minirack rack;
        std::atomic<const MatrixShapes*> shapes { nullptr };

    private:
        void readParams();
        void handleMidi (const juce::MidiMessage&, juce::MidiBuffer* arpIn);
        void noteOn (int ch, int note, float vel);
        void noteOff (int ch, int note);
        Voice* findVoice();
        void renderVoices (float* L, float* R, int n);
        void updateQuality();

        juce::AudioProcessorValueTreeState& apvts;
        std::vector<std::atomic<float>*> raw;   // lookup by name below
        std::atomic<float>* P (const juce::String& id) { return apvts.getRawParameterValue (id); }

        // cached parameter pointers
        struct Ptrs
        {
            std::atomic<float>* osc[numOsc][19];
            std::atomic<float> *subOn, *subShape, *subOct, *subLevel, *subRoute,
                               *noiseOn, *noiseType, *noiseColor, *noiseLevel, *noiseRoute,
                               *smpOn, *smpMode, *smpRoot, *smpKey, *smpStart, *smpLevel, *smpPan, *gPos, *gSize, *gDens, *gSpray, *gPitch, *smpRoute;
            std::atomic<float>* flt[2][8];
            std::atomic<float> *fltRouting, *shpOn, *shpType, *shpDrive, *shpPos, *shpMix;
            std::atomic<float>* env[numEnv][7];
            std::atomic<float>* lfo[numLfo][7];
            std::atomic<float>* macro[numMacros];
            std::atomic<float>* slot[numSlots][3];
            std::atomic<float> *master, *quality, *poly, *voiceMode, *glide, *glideMode, *bendRange, *mpe, *velSens, *ampLevel, *ampPan;
            std::atomic<float> *arpOn, *drift, *width, *chordType, *chordInv, *chordVoicing, *chordStrum;
            const std::atomic<float>* arp[8 + numSteps * 3];
        } p {};

        BlockP bp {};
        Resources res;
        double sr = 44100.0;
        int maxBlock = 512, osFactor = 1, qualityNow = -1, latency = 0;
        Voice voices[maxVoices];
        uint32_t ageCounter = 0;
        ChannelState chans[17];
        float modWheel = 0.0f;
        int monoStack[64] {}; int monoCount = 0;
        float lastNotePitch = 60.0f;
        Rng rng;
        Arp arp;
        juce::MidiBuffer arpOut, arpIn;
        bool arpWasOn = false;
        std::unique_ptr<juce::dsp::Oversampling<float>> os2, os4;
        juce::AudioBuffer<float> work;
        double beatPos = 0.0;
        float globalLfoPhase[numLfo] {};
        float lastBpm = 120.0f;
        float smoothMaster = 0.5f;
        float lastLevel = 0.0f;
        bool sustainDown = false;
        bool pendingOff[128] {};
        // chord memory: each held key remembers the notes it generated; strummed notes wait in a small queue
        void applyChords (const juce::MidiBuffer& in, juce::MidiBuffer& out, int numSamples);
        signed char chordNotes[16][128][7] {};   // per channel, per key: generated notes (-1 = none)
        struct Pending { int ch, note, delay; float vel; };
        Pending pending[64] {}; int numPending = 0;
        juce::MidiBuffer chordBuf, eventsBuf;
    public:
        bool bypassFx = false;   // used when resampling to a wavetable
        // offline renders: same random choices every time, so measuring a patch is repeatable
        void reseed (uint32_t base) { for (int i = 0; i < maxVoices; ++i) voices[i].reseed (base + (uint32_t) i * 7919u); rng.seed (base ^ 0x5bd1e995u); }
    };
}
