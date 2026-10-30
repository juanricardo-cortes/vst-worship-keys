#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>

// Post-synth effects: tone EQ -> stereo chorus -> ping-pong delay send -> FDN reverb send
// (with shimmer) -> master -> pump -> limiter. The delay line, biquad and reverb tank are the
// same designs as in Sanctuary Vocal, retuned for a full-range synth.
namespace sfx
{
//==============================================================================
struct DelayLine
{
    std::vector<float> buf;
    int w = 0;

    void init (int size) { buf.assign ((size_t) juce::jmax (4, size), 0.0f); w = 0; }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    void push (float x) { buf[(size_t) w] = x; if (++w >= (int) buf.size()) w = 0; }

    // delay = 1 returns the most recently pushed sample
    float read (float delay) const
    {
        auto size = (int) buf.size();
        auto rp = (float) w - juce::jlimit (1.0f, (float) (size - 2), delay);
        if (rp < 0.0f) rp += (float) size;
        auto i0 = (int) rp;
        auto f = rp - (float) i0;
        auto i1 = i0 + 1 >= size ? 0 : i0 + 1;
        return buf[(size_t) i0] + f * (buf[(size_t) i1] - buf[(size_t) i0]);
    }
};

//==============================================================================
// RBJ cookbook biquad, transposed direct form II, up to two channels.
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1[2] {}, z2[2] {};

    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0.0f; }

    float process (float x, int ch = 0)
    {
        auto y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return y;
    }

    void set (double nb0, double nb1, double nb2, double na0, double na1, double na2)
    {
        b0 = (float) (nb0 / na0); b1 = (float) (nb1 / na0); b2 = (float) (nb2 / na0);
        a1 = (float) (na1 / na0); a2 = (float) (na2 / na0);
    }

    struct W { double c, s, alpha, A; };
    static W prep (double sr, double f, double q, double db)
    {
        auto w0 = juce::MathConstants<double>::twoPi * juce::jlimit (10.0, sr * 0.49, f) / sr;
        return { std::cos (w0), std::sin (w0), std::sin (w0) / (2.0 * q), std::pow (10.0, db / 40.0) };
    }

    void highPass (double sr, double f, double q = 0.7071)
    {
        auto w = prep (sr, f, q, 0);
        set ((1 + w.c) / 2, -(1 + w.c), (1 + w.c) / 2, 1 + w.alpha, -2 * w.c, 1 - w.alpha);
    }
    void lowPass (double sr, double f, double q = 0.7071)
    {
        auto w = prep (sr, f, q, 0);
        set ((1 - w.c) / 2, 1 - w.c, (1 - w.c) / 2, 1 + w.alpha, -2 * w.c, 1 - w.alpha);
    }
    void highShelf (double sr, double f, double db, double q = 0.7071)
    {
        auto w = prep (sr, f, q, db);
        auto A = w.A, c = w.c, sa = 2 * std::sqrt (A) * w.alpha;
        set (A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
             (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
    }
};

inline float timeCoef (double sr, double ms) { return (float) std::exp (-1.0 / (juce::jmax (0.01, ms) * 0.001 * sr)); }
inline float toDb (float g) { return 20.0f * std::log10 (juce::jmax (g, 1.0e-6f)); }
inline float fromDb (float db) { return std::pow (10.0f, db * 0.05f); }

//==============================================================================
struct Params
{
    float lowCut = 30, brightDb = 0;
    float chorusMix = 0.25f, chorusRate = 0.5f, chorusDepth = 0.5f;
    float delayMix = 0.15f, delayFeedback = 0.35f, delaySamples = 24000;
    float revMix = 0.25f, revDecay = 3.5f, revSize = 0.75f, revToneHz = 7000, shimmer = 0;
    float masterDb = 0;
};

//==============================================================================
// Juno-style stereo chorus: two modulated taps in opposite phase, one per side.
class Chorus
{
public:
    void prepare (double newSr)
    {
        sr = newSr;
        for (auto& l : lines) l.init ((int) (0.05 * sr));
        phase = 0;
    }

    void process (float* L, float* R, int n, float mix, float rateHz, float depth)
    {
        if (mix < 0.001f)
        {
            for (int i = 0; i < n; ++i) { lines[0].push (L[i]); lines[1].push (R[i]); }
            return;
        }
        auto inc = rateHz / sr;
        auto ms = (float) sr * 0.001f;
        auto d = 0.6f + 4.4f * depth;
        auto dryGain = 1.0f - 0.35f * mix;
        for (int i = 0; i < n; ++i)
        {
            lines[0].push (L[i]);
            lines[1].push (R[i]);
            auto lfo = (float) std::sin (juce::MathConstants<double>::twoPi * phase);
            phase += inc;
            if (phase >= 1.0) phase -= 1.0;
            auto wl = lines[0].read ((7.0f + d * (1.0f + lfo)) * ms);
            auto wr = lines[1].read ((7.0f + d * (1.0f - lfo)) * ms);
            // a touch of cross-feed widens without hollowing out mono
            L[i] = dryGain * L[i] + mix * (0.85f * wl - 0.25f * wr);
            R[i] = dryGain * R[i] + mix * (0.85f * wr - 0.25f * wl);
        }
    }

private:
    double sr = 44100, phase = 0;
    DelayLine lines[2];
};

//==============================================================================
// Ping-pong delay with band-limited repeats (220 Hz - 6.5 kHz). Mono send in, stereo wet out.
class PingPongDelay
{
public:
    void prepare (double newSr)
    {
        sr = newSr;
        for (auto& d : lines) d.init ((int) (3.1 * sr));
        for (auto& b : hp) { b.highPass (sr, 220.0); b.reset(); }
        for (auto& b : lp) { b.lowPass (sr, 6500.0); b.reset(); }
        current = -1;
    }

    void process (const float* in, float* outL, float* outR, int n, float feedback, float delaySamples)
    {
        auto target = juce::jlimit (16.0f, (float) lines[0].buf.size() - 4.0f, delaySamples);
        if (current < 0) current = target;
        for (int i = 0; i < n; ++i)
        {
            current += (target - current) * 0.0004f;
            auto dl = lines[0].read (current);
            auto dr = lines[1].read (current);
            auto fl = lp[0].process (hp[0].process (dr));
            auto fr = lp[1].process (hp[1].process (dl));
            lines[0].push (in[i] + feedback * fl);
            lines[1].push (feedback * fr);
            outL[i] = dl;
            outR[i] = 0.85f * dr + 0.15f * dl;
        }
    }

private:
    double sr = 44100;
    float current = -1;
    DelayLine lines[2];
    Biquad hp[2], lp[2];
};

//==============================================================================
// Two-head granular pitch shifter, fixed +12 semitones (for shimmer).
struct OctaveUp
{
    DelayLine d;
    float window = 2000, phase = 0;

    void prepare (double sr) { window = (float) (0.07 * sr); d.init ((int) window + 16); phase = 0; }

    float process (float x)
    {
        d.push (x);
        float out = 0;
        for (auto offset : { 0.0f, 0.5f })
        {
            auto p = phase + offset;
            if (p >= 1.0f) p -= 1.0f;
            auto w = std::sin (juce::MathConstants<float>::pi * p);
            out += w * w * d.read (2.0f + (1.0f - p) * window);
        }
        phase += 1.0f / window;
        if (phase >= 1.0f) phase -= 1.0f;
        return out;
    }
};

//==============================================================================
// 8-line feedback delay network hall: input diffusion, damping, slow modulation and an
// optional octave-up shimmer fed back into the tank. Mono send in, stereo wet out.
class Reverb
{
public:
    void prepare (double newSr)
    {
        sr = newSr;
        static constexpr float apMs[4] = { 4.77f, 3.59f, 12.73f, 9.31f };
        for (int i = 0; i < 4; ++i) { ap[i].init ((int) (apMs[i] * 0.001 * sr) + 4); apLen[i] = apMs[i] * 0.001f * (float) sr; }
        for (int i = 0; i < N; ++i)
        {
            lines[i].init ((int) (0.2 * sr));
            len[i] = baseMs[i] * 0.001f * (float) sr;
            damp[i] = 0;
        }
        pre.init ((int) (0.05 * sr));
        inHp.highPass (sr, 160.0); inHp.reset();   // keeps bass and sub out of the tail
        inLp.lowPass (sr, 11000.0); inLp.reset();
        shHp.highPass (sr, 500.0); shHp.reset();
        shLp.lowPass (sr, 7500.0); shLp.reset();
        shifter.prepare (sr);
        shimmerFb = 0;
        modPhase = 0;
    }

    void process (const float* in, float* outL, float* outR, int n, const Params& p)
    {
        auto sizeScale = 0.55f + 1.15f * p.revSize;
        float target[N], fb[N];
        for (int k = 0; k < N; ++k)
        {
            target[k] = baseMs[k] * sizeScale * 0.001f * (float) sr;
            fb[k] = std::pow (10.0f, -3.0f * target[k] / (juce::jmax (0.2f, p.revDecay) * (float) sr));
        }
        auto preSamples = 0.022f * (float) sr;
        auto dampCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * p.revToneHz / (float) sr);
        auto modInc = 0.37 / sr;
        auto modDepth = 0.0014f * (float) sr * 0.5f;
        auto shimmer = p.shimmer;

        for (int i = 0; i < n; ++i)
        {
            for (int k = 0; k < N; ++k)
                len[k] += (target[k] - len[k]) * 0.0002f;

            pre.push (inLp.process (inHp.process (in[i])));
            auto x = pre.read (preSamples) + shimmerFb;

            for (int a = 0; a < 4; ++a)
            {
                auto delayed = ap[a].read (apLen[a]);
                auto v = x - 0.6f * delayed;
                ap[a].push (v);
                x = delayed + 0.6f * v;
            }

            auto m = (float) std::sin (juce::MathConstants<double>::twoPi * modPhase);
            modPhase += modInc;
            if (modPhase >= 1.0) modPhase -= 1.0;

            float d[N], sum = 0;
            for (int k = 0; k < N; ++k)
            {
                auto mod = (k == 1 ? m : k == 5 ? -m : k == 3 ? 0.6f * m : 0.0f) * modDepth;
                d[k] = lines[k].read (len[k] + mod);
                damp[k] += dampCoef * (d[k] - damp[k]);
                d[k] = damp[k];
                sum += d[k];
            }
            sum *= 2.0f / (float) N; // Householder reflection

            for (int k = 0; k < N; ++k)
                lines[k].push ((d[k] - sum) * fb[k] + ((k & 1) ? -x : x) * 0.5f);

            auto wl = (d[0] - d[2] + d[4] - d[6]) * 0.5f;
            auto wr = (d[1] - d[3] + d[5] - d[7]) * 0.5f;

            if (shimmer > 0.001f)
            {
                auto sh = shLp.process (shHp.process (shifter.process (0.5f * (wl + wr))));
                shimmerFb = std::tanh (sh * shimmer * 1.1f) * 0.7f;
                wl += sh * shimmer * 0.5f;
                wr += sh * shimmer * 0.5f;
            }
            else
                shimmerFb = 0;

            outL[i] = wl;
            outR[i] = wr;
        }
    }

private:
    static constexpr int N = 8;
    static constexpr float baseMs[N] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.1f, 59.9f, 67.7f, 73.3f };
    double sr = 44100, modPhase = 0;
    DelayLine pre, ap[4], lines[N];
    float apLen[4] {}, len[N] {}, damp[N] {};
    Biquad inHp, inLp, shHp, shLp;
    OctaveUp shifter;
    float shimmerFb = 0;
};

