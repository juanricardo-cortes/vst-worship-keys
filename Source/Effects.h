#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>

namespace fx
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

    float process (float x, int ch)
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
    void peak (double sr, double f, double q, double db)
    {
        auto w = prep (sr, f, q, db);
        set (1 + w.alpha * w.A, -2 * w.c, 1 - w.alpha * w.A, 1 + w.alpha / w.A, -2 * w.c, 1 - w.alpha / w.A);
    }
    void lowShelf (double sr, double f, double db, double q = 0.7071)
    {
        auto w = prep (sr, f, q, db);
        auto A = w.A, c = w.c, sa = 2 * std::sqrt (A) * w.alpha;
        set (A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sa),
             (A + 1) + (A - 1) * c + sa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
    }
    void highShelf (double sr, double f, double db, double q = 0.7071)
    {
        auto w = prep (sr, f, q, db);
        auto A = w.A, c = w.c, sa = 2 * std::sqrt (A) * w.alpha;
        set (A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
             (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
    }
};

//==============================================================================
struct Params
{
    // EQ
    float hpf = 60, low = 0, lowMid = 0, presence = 0, air = 0;
    // Dynamics / colour
    float comp = 0.3f, drive = 0.1f;
    // Space
    float chorusMix = 0.2f, chorusRate = 0.3f;
    float delayMix = 0.0f, delayFeedback = 0.3f, delaySamples = 22050;
    float revMix = 0.3f, revDecay = 3.0f, revSize = 0.7f, revPreDelayMs = 30, revToneHz = 6000, shimmer = 0.0f;
    float masterDb = 0;
};

//==============================================================================
class Equaliser
{
public:
    void prepare (double newSr) { sr = newSr; for (auto& b : bands) b.reset(); }

    void update (const Params& p)
    {
        bands[0].highPass (sr, p.hpf);
        bands[1].lowShelf (sr, 100.0, p.low);
        bands[2].peak (sr, 300.0, 1.0, p.lowMid);
        bands[3].peak (sr, 4000.0, 0.8, p.presence);
        bands[4].highShelf (sr, 10000.0, p.air);
        bands[5].lowPass (sr, 16000.0); // trims digital sizzle
    }

    void process (float* L, float* R, int n)
    {
        for (auto& b : bands)
            for (int i = 0; i < n; ++i)
            {
                L[i] = b.process (L[i], 0);
                R[i] = b.process (R[i], 1);
            }
    }

private:
    double sr = 44100;
    Biquad bands[6];
};

//==============================================================================
// Stereo-linked soft-knee compressor. "Amount" sweeps threshold and ratio together:
// 0.3 ~ -12 dB / 2.8:1 (gentle glue), 0.7 ~ -20 dB / 5:1 (the bright, squashed worship piano).
class Compressor
{
public:
    void prepare (double newSr)
    {
        sr = newSr;
        envDb = 0;
        attackCoef = (float) std::exp (-1.0 / (0.015 * sr));
        releaseCoef = (float) std::exp (-1.0 / (0.14 * sr));
    }

    void process (float* L, float* R, int n, float amount)
    {
        if (amount <= 0.001f) { grDb = 0; return; }
        auto thr = -6.0f - 20.0f * amount;
        auto ratio = 1.0f + 6.0f * amount;
        auto knee = 8.0f;
        auto slope = 1.0f / ratio - 1.0f;
        auto makeup = amount * 6.0f;
        float maxGr = 0;

        for (int i = 0; i < n; ++i)
        {
            auto level = juce::jmax (std::abs (L[i]), std::abs (R[i]));
            auto over = juce::Decibels::gainToDecibels (level, -120.0f) - thr;
            float gr = 0;
            if (over > knee * 0.5f)       gr = slope * over;
            else if (over > -knee * 0.5f) gr = slope * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);

            auto coef = gr < envDb ? attackCoef : releaseCoef;
            envDb = gr + coef * (envDb - gr);
            maxGr = juce::jmin (maxGr, envDb);

            auto g = juce::Decibels::decibelsToGain (envDb + makeup);
            L[i] *= g;
            R[i] *= g;
        }
        grDb = maxGr;
    }

    float grDb = 0; // most recent block's peak gain reduction (<= 0), for metering

private:
    double sr = 44100;
    float envDb = 0, attackCoef = 0, releaseCoef = 0;
};

