#pragma once

// XO effects rack: 8 effects in a user-defined order.

#include <juce_audio_processors/juce_audio_processors.h>
#include "SynthDSP.h"
#include "Params.h"

namespace xo
{
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        float process (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
        void reset() { z1 = z2 = 0; }
        // type 0 low shelf, 1 peak, 2 high shelf
        void set (int type, float hz, float q, float db, float sr)
        {
            const float A = std::pow (10.0f, db / 40.0f), w = twoPi * clampf (hz, 10.0f, sr * 0.45f) / sr;
            const float cw = std::cos (w), sw = std::sin (w), alpha = sw / (2.0f * q), sq = 2.0f * std::sqrt (A) * alpha;
            float B0, B1, B2, A0, A1, A2;
            if (type == 1) { B0 = 1 + alpha * A; B1 = -2 * cw; B2 = 1 - alpha * A; A0 = 1 + alpha / A; A1 = -2 * cw; A2 = 1 - alpha / A; }
            else if (type == 0) { B0 = A * ((A + 1) - (A - 1) * cw + sq); B1 = 2 * A * ((A - 1) - (A + 1) * cw); B2 = A * ((A + 1) - (A - 1) * cw - sq);
                                  A0 = (A + 1) + (A - 1) * cw + sq; A1 = -2 * ((A - 1) + (A + 1) * cw); A2 = (A + 1) + (A - 1) * cw - sq; }
            else { B0 = A * ((A + 1) + (A - 1) * cw + sq); B1 = -2 * A * ((A - 1) + (A + 1) * cw); B2 = A * ((A + 1) + (A - 1) * cw - sq);
                   A0 = (A + 1) - (A - 1) * cw + sq; A1 = 2 * ((A - 1) - (A + 1) * cw); A2 = (A + 1) - (A - 1) * cw - sq; }
            b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
        }
    };

    // 8-line feedback delay network reverb with modulation and input diffusion
    class FdnReverb
    {
    public:
        void prepare (float sampleRate)
        {
            sr = sampleRate;
            for (auto& d : lines) d.prepare ((int) (0.25f * sr));
            for (auto& d : diff) d.prepare ((int) (0.03f * sr));
            pre[0].prepare ((int) (0.25f * sr)); pre[1].prepare ((int) (0.25f * sr));
            reset();
        }
        void reset() { for (auto& d : lines) d.reset(); for (auto& d : diff) d.reset(); pre[0].reset(); pre[1].reset(); for (auto& l : lp) l.reset(); phase = 0; }