//==============================================================================
// Instant-attack peak limiter (ceiling -0.8 dBFS) with a soft-clip backstop.
class Limiter
{
public:
    void prepare (double newSr) { sr = newSr; gain = 1; rel = timeCoef (sr, 140); }

    void process (float* L, float* R, int n)
    {
        constexpr float ceiling = 0.912f;
        for (int i = 0; i < n; ++i)
        {
            auto peak = juce::jmax (std::abs (L[i]), std::abs (R[i]));
            auto target = peak > ceiling ? ceiling / peak : 1.0f;
            gain = target < gain ? target : target + rel * (gain - target);
            L[i] = soft (L[i] * gain);
            R[i] = soft (R[i] * gain);
            minGain = juce::jmin (minGain, gain);
        }
    }

    float takeMinGain() { auto g = minGain; minGain = 1; return g; }

private:
    static float soft (float x) { return std::abs (x) < 0.9f ? x : std::copysign (0.9f + 0.09f * std::tanh ((std::abs (x) - 0.9f) / 0.09f), x); }
    double sr = 44100;
    float gain = 1, rel = 0, minGain = 1;
};

//==============================================================================
class Chain
{
public:
    void prepare (double newSr, int maxBlock)
    {
        sr = newSr;
        for (auto& b : lowCut) b.reset();
        for (auto& b : bright) b.reset();
        chorus.prepare (sr);
        delay.prepare (sr);
        reverb.prepare (sr);
        limiter.prepare (sr);
        auto size = (size_t) juce::jmax (maxBlock, 2048);
        for (auto* v : { &send, &dL, &dR, &rIn, &rL, &rR }) v->assign (size, 0.0f);
        lastLowCut = lastBright = -1000;
        master.reset (sr, 0.05);
        master.setCurrentAndTargetValue (1.0f);
    }

