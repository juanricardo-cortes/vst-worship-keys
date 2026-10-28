#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "SampleBank.h"
#include <array>
#include <cmath>

namespace wk
{
constexpr double twoPi = 6.283185307179586;

// Per-sample multiplier that decays to -60 dB over `seconds`.
inline float t60Coef (float seconds, double sr) { return (float) std::exp (std::log (0.001) / (juce::jmax (0.001, (double) seconds) * sr)); }
inline float tauCoef (float seconds, double sr) { return (float) std::exp (-1.0 / (juce::jmax (0.0005, (double) seconds) * sr)); }
inline double noteHz (double note) { return 440.0 * std::pow (2.0, (note - 69.0) / 12.0); }
}

struct EngineParams
{
    float velCurve = 1.0f;     // exponent applied to velocity
    float pianoRelease = 0.4f; // damper release, s
    float epBell = 0.5f;       // tine "ping"
    float epBark = 0.4f;       // velocity growl
    float epDecay = 1.0f;      // decay length multiplier
    float padAttack = 1.5f, padRelease = 4.0f;
    float padCutoff = 2500.0f, padReso = 0.15f, padDetune = 0.4f;
    int padOctave = 0;
    bool pianoOn = true, epOn = true, padOn = true;
};

struct VoiceBase
{
    bool active = false, keyDown = false, sustained = false;
    int note = -1;
    uint32_t age = 0;
};

//==============================================================================
struct PianoVoice : VoiceBase
{
    const SampleZone* zone[2] {};
    float zoneGain[2] {};
    double pos = 0, inc = 1;
    float gain = 1, env = 1, relCoef = 1;
    bool releasing = false;

    void start (const SampleBank& bank, int n, float vc, double sr)
    {
        auto layers = bank.getNumLayers();
        auto layerF = vc * (float) (layers - 1);
        auto lo = juce::jlimit (0, layers - 1, (int) layerF);
        auto hi = juce::jmin (lo + 1, layers - 1);
        auto frac = layerF - (float) lo;

        zone[0] = bank.find (n, lo);
        zone[1] = bank.find (n, hi);
        if (zone[1] == zone[0] || zone[1]->rootNote != zone[0]->rootNote || zone[1]->sampleRate != zone[0]->sampleRate)
        {
            zone[1] = nullptr;
            frac = 0.0f;
        }
        zoneGain[0] = 1.0f - frac;
        zoneGain[1] = frac;

        inc = std::pow (2.0, (n - zone[0]->rootNote + zone[0]->tuneCents / 100.0) / 12.0) * zone[0]->sampleRate / sr;
        pos = 0;
        env = 1.0f;
        releasing = false;
        gain = 0.6f + 0.4f * vc; // layers already carry the dynamics; this just widens them a little
        note = n;
        active = true;
    }

    void release (float seconds, double sr)
    {
        // Strings above ~F6 have no dampers on a real grand: let them ring.
        if (note >= 89)
            seconds = juce::jmax (seconds, 4.0f);
        relCoef = wk::t60Coef (seconds, sr);
        releasing = true;
    }

