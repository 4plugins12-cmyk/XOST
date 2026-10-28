#pragma once
// XO's two "tools": the Matrix (a tempo-synced gate / filter / pan / time shaper on the output, in the spirit of
// Gross Beat and ShaperBox) and the Minirack (six eurorack-style modulation modules patched to any destination).
#include <juce_audio_processors/juce_audio_processors.h>
#include "SynthDSP.h"
#include "Params.h"

namespace xo
{
    // ------------------------------------------------------------------ Matrix lanes (shared with the UI)
    struct MatrixShapes
    {
        float lane[numMxLanes][mxTableSize] {};

        static float sample (const float* t, double phase)
        {
            const double p = phase * (double) mxTableSize;
            const int i = (int) p;
            const float f = (float) (p - (double) i);
            const float a = t[i & (mxTableSize - 1)], b = t[(i + 1) & (mxTableSize - 1)];
            return a + (b - a) * f;
        }
    };

    // A lane is edited as a list of breakpoints (x 0..1, y 0..1, curve -1..1) and rendered into a table.
    struct Breakpoint { float x, y, curve; };
    inline void renderLane (const std::vector<Breakpoint>& pts, float* table)
    {
        if (pts.empty()) { for (int i = 0; i < mxTableSize; ++i) table[i] = 1.0f; return; }
        for (int i = 0; i < mxTableSize; ++i)
        {
            const float x = (float) i / (float) mxTableSize;
            // find the segment (the curve wraps from the last point to the first one)
            size_t k = 0;
            while (k + 1 < pts.size() && pts[k + 1].x <= x) ++k;
            Breakpoint a = pts[k], b;
            float span, t;
            if (x < pts.front().x) { a = pts.back(); b = pts.front(); span = (1.0f - a.x) + b.x; t = (x + 1.0f - a.x) / std::max (1.0e-5f, span); }
            else if (k + 1 < pts.size()) { b = pts[k + 1]; span = b.x - a.x; t = (x - a.x) / std::max (1.0e-5f, span); }
            else { b = pts.front(); span = (1.0f - a.x) + b.x; t = (x - a.x) / std::max (1.0e-5f, span); }
            t = clampf (t, 0.0f, 1.0f);
            const float c = a.curve;
            if (std::abs (c) > 0.01f) t = c > 0 ? std::pow (t, 1.0f + c * 4.0f) : 1.0f - std::pow (1.0f - t, 1.0f - c * 4.0f);
            table[i] = clampf (a.y + (b.y - a.y) * t, 0.0f, 1.0f);
        }
    }

    class MatrixFx
    {
    public:
        void bind (juce::AudioProcessorValueTreeState& s)
        {
            auto g = [&s] (const juce::String& id) { return s.getRawParameterValue (id); };
            on = g ("mx_on"); mix = g ("mx_mix"); smooth = g ("mx_smooth");
            for (int l = 0; l < numMxLanes; ++l)
            {
                const juce::String id = juce::String ("mx_") + mxLaneIds[l] + "_";
                laneOn[l] = g (id + "on"); depth[l] = g (id + "depth"); len[l] = g (id + "len");
            }
            fltType = g ("mx_flt_type"); fltReso = g ("mx_flt_reso"); timeRange = g ("mx_time_range");
        }
        void prepare (double sampleRate)
        {
            sr = (float) sampleRate;
            const int size = (int) (sr * 6.5f);
            for (auto& b : buf) b.assign ((size_t) size, 0.0f);
            w = 0;
            for (auto& f : svf) f.reset();
            for (auto& v : sm) v = -1.0f;
        }
        bool isOn() const { return on != nullptr && on->load() > 0.5f; }

