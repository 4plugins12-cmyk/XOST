#include "Engine.h"
#include <cstring>

namespace xo
{
    namespace
    {
        constexpr int ctrlBlock = 32;
        const float invSqrt[9] = { 1.0f, 1.0f, 0.7071f, 0.5774f, 0.5f, 0.4472f, 0.4082f, 0.378f, 0.3536f };

        inline float wrap01 (float p) { p -= std::floor (p); return p; }

        inline float applyWarp (int warp, float ph, float amt, float prev)
        {
            switch (warp)
            {
                case 1: return wrap01 (ph * (1.0f + amt * 7.0f));                                   // sync
                case 2: { const float e = std::exp2 ((amt - 0.5f) * 4.0f); return std::pow (ph, e); } // bend
                case 3: { const float m = ph < 0.5f ? ph * 2.0f : 2.0f - ph * 2.0f;                  // mirror
                          return wrap01 (ph + (m * 0.5f - ph) * amt); }
                case 4: { const float w = 0.5f - 0.48f * amt;                                       // pwm squeeze
                          return ph < w ? ph * 0.5f / w : 0.5f + (ph - w) * 0.5f / (1.0f - w); }
                case 5: { const float steps = 2.0f + (1.0f - amt) * 62.0f; return std::floor (ph * steps) / steps; }
                case 6: return wrap01 (ph + amt * 0.4f * prev);                                     // self FM
                default: return ph;
            }
        }
    }

    // =================================================================== Voice
    void Voice::prepare (double sampleRate, int)
    {
        sr = (float) sampleRate;
        for (int f = 0; f < 2; ++f)
            for (int c = 0; c < 2; ++c) comb[f][c].prepare ((int) (sr / 18.0f));
        for (int c = 0; c < 2; ++c) { dc[c].prepare (sr); dc[c].reset(); }
        rng.seed ((uint32_t) (reinterpret_cast<uintptr_t> (this) & 0xffffffu) + 17u);
        active = false;
    }

    void Voice::start (int n, float vel, int ch, float startPitch, bool, const BlockP& bp, const Resources& r, Rng& erng)
    {
        juce::ignoreUnused (r);
        active = true; releasing = false; stealing = false; stealGain = 1.0f;
        note = n; velocity = vel; channel = ch;
        randomVal = erng.bi();
        rng.seed (erng.next() | 1u);
        pitch = startPitch; targetPitch = (float) n;
        driftNote = rng.bi(); driftCut = rng.bi(); driftWalk = rng.bi() * 0.5f; driftTarget = rng.bi(); driftTimer = 0;
        for (auto& e : env) e.noteOn (false);
        for (int l = 0; l < numLfo; ++l)
        {
            lfoPh[l] = bp.lfoPhase[l]; lfoDone[l] = false;
            lfoPrevHeld[l] = lfoHeld[l]; lfoHeld[l] = rng.bi();
            lfoOut[l] = 0.0f;
        }
        for (int o = 0; o < numOsc; ++o)
            for (int u = 0; u < maxUnison; ++u)
                phases[o][u] = initPhase[o][u] = wrap01 (bp.osc[o].phase + bp.osc[o].rand * rng.uni());
        subPhase = 0.0f;
        smpPos = 0.0;
        for (auto& g : grains) g.on = false;
        grainTimer = 0.0f;
        for (auto& v : oscOut) v = 0.0f;
        for (auto& v : oscPrev) v = 0.0f;
        for (int f = 0; f < 2; ++f)
            for (int c = 0; c < 2; ++c)
            {
                svfA[f][c].reset(); svfB[f][c].reset(); ladder[f][c].reset(); comb[f][c].reset(); combFb[f][c] = 0;
                for (auto& s : formant[f][c]) s.reset();
            }
        ampGain = 0.0f;
        polyPressure = 0.0f;
    }

    void Voice::glideTo (int n, float vel, bool retrigger, const BlockP& bp)
    {
        note = n; targetPitch = (float) n; velocity = vel; releasing = false;
        if (retrigger)
        {
            for (auto& e : env) e.noteOn (true);
            for (int l = 0; l < numLfo; ++l) if (bp.lfoMode[l] != 0) { lfoPh[l] = bp.lfoPhase[l]; lfoDone[l] = false; }
        }
    }

    void Voice::release()
    {
        if (! active) return;
        releasing = true;
        for (auto& e : env) e.noteOff();
    }

    void Voice::steal() { if (active) stealing = true; }