        void process (float* L, float* R, int n, float size, float decaySec, float damp, float preMs, float width, float mix)
        {
            static const float base[8] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.7f, 73.1f };
            static const float dTimes[4] = { 4.7f, 3.6f, 12.7f, 9.3f };
            float delays[8], gains[8];
            const float scale = 0.35f + size * 1.3f;
            for (int i = 0; i < 8; ++i)
            {
                delays[i] = base[i] * scale * 0.001f * sr;
                gains[i] = std::pow (10.0f, -3.0f * (delays[i] / sr) / std::max (0.1f, decaySec));
                lp[i].setCutoff (18000.0f * std::exp2 (-damp * 4.5f), sr);
            }
            const float preS = preMs * 0.001f * sr + 1.0f;
            for (int s = 0; s < n; ++s)
            {
                phase += 0.5f / sr; if (phase >= 1.0f) phase -= 1.0f;
                pre[0].push (L[s]); pre[1].push (R[s]);
                float in = 0.5f * (pre[0].read (preS) + pre[1].read (preS));
                // input diffusion
                for (int k = 0; k < 4; ++k)
                {
                    const float d = diff[k].read (dTimes[k] * 0.001f * sr);
                    const float v = in + 0.6f * d;
                    diff[k].push (v);
                    in = d - 0.6f * v;
                }
                float o[8], sum = 0.0f;
                for (int i = 0; i < 8; ++i)
                {
                    const float m = (i & 1 ? 1.0f : -1.0f) * std::sin (twoPi * (phase + 0.125f * (float) i)) * 0.0008f * sr;
                    o[i] = lp[i].process (lines[i].read (delays[i] + m));
                    sum += o[i];
                }
                const float h = sum * 0.25f;   // Householder: x - 2/N * sum
                float wl = 0, wr = 0;
                for (int i = 0; i < 8; ++i)
                {
                    lines[i].push ((o[i] - h) * gains[i] + in * 0.35f);
                    (i & 1 ? wr : wl) += o[i];
                }
                wl *= 0.35f; wr *= 0.35f;
                const float mid = 0.5f * (wl + wr), side = 0.5f * (wl - wr) * width;
                L[s] = L[s] * (1.0f - 0.5f * mix) + (mid + side) * mix;
                R[s] = R[s] * (1.0f - 0.5f * mix) + (mid - side) * mix;
            }
        }
    private:
        float sr = 44100.0f, phase = 0.0f;
        Delay lines[8], diff[4], pre[2];
        OnePole lp[8];
    };

    class FxRack
    {
    public:
        enum { distortion, chorus, phaser, flanger, delay, reverb, eq, compressor, tape };
        std::atomic<int> order[numFx] { { 0 }, { 8 }, { 1 }, { 2 }, { 3 }, { 4 }, { 5 }, { 6 }, { 7 } };   // tape sits after distortion by default
        float globalMod[numDests] {};   // written by the engine each block

        void bind (juce::AudioProcessorValueTreeState& s)
        {
            auto g = [&s] (const char* id) { return s.getRawParameterValue (id); };
            on[distortion] = g ("dist_on"); on[chorus] = g ("ch_on"); on[phaser] = g ("ph_on"); on[flanger] = g ("fl_on");
            on[delay] = g ("dly_on"); on[reverb] = g ("rev_on"); on[eq] = g ("eq_on"); on[compressor] = g ("comp_on"); on[tape] = g ("tape_on");
            const char* ids[] = { "dist_type", "dist_drive", "dist_tone", "dist_mix", "ch_rate", "ch_depth", "ch_mix",
                                  "ph_rate", "ph_depth", "ph_fb", "ph_mix", "fl_rate", "fl_depth", "fl_fb", "fl_mix",
                                  "dly_l", "dly_r", "dly_fb", "dly_ping", "dly_tone", "dly_mix",
                                  "rev_size", "rev_decay", "rev_damp", "rev_pre", "rev_width", "rev_mix",
                                  "eq_low", "eq_mid", "eq_midfreq", "eq_high",
                                  "comp_thresh", "comp_ratio", "comp_attack", "comp_release", "comp_gain", "comp_mix",
                                  "tape_wow", "tape_flutter", "tape_sat", "tape_hiss", "tape_age", "tape_mix" };
            for (size_t i = 0; i < std::size (ids); ++i) prm[i] = g (ids[i]);
        }

        void prepare (double sampleRate, int)
        {
            sr = (float) sampleRate;
            for (int c = 0; c < 2; ++c)
            {
                tapeD[c].prepare ((int) (0.03f * sr)); tapeLp[c].reset(); tapeHp[c].reset(); tapeHiss[c].reset(); tapeHold[c] = 0;
                chorusD[c].prepare ((int) (0.05f * sr)); flangeD[c].prepare ((int) (0.02f * sr)); dly[c].prepare ((int) (8.5f * sr));
                distLp[c].reset(); dlyLp[c].reset(); dlyHp[c].reset(); for (auto& b : eqf[c]) b.reset();
                for (auto& a : phaserZ[c]) a = 0.0f;
                phFb[c] = 0; flFb[c] = 0; dc[c].prepare (sr); dc[c].reset();
            }
            rev.prepare (sr);
            compEnv = 0; chPh = phPh = flPh = 0; wowPh = flutPh = 0; tapeEnv = 0; tapeHoldCount = 0; flutRnd = flutRndT = 0;
        }
        void reset() { prepare (sr, 0); }
        void setOrderFrom (const FxRack& o) { for (int i = 0; i < numFx; ++i) order[i] = o.order[i].load(); }

        void process (float* L, float* R, int n, double bpm)
        {
            for (int k = 0; k < numFx; ++k)
            {
                const int fxIdx = order[k].load();
                if (fxIdx < 0 || fxIdx >= numFx || on[fxIdx]->load() < 0.5f) continue;
                switch (fxIdx)
                {
                    case distortion: doDist (L, R, n); break;
                    case chorus:     doChorus (L, R, n); break;
                    case phaser:     doPhaser (L, R, n); break;
                    case flanger:    doFlanger (L, R, n); break;
                    case delay:      doDelay (L, R, n, bpm); break;
                    case reverb:     rev.process (L, R, n, clampf (v (21) + globalMod[dstReverbSize], 0, 1), v (22), v (23), v (24), v (25),
                                                  clampf (v (26) + globalMod[dstReverbMix], 0, 1)); break;
                    case eq:         doEq (L, R, n); break;
                    case compressor: doComp (L, R, n); break;
                    case tape:       doTape (L, R, n); break;
                    default: break;
                }
            }
        }

    private:
        float v (int i) const { return prm[(size_t) i]->load(); }

        void doDist (float* L, float* R, int n)
        {
            const int type = (int) v (0);
            const float drive = clampf (v (1) + globalMod[dstDistDrive], 0, 1), mix = clampf (v (3) + globalMod[dstDistMix], 0, 1);
            const float g = 1.0f + drive * drive * 40.0f, makeup = 1.0f / std::sqrt (std::sqrt (g));
            for (int c = 0; c < 2; ++c) distLp[c].setCutoff (700.0f * std::exp2 (v (2) * 4.8f), sr);
            for (int i = 0; i < n; ++i)
                for (int c = 0; c < 2; ++c)
                {
                    float* x = c == 0 ? L : R;
                    const float in = x[i], u = in * g;
                    float y;
                    switch (type)
                    {
                        case 1:  y = u > 0 ? fastTanh (u) : fastTanh (u * 0.6f) * 1.2f; break;         // tube (asymmetric)
                        case 2:  y = clampf (u, -1.0f, 1.0f); break;
                        case 3:  { const float lv = std::exp2 (12.0f - drive * 10.0f); y = std::round (fastTanh (u * 0.3f) * lv) / lv; break; }
                        case 4:  y = std::sin (u * 0.5f); break;
                        case 5:  y = std::abs (fastTanh (u)) * 2.0f - 0.6f; break;
                        default: y = fastTanh (u);
                    }
                    y = dc[c].process (distLp[c].process (y)) * makeup;
                    x[i] = in + (y - in) * mix;
                }
        }

        void doChorus (float* L, float* R, int n)
        {
            const float rate = v (4), depth = v (5), mix = clampf (v (6) + globalMod[dstChorusMix], 0, 1);
            for (int i = 0; i < n; ++i)
            {
                chPh += rate / sr; if (chPh >= 1) chPh -= 1;
                for (int c = 0; c < 2; ++c)
                {
                    float* x = c == 0 ? L : R;
                    chorusD[c].push (x[i]);
                    float w = 0;
                    for (int k = 0; k < 3; ++k)
                    {
                        const float ms = 9.0f + 6.0f * depth * (0.5f + 0.5f * std::sin (twoPi * (chPh + (float) k / 3.0f + (c ? 0.17f : 0.0f))));
                        w += chorusD[c].read (ms * 0.001f * sr);
                    }
                    x[i] += (w / 3.0f - x[i]) * mix;
                }
            }
        }

        void doPhaser (float* L, float* R, int n)
        {
            const float rate = v (7), depth = v (8), fb = v (9), mix = clampf (v (10) + globalMod[dstPhaserMix], 0, 1);
            for (int i = 0; i < n; ++i)
            {
                phPh += rate / sr; if (phPh >= 1) phPh -= 1;
                for (int c = 0; c < 2; ++c)
                {
                    float* x = c == 0 ? L : R;
                    const float sweep = 0.5f + 0.5f * std::sin (twoPi * (phPh + (c ? 0.25f : 0.0f)));
                    const float f = 200.0f * std::exp2 (sweep * 4.3f * depth + 0.2f);
                    const float t = std::tan (pi * std::min (f, sr * 0.45f) / sr), a = (1 - t) / (1 + t);
                    float y = x[i] + phFb[c] * fb;
                    for (auto& z : phaserZ[c]) { const float o = -a * y + z; z = y + a * o; y = o; }
                    phFb[c] = fastTanh (y);
                    x[i] += (0.5f * (x[i] + y) - x[i]) * mix;
                }
            }
        }

        void doFlanger (float* L, float* R, int n)
        {
            const float rate = v (11), depth = v (12), fb = v (13), mix = v (14);
            for (int i = 0; i < n; ++i)
            {
                flPh += rate / sr; if (flPh >= 1) flPh -= 1;
                for (int c = 0; c < 2; ++c)
                {
                    float* x = c == 0 ? L : R;
                    const float ms = 0.4f + 5.0f * depth * (0.5f + 0.5f * std::sin (twoPi * (flPh + (c ? 0.5f : 0.0f))));
                    const float d = flangeD[c].read (ms * 0.001f * sr);
                    flangeD[c].push (x[i] + fastTanh (d * fb));
                    x[i] += (0.5f * (x[i] + d) - x[i]) * mix;
                }
            }
        }

        void doDelay (float* L, float* R, int n, double bpm)
        {
            const float tl = clampf ((float) (Choice::divBeats[juce::jlimit (0, 16, (int) v (15))] * 60.0 / bpm) * sr, 10.0f, 8.0f * sr);
            const float tr = clampf ((float) (Choice::divBeats[juce::jlimit (0, 16, (int) v (16))] * 60.0 / bpm) * sr, 10.0f, 8.0f * sr);
            const float fb = clampf (v (17) + globalMod[dstDelayFb] * 0.5f, 0.0f, 0.95f), ping = v (18);
            const float mix = clampf (v (20) + globalMod[dstDelayMix], 0, 1);
            for (int c = 0; c < 2; ++c) { dlyLp[c].setCutoff (1500.0f * std::exp2 (v (19) * 3.7f), sr); dlyHp[c].setCutoff (120.0f, sr); }
            for (int i = 0; i < n; ++i)
            {
                const float dl = dly[0].read (tl), dr = dly[1].read (tr);
                float fl = dlyLp[0].process (dl); fl -= dlyHp[0].process (fl);
                float fr = dlyLp[1].process (dr); fr -= dlyHp[1].process (fr);
                const float inL = L[i], inR = R[i];
                dly[0].push (fastTanh (inL * (1.0f - ping * 0.5f) + (fl * (1 - ping) + fr * ping) * fb));
                dly[1].push (fastTanh (inR * (1.0f - ping) + (fr * (1 - ping) + fl * ping) * fb));
                L[i] += dl * mix; R[i] += dr * mix;
            }
        }

        void doEq (float* L, float* R, int n)
        {
            for (int c = 0; c < 2; ++c)
            {
                eqf[c][0].set (0, 110.0f, 0.7f, v (27), sr);
                eqf[c][1].set (1, v (29), 0.9f, v (28), sr);
                eqf[c][2].set (2, 7000.0f, 0.7f, v (30), sr);
            }
            for (int i = 0; i < n; ++i)
                for (int c = 0; c < 2; ++c) { float* x = c == 0 ? L : R; for (auto& b : eqf[c]) x[i] = b.process (x[i]); }
        }

        void doComp (float* L, float* R, int n)
        {
            const float thr = v (31), ratio = v (32), mk = juce::Decibels::decibelsToGain (v (35)), mix = v (36);
            const float att = std::exp (-1.0f / (0.001f * v (33) * sr)), rel = std::exp (-1.0f / (0.001f * v (34) * sr));
            for (int i = 0; i < n; ++i)
            {
                const float lvl = std::max (std::abs (L[i]), std::abs (R[i]));
                const float db = 20.0f * std::log10 (lvl + 1.0e-9f);
                const float over = std::max (0.0f, db - thr);
                const float target = over * (1.0f - 1.0f / ratio);
                compEnv = target + (target > compEnv ? att : rel) * (compEnv - target);
                const float g = juce::Decibels::decibelsToGain (-compEnv) * mk;
                L[i] += (L[i] * g - L[i]) * mix; R[i] += (R[i] * g - R[i]) * mix;
            }
        }

        // Tape: wow & flutter (modulated delay), saturation, hiss that follows the signal, and "age"
        // (high cut, low cut and a touch of sample-rate reduction).
        void doTape (float* L, float* R, int n)
        {
            const float wow = v (37), flut = v (38), sat = v (39), hiss = v (40), age = v (41), mix = v (42);
            const float drive = 1.0f + sat * 6.0f, makeup = 1.0f / (1.0f + sat * 1.6f);
            for (int c = 0; c < 2; ++c)
            {
                tapeLp[c].setCutoff (18000.0f * std::exp2 (-age * 2.6f), sr);
                tapeHp[c].setCutoff (20.0f + age * 140.0f, sr);
                tapeHiss[c].setCutoff (6000.0f, sr);
            }
            const int holdLen = 1 + (int) (age * age * 3.0f * sr / 44100.0f);
            const float envA = std::exp (-1.0f / (0.25f * sr));
            for (int i = 0; i < n; ++i)
            {
                wowPh += 0.55f / sr; if (wowPh >= 1) wowPh -= 1;
                flutPh += 6.8f / sr; if (flutPh >= 1) flutPh -= 1;
                if (--flutRndT <= 0) { flutRndT = (int) (0.05f * sr); flutRndTarget = rng.bi(); }
                flutRnd += (flutRndTarget - flutRnd) * 0.0005f;
                const float delayMs = 6.0f + wow * 3.2f * std::sin (twoPi * wowPh) + flut * (0.35f * std::sin (twoPi * flutPh) + 0.25f * flutRnd);
                const float lvl = 0.5f * (std::abs (L[i]) + std::abs (R[i]));
                tapeEnv = std::max (lvl, tapeEnv * envA);
                const bool hold = ++tapeHoldCount >= holdLen;
                if (hold) tapeHoldCount = 0;
                for (int c = 0; c < 2; ++c)
                {
                    float* x = c == 0 ? L : R;
                    const float in = x[i];
                    tapeD[c].push (in);
                    float y = tapeD[c].read (delayMs * 0.001f * sr);
                    y = fastTanh (y * drive) * makeup;
                    y = tapeLp[c].process (y);
                    y -= tapeHp[c].process (y);
                    if (hold) tapeHold[c] = y;
                    y = tapeHold[c];
                    const float hn = rng.bi();
                    y += (hn - tapeHiss[c].process (hn)) * hiss * 0.05f * std::min (1.0f, tapeEnv * 4.0f + 0.02f);
                    x[i] = in + (y - in) * mix;
                }
            }
        }

        float sr = 44100.0f;
        std::atomic<float>* on[numFx] {};
        std::array<std::atomic<float>*, 43> prm {};
        OnePole distLp[2], dlyLp[2], dlyHp[2];
        DcBlocker dc[2];
        Delay chorusD[2], flangeD[2], dly[2];
        float phaserZ[2][6] {}, phFb[2] {}, flFb[2] {};
        float chPh = 0, phPh = 0, flPh = 0, compEnv = 0;
        Biquad eqf[2][3];
        FdnReverb rev;
        Delay tapeD[2];
        OnePole tapeLp[2], tapeHp[2], tapeHiss[2];
        float tapeHold[2] {}, wowPh = 0, flutPh = 0, tapeEnv = 0, flutRnd = 0, flutRndTarget = 0;
        int tapeHoldCount = 0, flutRndT = 0;
        Rng rng;
    };
}