    static float hermite (const int16_t* d, int i, int frames, int ch, float f)
    {
        auto at = [&] (int k) { return (float) d[(size_t) juce::jlimit (0, frames - 1, k) * 2 + (size_t) ch] * (1.0f / 32768.0f); };
        auto xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
        auto c1 = 0.5f * (x1 - xm1);
        auto c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        auto c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

    void render (float* L, float* R, int num)
    {
        auto* z0 = zone[0];
        auto* z1 = zone[1];
        for (int i = 0; i < num; ++i)
        {
            auto ip = (int) pos;
            if (ip + 2 >= z0->numFrames) { active = false; return; }
            auto f = (float) (pos - ip);

            auto l = hermite (z0->data.data(), ip, z0->numFrames, 0, f) * zoneGain[0];
            auto r = hermite (z0->data.data(), ip, z0->numFrames, 1, f) * zoneGain[0];
            if (z1 != nullptr && ip + 2 < z1->numFrames)
            {
                l += hermite (z1->data.data(), ip, z1->numFrames, 0, f) * zoneGain[1];
                r += hermite (z1->data.data(), ip, z1->numFrames, 1, f) * zoneGain[1];
            }

            if (releasing)
            {
                env *= relCoef;
                if (env < 1.0e-4f) { active = false; return; }
            }

            auto g = gain * env;
            L[i] += l * g;
            R[i] += r * g;
            pos += inc;
        }
    }
};

//==============================================================================
// Two-operator-pair FM electric piano, in the spirit of the DX7 "E.PIANO 1":
// a 1:1 pair for body/bark plus a 1:14 pair for the tine ping.
struct EPVoice : VoiceBase
{
    double phase = 0, inc = 0;
    float idxPeak = 0, idxFloor = 0, idxEnv = 1, idxCoef = 1;
    float bellIdx = 0, bellIdxCoef = 1, bellAmp = 0, bellAmpCoef = 1;
    float amp = 0, ampCoef = 1, attack = 0, attackInc = 1;
    float relEnv = 1, relCoef = 1;
    float panL = 0.7f, panR = 0.7f;
    bool releasing = false;

    void start (int n, float v, double sr, const EngineParams& p)
    {
        auto hz = wk::noteHz (n);
        inc = hz / sr;
        phase = 0;
        auto keyScale = juce::jlimit (0.3f, 1.3f, 1.0f - (float) (n - 60) / 48.0f);

        idxPeak = (0.5f + p.epBark * 3.0f * v * v) * keyScale;
        idxFloor = 0.12f * keyScale;
        idxEnv = 1.0f;
        idxCoef = wk::tauCoef (0.35f, sr);

        // Kill the 14x pair where its sidebands would alias.
        auto bellOk = hz * 16.0 < sr * 0.45;
        bellIdx = bellOk ? (1.2f + 1.8f * v) * keyScale : 0.0f;
        bellIdxCoef = wk::tauCoef (0.07f, sr);
        bellAmp = bellOk ? p.epBell * (0.35f + 0.65f * v) * 0.6f : 0.0f;
        bellAmpCoef = wk::tauCoef (0.5f, sr);

        auto decay = p.epDecay * 3.2f * std::pow (2.0f, -(float) (n - 48) / 24.0f);
        amp = 0.25f + 0.75f * std::pow (v, 1.4f);
        ampCoef = wk::tauCoef (decay, sr);
        attack = 0.0f;
        attackInc = (float) (1.0 / (0.0015 * sr));
        relEnv = 1.0f;
        releasing = false;

        auto pan = juce::jlimit (-1.0f, 1.0f, (float) (n - 64) / 40.0f) * 0.35f;
        panL = std::cos ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f);
        panR = std::sin ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f);
        note = n;
        active = true;
    }

    void release (double sr, float seconds = 0.12f)
    {
        relCoef = wk::tauCoef (seconds / 4.0f, sr);
        releasing = true;
    }

    void render (float* L, float* R, int num)
    {
        for (int i = 0; i < num; ++i)
        {
            auto th = wk::twoPi * phase;
            auto idx = idxFloor + (idxPeak - idxFloor) * idxEnv;
            auto body = (float) std::sin (th + idx * std::sin (th));
            auto bell = bellAmp * (float) std::sin (th + bellIdx * std::sin (14.0 * th));

            attack = juce::jmin (1.0f, attack + attackInc);
            auto s = (body * amp + bell) * attack * relEnv * 0.3f;
            L[i] += s * panL;
            R[i] += s * panR;

            idxEnv *= idxCoef;
            bellIdx *= bellIdxCoef;
            bellAmp *= bellAmpCoef;
            amp *= ampCoef;
            if (releasing) relEnv *= relCoef;
            if (relEnv < 1.0e-4f || amp < 1.0e-4f) { active = false; return; }

            phase += inc;
            if (phase >= 1.0) phase -= 1.0;
        }
    }
};