    void Voice::controlUpdate (int n, const BlockP& bp, const ChannelState* chans, float modWheel, double beatPos, float bpm, const float* gPhase)
    {
        // ---- envelopes
        for (int e = 0; e < numEnv; ++e)
            env[e].set (bp.env[e][0], bp.env[e][1], bp.env[e][2], bp.env[e][3], bp.env[e][4], bp.env[e][5], bp.env[e][6], sr);
        const float e1 = env[0].advance (n), e2 = env[1].advance (n), e3 = env[2].advance (n);
        if (! env[0].active()) active = false;

        // ---- LFOs
        for (int l = 0; l < numLfo; ++l)
        {
            float hz = bp.lfoSync[l] ? bpm / 60.0f / bp.lfoBeats[l] : bp.lfoRate[l];
            hz *= std::exp2 (mod[dstLfo1Rate + l] * 4.0f);
            float ph;
            if (bp.lfoMode[l] == 0)
            {
                ph = bp.lfoSync[l] ? (float) std::fmod (beatPos / bp.lfoBeats[l], 1.0) : gPhase[l];
                ph = wrap01 (ph + bp.lfoPhase[l]);
                if (ph < lfoPh[l]) { lfoPrevHeld[l] = lfoHeld[l]; lfoHeld[l] = rng.bi(); }
                lfoPh[l] = ph;
            }
            else
            {
                if (! lfoDone[l]) lfoPh[l] += hz * (float) n / sr;
                if (lfoPh[l] >= 1.0f)
                {
                    if (bp.lfoMode[l] == 2) { lfoPh[l] = 0.9999f; lfoDone[l] = true; }
                    else { lfoPh[l] -= std::floor (lfoPh[l]); lfoPrevHeld[l] = lfoHeld[l]; lfoHeld[l] = rng.bi(); }
                }
                ph = lfoPh[l];
            }
            float held = lfoHeld[l];
            if (bp.lfoShape[l] == 6) { const float t = 0.5f - 0.5f * std::cos (pi * ph); held = lfoPrevHeld[l] + (lfoHeld[l] - lfoPrevHeld[l]) * t; }
            const float raw = lfoShape (bp.lfoShape[l], ph, held);
            const float sm = bp.lfoSmooth[l];
            lfoOut[l] = sm > 0.001f ? lfoOut[l] + (raw - lfoOut[l]) * (1.0f - sm * 0.97f) : raw;
        }

        // ---- sources
        const ChannelState& cs = bp.mpe ? chans[juce::jlimit (1, 16, channel)] : chans[0];
        srcVals[srcNone] = 0.0f;
        srcVals[srcEnv1] = e1; srcVals[srcEnv2] = e2; srcVals[srcEnv3] = e3;
        for (int l = 0; l < numLfo; ++l) srcVals[srcLfo1 + l] = lfoOut[l];
        srcVals[srcVelocity] = velocity;
        srcVals[srcNote] = ((float) note - 60.0f) / 60.0f;
        srcVals[srcPressure] = bp.mpe ? cs.pressure : std::max (chans[0].pressure, polyPressure);
        srcVals[srcModWheel] = modWheel;
        srcVals[srcBend] = (bp.mpe ? cs.bend : chans[0].bend);
        srcVals[srcSlide] = cs.slide;
        srcVals[srcRandom] = randomVal;
        for (int m = 0; m < numMacros; ++m) srcVals[srcMacro1 + m] = bp.macro[m];

        // ---- matrix
        for (auto& d : mod) d = 0.0f;
        for (int s = 0; s < numSlots; ++s)
            if (bp.slotSrc[s] > 0 && bp.slotDst[s] > 0)
                mod[bp.slotDst[s]] += srcVals[bp.slotSrc[s]] * bp.slotAmt[s];
        for (int d = 1; d < firstGlobalDest; ++d) mod[d] += bp.rackMod[d];

        // ---- pitch / glide
        const float gk = bp.glideMs <= 0.5f ? 0.0f : std::exp (-(float) n / (0.001f * bp.glideMs * sr));
        pitch = targetPitch + (pitch - targetPitch) * gk;
        const float bendSemis = (bp.mpe ? cs.bend : chans[0].bend) * (float) bp.bendRange;
        // ---- analog drift: a slow random walk per voice plus a fixed per-note offset
        if (bp.drift > 0.0001f)
        {
            driftTimer -= n;
            if (driftTimer <= 0) { driftTimer = (int) (sr * (0.15f + 0.35f * rng.uni())); driftTarget = rng.bi(); }
            driftWalk += (driftTarget - driftWalk) * std::min (1.0f, (float) n / (sr * 0.4f));
        }
        const float driftSemis = bp.drift * (driftWalk * 0.22f + driftNote * 0.08f);
        const float basePitch = pitch + bendSemis + driftSemis;

        // ---- oscillators
        for (int o = 0; o < numOsc; ++o)
        {
            const auto& op = bp.osc[o];
            const int base = dstAPitch + o * 7;
            const float semis = basePitch + (float) (op.oct * 12) + op.semi + op.fine * 0.01f + mod[base] * 24.0f;
            const int uni = juce::jlimit (1, maxUnison, op.uni);
            const float det = clampf (op.detune + mod[base + 4], 0.0f, 1.0f);
            const float detSemis = det * det * 0.9f + det * 0.15f;
            const float pan = clampf (op.pan + mod[base + 3], -1.0f, 1.0f);
            float maxHz = 1.0f;
            for (int u = 0; u < uni; ++u)
            {
                const float spread = uni == 1 ? 0.0f : ((float) u / (float) (uni - 1)) * 2.0f - 1.0f;
                const float hz = midiToHz (semis + spread * detSemis);
                maxHz = std::max (maxHz, hz);
                oscInc[o][u] = hz / sr;
                const float pp = clampf (pan + spread * op.width, -1.0f, 1.0f);
                oscPanL[o][u] = std::cos ((pp + 1.0f) * pi * 0.25f) * 1.4142f * invSqrt[uni];
                oscPanR[o][u] = std::sin ((pp + 1.0f) * pi * 0.25f) * 1.4142f * invSqrt[uni];
            }
            oscMip[o] = Wavetable::levelFor (maxHz * (op.mod == 1 ? 1.0f + op.modAmt * 4.0f : 1.0f) * (op.warp == 1 ? 1.0f + op.warpAmt * 7.0f : 1.0f), sr);
            oscLevel[o] = oscLevelTarget[o];
            oscLevelTarget[o] = op.on ? clampf (op.level + mod[base + 2], 0.0f, 1.0f) : 0.0f;
            oscPos[o] = clampf (op.pos + mod[base + 1], 0.0f, 1.0f);
            oscWarp[o] = clampf (op.warpAmt + mod[base + 5], 0.0f, 1.0f);
            oscModAmt[o] = clampf (op.modAmt + mod[base + 6], 0.0f, 1.0f);
        }
        subInc = midiToHz (basePitch + (float) ((bp.subOct - 2) * 12)) / sr;
        subGain = bp.subOn ? clampf (bp.subLevel + mod[dstSubLevel], 0.0f, 1.0f) : 0.0f;
        noiseGain = bp.noiseOn ? clampf (bp.noiseLevel + mod[dstNoiseLevel], 0.0f, 1.0f) : 0.0f;
        const float ncol = clampf (bp.noiseColor + mod[dstNoiseColor], 0.0f, 1.0f);
        noiseLp[0].setCutoff (200.0f * std::exp2 (ncol * 6.6f), sr); noiseLp[1].a = noiseLp[0].a;
        smpGain = bp.smpOn ? clampf (bp.smpLevel + mod[dstSmpLevel], 0.0f, 1.0f) : 0.0f;
        smpInc = (float) std::exp2 (bp.smpKey ? (basePitch - (float) bp.smpRoot) / 12.0f : 0.0f);
        grainPos = clampf (bp.gPos + mod[dstSmpPos], 0.0f, 1.0f);
        grainSize = clampf (bp.gSize * std::exp2 (mod[dstGrainSize] * 2.0f), 3.0f, 800.0f) * 0.001f;

        // ---- filters
        for (int f = 0; f < 2; ++f)
        {
            const int dCut = f == 0 ? dstF1Cutoff : dstF2Cutoff, dRes = f == 0 ? dstF1Reso : dstF2Reso;
            const float oct = std::log2 (bp.fCut[f]) + bp.fEnv[f] * e2 * 7.0f + bp.fKey[f] * (pitch - 60.0f) / 12.0f + mod[dCut] * 5.0f
                              + bp.drift * (driftCut * 0.25f + driftWalk * 0.12f);
            cutHz[f] = clampf (std::exp2 (oct), 20.0f, 20000.0f);
            resoV[f] = clampf (bp.fReso[f] + mod[dRes], 0.0f, 1.0f);
            driveV[f] = clampf (bp.fDrive[f] + (f == 0 ? mod[dstF1Drive] : 0.0f), 0.0f, 1.0f);
            if (! bp.fOn[f]) continue;
            const int t = bp.fType[f];
            for (int c = 0; c < 2; ++c)
            {
                const float hz = cutHz[f] * (c == 1 ? 1.004f : 1.0f);
                if (t == 2) ladder[f][c].set (hz, resoV[f], sr);
                else if (t == 9)
                {
                    static const float fm[5][3] = { { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 350, 2300, 3000 }, { 450, 800, 2830 }, { 325, 700, 2530 } };
                    const float x = clampf ((std::log2 (cutHz[f]) - std::log2 (200.0f)) / std::log2 (25.0f), 0.0f, 1.0f) * 4.0f;
                    const int v0 = std::min (3, (int) x); const float fr = x - (float) v0;
                    for (int k = 0; k < 3; ++k)
                        formant[f][c][k].setQ (fm[v0][k] + (fm[v0 + 1][k] - fm[v0][k]) * fr, 4.0f + resoV[f] * 16.0f, sr);
                }
                else if (t != 8)
                {
                    svfA[f][c].set (hz, t == 1 || t == 4 ? resoV[f] * 0.8f : resoV[f], sr);
                    svfB[f][c].set (hz, resoV[f] * 0.35f, sr);
                }
            }
        }
        shaperDrive = clampf (bp.shpDrive + mod[dstShaperDrive], 0.0f, 1.0f);

        // ---- amp
        const float velScale = 1.0f - bp.velSens * (1.0f - velocity);
        ampGain = ampGainTarget;
        ampGainTarget = e1 * velScale * clampf (bp.ampLevel * (1.0f + mod[dstAmp]), 0.0f, 1.5f);
        const float vp = clampf (bp.ampPan + mod[dstVoicePan], -1.0f, 1.0f);
        panL = std::cos ((vp + 1.0f) * pi * 0.25f) * 1.4142f;
        panR = std::sin ((vp + 1.0f) * pi * 0.25f) * 1.4142f;
        lastEnv1 = e1;
    }

