#pragma once

// Band-limited, mip-mapped wavetables. Each table has 16 frames of 2048 samples and 11 mip levels
// (level L keeps harmonics up to 1024 >> L) so oscillators stay alias-free across the keyboard.

#include <vector>
#include <cmath>
#include <atomic>
#include <memory>
#include <functional>
#include <juce_core/juce_core.h>

namespace xo
{
    class Wavetable
    {
    public:
        static constexpr int N = 2048, levels = 11, maxFrames = 16;

        Wavetable() = default;

        // build from time-domain single-cycle frames (each exactly N samples)
        void buildFromFrames (const std::vector<std::vector<float>>& frames);
        // build from harmonic spectra: fn(frame, n) -> amplitude of sine harmonic n (1..1023)
        void buildFromHarmonics (int numFrames, const std::function<float (int frame, int harmonic)>& fn);

        int getNumFrames() const noexcept { return numFrames; }
        const float* frame (int f, int level) const noexcept { return data.data() + ((size_t) f * levels + (size_t) level) * (N + 1); }

        // mip level for a given frequency
        static int levelFor (float hz, float sampleRate) noexcept
        {
            const float allowed = 0.5f * sampleRate / std::max (hz, 1.0f);
            if (allowed >= 1024.0f) return 0;
            const int l = (int) std::ceil (std::log2 (1024.0f / std::max (allowed, 1.0f)));
            return l < 0 ? 0 : (l >= levels ? levels - 1 : l);
        }

        // sample frame-interpolated table at phase [0,1)
        float lookup (float phase, float pos, int level) const noexcept
        {
            const float fpos = pos * (float) (numFrames - 1);
            const int f0 = (int) fpos;
            const int f1 = f0 + 1 < numFrames ? f0 + 1 : f0;
            const float ff = fpos - (float) f0;
            const float idx = phase * (float) N;
            const int i = (int) idx;
            const float fr = idx - (float) i;
            const float* a = frame (f0, level);
            const float* b = frame (f1, level);
            const float va = a[i] + (a[i + 1] - a[i]) * fr;
            const float vb = b[i] + (b[i + 1] - b[i]) * fr;
            return va + (vb - va) * ff;
        }

        // raw frame (level 0) for display
        void getDisplayFrame (float pos, std::vector<float>& out, int points) const;

        // export / import for saving user tables in plugin state
        juce::String toBase64() const;
        bool fromBase64 (const juce::String&);
        std::vector<std::vector<float>> sourceFrames;   // time-domain level-0 frames (kept for export)

    private:
        void buildMips (const std::vector<std::vector<float>>& spectraRe, const std::vector<std::vector<float>>& spectraIm);
        int numFrames = 1;
        std::vector<float> data;
    };

    // the 16 factory tables (built once, shared by every instance)
    class WavetableBank
    {
    public:
        static const WavetableBank& get();
        const Wavetable& table (int index) const { return tables[(size_t) juce::jlimit (0, (int) tables.size() - 1, index)]; }
        int size() const { return (int) tables.size(); }
    private:
        WavetableBank();
        std::vector<Wavetable> tables;
    };

    // turn arbitrary audio into a wavetable (Serum-style 2048-sample frames, or slicing longer audio)
    std::unique_ptr<Wavetable> wavetableFromAudio (const float* samples, int numSamples, bool exactCycles);

    // in-place radix-2 complex FFT (size must be a power of two)
    void fft (std::vector<float>& re, std::vector<float>& im, bool inverse);
}