//==============================================================================
// Warm analog-style pad: three detuned PolyBLEP saws + sub sine through a 24 dB/oct low-pass.
struct PadVoice : VoiceBase
{
    double ph[4] {}, inc[4] {};
    float ic1[2][2] {}, ic2[2][2] {}; // [stage][channel]
    float vel = 1;
    juce::ADSR adsr;

    void start (int n, float v, double sr, const EngineParams& p, juce::Random& rng)
    {
        auto pitch = (double) (n + 12 * p.padOctave);
        auto cents = p.padDetune * 18.0;
        double detune[3] = { -cents, 0.0, cents };
        for (int k = 0; k < 3; ++k)
        {
            inc[k] = wk::noteHz (pitch + detune[k] / 100.0) / sr;
            ph[k] = rng.nextDouble();
        }
        inc[3] = wk::noteHz (pitch - 12.0) / sr; // sub
        ph[3] = 0;
        std::fill (&ic1[0][0], &ic1[0][0] + 4, 0.0f);
        std::fill (&ic2[0][0], &ic2[0][0] + 4, 0.0f);

        adsr.setSampleRate (sr);
        adsr.setParameters ({ p.padAttack, 1.5f, 0.85f, p.padRelease });
        adsr.reset();
        adsr.noteOn();
        vel = 0.55f + 0.45f * v;
        note = n;
        active = true;
    }

    void release() { adsr.noteOff(); }

    static float blep (double t, double dt)
    {
        if (t < dt) { t /= dt; return (float) (t + t - t * t - 1.0); }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
        return 0.0f;
    }

    void render (float* L, float* R, int num, float cutoffHz, float reso, double sr)
    {
        // Gentle key tracking keeps high pads from going dull.
        auto fc = juce::jlimit (40.0f, (float) sr * 0.45f, cutoffHz * std::pow (2.0f, (float) (note - 60) / 12.0f * 0.3f));
        auto g = (float) std::tan (juce::MathConstants<double>::pi * fc / sr);
        auto k = 2.0f - 1.9f * reso; // 1/Q
        auto a1 = 1.0f / (1.0f + g * (g + k));
        auto a2 = g * a1;
        auto a3 = g * a2;

        for (int i = 0; i < num; ++i)
        {
            float saw[3];
            for (int s = 0; s < 3; ++s)
            {
                saw[s] = (float) (2.0 * ph[s] - 1.0) - blep (ph[s], inc[s]);
                ph[s] += inc[s];
                if (ph[s] >= 1.0) ph[s] -= 1.0;
            }
            auto sub = (float) std::sin (wk::twoPi * ph[3]) * 0.45f;
            ph[3] += inc[3];
            if (ph[3] >= 1.0) ph[3] -= 1.0;

            float in[2] = { 0.75f * saw[0] + 0.25f * saw[1] + 0.5f * saw[2] + sub,
                            0.25f * saw[0] + 0.75f * saw[1] + 0.5f * saw[2] + sub };

            auto e = adsr.getNextSample() * vel * 0.16f;
            for (int ch = 0; ch < 2; ++ch)
            {
                auto x = in[ch];
                for (int st = 0; st < 2; ++st)
                {
                    auto v3 = x - ic2[st][ch];
                    auto v1 = a1 * ic1[st][ch] + a2 * v3;
                    auto v2 = ic2[st][ch] + a2 * ic1[st][ch] + a3 * v3;
                    ic1[st][ch] = 2.0f * v1 - ic1[st][ch];
                    ic2[st][ch] = 2.0f * v2 - ic2[st][ch];
                    x = v2;
                }
                (ch == 0 ? L : R)[i] += x * e;
            }
        }
        if (! adsr.isActive())
            active = false;
    }
};