    float Voice::filterSample (int f, int c, float x, const BlockP& bp)
    {
        const float dr = driveV[f];
        if (dr > 0.001f) x = fastTanh (x * (1.0f + dr * 6.0f)) / (1.0f + dr * 1.5f);
        float lp, bp_, hp;
        float y;
        switch (bp.fType[f])
        {
            case 0: svfA[f][c].tick (x, lp, bp_, hp); y = lp; break;
            case 1: { svfA[f][c].tick (x, lp, bp_, hp); float l2, b2, h2; svfB[f][c].tick (lp, l2, b2, h2); y = l2; break; }
            case 2: y = ladder[f][c].process (x); break;
            case 3: svfA[f][c].tick (x, lp, bp_, hp); y = hp; break;
            case 4: { svfA[f][c].tick (x, lp, bp_, hp); float l2, b2, h2; svfB[f][c].tick (hp, l2, b2, h2); y = h2; break; }
            case 5: svfA[f][c].tick (x, lp, bp_, hp); y = bp_ * svfA[f][c].k; break;
            case 6: svfA[f][c].tick (x, lp, bp_, hp); y = lp + hp; break;
            case 7: svfA[f][c].tick (x, lp, bp_, hp); y = x + bp_ * svfA[f][c].k * resoV[f] * 3.0f; break;
            case 8:
            {
                const float d = comb[f][c].read (sr / cutHz[f]);
                const float fb = resoV[f] * 0.96f;
                const float v = x + d * fb;
                comb[f][c].push (fastTanh (v));
                y = v * (1.0f - fb * 0.5f);
                break;
            }
            default:
            {
                float sum = 0.0f;
                for (int k = 0; k < 3; ++k) { formant[f][c][k].tick (x, lp, bp_, hp); sum += bp_ * (k == 0 ? 1.0f : 0.7f); }
                y = sum * 1.6f;
            }
        }
        const float m = bp.fMix[f];
        return x + (y - x) * m;
    }

