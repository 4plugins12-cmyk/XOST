#include "SampleTools.h"
#include <vector>
#include <algorithm>

namespace uno
{
    static std::vector<float> mono (const juce::AudioBuffer<float>& b)
    {
        std::vector<float> m ((size_t) b.getNumSamples(), 0.0f);
        const float g = 1.0f / (float) juce::jmax (1, b.getNumChannels());
        for (int c = 0; c < b.getNumChannels(); ++c)
        {
            const float* d = b.getReadPointer (c);
            for (int i = 0; i < b.getNumSamples(); ++i) m[(size_t) i] += d[i] * g;
        }
        return m;
    }

    TrimInfo findTrim (const juce::AudioBuffer<float>& raw, double sr, float thresholdDb)
    {
        TrimInfo t;
        const int n = raw.getNumSamples();
        t.end = n;
        if (n == 0) return t;
        float peak = 0.0f;
        for (int c = 0; c < raw.getNumChannels(); ++c) peak = std::max (peak, raw.getMagnitude (c, 0, n));
        if (peak <= 1.0e-7f) return t;
        const float thrStart = peak * juce::Decibels::decibelsToGain (thresholdDb);
        const float thrEnd = peak * juce::Decibels::decibelsToGain (thresholdDb - 12.0f);
        auto level = [&raw] (int i) { float v = 0.0f; for (int c = 0; c < raw.getNumChannels(); ++c) v = std::max (v, std::abs (raw.getSample (c, i))); return v; };
        int s = 0;
        while (s < n && level (s) < thrStart) ++s;
        // keep a hair of pre-roll so the transient isn't chopped, then snap back to the previous quiet point
        const int pre = (int) (0.0004 * sr);
        int start = std::max (0, s - pre);
        while (start > 0 && start > s - (int) (0.003 * sr) && level (start) > thrEnd) --start;
        int e = n - 1;
        while (e > start && level (e) < thrEnd) --e;
        t.start = start;
        t.end = std::min (n, e + 1 + (int) (0.005 * sr));
        if (t.end - t.start < 16) { t.start = 0; t.end = n; }
        return t;
    }

    juce::AudioBuffer<float> makeTrimmed (const juce::AudioBuffer<float>& raw, double sr, TrimInfo t, bool normalise)
    {
        const int len = std::max (1, t.end - t.start);
        juce::AudioBuffer<float> out (raw.getNumChannels(), len);
        for (int c = 0; c < raw.getNumChannels(); ++c) out.copyFrom (c, 0, raw, c, t.start, len);
        const int fadeIn = std::min (len / 4, (int) (0.0005 * sr)), fadeOut = std::min (len / 4, (int) (0.004 * sr));
        if (fadeIn > 1) out.applyGainRamp (0, fadeIn, 0.0f, 1.0f);
        if (fadeOut > 1) out.applyGainRamp (len - fadeOut, fadeOut, 1.0f, 0.0f);
        if (normalise)
        {
            float peak = 0.0f;
            for (int c = 0; c < out.getNumChannels(); ++c) peak = std::max (peak, out.getMagnitude (c, 0, len));
            if (peak > 1.0e-6f) out.applyGain (juce::Decibels::decibelsToGain (-0.3f) / peak);
        }
        return out;
    }

    float detectFrameHz (const float* x, int n, double sr, float& confidence)
    {
        // YIN: cumulative mean normalised difference, first dip under the threshold, parabolic refinement
        const int minTau = std::max (2, (int) (sr / 2200.0));
        const int maxTau = std::min (n / 2, (int) (sr / 27.0));
        confidence = 0.0f;
        if (maxTau <= minTau + 2) return 0.0f;
        const int w = n - maxTau;
        std::vector<float> d ((size_t) maxTau + 1, 0.0f);
        for (int tau = 1; tau <= maxTau; ++tau)
        {
            double s = 0.0;
            for (int i = 0; i < w; ++i) { const float diff = x[i] - x[i + tau]; s += (double) diff * diff; }
            d[(size_t) tau] = (float) s;
        }
        std::vector<float> cm ((size_t) maxTau + 1, 1.0f);
        double run = 0.0;
        for (int tau = 1; tau <= maxTau; ++tau)
        {
            run += d[(size_t) tau];
            cm[(size_t) tau] = run > 0.0 ? (float) ((double) d[(size_t) tau] * (double) tau / run) : 1.0f;
        }
        int best = -1;
        for (int tau = minTau; tau < maxTau; ++tau)
            if (cm[(size_t) tau] < 0.15f)
            {
                while (tau + 1 < maxTau && cm[(size_t) tau + 1] < cm[(size_t) tau]) ++tau;
                best = tau;
                break;
            }
        if (best < 0)
        {
            // no clear dip: take the global minimum, but report low confidence
            best = minTau;
            for (int tau = minTau; tau < maxTau; ++tau) if (cm[(size_t) tau] < cm[(size_t) best]) best = tau;
        }
        confidence = juce::jlimit (0.0f, 1.0f, 1.0f - cm[(size_t) best]);
        float tauF = (float) best;
        if (best > 1 && best < maxTau)
        {
            const float a = cm[(size_t) best - 1], b = cm[(size_t) best], c = cm[(size_t) best + 1];
            const float den = a - 2.0f * b + c;
            if (std::abs (den) > 1.0e-9f) tauF += 0.5f * (a - c) / den;
        }
        return (float) (sr / tauF);
    }

