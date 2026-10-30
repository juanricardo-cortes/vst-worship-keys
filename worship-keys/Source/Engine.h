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
    float pianoTone = 0.55f;   // 0 = dark felt, 1 = open (filter bypassed)
    float epBell = 0.5f;       // tine "ping"
    float epBark = 0.4f;       // velocity growl
    float epDecay = 1.0f;      // decay length multiplier
    float padAttack = 1.5f, padRelease = 4.0f;
    float padCutoff = 2500.0f, padReso = 0.15f, padDetune = 0.4f;
    int padOctave = 0;
    float bellTone = 0.5f;     // FM brightness of the bell strike
    float bellDecay = 2.5f;    // ring time (T60) at middle C, s
    int bellOctave = 1;        // bells usually sit an octave or two above the hands
    bool pianoOn = true, epOn = true, padOn = true, bellOn = false;
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
    float toneScale = 1;           // velocity + key tracking for the tone filter
    float lp1[2] {}, lp2[2] {};    // two cascaded one-poles (12 dB/oct)
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
        // Like a stage piano's "dynamic" timbre: soft playing is darker, hard playing opens up, treble keys stay clear.
        toneScale = std::pow (2.0f, (vc - 0.6f) * 2.0f + (float) (n - 60) / 36.0f);
        lp1[0] = lp1[1] = lp2[0] = lp2[1] = 0.0f;
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

    void render (float* L, float* R, int num, float tone, double sr)
    {
        auto cutoff = 700.0f * std::pow (2.0f, tone * 5.0f) * toneScale;
        auto filtered = tone < 0.98f && cutoff < 0.4f * (float) sr;
        auto a = filtered ? 1.0f - std::exp (-(float) wk::twoPi * cutoff / (float) sr) : 1.0f;

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

            if (filtered)
            {
                lp1[0] += a * (l - lp1[0]); lp2[0] += a * (lp1[0] - lp2[0]); l = lp2[0];
                lp1[1] += a * (r - lp1[1]); lp2[1] += a * (lp1[1] - lp2[1]); r = lp2[1];
            }

            auto g = gain * env;
            L[i] += l * g;
            R[i] += r * g;
            pos += inc;
        }
    }
};

//==============================================================================
// Modeled tine electric piano (Rhodes-style). The hammer sets a tine vibrating: a nearly pure
// fundamental with a fast "prompt" drop then a long sustain, plus the tine's second bending mode
// (~6.27x, the short metallic "ding"). The magnetic pickup is off-centre and non-linear, so
// soft notes come out round and loud notes "bark" with even and odd harmonics.
struct EPVoice : VoiceBase
{
    double phase = 0, inc = 0, ph2 = 0, inc2 = 0, phBeat = 0, incBeat = 0;
    float fast = 0, fastCoef = 1, slow = 0, slowCoef = 1;
    float mode2 = 0, mode2Coef = 1;
    float drive = 1, offset = 0.4f, offsetOut = 0, norm = 1;
    float thump = 0, thumpCoef = 1, thumpLp = 0, thumpA = 0;
    float lp = 0, lpA = 1;
    float attack = 0, attackInc = 1;
    float relEnv = 1, relCoef = 1;
    float panL = 0.7f, panR = 0.7f;
    uint32_t noise = 22222;
    bool releasing = false;

