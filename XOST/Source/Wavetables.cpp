#include "Wavetables.h"

namespace xo
{
    constexpr float kPi = 3.14159265358979f;

    void fft (std::vector<float>& re, std::vector<float>& im, bool inverse)
    {
        const int n = (int) re.size();
        for (int i = 1, j = 0; i < n; ++i)
        {
            int bit = n >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) { std::swap (re[(size_t) i], re[(size_t) j]); std::swap (im[(size_t) i], im[(size_t) j]); }
        }
        for (int len = 2; len <= n; len <<= 1)
        {
            const double ang = 2.0 * 3.14159265358979323846 / len * (inverse ? 1.0 : -1.0);
            const float wr = (float) std::cos (ang), wi = (float) std::sin (ang);
            for (int i = 0; i < n; i += len)
            {
                float cr = 1.0f, ci = 0.0f;
                for (int k = 0; k < len / 2; ++k)
                {
                    const size_t a = (size_t) (i + k), b = (size_t) (i + k + len / 2);
                    const float tr = re[b] * cr - im[b] * ci, ti = re[b] * ci + im[b] * cr;
                    re[b] = re[a] - tr; im[b] = im[a] - ti;
                    re[a] += tr;        im[a] += ti;
                    const float ncr = cr * wr - ci * wi;
                    ci = cr * wi + ci * wr; cr = ncr;
                }
            }
        }
        if (inverse) for (int i = 0; i < n; ++i) { re[(size_t) i] /= (float) n; im[(size_t) i] /= (float) n; }
    }

    void Wavetable::buildMips (const std::vector<std::vector<float>>& specRe, const std::vector<std::vector<float>>& specIm)
    {
        numFrames = (int) specRe.size();
        data.assign ((size_t) numFrames * levels * (N + 1), 0.0f);
        sourceFrames.assign ((size_t) numFrames, std::vector<float> ((size_t) N, 0.0f));
        std::vector<float> re ((size_t) N), im ((size_t) N);

        for (int f = 0; f < numFrames; ++f)
        {
            float scale = 1.0f;
            for (int L = 0; L < levels; ++L)
            {
                const int maxH = L == 0 ? N / 2 - 1 : (1024 >> L);
                for (int k = 0; k < N; ++k)
                {
                    const int h = k <= N / 2 ? k : N - k;
                    const bool keep = h >= 1 && h <= maxH;
                    re[(size_t) k] = keep ? specRe[(size_t) f][(size_t) k] : 0.0f;
                    im[(size_t) k] = keep ? specIm[(size_t) f][(size_t) k] : 0.0f;
                }
                fft (re, im, true);
                if (L == 0)
                {
                    float peak = 1.0e-6f;
                    for (int i = 0; i < N; ++i) peak = std::max (peak, std::abs (re[(size_t) i]));
                    scale = 0.95f / peak;
                    for (int i = 0; i < N; ++i) sourceFrames[(size_t) f][(size_t) i] = re[(size_t) i] * scale;
                }
                float* dst = data.data() + ((size_t) f * levels + (size_t) L) * (N + 1);
                for (int i = 0; i < N; ++i) dst[i] = re[(size_t) i] * scale;
                dst[N] = dst[0];
            }
        }
    }

    void Wavetable::buildFromFrames (const std::vector<std::vector<float>>& frames)
    {
        std::vector<std::vector<float>> sr, si;
        for (auto& fr : frames)
        {
            std::vector<float> re (fr.begin(), fr.end()), im ((size_t) N, 0.0f);
            re.resize ((size_t) N, 0.0f);
            fft (re, im, false);
            sr.push_back (re); si.push_back (im);
            if ((int) sr.size() >= maxFrames) break;
        }
        if (sr.empty()) { sr.assign (1, std::vector<float> ((size_t) N, 0.0f)); si = sr; }
        buildMips (sr, si);
    }

    void Wavetable::buildFromHarmonics (int frames, const std::function<float (int, int)>& fn)
    {
        std::vector<std::vector<float>> sr ((size_t) frames, std::vector<float> ((size_t) N, 0.0f)), si = sr;
        for (int f = 0; f < frames; ++f)
            for (int n = 1; n < N / 2; ++n)
            {
                const float a = fn (f, n);
                if (std::abs (a) < 1.0e-12f) continue;
                // a * sin(2 pi n t)  ->  X[n] = -i a N/2, X[N-n] = +i a N/2
                si[(size_t) f][(size_t) n] = -a * (float) N * 0.5f;
                si[(size_t) f][(size_t) (N - n)] = a * (float) N * 0.5f;
            }
        buildMips (sr, si);
    }

    void Wavetable::getDisplayFrame (float pos, std::vector<float>& out, int points) const
    {
        out.resize ((size_t) points);
        for (int i = 0; i < points; ++i)
            out[(size_t) i] = lookup ((float) i / (float) points, pos, 0);
    }

    juce::String Wavetable::toBase64() const
    {
        juce::MemoryOutputStream mo;
        mo.writeInt (numFrames);
        for (auto& fr : sourceFrames) for (float v : fr) mo.writeFloat (v);
        return mo.getMemoryBlock().toBase64Encoding();
    }

    bool Wavetable::fromBase64 (const juce::String& s)
    {
        juce::MemoryBlock mb;
        if (! mb.fromBase64Encoding (s)) return false;
        juce::MemoryInputStream mi (mb, false);
        const int frames = mi.readInt();
        if (frames < 1 || frames > maxFrames || mb.getSize() < (size_t) (4 + frames * N * 4)) return false;
        std::vector<std::vector<float>> fr ((size_t) frames, std::vector<float> ((size_t) N));
        for (auto& f : fr) for (auto& v : f) v = mi.readFloat();
        buildFromFrames (fr);
        return true;
    }

    std::unique_ptr<Wavetable> wavetableFromAudio (const float* x, int n, bool exactCycles)
    {
        constexpr int N = Wavetable::N;
        std::vector<std::vector<float>> frames;
        auto resampled = [&] (int start, int len)
        {
            std::vector<float> f ((size_t) N);
            for (int i = 0; i < N; ++i)
            {
                const float p = (float) start + (float) i * (float) len / (float) N;
                const int a = juce::jlimit (0, n - 1, (int) p), b = juce::jlimit (0, n - 1, a + 1);
                f[(size_t) i] = x[a] + (x[b] - x[a]) * (p - (float) a);
            }
            if (! exactCycles)   // smooth the seam of non-periodic slices
                for (int i = 0; i < 64; ++i)
                {
                    const float w = (float) i / 64.0f;
                    const float mixv = f[(size_t) i] * w + f[(size_t) (N - 64 + i)] * (1.0f - w);
                    f[(size_t) i] = mixv;
                }
            return f;
        };
        if (n <= 0) return nullptr;
        if (exactCycles || (n % N == 0 && n >= N))
        {
            const int total = n / N;
            const int count = std::min (total, Wavetable::maxFrames);
            for (int k = 0; k < count; ++k)
            {
                const int idx = count == 1 ? 0 : (int) std::round ((double) k * (total - 1) / (count - 1));
                frames.push_back (std::vector<float> (x + idx * N, x + idx * N + N));
            }
        }
        else if (n < 2 * N)
            frames.push_back (resampled (0, n));
        else
        {
            const int len = std::min (N, n / Wavetable::maxFrames);
            for (int k = 0; k < Wavetable::maxFrames; ++k)
                frames.push_back (resampled ((int) ((double) k * (n - len) / (Wavetable::maxFrames - 1)), len));
        }
        auto wt = std::make_unique<Wavetable>();
        wt->buildFromFrames (frames);
        return wt;
    }

    // ================================================================ factory tables
    namespace
    {
        float tri (int n)    { return (n % 2 == 1) ? 8.0f / (kPi * kPi) * ((((n - 1) / 2) % 2) ? -1.0f : 1.0f) / (float) (n * n) : 0.0f; }
        float saw (int n)    { return 2.0f / kPi * ((n % 2) ? 1.0f : -1.0f) / (float) n; }
        float square (int n) { return (n % 2 == 1) ? 4.0f / (kPi * (float) n) : 0.0f; }

        std::vector<std::vector<float>> timeFrames (int frames, const std::function<float (int frame, float phase)>& fn)
        {
            std::vector<std::vector<float>> out ((size_t) frames, std::vector<float> ((size_t) Wavetable::N));
            for (int f = 0; f < frames; ++f)
                for (int i = 0; i < Wavetable::N; ++i)
                    out[(size_t) f][(size_t) i] = fn (f, (float) i / (float) Wavetable::N);
            return out;
        }
    }

    const WavetableBank& WavetableBank::get()
    {
        static WavetableBank bank;
        return bank;
    }

    WavetableBank::WavetableBank()
    {
        static constexpr int F = 16;
        auto t01 = [] (int f) { return (float) f / (float) (F - 1); };
        tables.resize (17);

        // 0 Basic: sine -> triangle -> saw -> square
        tables[0].buildFromHarmonics (F, [&] (int f, int n)
        {
            const float x = t01 (f) * 3.0f;
            const float s = n == 1 ? 1.0f : 0.0f;
            if (x < 1.0f) return s + (tri (n) - s) * x;
            if (x < 2.0f) return tri (n) + (saw (n) - tri (n)) * (x - 1.0f);
            return saw (n) + (square (n) - saw (n)) * (x - 2.0f);
        });
        // 1 PWM: 50% -> 4%
        tables[1].buildFromFrames (timeFrames (F, [&] (int f, float p) { const float w = 0.5f - 0.46f * t01 (f); return p < w ? 1.0f : -1.0f; }));
        // 2 Harmonic sweep
        tables[2].buildFromHarmonics (F, [&] (int f, int n) { const int top = 1 + f * 3; return n <= top ? 1.0f / (float) n : 0.0f; });
        // 3 Bell FM (integer ratios keep it periodic)
        tables[3].buildFromFrames (timeFrames (F, [&] (int f, float p)
        {
            const float ratio = f < 8 ? 3.0f : 5.0f, index = (float) (f % 8) / 7.0f * 5.0f;
            return std::sin (2.0f * kPi * p + index * std::sin (2.0f * kPi * ratio * p));
        }));
        // 4 Glass: sine with sparkling upper partials
        tables[4].buildFromHarmonics (F, [&] (int f, int n)
        {
            const float t = t01 (f);
            switch (n) { case 1: return 1.0f; case 2: return 0.2f * t; case 3: return 0.45f * t; case 5: return 0.3f * t;
                         case 7: return 0.5f * t; case 9: return 0.25f * t; case 12: return 0.35f * t * t; case 16: return 0.25f * t * t; default: return 0.0f; }
        });
        // 5 Vocal: formants a-e-i-o-u over a saw source (reference pitch 130 Hz)
        {
            const float formants[5][3] = { { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 350, 2300, 3000 }, { 450, 800, 2830 }, { 325, 700, 2530 } };
            tables[5].buildFromHarmonics (F, [&] (int f, int n)
            {
                const float x = t01 (f) * 4.0f;
                const int v0 = std::min (3, (int) x);
                const float fr = x - (float) v0;
                const float hz = 130.0f * (float) n;
                if (hz > 8000.0f) return 0.0f;
                float g = 0.0f;
                for (int k = 0; k < 3; ++k)
                {
                    const float fc = formants[v0][k] + (formants[v0 + 1][k] - formants[v0][k]) * fr;
                    const float bw = 80.0f + 40.0f * (float) k;
                    g += std::exp (-0.5f * std::pow ((hz - fc) / bw, 2.0f)) * (k == 0 ? 1.0f : 0.6f);
                }
                return saw (n) * (0.08f + g);
            });
        }
        // 6 Digital: bit/rate reduced saw
        tables[6].buildFromFrames (timeFrames (F, [&] (int f, float p)
        {
            const float bits = 7.0f - 5.5f * t01 (f);
            const float levels = std::exp2 (bits);
            const float steps = 256.0f - 230.0f * t01 (f);
            const float ph = std::floor (p * steps) / steps;
            return std::round ((2.0f * ph - 1.0f) * levels) / levels;
        }));
        // 7 Hollow: odd harmonics, soft -> reedy
        tables[7].buildFromHarmonics (F, [&] (int f, int n)
        {
            const float pw = 2.2f - 1.2f * t01 (f);
            return (n % 2 == 1) ? 1.0f / std::pow ((float) n, pw) : 0.04f * t01 (f) / (float) n;
        });
        // 8 Bubble: sweet sine with rising 2nd/3rd/4th
        tables[8].buildFromHarmonics (F, [&] (int f, int n)
        {
            const float t = t01 (f);
            switch (n) { case 1: return 1.0f; case 2: return 0.55f * t; case 3: return 0.3f * t * t; case 4: return 0.25f * t; case 6: return 0.12f * t * t; default: return 0.0f; }
        });
        // 9 Super Saw: dark -> bright saw
        tables[9].buildFromHarmonics (F, [&] (int f, int n)
        {
            const float tilt = std::exp (-(float) n * 0.12f * (1.0f - t01 (f)) * (1.0f - t01 (f)));
            return saw (n) * tilt;
        });
        // 10 Organ: drawbar registrations
        {
            const float bars[8][6] = { { 1, 0, 0, 0, 0, 0 }, { 1, .8f, 0, 0, 0, 0 }, { 1, .8f, .6f, 0, 0, 0 }, { 1, 0, .7f, 0, .4f, 0 },
                                       { 1, .5f, .5f, .5f, .3f, .2f }, { .6f, 1, 0, .6f, 0, .4f }, { 1, 1, 1, .8f, .6f, .5f }, { .8f, .3f, 1, .2f, .7f, .6f } };
            const int harm[6] = { 1, 2, 3, 4, 6, 8 };
            tables[10].buildFromHarmonics (F, [&] (int f, int n)
            {
                const float x = t01 (f) * 7.0f; const int b0 = std::min (6, (int) x); const float fr = x - (float) b0;
                for (int k = 0; k < 6; ++k) if (harm[k] == n) return bars[b0][k] + (bars[b0 + 1][k] - bars[b0][k]) * fr;
                return 0.0f;
            });
        }
        // 11 Growl: heavy FM + drive
        tables[11].buildFromFrames (timeFrames (F, [&] (int f, float p)
        {
            const float idx = 1.0f + 9.0f * t01 (f);
            return std::tanh (1.6f * std::sin (2.0f * kPi * p + idx * std::sin (4.0f * kPi * p)));
        }));
        // 12 Chiptune: 12.5/25/50% pulses and a 4-bit triangle
        tables[12].buildFromFrames (timeFrames (F, [&] (int f, float p)
        {
            const int kind = f / 4;
            if (kind == 3) { const float trv = 1.0f - 4.0f * std::abs (p - 0.5f); return std::round (trv * 7.5f) / 7.5f; }
            const float w = kind == 0 ? 0.125f : (kind == 1 ? 0.25f : 0.5f);
            return p < w ? 1.0f : -1.0f;
        }));
        // 13 Flute: pure with a touch of overtone
        tables[13].buildFromHarmonics (F, [&] (int f, int n)
        {
            const float t = t01 (f);
            switch (n) { case 1: return 1.0f; case 2: return 0.18f * t; case 3: return 0.12f + 0.15f * t; case 4: return 0.05f * t; case 5: return 0.04f * t; default: return 0.0f; }
        });
        // 14 Pluck: bright saw at 0 -> nearly sine at 1 (sweep position with an envelope for plucks)
        tables[14].buildFromHarmonics (F, [&] (int f, int n)
        {
            const float t = t01 (f);
            return saw (n) * std::exp (-(float) (n - 1) * (0.02f + t * t * 1.5f));
        });
        // 15 Sine stack: octave partials fading in
        tables[15].buildFromHarmonics (F, [&] (int f, int n)
        {
            const float t = t01 (f);
            switch (n) { case 1: return 1.0f; case 2: return 0.7f * std::min (1.0f, t * 3.0f); case 4: return 0.5f * juce::jlimit (0.0f, 1.0f, t * 3.0f - 1.0f);
                         case 8: return 0.35f * juce::jlimit (0.0f, 1.0f, t * 3.0f - 2.0f); default: return 0.0f; }
        });
        // 16 User placeholder (each oscillator replaces this with its own imported table)
        tables[16].buildFromHarmonics (1, [] (int, int n) { return n == 1 ? 1.0f : 0.0f; });
    }
}
