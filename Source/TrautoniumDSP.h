// Trautonium DSP engine - header only, no JUCE dependency.
//
// Signal flow (modelled on Oskar Sala's Mixtur-Trautonium, 1952, one manual):
//
//   String manual ---> master oscillator (relaxation saw, neon/thyratron character)
//        |                 |--> 4 integer frequency dividers (1..24)  = "subharmonics" (saw out)   x 3 mixtures
//        |                 |--> side oscillator (Nebenoszillator, free tuned, beats against master)
//        |                 '--> noise generator
//        |                              v
//        |-- pressure --> channel amplifier (normal | Schlagwerk 1 = AR trigger | Schlagwerk 2 = free run)
//                                       v
//                         4 formant filters in parallel (each Off / Band / Low) --> summing amplifier
//                                       v                                              ^
//                              bandwidth / amp stage                            foot pedal (volume)
//                                       v
//                    frequency shifter -> 2 digital delays -> reverb -> out
#pragma once
#include "Params.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace traut
{
constexpr double kPi = 3.14159265358979323846;

inline float polyBlep (float t, float dt) noexcept
{
    if (t < dt)        { t /= dt;           return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

inline float softLimit (float x) noexcept
{
    const float a = std::fabs (x);
    if (a < 0.9f) return x;
    const float y = 0.9f + 0.1f * std::tanh ((a - 0.9f) / 0.1f);
    return x < 0 ? -y : y;
}

struct SVF // Zavalishin topology-preserving state variable filter
{
    float ic1 = 0, ic2 = 0, g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0;
    void set (float fc, float Q, float sr) noexcept
    {
        fc = std::min (std::max (fc, 20.0f), 0.45f * sr);
        g = std::tan ((float) kPi * fc / sr);
        k = 1.0f / std::max (Q, 0.3f);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    inline void process (float x, float& low, float& band) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1;
        ic2 = 2 * v2 - ic2;
        low = v2; band = v1;
    }
    void reset() noexcept { ic1 = ic2 = 0; }
};

class Engine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        // Hilbert FIR (127 taps, Blackman) for the frequency shifter
        for (int k = 0; k < kHilbN; ++k)
        {
            const int m = k - kHilbN / 2;
            double h = 0.0;
            if (m % 2 != 0) h = 2.0 / (kPi * m);
            const double w = 0.42 - 0.5 * std::cos (2 * kPi * k / (kHilbN - 1)) + 0.08 * std::cos (4 * kPi * k / (kHilbN - 1));
            hilb[(size_t) k] = (float) (h * w);
        }
        dcR = 1.0f - (float) (2 * kPi * 5.0 / sr);
        smoothCoef = 1.0f - std::exp (-1.0f / (float) (0.005 * sr));
        driftCoef = (float) (2 * kPi * 0.7 / sr);
        driftGain = 1.0f / (std::sqrt (driftCoef * 0.5f) * 0.577f);
        const int maxDelay = (int) (1.6 * sr);
        dlyBufL.assign ((size_t) maxDelay, 0.0f);
        dlyBufR.assign ((size_t) maxDelay, 0.0f);
        initReverb();
        reset();
    }

    void reset()
    {
        masterPhase = sidePhase = 0; for (auto& c : cnt) c = 0;
        for (auto& f : filt) f.reset();
        bwFilter.reset();
        env = 0; envPhase = 0; schlagPhase = 0; schlagGate = 0; gate = false; nHeld = 0;
        std::fill (dlyBufL.begin(), dlyBufL.end(), 0.0f);
        std::fill (dlyBufR.begin(), dlyBufR.end(), 0.0f);
        std::fill (hilbBuf.begin(), hilbBuf.end(), 0.0f);
        dlyPos = 0; hilbPos = 0; shiftPhase = 0; dcX1 = dcY1 = 0;
        for (auto& c : combL) c.clear();
        for (auto& c : combR) c.clear();
        for (auto& a : apL) a.clear();
        for (auto& a : apR) a.clear();
        logF = 0; snapPitch = true;
    }

    void setParams (const Params& p)
    {
        P = p;
        for (int k = 0; k < 4; ++k) cnt[k] %= std::max (1, activeDiv (k));
    }
    void setPitchBend (float v)   { bendTarget = v; }                 // -1..1
    void setModWheel (float v)    { modTarget = v; }                  // 0..1
    void setPressure (float v)    { pressure = v; }                   // 0..1 (channel aftertouch)
    void setExpression (float v)  { expTarget = v; }                  // 0..1 (CC11, foot pedal)
    void setMixtureOverride (int m) { mixOverride = m; }              // -1 = follow parameter

    void noteOn (int note, float vel)
    {
        removeHeld (note);
        if (nHeld < kMaxHeld) { heldNote[nHeld] = note; heldVel[nHeld] = vel; ++nHeld; }
        const bool wasGate = gate;
        curNote = note; curVel = vel; gate = true;
        if (! wasGate && env < 1e-4f) snapPitch = true;               // no glide from silence
        if (! wasGate || ampMode() == 1) triggerEnvelope();
    }
    void noteOff (int note)
    {
        removeHeld (note);
        if (nHeld > 0) { curNote = heldNote[nHeld - 1]; curVel = heldVel[nHeld - 1]; }
        else gate = false;
    }
    void allNotesOff() { nHeld = 0; gate = false; }

    // Renders n samples of stereo audio.
    void render (float* outL, float* outR, int n)
    {
        int done = 0;
        while (done < n)
        {
            const int chunk = std::min (32, n - done);
            updateBlockState();
            for (int i = 0; i < chunk; ++i)
            {
                float l, r;
                renderSample (l, r);
                outL[done + i] = l; outR[done + i] = r;
            }
            done += chunk;
        }
    }

private:
    static constexpr int kMaxHeld = 16;
    static constexpr int kHilbN = 127;

    // ---- helpers
    int ampMode() const { return (int) P[kAmpMode]; }
    int activeMix() const { return mixOverride >= 0 ? mixOverride : (int) P[kMixture]; }
    int activeDiv (int k) const { return std::max (1, (int) P[kDivBase + activeMix() * 4 + k]); }
    float activeLvl (int k) const { return P[kLvlBase + activeMix() * 4 + k]; }

    void removeHeld (int note)
    {
        for (int i = 0; i < nHeld; ++i)
            if (heldNote[i] == note)
            {
                for (int j = i; j < nHeld - 1; ++j) { heldNote[j] = heldNote[j + 1]; heldVel[j] = heldVel[j + 1]; }
                --nHeld; return;
            }
    }

    void triggerEnvelope() { envPhase = 1; if (ampMode() == 1) env = 0; schlagPhase = 0; }

    void updateBlockState()
    {
        const float gl = P[kGlide];
        glideCoef = gl < 1.0f ? 1.0f : 1.0f - std::exp (-1.0f / (float) (sr * gl * 0.001 / 3.0));
        attackCoef  = 1.0f - std::exp (-1.0f / (float) (sr * std::max (1.0f, P[kAttack])  * 0.001 / 3.0));
        releaseCoef = 1.0f - std::exp (-1.0f / (float) (sr * std::max (5.0f, P[kRelease]) * 0.001 / 3.0));
        const float trackF = P[kFiltKey] > 0.0f ? std::pow (std::max (1.0f, lastF) / 261.63f, P[kFiltKey]) : 1.0f;
        for (int j = 0; j < 4; ++j)
        {
            const int b = kFiltBase + j * 4;
            filt[j].set (P[b + 1] * trackF, P[b + 2], (float) sr);
        }
        bwFilter.set (P[kBandwidth], 0.7f, (float) sr);
        sideRatio = std::exp2 (P[kSideCents] / 1200.0f);
        dlyL = std::min ((int) dlyBufL.size() - 1, std::max (1, (int) (P[kDlyL] * 0.001 * sr)));
        dlyR = std::min ((int) dlyBufR.size() - 1, std::max (1, (int) (P[kDlyR] * 0.001 * sr)));
        const float fb = 0.70f + 0.28f * P[kRevSize];
        for (auto& c : combL) c.fb = fb;
        for (auto& c : combR) c.fb = fb;
    }

    inline float sawShaped (float phase, float dt, float k) const noexcept
    {
        float s = phase;
        if (k > 0.01f) s = (1.0f - std::exp (-k * phase)) / (1.0f - std::exp (-k)); // RC charging curve
        return 2.0f * s - 1.0f - polyBlep (phase, dt);
    }

    inline float noise() noexcept
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (float) (int32_t) rng * (1.0f / 2147483648.0f);
    }

    inline void renderSample (float& outL, float& outR)
    {
        // ---------- pitch (string manual: continuous, unquantised) ----------
        const float targetLog = (float) (curNote - 69) / 12.0f + P[kMasterOct];
        if (snapPitch) { logF = targetLog; snapPitch = false; }
        logF += (targetLog - logF) * glideCoef;
        bend += (bendTarget - bend) * smoothCoef;
        mod  += (modTarget  - mod)  * smoothCoef;
        expr += (expTarget  - expr) * smoothCoef;

        vibPhase += P[kVibRate] / sr; if (vibPhase >= 1.0) vibPhase -= 1.0;
        const float vib = std::sin ((float) (2 * kPi * vibPhase)) * P[kVibDepth] / 1200.0f * mod;

        driftState += ((noise() * 1.0f) - driftState) * driftCoef;
        const float driftCents = driftState * driftGain * 2.0f * P[kDrift];

        const double f = 440.0 * std::exp2 ((double) (logF + vib + bend * P[kBend] / 12.0f + driftCents / 1200.0f));
        lastF = (float) f;
        const float dt = (float) std::min (0.45, std::max (1e-5, f / sr));

        // ---------- master oscillator + integer dividers ----------
        masterPhase += dt;
        if (masterPhase >= 1.0f)
        {
            masterPhase -= 1.0f;
            for (int k = 0; k < 4; ++k) cnt[k] = (cnt[k] + 1) % activeDiv (k);
        }
        const float curve = P[kNeon] * 5.0f;
        float mix = 0.0f;
        for (int k = 0; k < 4; ++k)
        {
            const float lv = activeLvl (k);
            if (lv <= 0.0f) continue;
            const int n = activeDiv (k);
            if (cnt[k] >= n) cnt[k] %= n;
            const float sp = ((float) cnt[k] + masterPhase) / (float) n;
            mix += lv * sawShaped (sp, dt / (float) n, curve);
        }
        // side oscillator (free tuned, gives beating / interference)
        if (P[kSideLevel] > 0.0f)
        {
            const float sdt = std::min (0.45f, dt * sideRatio);
            sidePhase += sdt; if (sidePhase >= 1.0f) sidePhase -= 1.0f;
            mix += P[kSideLevel] * sawShaped (sidePhase, sdt, curve);
        }
        if (P[kNoise] > 0.0f) mix += P[kNoise] * 0.6f * noise();
        mix *= 0.35f;

        // ---------- channel amplifier ----------
        const float amp = stepEnvelope();
        float x = mix * amp;

        // ---------- formant filters (static, parallel) ----------
        const float u = std::tanh (P[kFiltDrive] * x);
        float sum = 0.0f; bool any = false;
        for (int j = 0; j < 4; ++j)
        {
            const int b = kFiltBase + j * 4;
            const int m = (int) P[b];
            if (m == 0) continue;
            any = true;
            float low, band;
            filt[j].process (u, low, band);
            sum += (m == 1 ? band * filt[j].k : low) * P[b + 3];
        }
        if (! any) sum = u;

        // ---------- summing amplifier (foot pedal) + output stage ----------
        float y = sum * P[kVolume] * expr;
        const float g = 1.0f + P[kAmpDrive] * 3.0f;
        y = std::tanh (g * y) / g * (1.0f + 0.5f * P[kAmpDrive]);
        float lowOut, bandOut; bwFilter.process (y, lowOut, bandOut); y = lowOut;
        const float dc = y - dcX1 + dcR * dcY1; dcX1 = y; dcY1 = dc; y = dc;

        // ---------- rack ----------
        float shifted = frequencyShift (y);
        const float sm = P[kShiftMix];
        const float in = y * (1.0f - sm) + shifted * sm;

        const size_t sz = dlyBufL.size();
        const float dl = dlyBufL[(dlyPos + sz - (size_t) dlyL) % sz];
        const float dr = dlyBufR[(dlyPos + sz - (size_t) dlyR) % sz];
        dlyBufL[dlyPos] = in + dr * P[kDlyFb];
        dlyBufR[dlyPos] = dl;
        dlyPos = (dlyPos + 1) % sz;
        float l = in + dl * P[kDlyMix];
        float r = in + dr * P[kDlyMix];

        if (P[kRevMix] > 0.0f)
        {
            const float rin = (l + r) * 0.5f * 0.015f;
            float wl = 0, wr = 0;
            for (auto& c : combL) wl += c.process (rin);
            for (auto& c : combR) wr += c.process (rin);
            for (auto& a : apL) wl = a.process (wl);
            for (auto& a : apR) wr = a.process (wr);
            l += wl * 3.0f * P[kRevMix]; r += wr * 3.0f * P[kRevMix];
        }
        outL = softLimit (l); outR = softLimit (r);
    }

    inline float stepEnvelope()
    {
        const int mode = ampMode();
        const float vel = std::pow (std::max (0.0f, curVel), 0.8f);
        if (mode == 0)
        {
            const float target = gate ? vel * pressure : 0.0f;
            env += (target - env) * (target > env ? attackCoef : releaseCoef);
            return env * std::sqrt (std::max (0.0f, env)); // fine resolution in the pianissimo range (glycerine resistor)
        }
        if (mode == 1)
        {
            if (envPhase == 1)
            {
                env += 1.0f / (float) (sr * std::max (1.0f, P[kAttack]) * 0.001);
                if (env >= 1.0f) { env = 1.0f; envPhase = 2; }
            }
            else if (envPhase == 2) env *= 1.0f - releaseCoef;
            return env * vel * pressure;
        }
        // mode 2: free-running AR (LFO) gated by the key
        schlagPhase += P[kSchlagRate] / sr; if (schlagPhase >= 1.0) schlagPhase -= 1.0;
        const float a = std::max (1.0f, P[kAttack]), r = std::max (5.0f, P[kRelease]);
        const float split = std::min (0.9f, std::max (0.1f, a / (a + r)));
        const float ph = (float) schlagPhase;
        const float shape = ph < split ? ph / split : (1.0f - ph) / (1.0f - split);
        schlagGate += ((gate ? 1.0f : 0.0f) - schlagGate) * 0.004f;
        env = shape;
        return shape * schlagGate * vel * pressure;
    }

    inline float frequencyShift (float x)
    {
        hilbBuf[(size_t) hilbPos] = x;
        float im = 0.0f;
        for (int k = 0; k < kHilbN; ++k)
            im += hilb[(size_t) k] * hilbBuf[(size_t) ((hilbPos - k + 2 * kHilbBuf) % kHilbBuf)];
        const float re = hilbBuf[(size_t) ((hilbPos - kHilbN / 2 + 2 * kHilbBuf) % kHilbBuf)];
        hilbPos = (hilbPos + 1) % kHilbBuf;
        shiftPhase += P[kShiftHz] / sr; shiftPhase -= std::floor (shiftPhase);
        const float c = std::cos ((float) (2 * kPi * shiftPhase)), s = std::sin ((float) (2 * kPi * shiftPhase));
        return re * c - im * s;
    }

    // ---- Freeverb-style reverb (Sala typically used spring/plate reverb on his productions)
    struct Comb
    {
        std::vector<float> buf; size_t idx = 0; float fb = 0.8f, damp = 0.25f, store = 0.0f;
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); idx = 0; store = 0; }
        float process (float x) { const float o = buf[idx]; store = o * (1 - damp) + store * damp; buf[idx] = x + store * fb; if (++idx >= buf.size()) idx = 0; return o; }
    };
    struct Allpass
    {
        std::vector<float> buf; size_t idx = 0;
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); idx = 0; }
        float process (float x) { const float b = buf[idx]; const float o = b - x; buf[idx] = x + b * 0.5f; if (++idx >= buf.size()) idx = 0; return o; }
    };
    void initReverb()
    {
        static const int ct[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        static const int at[4] = { 556, 441, 341, 225 };
        const double sc = sr / 44100.0;
        for (int i = 0; i < 8; ++i)
        {
            combL[(size_t) i].buf.assign ((size_t) (ct[i] * sc), 0.0f);
            combR[(size_t) i].buf.assign ((size_t) ((ct[i] + 23) * sc), 0.0f);
        }
        for (int i = 0; i < 4; ++i)
        {
            apL[(size_t) i].buf.assign ((size_t) (at[i] * sc), 0.0f);
            apR[(size_t) i].buf.assign ((size_t) ((at[i] + 23) * sc), 0.0f);
        }
    }

    // ---- state
    double sr = 44100.0;
    Params P;
    int heldNote[kMaxHeld] {}; float heldVel[kMaxHeld] {}; int nHeld = 0;
    int curNote = 60; float curVel = 0.8f; bool gate = false;
    float logF = 0, lastF = 261.63f; bool snapPitch = true;
    float glideCoef = 1, attackCoef = 0.01f, releaseCoef = 0.001f, smoothCoef = 0.01f;
    float bend = 0, bendTarget = 0, mod = 0, modTarget = 0, expr = 1, expTarget = 1, pressure = 1;
    int mixOverride = -1;
    float masterPhase = 0, sidePhase = 0, sideRatio = 1; int cnt[4] { 0, 0, 0, 0 };
    double vibPhase = 0, schlagPhase = 0; uint32_t rng = 2463534242u;
    float driftState = 0, driftCoef = 0.0001f, driftGain = 1.0f;
    float env = 0, schlagGate = 0; int envPhase = 0;
    SVF filt[4], bwFilter; float dcX1 = 0, dcY1 = 0, dcR = 0.9995f;
    // shifter
    static constexpr int kHilbBuf = 128;
    std::array<float, kHilbN> hilb {}; std::array<float, kHilbBuf> hilbBuf {}; int hilbPos = 0; double shiftPhase = 0;
    // delay
    std::vector<float> dlyBufL, dlyBufR; size_t dlyPos = 0; int dlyL = 1000, dlyR = 1000;
    Comb combL[8], combR[8]; Allpass apL[4], apR[4];
};

} // namespace traut