//==============================================================================
// Unity-gain at low level, rounds off peaks: tanh(g x) / g.
inline void softClip (float* L, float* R, int n, float drive)
{
    if (drive <= 0.001f) return;
    auto g = 1.0f + drive * 3.0f;
    auto inv = 1.0f / g;
    for (int i = 0; i < n; ++i)
    {
        L[i] = std::tanh (g * L[i]) * inv;
        R[i] = std::tanh (g * R[i]) * inv;
    }
}

//==============================================================================
class Chorus
{
public:
    void prepare (double newSr)
    {
        sr = newSr;
        for (auto& d : lines) d.init ((int) (0.05 * sr));
        for (auto& h : lowCut) h.highPass (sr, 300.0); // keep the bass mono and focused
    }

    void process (float* L, float* R, int n, float mix, float rateHz)
    {
        if (mix <= 0.001f) { for (int i = 0; i < n; ++i) { lines[0].push (L[i]); lines[1].push (R[i]); } return; }
        auto inc = rateHz / sr;
        auto norm = 1.0f / (1.0f + 0.5f * mix);
        for (int i = 0; i < n; ++i)
        {
            auto lfoL = (float) std::sin (juce::MathConstants<double>::twoPi * phase);
            auto lfoR = (float) std::cos (juce::MathConstants<double>::twoPi * phase);
            phase += inc;
            if (phase >= 1.0) phase -= 1.0;

            lines[0].push (L[i]);
            lines[1].push (R[i]);
            auto msBase = 9.0f, msDepth = 3.5f;
            auto wl = lowCut[0].process (lines[0].read ((msBase + msDepth * (1.0f + lfoL)) * (float) sr * 0.001f), 0);
            auto wr = lowCut[1].process (lines[1].read ((msBase + msDepth * (1.0f + lfoR)) * (float) sr * 0.001f), 0);
            L[i] = (L[i] + mix * wl) * norm;
            R[i] = (R[i] + mix * wr) * norm;
        }
    }

private:
    double sr = 44100, phase = 0;
    DelayLine lines[2];
    Biquad lowCut[2];
};

//==============================================================================
// Ping-pong delay with band-limited repeats (300 Hz - 4 kHz), the ambient worship-guitar trick.
class TempoDelay
{
public:
    void prepare (double newSr)
    {
        sr = newSr;
        for (auto& d : lines) d.init ((int) (4.1 * sr));
        for (auto& b : hp) b.highPass (sr, 300.0);
        for (auto& b : lp) b.lowPass (sr, 4000.0);
        current = -1;
    }

    void process (float* L, float* R, int n, float mix, float feedback, float delaySamples)
    {
        auto target = juce::jlimit (16.0f, (float) lines[0].buf.size() - 4.0f, delaySamples);
        if (current < 0) current = target;
        if (mix <= 0.001f)
        {
            for (int i = 0; i < n; ++i) { lines[0].push (0); lines[1].push (0); }
            return;
        }
        for (int i = 0; i < n; ++i)
        {
            current += (target - current) * 0.0005f;
            auto dl = lines[0].read (current);
            auto dr = lines[1].read (current);
            auto fl = lp[0].process (hp[0].process (dr, 0), 0);
            auto fr = lp[1].process (hp[1].process (dl, 0), 0);
            lines[0].push (0.5f * (L[i] + R[i]) + feedback * fl);
            lines[1].push (feedback * fr);
            L[i] += mix * dl;
            R[i] += mix * dr;
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
        phase += 1.0f / window; // ratio 2 -> read head gains one sample per sample
        if (phase >= 1.0f) phase -= 1.0f;
        return out;
    }
};

//==============================================================================
// 8-line feedback delay network with input diffusion, damping, slow modulation,
// and an octave-up shimmer fed back into the tank.
class ShimmerReverb
{
public:
    void prepare (double newSr)
    {
        sr = newSr;
        pre.init ((int) (0.31 * sr));
        static constexpr float apMs[4] = { 4.77f, 3.59f, 12.73f, 9.31f };
        for (int i = 0; i < 4; ++i) { ap[i].init ((int) (apMs[i] * 0.001 * sr) + 4); apLen[i] = apMs[i] * 0.001f * (float) sr; }
        for (int i = 0; i < N; ++i)
        {
            lines[i].init ((int) (0.2 * sr));
            len[i] = baseMs[i] * 0.001f * (float) sr;
            damp[i] = 0;
        }
        inHp.highPass (sr, 220.0); // keep reverb out of the low end
        shHp.highPass (sr, 400.0);
        shLp.lowPass (sr, 7000.0);
        shifter.prepare (sr);
        shimmerFb = 0;
        modPhase = 0;
    }

