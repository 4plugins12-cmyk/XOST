#pragma once
// Uno's sample analysis: silence trimming, peak normalising and root-note (pitch) detection.
#include <juce_audio_basics/juce_audio_basics.h>

namespace uno
{
    struct TrimInfo { int start = 0, end = 0; };

    // Finds where the sound really starts and ends. thresholdDb is relative to the sample's peak.
    TrimInfo findTrim (const juce::AudioBuffer<float>& raw, double sampleRate, float thresholdDb);

    // Copies the trimmed region, adds tiny fades so there are no clicks, optionally normalises to -0.3 dBFS.
    juce::AudioBuffer<float> makeTrimmed (const juce::AudioBuffer<float>& raw, double sampleRate, TrimInfo t, bool normalise);

    struct PitchResult
    {
        float hz = 0.0f;          // 0 = no clear pitch (drums, noise)
        float confidence = 0.0f;  // 0..1
        float midi = 0.0f;        // fractional MIDI note
    };

    // YIN-based pitch detection over the steady part of a one-shot (skips the attack, ignores the quiet tail).
    PitchResult detectPitch (const juce::AudioBuffer<float>& buf, double sampleRate);
    float detectFrameHz (const float* x, int n, double sampleRate, float& confidence);

    // naming: middleCOctave = the octave number shown for MIDI 60 (FL Studio = 5, Ableton = 3, Logic/most = 4)
    juce::String noteName (int midi, int middleCOctave);
}