    PitchResult detectPitch (const juce::AudioBuffer<float>& buf, double sr)
    {
        PitchResult r;
        auto m = mono (buf);
        const int n = (int) m.size();
        if (n < 512) return r;
        // work at ~22 kHz: plenty for 27 Hz - 2.2 kHz and 4x cheaper
        int dec = 1;
        while (sr / (dec * 2) >= 20000.0) dec *= 2;
        std::vector<float> x;
        if (dec > 1)
        {
            juce::IIRFilter lp; lp.setCoefficients (juce::IIRCoefficients::makeLowPass (sr, sr / (2.5 * dec)));
            juce::IIRFilter lp2; lp2.setCoefficients (juce::IIRCoefficients::makeLowPass (sr, sr / (2.5 * dec)));
            for (int i = 0; i < n; ++i) { const float v = lp2.processSingleSampleRaw (lp.processSingleSampleRaw (m[(size_t) i])); if (i % dec == 0) x.push_back (v); }
        }
        else x = m;
        const double fs = sr / dec;
        const int len = (int) x.size();
        const int frame = std::min (len, (int) (fs * 0.09));       // 90 ms frames: two periods of a 27 Hz 808
        const int hop = std::max (64, frame / 3);
        float peak = 0.0f;
        for (float v : x) peak = std::max (peak, std::abs (v));
        if (peak <= 1.0e-6f || frame < 256) return r;
        // skip the attack (pitch drops, clicks) on longer sounds
        const int skip = len > (int) (fs * 0.5) ? (int) (fs * 0.07) : (len > (int) (fs * 0.15) ? (int) (fs * 0.025) : 0);
        struct F { float midi, conf, weight; };
        std::vector<F> frames;
        for (int s = skip; s + frame <= len && (int) frames.size() < 40; s += hop)
        {
            float rms = 0.0f;
            for (int i = 0; i < frame; ++i) rms += x[(size_t) (s + i)] * x[(size_t) (s + i)];
            rms = std::sqrt (rms / (float) frame);
            if (rms < peak * 0.02f) { if (! frames.empty()) break; else continue; }   // quiet tail
            float conf = 0.0f;
            const float hz = detectFrameHz (x.data() + s, frame, fs, conf);
            if (hz > 0.0f && conf > 0.75f) frames.push_back ({ 69.0f + 12.0f * std::log2 (hz / 440.0f), conf, rms * conf });
        }
        if (frames.size() < 2) return r;
        // weighted median of the frame pitches
        std::sort (frames.begin(), frames.end(), [] (const F& a, const F& b) { return a.midi < b.midi; });
        float total = 0.0f, confSum = 0.0f;
        for (auto& f : frames) { total += f.weight; confSum += f.conf; }
        float acc = 0.0f, med = frames.front().midi;
        for (auto& f : frames) { acc += f.weight; if (acc >= total * 0.5f) { med = f.midi; break; } }
        // how many frames agree with the median (within a semitone)
        int agree = 0;
        for (auto& f : frames) if (std::abs (f.midi - med) < 1.0f) ++agree;
        r.midi = med;
        r.hz = 440.0f * std::exp2 ((med - 69.0f) / 12.0f);
        r.confidence = (confSum / (float) frames.size()) * ((float) agree / (float) frames.size());
        if (r.confidence < 0.45f) r = {};
        return r;
    }

    juce::String noteName (int midi, int middleCOctave)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const int oct = midi / 12 - 5 + middleCOctave;
        return juce::String (names[((midi % 12) + 12) % 12]) + juce::String (oct);
    }
}
