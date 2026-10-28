#pragma once

// Small DSP helpers used by the XO voices and effects. Allocation-free after setup.

#include <cmath>
#include <cstdint>
#include <algorithm>
#include <vector>
#include <iterator>
#include <array>

namespace xo
{
    constexpr float pi = 3.14159265358979f, twoPi = 6.28318530717959f;
    inline float clampf (float x, float lo, float hi) { return std::min (hi, std::max (lo, x)); }
    inline float fastTanh (float x) { x = clampf (x, -4.5f, 4.5f); const float x2 = x * x; return x * (27.0f + x2) / (27.0f + 9.0f * x2); }
    inline float midiToHz (float n) { return 440.0f * std::exp2 ((n - 69.0f) / 12.0f); }

    struct Rng
    {
        uint32_t s = 0x1234567u;
        void seed (uint32_t v) { s = v ? v : 0x1234567u; }
        uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
        float uni() { return (float) (next() >> 8) * (1.0f / 16777216.0f); }
        float bi()  { return uni() * 2.0f - 1.0f; }
    };

    struct OnePole
    {
        float z = 0, a = 0;
        void setCutoff (float hz, float sr) { a = std::exp (-twoPi * std::min (hz, sr * 0.49f) / sr); }
        void setTime (float ms, float sr) { a = ms <= 0 ? 0.0f : std::exp (-1.0f / (0.001f * ms * sr)); }
        float process (float x) { z = x + a * (z - x); return z; }
        void reset (float v = 0) { z = v; }
    };

    struct DcBlocker
    {
        float x1 = 0, y1 = 0, r = 0.995f;
        void prepare (float sr) { r = 1.0f - twoPi * 12.0f / sr; }
        float process (float x) { const float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
        void reset() { x1 = y1 = 0; }
    };