        // phase source: beat position (host or internal)
        void process (float* L, float* R, int n, double beatPos, double bpm, const MatrixShapes* shapes)
        {
            if (shapes == nullptr || buf[0].empty()) return;
            const bool active = isOn();
            const int size = (int) buf[0].size();
            const double beatsPerSample = bpm / 60.0 / (double) sr;
            const float wet = mix->load();
            const float smA = std::exp (-1.0f / (std::max (0.0005f, 0.0005f + smooth->load() * 0.03f) * sr));
            bool lon[numMxLanes]; float dep[numMxLanes]; double lenB[numMxLanes];
            for (int l = 0; l < numMxLanes; ++l)
            {
                lon[l] = active && laneOn[l]->load() > 0.5f;
                dep[l] = depth[l]->load();
                lenB[l] = Choice::mxLenBeats[juce::jlimit (0, 4, (int) len[l]->load())];
            }
            const int ftype = (int) fltType->load();
            const float reso = fltReso->load();
            const float maxDelay = (float) (Choice::mxTimeBeats[juce::jlimit (0, 4, (int) timeRange->load())] * 60.0 / bpm) * sr;

            for (int i = 0; i < n; ++i)
            {
                const double bp = beatPos + (double) i * beatsPerSample;
                float target[numMxLanes];
                for (int l = 0; l < numMxLanes; ++l)
                    target[l] = lon[l] ? MatrixShapes::sample (shapes->lane[l], std::fmod (bp / lenB[l], 1.0)) : (l == 0 ? 1.0f : (l == 2 ? 0.5f : (l == 3 ? 0.0f : 1.0f)));
                for (int l = 0; l < numMxLanes; ++l) sm[l] = sm[l] < 0.0f ? target[l] : target[l] + smA * (sm[l] - target[l]);

                // record the dry signal for the time lane
                buf[0][(size_t) w] = L[i]; buf[1][(size_t) w] = R[i];
                float l = L[i], r = R[i];
                if (lon[3] && dep[3] > 0.0001f)
                {
                    const float d = clampf (sm[3] * dep[3] * maxDelay, 0.0f, (float) size - 4.0f);
                    const float rp = (float) w - d;
                    const float fl = std::floor (rp);
                    const int i0 = ((int) fl % size + size) % size, i1 = (i0 + 1) % size;
                    const float fr = rp - fl;
                    l = buf[0][(size_t) i0] + (buf[0][(size_t) i1] - buf[0][(size_t) i0]) * fr;
                    r = buf[1][(size_t) i0] + (buf[1][(size_t) i1] - buf[1][(size_t) i0]) * fr;
                }
                w = (w + 1) % size;
                if (lon[1])
                {
                    const float open = ftype == 1 ? std::log2 (20.0f) : (ftype == 2 ? std::log2 (1000.0f) : std::log2 (20000.0f));
                    const float shaped = std::log2 (20.0f) + sm[1] * (std::log2 (20000.0f) - std::log2 (20.0f));
                    const float hz = std::exp2 (open + (shaped - open) * dep[1]);
                    if ((i & 15) == 0) for (auto& f : svf) f.set (clampf (hz, 20.0f, sr * 0.45f), 0.1f + reso * 0.85f, sr);
                    float lp, bpo, hp;
                    svf[0].tick (l, lp, bpo, hp); l = ftype == 0 ? lp : (ftype == 1 ? hp : bpo * 1.4f);
                    svf[1].tick (r, lp, bpo, hp); r = ftype == 0 ? lp : (ftype == 1 ? hp : bpo * 1.4f);
                }
                if (lon[2])
                {
                    const float pan = clampf ((sm[2] * 2.0f - 1.0f) * dep[2], -1.0f, 1.0f);
                    const float gl = pan > 0 ? 1.0f - pan : 1.0f, gr = pan < 0 ? 1.0f + pan : 1.0f;
                    const float ml = l, mr = r;
                    l = ml * gl + (pan < 0 ? mr * -pan * 0.5f : 0.0f);
                    r = mr * gr + (pan > 0 ? ml * pan * 0.5f : 0.0f);
                }
                if (lon[0])
                {
                    const float g = 1.0f - dep[0] * (1.0f - sm[0]);
                    l *= g; r *= g;
                }
                if (active) { L[i] += (l - L[i]) * wet; R[i] += (r - R[i]) * wet; }
            }
            uiPhase = (float) std::fmod (beatPos / lenB[0], 1.0);
            for (int k = 0; k < numMxLanes; ++k) uiLanePhase[k] = (float) std::fmod (beatPos / lenB[k], 1.0);
        }

        std::atomic<float> uiPhase { 0.0f };
        std::atomic<float> uiLanePhase[numMxLanes] {};
    private:
        std::atomic<float> *on = nullptr, *mix = nullptr, *smooth = nullptr, *fltType = nullptr, *fltReso = nullptr, *timeRange = nullptr;
        std::atomic<float>* laneOn[numMxLanes] {}; std::atomic<float>* depth[numMxLanes] {}; std::atomic<float>* len[numMxLanes] {};
        float sr = 44100.0f;
        std::vector<float> buf[2];
        int w = 0;
        SVF svf[2];
        float sm[numMxLanes] { -1.0f, -1.0f, -1.0f, -1.0f };
    };