    void process (float* L, float* R, int n, const Params& p)
    {
        auto sizeScale = 0.6f + 1.1f * p.revSize;
        float target[N], fb[N];
        for (int k = 0; k < N; ++k)
        {
            target[k] = baseMs[k] * sizeScale * 0.001f * (float) sr;
            fb[k] = std::pow (10.0f, -3.0f * target[k] / (juce::jmax (0.2f, p.revDecay) * (float) sr));
        }
        auto preSamples = juce::jmax (1.0f, p.revPreDelayMs * 0.001f * (float) sr);
        auto dampCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * p.revToneHz / (float) sr);
        auto modInc = 0.37 / sr;
        auto modDepth = 0.0012f * (float) sr * 0.5f;
        auto shimmer = p.shimmer;
        auto wetGain = p.revMix * 0.9f;
        auto dryGain = 1.0f - 0.25f * p.revMix;

        for (int i = 0; i < n; ++i)
        {
            for (int k = 0; k < N; ++k)
                len[k] += (target[k] - len[k]) * 0.0002f;

            auto x = inHp.process (0.5f * (L[i] + R[i]), 0);
            pre.push (x);
            x = pre.read (preSamples) + shimmerFb;

            for (int a = 0; a < 4; ++a) // Schroeder allpass diffusers
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
                auto mod = (k == 1 ? m : k == 5 ? -m : 0.0f) * modDepth;
                d[k] = lines[k].read (len[k] + mod);
                damp[k] += dampCoef * (d[k] - damp[k]);
                d[k] = damp[k];
                sum += d[k];
            }
            sum *= 2.0f / (float) N; // Householder reflection

            for (int k = 0; k < N; ++k)
            {
                lines[k].push ((d[k] - sum) * fb[k] + ((k & 1) ? -x : x) * 0.5f);
            }

            auto wl = (d[0] - d[2] + d[4] - d[6]) * 0.5f;
            auto wr = (d[1] - d[3] + d[5] - d[7]) * 0.5f;

            if (shimmer > 0.001f)
            {
                auto sh = shLp.process (shHp.process (shifter.process (0.5f * (wl + wr)), 0), 0);
                shimmerFb = std::tanh (sh * shimmer * 1.1f) * 0.75f;
                wl += sh * shimmer * 0.5f;
                wr += sh * shimmer * 0.5f;
            }
            else
                shimmerFb = 0;

            L[i] = L[i] * dryGain + wl * wetGain;
            R[i] = R[i] * dryGain + wr * wetGain;
        }
    }

private:
    static constexpr int N = 8;
    static constexpr float baseMs[N] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.1f, 59.9f, 67.7f, 73.3f };
    double sr = 44100, modPhase = 0;
    DelayLine pre, ap[4], lines[N];
    float apLen[4] {}, len[N] {}, damp[N] {};
    Biquad inHp, shHp, shLp;
    OctaveUp shifter;
    float shimmerFb = 0;
};

//==============================================================================
class Chain
{
public:
    void prepare (double sr, int maxBlock)
    {
        eq.prepare (sr);
        comp.prepare (sr);
        chorus.prepare (sr);
        delay.prepare (sr);
        reverb.prepare (sr);
        limiter.prepare ({ sr, (juce::uint32) maxBlock, 2 });
        limiter.setThreshold (-0.8f);
        limiter.setRelease (120.0f);
        master.reset (sr, 0.05);
    }

    void process (juce::AudioBuffer<float>& buf, int n, const Params& p)
    {
        auto* L = buf.getWritePointer (0);
        auto* R = buf.getWritePointer (1);

        eq.update (p);
        eq.process (L, R, n);
        comp.process (L, R, n, p.comp);
        softClip (L, R, n, p.drive);
        chorus.process (L, R, n, p.chorusMix, p.chorusRate);
        delay.process (L, R, n, p.delayMix, p.delayFeedback, p.delaySamples);
        reverb.process (L, R, n, p);

        master.setTargetValue (juce::Decibels::decibelsToGain (p.masterDb));
        for (int i = 0; i < n; ++i)
        {
            auto g = master.getNextValue();
            L[i] *= g;
            R[i] *= g;
        }

        juce::dsp::AudioBlock<float> block (buf);
        auto sub = block.getSubBlock (0, (size_t) n);
        limiter.process (juce::dsp::ProcessContextReplacing<float> (sub));
    }

    float getGainReductionDb() const { return comp.grDb; }

private:
    Equaliser eq;
    Compressor comp;
    Chorus chorus;
    TempoDelay delay;
    ShimmerReverb reverb;
    juce::dsp::Limiter<float> limiter;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> master { 1.0f };
};
} // namespace fx