//==============================================================================
class KeysEngine
{
public:
    void prepare (double newSr)
    {
        sr = newSr;
        allNotesOff();
    }

    void noteOn (const SampleBank* bank, int note, float velocity, const EngineParams& p)
    {
        auto vc = std::pow (juce::jlimit (0.0f, 1.0f, velocity), p.velCurve);
        ++clock;

        if (p.pianoOn && bank != nullptr)
        {
            for (auto& v : piano) // re-strike: damp the old string quickly
                if (v.active && v.note == note) { v.keyDown = v.sustained = false; v.release (0.08f, sr); }
            auto& v = allocate (piano);
            v.start (*bank, note, vc, sr);
            mark (v);
        }
        if (p.epOn)
        {
            for (auto& v : ep)
                if (v.active && v.note == note) { v.keyDown = v.sustained = false; v.release (sr, 0.05f); }
            auto& v = allocate (ep);
            v.start (note, vc, sr, p);
            mark (v);
        }
        if (p.padOn)
        {
            for (auto& v : pad)
                if (v.active && v.note == note) { v.keyDown = v.sustained = false; v.release(); }
            auto& v = allocate (pad);
            v.start (note, vc, sr, p, rng);
            mark (v);
        }
    }

    void noteOff (int note, const EngineParams& p)
    {
        auto off = [&] (auto& v, auto&& doRelease)
        {
            if (! v.active || v.note != note || ! v.keyDown) return;
            v.keyDown = false;
            if (sustainDown) v.sustained = true;
            else doRelease (v);
        };
        for (auto& v : piano) off (v, [&] (PianoVoice& x) { x.release (p.pianoRelease, sr); });
        for (auto& v : ep)    off (v, [&] (EPVoice& x) { x.release (sr); });
        for (auto& v : pad)   off (v, [&] (PadVoice& x) { x.release(); });
    }

    void setSustain (bool down, const EngineParams& p)
    {
        sustainDown = down;
        if (down) return;
        for (auto& v : piano) if (v.active && v.sustained) { v.sustained = false; v.release (p.pianoRelease, sr); }
        for (auto& v : ep)    if (v.active && v.sustained) { v.sustained = false; v.release (sr); }
        for (auto& v : pad)   if (v.active && v.sustained) { v.sustained = false; v.release(); }
    }

    void allNotesOff()
    {
        for (auto& v : piano) v.active = false;
        for (auto& v : ep) v.active = false;
        for (auto& v : pad) v.active = false;
        sustainDown = false;
    }

    void renderPiano (float* L, float* R, int n) { for (auto& v : piano) if (v.active) v.render (L, R, n); }
    void renderEP (float* L, float* R, int n)    { for (auto& v : ep) if (v.active) v.render (L, R, n); }
    void renderPad (float* L, float* R, int n, const EngineParams& p, float cutoffMod)
    {
        for (auto& v : pad)
            if (v.active) v.render (L, R, n, p.padCutoff * cutoffMod, p.padReso, sr);
    }

private:
    template <typename V, size_t N>
    V& allocate (std::array<V, N>& voices)
    {
        V* best = nullptr;
        for (auto& v : voices)
        {
            if (! v.active) return v;
            // Prefer stealing the oldest voice that's no longer held.
            auto held = v.keyDown || v.sustained;
            if (best == nullptr || (! held && (best->keyDown || best->sustained)) || (held == (best->keyDown || best->sustained) && v.age < best->age))
                best = &v;
        }
        return *best;
    }

    template <typename V>
    void mark (V& v)
    {
        v.keyDown = true;
        v.sustained = false;
        v.age = clock;
    }

    double sr = 44100.0;
    uint32_t clock = 0;
    bool sustainDown = false;
    juce::Random rng { 1234 };
    std::array<PianoVoice, 64> piano;
    std::array<EPVoice, 32> ep;
    std::array<PadVoice, 16> pad;
};