    int maxBlock() const { return (int) send.size(); }

    // pumpGain (may be null): per-sample gain from the pump, applied to the whole wet+dry mix.
    void process (float* L, float* R, int n, const Params& p, const float* pumpGain)
    {
        if (std::abs (p.lowCut - lastLowCut) > 0.01f)
        {
            lastLowCut = p.lowCut;
            for (auto& b : lowCut) b.highPass (sr, p.lowCut, 0.6);
        }
        if (std::abs (p.brightDb - lastBright) > 0.01f)
        {
            lastBright = p.brightDb;
            for (auto& b : bright) b.highShelf (sr, 5000.0, p.brightDb, 0.6);
        }
        for (int i = 0; i < n; ++i)
        {
            L[i] = bright[0].process (lowCut[0].process (L[i]));
            R[i] = bright[1].process (lowCut[1].process (R[i]));
        }

        chorus.process (L, R, n, p.chorusMix, p.chorusRate, p.chorusDepth);

        for (int i = 0; i < n; ++i)
            send[(size_t) i] = 0.5f * (L[i] + R[i]);
        delay.process (send.data(), dL.data(), dR.data(), n, p.delayFeedback, p.delaySamples);

        for (int i = 0; i < n; ++i)
            rIn[(size_t) i] = send[(size_t) i] + 0.5f * p.delayMix * (dL[(size_t) i] + dR[(size_t) i]);
        reverb.process (rIn.data(), rL.data(), rR.data(), n, p);

        master.setTargetValue (fromDb (p.masterDb));
        auto dry = 1.0f - 0.3f * p.revMix;
        for (int i = 0; i < n; ++i)
        {
            auto g = master.getNextValue() * (pumpGain != nullptr ? pumpGain[i] : 1.0f);
            L[i] = g * (dry * L[i] + p.delayMix * dL[(size_t) i] + 1.3f * p.revMix * rL[(size_t) i]);
            R[i] = g * (dry * R[i] + p.delayMix * dR[(size_t) i] + 1.3f * p.revMix * rR[(size_t) i]);
        }
        limiter.process (L, R, n);
    }

    float takeLimiterGain() { return limiter.takeMinGain(); }

private:
    double sr = 44100;
    Biquad lowCut[2], bright[2];
    Chorus chorus;
    PingPongDelay delay;
    Reverb reverb;
    Limiter limiter;
    std::vector<float> send, dL, dR, rIn, rL, rR;
    float lastLowCut = -1000, lastBright = -1000;
    juce::SmoothedValue<float> master;
};
} // namespace sfx