    // ------------------------------------------------------------------ Minirack
    class Minirack
    {
    public:
        enum Type { empty, lfo, sampleHold, drunk, turing, chaos, sequencer, bounce, follower };

        void bind (juce::AudioProcessorValueTreeState& s)
        {
            auto g = [&s] (const juce::String& id) { return s.getRawParameterValue (id); };
            on = g ("rk_on"); depth = g ("rk_depth");
            for (int m = 0; m < numRack; ++m)
            {
                auto& p = prm[m];
                p.type = g (rackId (m, "type")); p.sync = g (rackId (m, "sync")); p.rate = g (rackId (m, "rate")); p.div = g (rackId (m, "div"));
                p.a = g (rackId (m, "a")); p.b = g (rackId (m, "b"));
                p.dst1 = g (rackId (m, "dst1")); p.amt1 = g (rackId (m, "amt1")); p.dst2 = g (rackId (m, "dst2")); p.amt2 = g (rackId (m, "amt2"));
            }
        }
        void reset()
        {
            for (int m = 0; m < numRack; ++m)
            {
                auto& st = state[m];
                st = {};
                st.rng.seed (0x9e3779b9u * (uint32_t) (m + 1));
                st.reg = st.rng.next() & 0xffff;
                st.cx = 0.1f; st.cy = 0.0f; st.cz = 0.0f;
            }
        }
        bool isOn() const { return on != nullptr && on->load() > 0.5f; }