    inline float polyBlep (float t, float dt)
    {
        if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    // power-of-two fractional delay line
    struct Delay
    {
        std::vector<float> buf; int mask = 0, w = 0;
        void prepare (int maxSamples) { int s = 1; while (s < maxSamples + 4) s <<= 1; buf.assign ((size_t) s, 0.0f); mask = s - 1; w = 0; }
        void reset() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
        void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
        float read (float d) const
        {
            d = clampf (d, 1.0f, (float) (mask - 2));
            const float rp = (float) w - 1.0f - d;
            const int i = (int) std::floor (rp); const float f = rp - (float) i;
            const float a = buf[(size_t) (i & mask)], b = buf[(size_t) ((i + 1) & mask)];
            return a + (b - a) * f;
        }
    };

    // TPT state-variable filter (12 dB)
    struct SVF
    {
        float ic1 = 0, ic2 = 0, g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0;
        void set (float hz, float reso, float sr)
        {
            g = std::tan (pi * clampf (hz, 15.0f, sr * 0.45f) / sr);
            k = 2.0f - 1.97f * clampf (reso, 0.0f, 1.0f);
            a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
        }
        void setQ (float hz, float q, float sr)
        {
            g = std::tan (pi * clampf (hz, 15.0f, sr * 0.45f) / sr);
            k = 1.0f / std::max (0.3f, q);
            a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
        }
        // returns lp, bp, hp through refs
        inline void tick (float x, float& lp, float& bp, float& hp)
        {
            const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
            lp = v2; bp = v1; hp = x - k * v1 - v2;
        }
        void reset() { ic1 = ic2 = 0; }
    };

    // ZDF transistor-ladder lowpass
    struct Ladder
    {
        float s[4] {}, G = 0, k = 0;
        void set (float hz, float reso, float sr)
        {
            const float g = std::tan (pi * clampf (hz, 15.0f, sr * 0.45f) / sr);
            G = g / (1.0f + g);
            k = reso * 4.0f;
        }
        float process (float x)
        {
            const float S = (1.0f - G) * (G * G * G * s[0] + G * G * s[1] + G * s[2] + s[3]);
            float u = (x * (1.0f + k * 0.25f) - k * S) / (1.0f + k * G * G * G * G);
            u = fastTanh (u);
            float in = u, y = 0;
            for (int i = 0; i < 4; ++i) { const float v = (in - s[i]) * G; y = v + s[i]; s[i] = y + v; in = y; }
            return y;
        }
        void reset() { for (auto& v : s) v = 0; }
    };

    // DAHDSR envelope with curve
    struct Envelope
    {
        enum Stage { idle, delay, attack, hold, decay, sustain, release };
        Stage stage = idle;
        float level = 0.0f, t = 0.0f, startLevel = 0.0f, relLevel = 0.0f;
        float dD = 0, aD = 0, hD = 0, dcD = 0, rD = 0, sus = 1, curve = 0;   // durations in samples

        void set (float delayMs, float attMs, float holdMs, float decMs, float susLvl, float relMs, float crv, float sr)
        {
            const float k = 0.001f * sr;
            dD = delayMs * k; aD = std::max (1.0f, attMs * k); hD = holdMs * k; dcD = std::max (1.0f, decMs * k);
            rD = std::max (1.0f, relMs * k); sus = susLvl; curve = crv;
        }
        void noteOn (bool fromCurrent) { startLevel = fromCurrent ? level : 0.0f; if (! fromCurrent) level = 0.0f; t = 0; stage = dD > 0.5f ? delay : attack; }
        void noteOff() { if (stage != idle) { relLevel = level; t = 0; stage = release; } }
        void kill() { stage = idle; level = 0; }
        bool active() const { return stage != idle; }

        inline float shape (float x) const   // x in 0..1 ; curve>0 = snappier
        {
            const float p = curve >= 0.0f ? 1.0f + curve * 5.0f : 1.0f / (1.0f - curve * 3.0f);
            return 1.0f - std::pow (1.0f - x, p);
        }
        inline float process (float inc = 1.0f)
        {
            switch (stage)
            {
                case delay:   if ((t += inc) >= dD) { t = 0; stage = attack; } break;
                case attack:  level = startLevel + (1.0f - startLevel) * shape (std::min (1.0f, (t += inc) / aD));
                              if (t >= aD) { t = 0; level = 1.0f; stage = hD > 0.5f ? hold : decay; } break;
                case hold:    if ((t += inc) >= hD) { t = 0; stage = decay; } break;
                case decay:   level = sus + (1.0f - sus) * (1.0f - shape (std::min (1.0f, (t += inc) / dcD)));
                              if (t >= dcD) { level = sus; stage = sustain; } break;
                case sustain: level = sus; break;
                case release: level = relLevel * (1.0f - shape (std::min (1.0f, (t += inc) / rD)));
                              if (t >= rD) { level = 0; stage = idle; } break;
                case idle: break;
            }
            return level;
        }
        // advance n samples at once (used for modulation-only envelopes)
        float advance (int n) { return process ((float) n); }
    };

    // LFO shape function, x in [0,1)
    inline float lfoShape (int shape, float x, float held)
    {
        switch (shape)
        {
            case 0: return std::sin (twoPi * x);
            case 1: return 1.0f - 4.0f * std::abs (x - 0.5f);
            case 2: return 2.0f * x - 1.0f;
            case 3: return 1.0f - 2.0f * x;
            case 4: return x < 0.5f ? 1.0f : -1.0f;
            case 5: case 6: return held;
            case 7: { // bounce: a ball bouncing lower each time
                const float seg[4] = { 0.4f, 0.27f, 0.2f, 0.13f }; float acc = 0, h = 1.0f;
                for (int i = 0; i < 4; ++i) { if (x < acc + seg[i]) { const float u = (x - acc) / seg[i]; return h * std::sin (pi * u) * 2.0f - 1.0f; } acc += seg[i]; h *= 0.55f; }
                return -1.0f; }
            case 8: return std::floor (x * 4.0f) / 1.5f - 1.0f;
            case 9: { const float v = 1.0f - std::pow (1.0f - x, 4.0f); return 2.0f * v - 1.0f; }   // pump (sidechain duck)
            default: return 0.0f;
        }
    }
}