    void Voice::render (float* L, float* R, int total, const BlockP& bp, const Resources& r, const ChannelState* chans,
                        float modWheel, double beatPos, float bpm, const float* gPhase)
    {
        const auto& bank = WavetableBank::get();
        const SampleData* smp = r.sample;
        const int quality = bp.quality;

        for (int start = 0; start < total && active; start += ctrlBlock)
        {
            const int n = std::min (ctrlBlock, total - start);
            controlUpdate (n, bp, chans, modWheel, beatPos, bpm, gPhase);
            const float invN = 1.0f / (float) n;
            const Wavetable* tables[numOsc];
            int unis[numOsc];
            for (int o = 0; o < numOsc; ++o)
            {
                const int t = bp.osc[o].table;
                tables[o] = (t == 16 && r.userTables[o] != nullptr) ? r.userTables[o] : &bank.table (t);
                unis[o] = juce::jlimit (1, quality == 0 ? 4 : maxUnison, bp.osc[o].uni);
            }
            const float stealStep = stealing ? 1.0f / (0.004f * sr) : 0.0f;

            for (int i = 0; i < n; ++i)
            {
                const float k = (float) (i + 1) * invN;
                float bus[4][2] {};

                // ---- wavetable oscillators
                for (int o = 0; o < numOsc; ++o)
                {
                    const auto& op = bp.osc[o];
                    oscWrapped[o] = false;
                    if (! op.on) { oscOut[o] = 0.0f; continue; }
                    const int mf = op.modFrom;
                    const float m = mf < o ? oscOut[mf] : oscPrev[mf];
                    const bool syncNow = op.mod == 5 && (mf < numOsc ? (mf < o ? oscWrapped[mf] : false) : false);
                    if (syncNow) for (int u = 0; u < unis[o]; ++u) phases[o][u] = initPhase[o][u];
                    const float amt = oscModAmt[o];
                    const float incMul = op.mod == 1 ? 1.0f + amt * amt * 8.0f * m : 1.0f;
                    const float phOff = op.mod == 2 ? amt * m * 1.5f : 0.0f;
                    float sl = 0, sr_ = 0, mono = 0;
                    const Wavetable* wt = tables[o];
                    const float pos = oscPos[o], wamt = oscWarp[o];
                    for (int u = 0; u < unis[o]; ++u)
                    {
                        float ph = phases[o][u] + oscInc[o][u] * incMul;
                        if (ph >= 1.0f || ph < 0.0f) { ph -= std::floor (ph); if (u == 0) oscWrapped[o] = true; }
                        phases[o][u] = ph;
                        float rp = std::abs (phOff) > 1.0e-9f ? wrap01 (ph + phOff) : ph;
                        if (op.warp != 0) rp = applyWarp (op.warp, rp, wamt, oscPrev[o]);
                        const float v = wt->lookup (rp, pos, oscMip[o]);
                        sl += v * oscPanL[o][u]; sr_ += v * oscPanR[o][u]; mono += v;
                    }
                    mono /= (float) unis[o];
                    if (op.mod == 3) { const float g = (1.0f - amt) + amt * m; sl *= g; sr_ *= g; mono *= g; }
                    else if (op.mod == 4) { const float g = 1.0f - amt * 0.5f * (1.0f - m); sl *= g; sr_ *= g; mono *= g; }
                    oscOut[o] = mono;
                    const float lvl = oscLevel[o] + (oscLevelTarget[o] - oscLevel[o]) * k;
                    bus[op.route][0] += sl * lvl;
                    bus[op.route][1] += sr_ * lvl;
                }

                // ---- sub
                if (subGain > 0.0001f)
                {
                    float sp = subPhase + subInc; if (sp >= 1.0f) sp -= 1.0f; subPhase = sp;
                    float v;
                    switch (bp.subShape)
                    {
                        case 1: v = 1.0f - 4.0f * std::abs (sp - 0.5f); break;
                        case 2: { v = sp < 0.5f ? 1.0f : -1.0f; v += polyBlep (sp, subInc); float p2 = sp + 0.5f; if (p2 >= 1) p2 -= 1; v -= polyBlep (p2, subInc); v *= 0.7f; break; }
                        case 3: v = (2.0f * sp - 1.0f - polyBlep (sp, subInc)) * 0.7f; break;
                        default: v = std::sin (twoPi * sp);
                    }
                    oscOut[3] = v;
                    bus[bp.subRoute][0] += v * subGain; bus[bp.subRoute][1] += v * subGain;
                }
                // ---- noise
                if (noiseGain > 0.0001f)
                {
                    for (int c = 0; c < 2; ++c)
                    {
                        const float w = rng.bi();
                        float v;
                        switch (bp.noiseType)
                        {
                            case 1: { auto* b = pinkB[c]; b[0] = 0.99765f * b[0] + w * 0.099046f; b[1] = 0.963f * b[1] + w * 0.2965164f;
                                      b[2] = 0.57f * b[2] + w * 1.0526913f; v = (b[0] + b[1] + b[2] + w * 0.1848f) * 0.25f; break; }
                            case 2: v = rng.uni() < 0.002f ? rng.bi() * 2.0f : 0.0f; break;
                            case 3: v = (rng.uni() < 0.0006f ? rng.bi() * 3.0f : 0.0f) + w * 0.05f; break;
                            default: v = w;
                        }
                        v = noiseLp[c].process (v);
                        if (c == 0) oscOut[4] = v;
                        bus[bp.noiseRoute][c] += v * noiseGain;
                    }
                }
                // ---- sampler
                if (smpGain > 0.0001f && smp != nullptr && smp->length() > 4)
                {
                    const int len = smp->length();
                    const bool stereo = ! smp->R.empty();
                    const double rateRatio = smp->sampleRate / (double) sr;
                    auto readAt = [&] (double p, int c)
                    {
                        const int a = (int) p; const float fr = (float) (p - a);
                        const auto& d = (c == 1 && stereo) ? smp->R : smp->L;
                        if (a < 0 || a + 1 >= len) return 0.0f;
                        return d[(size_t) a] + (d[(size_t) a + 1] - d[(size_t) a]) * fr;
                    };
                    float vl = 0, vr = 0;
                    if (bp.smpMode == 2)
                    {
                        grainTimer -= 1.0f;
                        if (grainTimer <= 0.0f)
                        {
                            grainTimer = sr / std::max (1.0f, bp.gDens);
                            for (auto& g : grains)
                                if (! g.on)
                                {
                                    g.on = true; g.age = 0;
                                    g.len = grainSize * sr;
                                    const float pos = clampf (grainPos + bp.gSpray * rng.bi() * 0.3f, 0.0f, 0.999f);
                                    g.pos = pos * (double) len;
                                    g.inc = rateRatio * smpInc * std::exp2 (bp.gPitch * rng.bi() * 0.5f);
                                    g.pan = rng.bi() * 0.6f;
                                    break;
                                }
                        }
                        for (auto& g : grains)
                        {
                            if (! g.on) continue;
                            const float w = std::sin (pi * g.age / g.len);
                            const float s0 = readAt (g.pos, 0), s1 = readAt (g.pos, 1);
                            vl += s0 * w * w * (1.0f - g.pan); vr += s1 * w * w * (1.0f + g.pan);
                            g.pos += g.inc; g.age += 1.0f;
                            if (g.age >= g.len || g.pos >= len - 2) g.on = false;
                        }
                        vl *= 0.6f; vr *= 0.6f;
                    }
                    else
                    {
                        if (smpPos <= 0.0) smpPos = bp.smpStart * (double) (len - 2);
                        if (smpPos < len - 2)
                        {
                            vl = readAt (smpPos, 0); vr = readAt (smpPos, 1);
                            smpPos += rateRatio * smpInc;
                            if (bp.smpMode == 1 && smpPos >= len - 2) smpPos = bp.smpStart * (double) (len - 2) + 1.0;
                        }
                    }
                    const float pl = std::cos ((bp.smpPan + 1.0f) * pi * 0.25f) * 1.4142f, pr = std::sin ((bp.smpPan + 1.0f) * pi * 0.25f) * 1.4142f;
                    bus[bp.smpRoute][0] += vl * smpGain * pl;
                    bus[bp.smpRoute][1] += vr * smpGain * pr;
                }

                // ---- shaper / filters
                float outLR[2];
                for (int c = 0; c < 2; ++c)
                {
                    float in1 = bus[0][c] + bus[2][c], in2 = bus[1][c];
                    auto shape = [&] (float x)
                    {
                        const float g = 1.0f + shaperDrive * shaperDrive * 20.0f;
                        float y;
                        switch (bp.shpType)
                        {
                            case 1: y = clampf (x * g, -1.0f, 1.0f); break;
                            case 2: { const float u = x * g; y = u > 0 ? fastTanh (u) : fastTanh (u * 0.5f) * 1.3f; break; }
                            case 3: y = std::sin (x * g * 0.7f); break;
                            case 4: { const float lv = std::exp2 (10.0f - shaperDrive * 8.0f); y = std::round (fastTanh (x * g * 0.5f) * lv) / lv; break; }
                            default: y = fastTanh (x * g);
                        }
                        y /= std::sqrt (std::sqrt (g));
                        return x + (y - x) * bp.shpMix;
                    };
                    if (bp.shpOn && ! bp.shpPost) { in1 = shape (in1); in2 = shape (in2); }
                    float y;
                    if (! bp.parallel)
                    {
                        const float y1 = bp.fOn[0] ? filterSample (0, c, in1, bp) : in1;
                        const float x2 = y1 + in2;
                        y = bp.fOn[1] ? filterSample (1, c, x2, bp) : x2;
                    }
                    else
                    {
                        const float y1 = bp.fOn[0] ? filterSample (0, c, in1, bp) : in1;
                        const float x2 = in2 + bus[2][c];
                        const float y2 = bp.fOn[1] ? filterSample (1, c, x2, bp) : x2;
                        y = y1 + y2;
                    }
                    y += bus[3][c];
                    if (bp.shpOn && bp.shpPost) y = shape (y);
                    outLR[c] = y;
                }

                float g = ampGain + (ampGainTarget - ampGain) * k;
                if (stealing)
                {
                    stealGain -= stealStep;
                    if (stealGain <= 0.0f) { active = false; stealGain = 0.0f; }
                    g *= std::max (0.0f, stealGain);
                }
                L[start + i] += dc[0].process (outLR[0]) * g * panL;
                R[start + i] += dc[1].process (outLR[1]) * g * panR;
                for (int q = 0; q < 5; ++q) oscPrev[q] = oscOut[q];
                if (! active) break;
            }
        }
    }

    // =================================================================== Arp
    void Arp::noteOn (int n, float v)
    {
        if (latched) { numHeld = 0; numOrdered = 0; latched = false; }
        for (int i = 0; i < numHeld; ++i) if (held[i] == n) return;
        if (numHeld < 32) { held[numHeld] = n; heldVel[numHeld] = v; ++numHeld; }
        numOrdered = numHeld;
        for (int i = 0; i < numHeld; ++i) ordered[i] = held[i];
        std::sort (ordered, ordered + numOrdered);
    }

    void Arp::noteOff (int n)
    {
        for (int i = 0; i < numHeld; ++i)
            if (held[i] == n)
            {
                for (int j = i; j < numHeld - 1; ++j) { held[j] = held[j + 1]; heldVel[j] = heldVel[j + 1]; }
                --numHeld;
                break;
            }
        numOrdered = numHeld;
        for (int i = 0; i < numHeld; ++i) ordered[i] = held[i];
        std::sort (ordered, ordered + numOrdered);
    }