        // advance n samples; adds each module's output to mods[dest]. level = synth output level (for the follower)
        void process (int n, double sr, double bpm, double beatPos, bool hostPlaying, float level, float* mods)
        {
            if (! isOn()) { for (auto& o : out) o = 0.0f; return; }
            const float dep = depth->load();
            const float dt = (float) n / (float) sr;
            for (int m = 0; m < numRack; ++m)
            {
                auto& p = prm[m]; auto& st = state[m];
                const int type = (int) p.type->load();
                if (type == empty) { out[m] = 0.0f; continue; }
                const bool sync = p.sync->load() > 0.5f;
                const double beats = Choice::divBeats[juce::jlimit (0, 16, (int) p.div->load())];
                const float hz = sync ? (float) (bpm / 60.0 / beats) : p.rate->load();
                const float a = p.a->load(), b = p.b->load();
                float ph;
                bool wrapped = false;
                if (sync && hostPlaying) { ph = (float) std::fmod (beatPos / beats, 1.0); wrapped = ph < st.phase; }
                else { ph = st.phase + hz * dt; wrapped = ph >= 1.0f; ph -= std::floor (ph); }
                st.phase = ph;
                float v = 0.0f;
                switch (type)
                {
                    case lfo:
                    {
                        // shape morphs sine -> triangle -> saw -> square; variation skews the cycle
                        const float skew = 0.5f + (b - 0.3f) * 0.6f;
                        const float x = ph < skew ? ph / std::max (0.01f, skew) * 0.5f : 0.5f + (ph - skew) / std::max (0.01f, 1.0f - skew) * 0.5f;
                        const float s0 = std::sin (twoPi * x), s1 = 1.0f - 4.0f * std::abs (x - 0.5f) - 0.0f;
                        const float tri = x < 0.5f ? 4.0f * x - 1.0f : 3.0f - 4.0f * x;
                        juce::ignoreUnused (s1);
                        const float saw = 2.0f * x - 1.0f, sq = x < 0.5f ? 1.0f : -1.0f;
                        const float k = a * 3.0f; const int i0 = std::min (2, (int) k); const float f = k - (float) i0;
                        const float shp[4] = { s0, tri, saw, sq };
                        v = shp[i0] + (shp[i0 + 1] - shp[i0]) * f;
                        break;
                    }
                    case sampleHold:
                        if (wrapped && st.rng.uni() < 0.35f + a * 0.65f) st.target = st.rng.bi();
                        st.value += (st.target - st.value) * (b < 0.02f ? 1.0f : std::min (1.0f, dt / (b * 0.4f)));
                        v = st.value; break;
                    case drunk:
                        if (wrapped) st.target = clampf (st.target + st.rng.bi() * (0.1f + a * 0.6f) - st.target * b * 0.5f, -1.0f, 1.0f);
                        st.value += (st.target - st.value) * std::min (1.0f, dt * 12.0f);
                        v = st.value; break;
                    case turing:
                    {
                        const int length = 2 + (int) (a * 14.0f);
                        if (wrapped)
                        {
                            uint32_t bit = (st.reg >> (length - 1)) & 1u;
                            if (st.rng.uni() < b * b) bit ^= 1u;
                            st.reg = ((st.reg << 1) | bit) & 0xffffu;
                        }
                        st.value = (float) (st.reg & 0xffu) / 127.5f - 1.0f;
                        v = st.value; break;
                    }
                    case chaos:
                    {
                        // Lorenz attractor, speed set by the rate
                        const float rho = 24.0f + b * 12.0f;
                        const int steps = 1 + (int) (hz * 4.0f);
                        const float h = std::min (0.02f, dt * hz * 0.8f / (float) steps * 10.0f);
                        for (int k = 0; k < steps; ++k)
                        {
                            const float dx = 10.0f * (st.cy - st.cx), dy = st.cx * (rho - st.cz) - st.cy, dz = st.cx * st.cy - 2.6667f * st.cz;
                            st.cx += dx * h; st.cy += dy * h; st.cz += dz * h;
                        }
                        if (! std::isfinite (st.cx)) { st.cx = 0.1f; st.cy = 0.0f; st.cz = 0.0f; }
                        v = clampf ((a < 0.5f ? st.cx : st.cy) / 20.0f, -1.0f, 1.0f);
                        break;
                    }
                    case sequencer:
                    {
                        const int step = (int) (ph * 8.0f) & 7;
                        uint32_t hsh = (uint32_t) (a * 997.0f) * 2654435761u + (uint32_t) step * 40503u;
                        hsh ^= hsh >> 13; hsh *= 0x5bd1e995u; hsh ^= hsh >> 15;
                        st.target = (float) (hsh & 1023u) / 511.5f - 1.0f;
                        st.value += (st.target - st.value) * (b < 0.02f ? 1.0f : std::min (1.0f, dt / (b * 0.15f)));
                        v = st.value; break;
                    }
                    case bounce:
                    {
                        // a ball dropped each cycle: bounces get lower and faster
                        const int bounces = 2 + (int) (a * 5.0f);
                        float acc = 0.0f, seg = 0.5f, height = 1.0f, y = 0.0f;
                        for (int k = 0; k < bounces; ++k)
                        {
                            if (ph < acc + seg || k == bounces - 1)
                            {
                                const float u = clampf ((ph - acc) / seg, 0.0f, 1.0f);
                                y = height * 4.0f * u * (1.0f - u);
                                break;
                            }
                            acc += seg; seg *= 0.5f + b * 0.3f; height *= 0.45f + b * 0.35f;
                        }
                        v = y * 2.0f - 1.0f; break;
                    }
                    case follower:
                    {
                        const float sens = 1.0f + b * 15.0f;
                        const float tc = 0.01f + a * 0.5f;
                        const float target = clampf (level * sens, 0.0f, 1.0f);
                        st.value += (target - st.value) * std::min (1.0f, dt / tc);
                        v = st.value; break;
                    }
                    default: break;
                }
                out[m] = v;
                const int d1 = (int) p.dst1->load(), d2 = (int) p.dst2->load();
                if (d1 > 0 && d1 < numDests) mods[d1] += v * p.amt1->load() * dep;
                if (d2 > 0 && d2 < numDests) mods[d2] += v * p.amt2->load() * dep;
            }
        }

        std::atomic<float> uiOut[numRack] {};
        float out[numRack] {};
    private:
        struct P { std::atomic<float> *type, *sync, *rate, *div, *a, *b, *dst1, *amt1, *dst2, *amt2; };
        struct S { float phase = 0, value = 0, target = 0; uint32_t reg = 0; float cx = 0.1f, cy = 0, cz = 0; Rng rng; };
        P prm[numRack] {};
        S state[numRack];
        std::atomic<float> *on = nullptr, *depth = nullptr;
    };
}