    void start (int n, float v, double sr, const EngineParams& p)
    {
        auto hz = wk::noteHz (n);
        inc = hz / sr;
        incBeat = inc * 1.0011;              // tonebar coupling: a slow, faint beat
        inc2 = inc * 6.267;
        phase = ph2 = phBeat = 0;
        auto octFromC3 = (float) (n - 48) / 12.0f;

        auto a = 0.07f + 0.93f * std::pow (v, 1.6f);
        fast = a * 0.4f;
        slow = a * 0.6f;
        fastCoef = wk::tauCoef (0.28f * std::pow (0.7f, octFromC3), sr);
        slowCoef = wk::tauCoef (p.epDecay * 2.3f * std::pow (2.0f, -octFromC3 / 1.8f), sr);

        // The ding: fades out as it nears the top of the band so the pickup can't fold it back down.
        auto room = (float) juce::jlimit (0.0, 1.0, (0.32 - inc2) / 0.12);
        mode2 = p.epBell * (0.15f + 0.85f * v) * 0.55f * room;
        mode2Coef = wk::tauCoef (0.09f * std::pow (0.75f, octFromC3), sr);

        // Bark: how hard the displaced tine drives the pickup. Low notes swing wider.
        drive = (0.7f + p.epBark * 3.2f * v) * juce::jlimit (0.6f, 1.4f, 1.0f - octFromC3 * 0.12f);
        offset = 0.42f;
        offsetOut = std::tanh (offset);
        norm = 1.0f / (drive * (1.0f - offsetOut * offsetOut));

        thump = 0.12f * v * v;
        thumpCoef = wk::tauCoef (0.004f, sr);
        thumpA = 1.0f - (float) std::exp (-wk::twoPi * 700.0 / sr);
        thumpLp = 0;

        // Hard hits sound brighter; the top end of the keyboard is naturally duller.
        auto fc = juce::jmin ((2200.0 + 7000.0 * v * v) * std::pow (2.0, octFromC3 * 0.25), sr * 0.45);
        lpA = 1.0f - (float) std::exp (-wk::twoPi * fc / sr);
        lp = 0;

        attack = 0.0f;
        attackInc = (float) (1.0 / (0.0008 * sr));
        relEnv = 1.0f;
        releasing = false;

        auto pan = juce::jlimit (-1.0f, 1.0f, (float) (n - 64) / 40.0f) * 0.3f;
        panL = std::cos ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f);
        panR = std::sin ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f);
        note = n;
        active = true;
    }

    // Felt damper: low tines ring on a little longer than high ones.
    void release (double sr, float seconds = -1.0f)
    {
        if (seconds < 0) seconds = juce::jlimit (0.06f, 0.22f, 0.14f - (float) (note - 60) * 0.003f);
        relCoef = wk::tauCoef (seconds / 4.0f, sr);
        releasing = true;
    }

    void render (float* L, float* R, int num)
    {
        for (int i = 0; i < num; ++i)
        {
            auto x = (fast + slow) * ((float) std::sin (wk::twoPi * phase) + 0.05f * (float) std::sin (wk::twoPi * phBeat))
                   + mode2 * (float) std::sin (wk::twoPi * ph2);
            auto y = (std::tanh (drive * x + offset) - offsetOut) * norm;

            noise = noise * 1664525u + 1013904223u;
            thumpLp += thumpA * (thump * ((float) (noise >> 9) * (2.0f / 8388608.0f) - 1.0f) - thumpLp);
            lp += lpA * (y + thumpLp - lp);

            attack = juce::jmin (1.0f, attack + attackInc);
            auto s = lp * attack * relEnv * 0.42f;
            L[i] += s * panL;
            R[i] += s * panR;

            fast *= fastCoef;
            slow *= slowCoef;
            mode2 *= mode2Coef;
            thump *= thumpCoef;
            if (releasing) relEnv *= relCoef;
            if (relEnv < 1.0e-4f || slow < 2.0e-5f) { active = false; return; }

            phase += inc;   if (phase >= 1.0) phase -= 1.0;
            ph2 += inc2;    if (ph2 >= 1.0) ph2 -= 1.0;
            phBeat += incBeat; if (phBeat >= 1.0) phBeat -= 1.0;
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
// Glassy FM bell / glockenspiel: an inharmonic 1:3.5 pair whose brightness dies quickly,
// plus a 1:1 sine body that rings. Lets go slowly after note-off, like a struck bar.
struct BellVoice : VoiceBase
{
    double phase = 0, inc = 0;
    float idx = 0, idxCoef = 1, ping = 0, pingCoef = 1;
    float amp = 0, ampCoef = 1, attack = 0, attackInc = 1;
    float relEnv = 1, relCoef = 1;
    float panL = 0.7f, panR = 0.7f;
    bool releasing = false;

    void start (int n, float v, double sr, const EngineParams& p)
    {
        auto played = n + 12 * p.bellOctave;
        auto hz = wk::noteHz (played);
        inc = hz / sr;
        phase = 0;

        // Highest sideband sits near (3.5 * (idx + 1)) * hz: shrink the index before it aliases.
        auto alias = juce::jlimit (0.0f, 1.0f, (float) (sr * 0.45 / (hz * 18.0)));
        idx = (0.8f + p.bellTone * 4.0f) * (0.5f + 0.5f * v) * alias;
        idxCoef = wk::tauCoef (0.12f + 0.25f * (1.0f - p.bellTone), sr);
        ping = hz * 4.0 < sr * 0.45 ? (0.15f + 0.35f * p.bellTone) * v : 0.0f;
        pingCoef = wk::tauCoef (0.05f, sr);

        auto decay = p.bellDecay * std::pow (2.0f, -(float) (played - 72) / 24.0f);
        amp = 0.3f + 0.7f * v;
        ampCoef = wk::t60Coef (decay, sr);
        attack = 0.0f;
        attackInc = (float) (1.0 / (0.001 * sr));
        relEnv = 1.0f;
        releasing = false;

        auto pan = juce::jlimit (-1.0f, 1.0f, (float) (n - 64) / 30.0f) * 0.5f;
        panL = std::cos ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f);
        panR = std::sin ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f);
        note = n;
        active = true;
    }

    void release (double sr, float seconds = 0.9f)
    {
        relCoef = wk::t60Coef (seconds, sr);
        releasing = true;
    }

    void render (float* L, float* R, int num)
    {
        for (int i = 0; i < num; ++i)
        {
            auto th = wk::twoPi * phase;
            auto s = (float) std::sin (th + idx * std::sin (3.5 * th)) + ping * (float) std::sin (4.0 * th);

            attack = juce::jmin (1.0f, attack + attackInc);
            s *= amp * attack * relEnv * 0.25f;
            L[i] += s * panL;
            R[i] += s * panR;

            idx *= idxCoef;
            ping *= pingCoef;
            amp *= ampCoef;
            if (releasing) relEnv *= relCoef;
            if (relEnv < 1.0e-4f || amp < 1.0e-4f) { active = false; return; }

            phase += inc;
            if (phase >= 1.0) phase -= 1.0;
        }
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
        if (p.bellOn)
        {
            for (auto& v : bell)
                if (v.active && v.note == note) { v.keyDown = v.sustained = false; v.release (sr, 0.05f); }
            auto& v = allocate (bell);
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
        for (auto& v : bell)  off (v, [&] (BellVoice& x) { x.release (sr); });
    }

    void setSustain (bool down, const EngineParams& p)
    {
        sustainDown = down;
        if (down) return;
        for (auto& v : piano) if (v.active && v.sustained) { v.sustained = false; v.release (p.pianoRelease, sr); }
        for (auto& v : ep)    if (v.active && v.sustained) { v.sustained = false; v.release (sr); }
        for (auto& v : pad)   if (v.active && v.sustained) { v.sustained = false; v.release(); }
        for (auto& v : bell)  if (v.active && v.sustained) { v.sustained = false; v.release (sr); }
    }

    void allNotesOff()
    {
        for (auto& v : piano) v.active = false;
        for (auto& v : ep) v.active = false;
        for (auto& v : pad) v.active = false;
        for (auto& v : bell) v.active = false;
        sustainDown = false;
    }

    void renderPiano (float* L, float* R, int n, const EngineParams& p) { for (auto& v : piano) if (v.active) v.render (L, R, n, p.pianoTone, sr); }
    void renderEP (float* L, float* R, int n)    { for (auto& v : ep) if (v.active) v.render (L, R, n); }
    void renderBell (float* L, float* R, int n)  { for (auto& v : bell) if (v.active) v.render (L, R, n); }
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
    std::array<BellVoice, 32> bell;
};