    void Arp::process (juce::MidiBuffer& out, int numSamples, double sr, double bpm, double ppq, bool hostPlaying,
                       const std::atomic<float>* const* p, int)
    {
        const int mode = (int) p[0]->load();
        const double stepBeats = Choice::divBeats[juce::jlimit (0, 16, (int) p[1]->load())];
        const int octaves = juce::jlimit (1, 4, (int) p[2]->load());
        const double gate = p[3]->load(), swing = p[4]->load();
        const bool latch = p[5]->load() > 0.5f;
        const int steps = juce::jlimit (1, numSteps, (int) p[6]->load());
        const double beatsPerSample = bpm / 60.0 / sr;

        // with latch on, keep the old chord until a new one starts
        const bool haveNotes = numHeld > 0 || (latch && numOrdered > 0);
        if (latch && numHeld == 0 && numOrdered > 0) latched = true;
        if (! latch && numHeld == 0) numOrdered = 0;

        for (int i = 0; i < numSamples; ++i)
        {
            const double beat = (hostPlaying ? ppq : nextTime) + (double) i * beatsPerSample;
            // pending note-offs
            if (noteOffAt >= 0.0 && beat >= noteOffAt)
            {
                for (int k = 0; k < playingCount; ++k) out.addEvent (juce::MidiMessage::noteOff (1, playingNotes[k]), i);
                playingCount = 0; noteOffAt = -1.0;
            }
            if (! haveNotes || numOrdered == 0)
            {
                if (playingCount > 0) { for (int k = 0; k < playingCount; ++k) out.addEvent (juce::MidiMessage::noteOff (1, playingNotes[k]), i); playingCount = 0; }
                step = 0; lastStepBeat = -1.0;
                continue;
            }
            const double idxD = std::floor (beat / stepBeats);
            const long long idx = (long long) idxD;
            const double stepStart = idxD * stepBeats + ((idx & 1) ? swing * stepBeats : 0.0);
            if (beat < stepStart || idxD <= lastStepBeat) continue;
            lastStepBeat = idxD;

            // end previous notes
            for (int k = 0; k < playingCount; ++k) out.addEvent (juce::MidiMessage::noteOff (1, playingNotes[k]), i);
            playingCount = 0;

            const int s = step % steps;
            currentStep = s;
            ++step;
            if (p[7 + s * 3]->load() < 0.5f) continue;                 // rest
            const int tr = (int) p[8 + s * 3]->load();
            const float vel = juce::jlimit (0.05f, 1.0f, p[9 + s * 3]->load());

            const int total = numOrdered * octaves;
            auto noteAt = [&] (int k) { return ordered[k % numOrdered] + 12 * (k / numOrdered); };
            int notes[16]; int count = 0;
            const int t = (step - 1);
            switch (mode)
            {
                case 0: notes[count++] = noteAt (t % total); break;
                case 1: notes[count++] = noteAt (total - 1 - (t % total)); break;
                case 2: { const int per = std::max (1, total * 2 - 2); const int ph = t % per; notes[count++] = noteAt (ph < total ? ph : per - ph); break; }
                case 3: notes[count++] = noteAt ((int) (rng.uni() * (float) total) % total); break;
                case 4: { const int k = t % (numHeld > 0 ? numHeld * octaves : total);
                          notes[count++] = (numHeld > 0 ? held[k % numHeld] : ordered[k % numOrdered]) + 12 * (k / std::max (1, numHeld > 0 ? numHeld : numOrdered)); break; }
                case 5: { const int o = t % octaves; for (int k = 0; k < numOrdered && count < 16; ++k) notes[count++] = ordered[k] + 12 * o; break; }
                default: notes[count++] = ordered[0]; break;                                   // sequence: lowest held note
            }
            for (int k = 0; k < count; ++k)
            {
                const int nn = juce::jlimit (0, 127, notes[k] + tr);
                out.addEvent (juce::MidiMessage::noteOn (1, nn, vel), i);
                if (playingCount < 16) playingNotes[playingCount++] = nn;
            }
            noteOffAt = stepStart + stepBeats * gate;
        }
        if (! hostPlaying) nextTime += (double) numSamples * beatsPerSample;
        else nextTime = ppq + (double) numSamples * beatsPerSample;
    }

    // =================================================================== Engine
    Engine::Engine (juce::AudioProcessorValueTreeState& s) : apvts (s)
    {
        static const char* oscParts[19] = { "on", "table", "pos", "oct", "semi", "fine", "level", "pan", "uni", "detune", "width",
                                            "phase", "rand", "warp", "warpamt", "mod", "modfrom", "modamt", "route" };
        for (int o = 0; o < numOsc; ++o) for (int k = 0; k < 19; ++k) p.osc[o][k] = P (oscId (o, oscParts[k]));
        p.subOn = P ("sub_on"); p.subShape = P ("sub_shape"); p.subOct = P ("sub_oct"); p.subLevel = P ("sub_level"); p.subRoute = P ("sub_route");
        p.noiseOn = P ("noise_on"); p.noiseType = P ("noise_type"); p.noiseColor = P ("noise_color"); p.noiseLevel = P ("noise_level"); p.noiseRoute = P ("noise_route");
        p.smpOn = P ("smp_on"); p.smpMode = P ("smp_mode"); p.smpRoot = P ("smp_root"); p.smpKey = P ("smp_keytrack"); p.smpStart = P ("smp_start");
        p.smpLevel = P ("smp_level"); p.smpPan = P ("smp_pan"); p.gPos = P ("smp_gpos"); p.gSize = P ("smp_gsize"); p.gDens = P ("smp_gdens");
        p.gSpray = P ("smp_gspray"); p.gPitch = P ("smp_gpitch"); p.smpRoute = P ("smp_route");
        static const char* fParts[8] = { "on", "type", "cutoff", "reso", "drive", "key", "env", "mix" };
        for (int f = 0; f < 2; ++f) for (int k = 0; k < 8; ++k) p.flt[f][k] = P (fltId (f, fParts[k]));
        p.fltRouting = P ("flt_routing"); p.shpOn = P ("shp_on"); p.shpType = P ("shp_type"); p.shpDrive = P ("shp_drive"); p.shpPos = P ("shp_pos"); p.shpMix = P ("shp_mix");
        static const char* eParts[7] = { "delay", "attack", "hold", "decay", "sustain", "release", "curve" };
        for (int e = 0; e < numEnv; ++e) for (int k = 0; k < 7; ++k) p.env[e][k] = P (envId (e, eParts[k]));
        static const char* lParts[7] = { "shape", "sync", "rate", "div", "phase", "mode", "smooth" };
        for (int l = 0; l < numLfo; ++l) for (int k = 0; k < 7; ++k) p.lfo[l][k] = P (lfoId (l, lParts[k]));
        for (int m = 0; m < numMacros; ++m) p.macro[m] = P (macroId (m));
        for (int s2 = 0; s2 < numSlots; ++s2) { p.slot[s2][0] = P (slotId (s2, "src")); p.slot[s2][1] = P (slotId (s2, "dst")); p.slot[s2][2] = P (slotId (s2, "amt")); }
        p.master = P ("master_vol"); p.quality = P ("quality"); p.poly = P ("polyphony"); p.voiceMode = P ("voice_mode");
        p.glide = P ("glide"); p.glideMode = P ("glide_mode"); p.bendRange = P ("bend_range"); p.mpe = P ("mpe");
        p.velSens = P ("vel_sens"); p.ampLevel = P ("amp_level"); p.ampPan = P ("amp_pan");
        p.arpOn = P ("arp_on");
        p.drift = P ("drift"); p.width = P ("out_width");
        p.chordType = P ("chord_type"); p.chordInv = P ("chord_inv"); p.chordVoicing = P ("chord_voicing"); p.chordStrum = P ("chord_strum");
        const char* arpIds[7] = { "arp_mode", "arp_rate", "arp_oct", "arp_gate", "arp_swing", "arp_latch", "arp_steps" };
        for (int k = 0; k < 7; ++k) p.arp[k] = P (arpIds[k]);
        for (int s2 = 0; s2 < numSteps; ++s2)
        {
            p.arp[7 + s2 * 3] = P (stepId (s2, "on"));
            p.arp[8 + s2 * 3] = P (stepId (s2, "tr"));
            p.arp[9 + s2 * 3] = P (stepId (s2, "vel"));
        }
        fx.bind (s);
        matrix.bind (s);
        rack.bind (s);
        rng.seed (987654u);
    }

    void Engine::prepare (double sampleRate, int block)
    {
        sr = sampleRate;
        maxBlock = std::max (32, block);
        os2 = std::make_unique<juce::dsp::Oversampling<float>> (2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
        os4 = std::make_unique<juce::dsp::Oversampling<float>> (2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
        os2->initProcessing ((size_t) maxBlock);
        os4->initProcessing ((size_t) maxBlock);
        work.setSize (2, maxBlock, false, true, false);
        fx.prepare (sr, maxBlock);
        matrix.prepare (sr);
        rack.reset();
        lastLevel = 0.0f;
        qualityNow = -1;
        updateQuality();
        arp.reset();
        for (auto& c : chans) c = {};
        monoCount = 0;
        smoothMaster = -1.0f;
        std::memset (chordNotes, -1, sizeof (chordNotes));
        numPending = 0;
        chordBuf.ensureSize (8192); eventsBuf.ensureSize (8192);
    }

    void Engine::applyChords (const juce::MidiBuffer& in, juce::MidiBuffer& out, int numSamples)
    {
        static const int chordShapes[][6] = {
            { 0, -1 }, { 0, 3, 7, -1 }, { 0, 4, 7, -1 }, { 0, 3, 7, 10, -1 }, { 0, 4, 7, 11, -1 }, { 0, 3, 7, 10, 14, -1 },
            { 0, 4, 7, 11, 14, -1 }, { 0, 3, 7, 14, -1 }, { 0, 2, 7, -1 }, { 0, 5, 7, -1 }, { 0, 7, 12, -1 }, { 0, 12, -1 },
            { 0, 3, 6, -1 }, { 0, 1, 7, -1 } };
        const int type = juce::jlimit (0, 13, (int) p.chordType->load());
        const int inv = juce::jlimit (0, 3, (int) p.chordInv->load());
        const int voicing = (int) p.chordVoicing->load();
        const int strum = (int) (p.chordStrum->load() * 0.001f * (float) sr);
        out.clear();

        // release queued strum notes that are now due
        for (int i = 0; i < numPending;)
        {
            auto& q = pending[i];
            if (q.delay < numSamples)
            {
                out.addEvent (juce::MidiMessage::noteOn (q.ch, q.note, q.vel), std::max (0, q.delay));
                pending[i] = pending[--numPending];
            }
            else { q.delay -= numSamples; ++i; }
        }

        for (const auto meta : in)
        {
            const auto m = meta.getMessage();
            const int pos = meta.samplePosition;
            const int ch = juce::jlimit (1, 16, m.getChannel()) - 1;
            if (m.isNoteOn())
            {
                const int key = m.getNoteNumber();
                auto* slot = chordNotes[ch][key];
                if (type == 0) { out.addEvent (m, pos); slot[0] = (signed char) key; slot[1] = -1; continue; }
                int notes[6]; int cnt = 0;
                for (int k = 0; k < 6 && chordShapes[type][k] >= 0; ++k) notes[cnt++] = key + chordShapes[type][k];
                for (int k = 0; k < inv && k < cnt - 1; ++k) { const int lo = notes[0]; for (int j = 0; j < cnt - 1; ++j) notes[j] = notes[j + 1]; notes[cnt - 1] = lo + 12; }
                if (voicing == 1 && cnt >= 3) notes[1] += 12;            // open: lift the second voice
                if (voicing == 2 && cnt >= 2) notes[0] -= 12;            // wide: drop the bass an octave
                int w = 0;
                for (int k = 0; k < cnt; ++k)
                {
                    const int nn = notes[k];
                    if (nn < 0 || nn > 127) continue;
                    slot[w++] = (signed char) nn;
                    const int d = pos + strum * k;
                    if (d < numSamples) out.addEvent (juce::MidiMessage::noteOn (m.getChannel(), nn, m.getFloatVelocity()), d);
                    else if (numPending < 64) pending[numPending++] = { m.getChannel(), nn, d - numSamples, m.getFloatVelocity() };
                }
                if (w < 7) slot[w] = -1;
            }
            else if (m.isNoteOff())
            {
                auto* slot = chordNotes[ch][m.getNoteNumber()];
                if (slot[0] < 0) { out.addEvent (m, pos); continue; }
                for (int k = 0; k < 7 && slot[k] >= 0; ++k)
                {
                    // drop any strum note still waiting
                    for (int i = 0; i < numPending; ++i)
                        if (pending[i].note == slot[k] && pending[i].ch == m.getChannel()) { pending[i] = pending[--numPending]; break; }
                    out.addEvent (juce::MidiMessage::noteOff (m.getChannel(), slot[k]), pos);
                }
                slot[0] = -1;
            }
            else out.addEvent (m, pos);
        }
    }

    void Engine::updateQuality()
    {
        const int q = juce::jlimit (0, 3, (int) p.quality->load());
        if (q == qualityNow) return;
        qualityNow = q;
        osFactor = q == 2 ? 2 : (q == 3 ? 4 : 1);
        latency = 0;
        if (osFactor == 2) { os2->reset(); latency = (int) std::round (os2->getLatencyInSamples()); }
        if (osFactor == 4) { os4->reset(); latency = (int) std::round (os4->getLatencyInSamples()); }
        for (auto& v : voices) v.prepare (sr * osFactor, maxBlock * osFactor);
    }

    void Engine::readParams()
    {
        for (int o = 0; o < numOsc; ++o)
        {
            auto* a = p.osc[o]; auto& op = bp.osc[o];
            op.on = a[0]->load() > 0.5f; op.table = (int) a[1]->load(); op.pos = a[2]->load(); op.oct = (int) a[3]->load();
            op.semi = a[4]->load(); op.fine = a[5]->load(); op.level = a[6]->load(); op.pan = a[7]->load(); op.uni = (int) a[8]->load();
            op.detune = a[9]->load(); op.width = a[10]->load(); op.phase = a[11]->load(); op.rand = a[12]->load(); op.warp = (int) a[13]->load();
            op.warpAmt = a[14]->load(); op.mod = (int) a[15]->load(); op.modFrom = (int) a[16]->load(); op.modAmt = a[17]->load(); op.route = (int) a[18]->load();
        }
        bp.subOn = p.subOn->load() > 0.5f; bp.subShape = (int) p.subShape->load(); bp.subOct = (int) p.subOct->load();
        bp.subLevel = p.subLevel->load(); bp.subRoute = (int) p.subRoute->load();
        bp.noiseOn = p.noiseOn->load() > 0.5f; bp.noiseType = (int) p.noiseType->load(); bp.noiseColor = p.noiseColor->load();
        bp.noiseLevel = p.noiseLevel->load(); bp.noiseRoute = (int) p.noiseRoute->load();
        bp.smpOn = p.smpOn->load() > 0.5f; bp.smpMode = (int) p.smpMode->load(); bp.smpRoot = (int) p.smpRoot->load(); bp.smpKey = p.smpKey->load() > 0.5f;
        bp.smpStart = p.smpStart->load(); bp.smpLevel = p.smpLevel->load(); bp.smpPan = p.smpPan->load(); bp.gPos = p.gPos->load();
        bp.gSize = p.gSize->load(); bp.gDens = p.gDens->load(); bp.gSpray = p.gSpray->load(); bp.gPitch = p.gPitch->load(); bp.smpRoute = (int) p.smpRoute->load();
        for (int f = 0; f < 2; ++f)
        {
            bp.fOn[f] = p.flt[f][0]->load() > 0.5f; bp.fType[f] = (int) p.flt[f][1]->load(); bp.fCut[f] = p.flt[f][2]->load();
            bp.fReso[f] = p.flt[f][3]->load(); bp.fDrive[f] = p.flt[f][4]->load(); bp.fKey[f] = p.flt[f][5]->load();
            bp.fEnv[f] = p.flt[f][6]->load(); bp.fMix[f] = p.flt[f][7]->load();
        }
        bp.parallel = p.fltRouting->load() > 0.5f;
        bp.shpOn = p.shpOn->load() > 0.5f; bp.shpType = (int) p.shpType->load(); bp.shpDrive = p.shpDrive->load();
        bp.shpPost = p.shpPos->load() > 0.5f; bp.shpMix = p.shpMix->load();
        for (int e = 0; e < numEnv; ++e) for (int k = 0; k < 7; ++k) bp.env[e][k] = p.env[e][k]->load();
        for (int l = 0; l < numLfo; ++l)
        {
            bp.lfoShape[l] = (int) p.lfo[l][0]->load(); bp.lfoSync[l] = p.lfo[l][1]->load() > 0.5f; bp.lfoRate[l] = p.lfo[l][2]->load();
            bp.lfoBeats[l] = (float) Choice::divBeats[juce::jlimit (0, 16, (int) p.lfo[l][3]->load())];
            bp.lfoPhase[l] = p.lfo[l][4]->load(); bp.lfoMode[l] = (int) p.lfo[l][5]->load(); bp.lfoSmooth[l] = p.lfo[l][6]->load();
        }
        for (int m = 0; m < numMacros; ++m) bp.macro[m] = p.macro[m]->load();
        for (int s = 0; s < numSlots; ++s)
        {
            bp.slotSrc[s] = (int) p.slot[s][0]->load(); bp.slotDst[s] = (int) p.slot[s][1]->load(); bp.slotAmt[s] = p.slot[s][2]->load();
        }
        bp.ampLevel = p.ampLevel->load(); bp.ampPan = p.ampPan->load(); bp.velSens = p.velSens->load(); bp.glideMs = p.glide->load();
        bp.voiceMode = (int) p.voiceMode->load(); bp.glideMode = (int) p.glideMode->load(); bp.polyphony = (int) p.poly->load();
        bp.bendRange = (int) p.bendRange->load(); bp.mpe = p.mpe->load() > 0.5f; bp.quality = (int) p.quality->load();
        bp.drift = p.drift->load();
    }

    Voice* Engine::findVoice()
    {
        int activeCount = 0;
        for (auto& v : voices) if (v.isActive() && ! v.isReleasing()) ++activeCount;
        const int limit = bp.quality == 0 ? std::min (bp.polyphony, 8) : bp.polyphony;
        if (activeCount >= limit)
        {
            // steal the oldest held voice (releasing ones are already fading)
            Voice* oldest = nullptr;
            for (auto& v : voices) if (v.isActive() && ! v.isReleasing() && (oldest == nullptr || v.age < oldest->age)) oldest = &v;
            if (oldest) oldest->steal();
        }
        for (auto& v : voices) if (! v.isActive()) return &v;
        Voice* oldest = &voices[0];
        for (auto& v : voices) if (v.age < oldest->age) oldest = &v;
        return oldest;
    }

    void Engine::noteOn (int ch, int note, float vel)
    {
        const bool glideOn = bp.glideMs > 0.5f;
        if (bp.voiceMode == 0)
        {
            bool anyHeld = false;
            for (auto& v : voices) if (v.isActive() && ! v.isReleasing()) anyHeld = true;
            const float startPitch = glideOn && (bp.glideMode == 0 || anyHeld) ? lastNotePitch : (float) note;
            Voice* v = findVoice();
            v->start (note, vel, ch, startPitch, false, bp, res, rng);
            v->age = ++ageCounter;
        }
        else
        {
            if (monoCount < 64) monoStack[monoCount++] = note;
            Voice& v = voices[0];
            for (int i = 1; i < maxVoices; ++i) if (voices[i].isActive()) voices[i].steal();
            if (v.isActive() && ! v.isReleasing())
                v.glideTo (note, vel, bp.voiceMode == 1, bp);
            else
            {
                v.start (note, vel, ch, glideOn && bp.glideMode == 0 ? lastNotePitch : (float) note, false, bp, res, rng);
                v.age = ++ageCounter;
            }
        }
        lastNotePitch = (float) note;
    }

    void Engine::noteOff (int ch, int note)
    {
        if (sustainDown && juce::isPositiveAndBelow (note, 128)) { pendingOff[note] = true; return; }
        if (bp.voiceMode == 0)
        {
            for (auto& v : voices)
                if (v.isActive() && ! v.isReleasing() && v.getNote() == note && (! bp.mpe || v.getChannel() == ch)) v.release();
            return;
        }
        for (int i = 0; i < monoCount; ++i)
            if (monoStack[i] == note) { for (int j = i; j < monoCount - 1; ++j) monoStack[j] = monoStack[j + 1]; --monoCount; break; }
        Voice& v = voices[0];
        if (monoCount == 0) v.release();
        else if (v.getNote() == note) { v.glideTo (monoStack[monoCount - 1], 0.8f, false, bp); lastNotePitch = (float) monoStack[monoCount - 1]; }
    }

    void Engine::allNotesOff()
    {
        for (auto& v : voices) v.release();
        monoCount = 0;
        arp.reset();
    }

    void Engine::handleMidi (const juce::MidiMessage& m, juce::MidiBuffer* arpIn_)
    {
        const int ch = m.getChannel();
        if (m.isNoteOn())
        {
            if (arpIn_ != nullptr) arp.noteOn (m.getNoteNumber(), m.getFloatVelocity());
            else noteOn (ch, m.getNoteNumber(), m.getFloatVelocity());
        }
        else if (m.isNoteOff())
        {
            if (arpIn_ != nullptr) arp.noteOff (m.getNoteNumber());
            else noteOff (ch, m.getNoteNumber());
        }
        else if (m.isPitchWheel())
        {
            const float b = ((float) m.getPitchWheelValue() - 8192.0f) / 8192.0f;
            if (bp.mpe && ch >= 2) chans[ch].bend = b; else chans[0].bend = b;
        }
        else if (m.isChannelPressure())
        {
            const float v = (float) m.getChannelPressureValue() / 127.0f;
            if (bp.mpe && ch >= 2) chans[ch].pressure = v; else chans[0].pressure = v;
        }
        else if (m.isAftertouch())
        {
            for (auto& v : voices) if (v.isActive() && v.getNote() == m.getNoteNumber()) v.polyPressure = (float) m.getAfterTouchValue() / 127.0f;
        }
        else if (m.isController())
        {
            const int cc = m.getControllerNumber();
            const float v = (float) m.getControllerValue() / 127.0f;
            if (cc == 1) modWheel = v;
            else if (cc == 74) { if (bp.mpe && ch >= 2) chans[ch].slide = v; else chans[0].slide = v; }
            else if (cc == 64)
            {
                const bool down = m.getControllerValue() >= 64;
                if (sustainDown && ! down)
                    for (int nn = 0; nn < 128; ++nn) if (pendingOff[nn]) { pendingOff[nn] = false; noteOff (ch, nn); }
                sustainDown = down;
            }
            else if (cc == 123 || cc == 120) allNotesOff();
        }
    }

    void Engine::renderVoices (float* L, float* R, int n)
    {
        for (auto& v : voices)
            if (v.isActive())
                v.render (L, R, n, bp, res, chans, modWheel, beatPos, lastBpm, globalLfoPhase);
    }

    void Engine::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, double bpm, double ppq, bool hostPlaying)
    {
        readParams();
        updateQuality();
        const int n = buffer.getNumSamples();
        lastBpm = (float) bpm;
        buffer.clear();

        // ---- chord memory (before the arp so the arp plays the chords)
        applyChords (midi, chordBuf, n);
        const juce::MidiBuffer& played = chordBuf;

        // ---- arpeggiator: swallow played notes, emit generated ones
        const bool arpOn = p.arpOn->load() > 0.5f;
        if (arpWasOn && ! arpOn) { arpOut.clear(); arp.allOff (arpOut); for (const auto m : arpOut) handleMidi (m.getMessage(), nullptr); arp.reset(); }
        arpWasOn = arpOn;
        juce::MidiBuffer& events = eventsBuf;
        events.clear();
        if (arpOn)
        {
            for (const auto meta : played)
            {
                const auto m = meta.getMessage();
                if (m.isNoteOnOrOff()) handleMidi (m, &arpIn);
                else events.addEvent (m, meta.samplePosition);
            }
            arpOut.clear();
            arp.process (arpOut, n, sr, bpm, ppq, hostPlaying, p.arp, 0);
            events.addEvents (arpOut, 0, n, 0);
            arpStep = arp.currentStep;
        }
        else events.addEvents (played, 0, n, 0);

        // ---- global LFO clock (free mode, unsynced)
        for (int l = 0; l < numLfo; ++l)
        {
            globalLfoPhase[l] += bp.lfoRate[l] * (float) n / (float) sr;
            globalLfoPhase[l] -= std::floor (globalLfoPhase[l]);
        }
        const double blockBeat = hostPlaying ? ppq : beatPos;
        beatPos = hostPlaying ? ppq : beatPos + (double) n * bpm / 60.0 / sr;

        // ---- Minirack: global modulation added to every voice (and to the effects)
        for (auto& v : bp.rackMod) v = 0.0f;
        rack.process (n, sr, bpm, blockBeat, hostPlaying, lastLevel, bp.rackMod);
        for (int m = 0; m < numRack; ++m) rack.uiOut[m] = rack.out[m];

        // ---- voices (sample-accurate MIDI), rendered at the internal rate
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::Oversampling<float>* os = osFactor == 2 ? os2.get() : (osFactor == 4 ? os4.get() : nullptr);
        juce::dsp::AudioBlock<float> up = os ? os->processSamplesUp (block) : block;
        up.clear();
        float* UL = up.getChannelPointer (0);
        float* UR = up.getNumChannels() > 1 ? up.getChannelPointer (1) : up.getChannelPointer (0);
        int pos = 0;
        for (const auto meta : events)
        {
            const int at = juce::jlimit (0, n, meta.samplePosition);
            if (at > pos) { renderVoices (UL + pos * osFactor, UR + pos * osFactor, (at - pos) * osFactor); pos = at; }
            handleMidi (meta.getMessage(), nullptr);
        }
        if (pos < n) renderVoices (UL + pos * osFactor, UR + pos * osFactor, (n - pos) * osFactor);
        if (os) os->processSamplesDown (block);
        if (bypassFx) return;

        // ---- global modulation (from the newest voice) for the effects
        Voice* newest = nullptr;
        int count = 0;
        for (auto& v : voices) if (v.isActive()) { ++count; if (newest == nullptr || v.age > newest->age) newest = &v; }
        activeVoices = count;
        float src[numSources] {};
        if (newest) std::copy (std::begin (newest->srcVals), std::end (newest->srcVals), std::begin (src));
        else
        {
            for (int m = 0; m < numMacros; ++m) src[srcMacro1 + m] = bp.macro[m];
            src[srcModWheel] = modWheel; src[srcBend] = chans[0].bend;
        }
        std::copy (std::begin (src), std::end (src), std::begin (uiSrc));
        for (auto& g : fx.globalMod) g = 0.0f;
        for (int s = 0; s < numSlots; ++s)
            if (bp.slotSrc[s] > 0 && bp.slotDst[s] >= firstGlobalDest)
                fx.globalMod[bp.slotDst[s]] += src[bp.slotSrc[s]] * bp.slotAmt[s];
        for (int d = firstGlobalDest; d < numDests; ++d) fx.globalMod[d] += bp.rackMod[d];
        for (int l = 0; l < numLfo; ++l) uiLfo[l] = src[srcLfo1 + l];
        for (int e = 0; e < numEnv; ++e) uiEnv[e] = src[srcEnv1 + e];

        // ---- effects + master
        float* L = buffer.getWritePointer (0);
        float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
        if (R == nullptr)
        {
            // mono output: run the rack on a stereo scratch copy
            float* tmpL = work.getWritePointer (0); float* tmpR = work.getWritePointer (1);
            std::copy (L, L + n, tmpL); std::copy (L, L + n, tmpR);
            fx.process (tmpL, tmpR, n, bpm);
            for (int i = 0; i < n; ++i) L[i] = 0.5f * (tmpL[i] + tmpR[i]);
        }
        else fx.process (L, R, n, bpm);

        // ---- stereo width (mid/side)
        if (R != nullptr)
        {
            const float w = p.width->load();
            if (std::abs (w - 1.0f) > 0.001f)
                for (int i = 0; i < n; ++i)
                {
                    const float m = 0.5f * (L[i] + R[i]), sd = 0.5f * (L[i] - R[i]) * w;
                    L[i] = m + sd; R[i] = m - sd;
                }
        }

        // ---- Matrix (gate / filter / pan / time) on the whole synth
        if (matrix.isOn())
        {
            if (R != nullptr) matrix.process (L, R, n, blockBeat, bpm, shapes.load());
            else
            {
                float* tmpR = work.getWritePointer (1);
                std::copy (L, L + n, tmpR);
                matrix.process (L, tmpR, n, blockBeat, bpm, shapes.load());
            }
        }
        {
            float lv = 0.0f;
            for (int i = 0; i < n; ++i) lv = std::max (lv, std::abs (L[i]));
            lastLevel = lv;
        }

        const float target = juce::Decibels::decibelsToGain (p.master->load() + fx.globalMod[dstMaster] * 12.0f);
        if (smoothMaster < 0.0f) smoothMaster = target;   // first block after prepare: no fade-in from a stale value
        for (int c = 0; c < buffer.getNumChannels(); ++c)
        {
            float* d = buffer.getWritePointer (c);
            float g = smoothMaster;
            for (int i = 0; i < n; ++i)
            {
                g += (target - g) * 0.002f;
                float o = d[i] * g;
                const float a = std::abs (o);
                if (a > 0.9f) o = (o > 0 ? 1.0f : -1.0f) * (0.9f + 0.1f * fastTanh ((a - 0.9f) * 10.0f));
                if (! std::isfinite (o)) o = 0.0f;
                d[i] = o;
            }
            if (c == buffer.getNumChannels() - 1) smoothMaster = g;
        }
    }
}
