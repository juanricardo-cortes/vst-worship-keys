#pragma once
#include "OrchSampleBank.h"

namespace orch
{
enum class Artic { stabs, sustain, stabHold, marcato };

// Output buses, each with its own fader.
enum Bus { strings, brass, lowEnd, timpani, hits, numBuses };

struct Params
{
    Artic artic = Artic::stabHold;
    float holdTime = 0.18f;   // Stab + Hold: how long a key must be held before the sustain section comes in, s
    float susAttack = 0.25f;  // sustain fade-in, s
    float release = 0.6f;     // sustain release, s
    float octUp = 0.0f;       // violins an octave above the right hand (level 0..1)
    float octDown = 0.7f;     // celli + trombones an octave below the right hand (level 0..1)
    int split = 55;           // keys below this are the left hand: celli, basses, trombones, tuba, timpani. 0 = no split
    int accentVel = 118;      // velocity at or above this adds a cymbal crash + bass drum. 128 = never
    float ensemble = 0.5f;    // doubles every player with a slightly detuned, later copy for a bigger section
    float velSens = 0.6f;
    bool timpOn = true, lowOn = true, hitsOn = true;
};

//==============================================================================
struct Voice
{
    const Region* reg = nullptr;
    int note = -1, pitch = -1, layer = 0, bus = 0; // note = key that triggered it, pitch = what it plays
    bool active = false, sus = false, oneShot = false, keyDown = false, sustained = false, releasing = false;
    bool holdVoice = false;  // a Stab + Hold sustain waiting out its delay: cancelled if the key comes up first
    int delay = 0;           // samples of silence before it starts
    double pos = 0, inc = 1;
    float gain = 1, gl = 1, gr = 1;
    float env = 0, attackStep = 1, relCoef = 1;
    uint32_t age = 0;

    static float hermite (const int16_t* d, int i, int frames, int ch, float f)
    {
        auto at = [&] (int k) { return (float) d[(size_t) juce::jlimit (0, frames - 1, k) * 2 + (size_t) ch] * (1.0f / 32768.0f); };
        auto xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
        auto c1 = 0.5f * (x1 - xm1);
        auto c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        auto c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

    void release (float seconds, double sr)
    {
        relCoef = (float) std::exp (std::log (0.001) / (juce::jmax (0.005, (double) seconds) * sr));
        releasing = true;
    }

    // Adds into the bus pair; bend multiplies the playback rate, swell scales sustains (mod wheel).
    void render (float* L, float* R, int num, float bend, float swellFrom, float swellTo)
    {
        auto* d = reg->data.data();
        auto frames = reg->numFrames;
        auto step = inc * bend;
        auto swellInc = num > 0 ? (swellTo - swellFrom) / (float) num : 0.0f;
        auto swell = swellFrom;
        for (int i = 0; i < num; ++i, swell += swellInc)
        {
            if (delay > 0) { --delay; continue; }
            auto ip = (int) pos;
            if (ip + 2 >= frames) { active = false; return; }
            auto f = (float) (pos - ip);

            auto l = hermite (d, ip, frames, 0, f);
            auto r = hermite (d, ip, frames, 1, f);
            if (reg->loopEnd > 0 && pos >= reg->loopEnd - reg->loopFade)
            {
                // Equal-power crossfade into the matching spot before the loop start, then jump there.
                auto loopLen = (double) (reg->loopEnd - reg->loopStart);
                auto t = (float) ((pos - (reg->loopEnd - reg->loopFade)) / juce::jmax (1, reg->loopFade));
                auto alt = pos - loopLen;
                auto ia = (int) alt;
                auto fa = (float) (alt - ia);
                auto ga = std::cos (t * 1.5707963f), gb = std::sin (t * 1.5707963f);
                l = l * ga + hermite (d, ia, frames, 0, fa) * gb;
                r = r * ga + hermite (d, ia, frames, 1, fa) * gb;
                if (pos + step >= reg->loopEnd)
                    pos -= loopLen;
            }

            if (releasing)
            {
                env *= relCoef;
                if (env < 1.0e-4f) { active = false; return; }
            }
            else if (env < 1.0f)
                env = juce::jmin (1.0f, env + attackStep);

            auto g = gain * env * (sus ? swell : 1.0f);
            L[i] += l * g * gl;
            R[i] += r * g * gr;
            pos += step;
        }
    }
};

//==============================================================================
// Turns one key into an orchestra: the right hand gets violins / violas / trumpets / horns with celli and
// trombones an octave below; the left hand gets celli, basses, trombones, tuba and timpani. Hard hits
// add a crash and a bass drum. Stabs, swelling sustains, or both.
class Engine
{
public:
    static constexpr int maxVoices = 320;

    void prepare (double newSr)
    {
        sr = newSr;
        for (auto& v : voices) v = {};
        sustainPedal = false;
        lastCrash = lastBoom = -1.0e9;
        clock = 0;
    }

    void advanceClock (int samples) { clock += samples; }

    void noteOn (const OrchSampleBank* bank, int note, int vel, const Params& p)
    {
        if (bank == nullptr)
            return;
        auto vf = (float) vel / 127.0f;
        auto velGain = (1.0f - p.velSens) + p.velSens * std::pow (vf, 1.4f) * 1.25f;
        auto rightHand = p.split <= 0 || note >= p.split;

        auto useStac = p.artic != Artic::sustain;
        auto useSus = p.artic != Artic::stabs;
        auto holdSamples = p.artic == Artic::stabHold ? (int) (p.holdTime * sr) : 0;

        auto section = [&] (int stacLayer, int susLayer, int n, int bus, float pan, float level)
        {
            if (level <= 0.001f) return;
            if (useStac) trigger (*bank, stacLayer, note, n, vel, bus, pan, level * velGain, false, 0, p);
            if (useSus)  trigger (*bank, susLayer, note, n, vel, bus, pan, level * velGain, true, holdSamples, p);
        };

        if (rightHand)
        {
            section (vlnStac, vlnSus, note, strings, -0.4f, 1.0f);
            if (note <= 86) section (vlaStac, vlaSus, note, strings, 0.1f, 0.75f);
            section (vcStac, vcSus, note - 12, strings, 0.4f, p.octDown);
            section (vlnStac, vlnSus, note + 12, strings, -0.25f, p.octUp * 0.8f);

            section (tptStac, tptSus, note, brass, 0.15f, 0.9f);
            section (hrnStac, hrnSus, note, brass, -0.2f, 0.8f);
            section (tbnStac, tbnSus, note - 12, brass, 0.3f, p.octDown * 0.9f);
        }
        else
        {
            section (vcStac, vcSus, note, strings, 0.35f, 1.0f);
            section (tbnStac, tbnSus, note, brass, 0.3f, 0.9f);
            section (hrnStac, hrnSus, note + 12, brass, -0.2f, 0.5f);
            if (p.lowOn)
            {
                section (cbStac, cbSus, note - 12, lowEnd, 0.5f, 1.0f);
                section (tubStac, tubSus, note - 12, lowEnd, 0.4f, 0.9f);
            }
            if (p.timpOn)
            {
                auto t = note;
                while (t > 55) t -= 12;
                while (t < 40) t += 12;
                trigger (*bank, timp, note, t, vel, timpani, 0.0f, velGain, false, 0, p, true);
            }
        }

        if (p.hitsOn && vel >= p.accentVel)
        {
            auto gap = 0.15 * sr;
            if (rightHand && clock - lastCrash > gap)
            {
                trigger (*bank, crash, note, 49, vel, hits, 0.2f, velGain, false, 0, p, true);
                lastCrash = clock;
            }
            if (clock - lastBoom > gap)
            {
                trigger (*bank, boom, note, 36, vel, hits, 0.0f, velGain, false, 0, p, true);
                lastBoom = clock;
            }
        }
    }

    void noteOff (int note, const Params& p)
    {
        for (auto& v : voices)
            if (v.active && v.keyDown && v.note == note)
            {
                v.keyDown = false;
                if (v.holdVoice && v.delay > 0) { v.active = false; continue; } // tapped: stab only
                if (! v.sus) continue;                                          // stabs ring out
                if (sustainPedal) v.sustained = true;
                else v.release (p.release, sr);
            }
    }

    void setSustain (bool down, const Params& p)
    {
        sustainPedal = down;
        if (! down)
            for (auto& v : voices)
                if (v.active && v.sustained)
                {
                    v.sustained = false;
                    v.release (p.release, sr);
                }
    }

    void allNotesOff()
    {
        for (auto& v : voices)
            if (v.active) v.release (0.05f, sr);
        sustainPedal = false;
    }

    // bus is 2 * numBuses channels: L/R per bus.
    void render (juce::AudioBuffer<float>& busBuf, int start, int num, float bend, float swellFrom, float swellTo)
    {
        for (auto& v : voices)
            if (v.active)
                v.render (busBuf.getWritePointer (v.bus * 2, start), busBuf.getWritePointer (v.bus * 2 + 1, start), num, bend, swellFrom, swellTo);
    }

    int activeVoices() const
    {
        int n = 0;
        for (auto& v : voices) n += v.active ? 1 : 0;
        return n;
    }

private:
    void trigger (const OrchSampleBank& bank, int layer, int key, int n, int vel, int bus, float pan, float level,
                  bool sus, int delaySamples, const Params& p, bool oneShot = false)
    {
        auto& set = bank.layer (layer);
        if (set.regions.empty())
            return;

        // Keep each player in their range: fold notes by octaves (a few semitones of stretch are fine).
        for (int i = 0; i < 4 && n > set.hiKey + 2; ++i) n -= 12;
        for (int i = 0; i < 4 && n < set.loKey - 2; ++i) n += 12;

        // Repeated stabs on the same pitch: let the previous one go quickly instead of piling up.
        for (auto& v : voices)
            if (v.active && v.layer == layer && v.sus == sus && ! v.releasing
                && v.pitch == n)
                v.release (sus ? 0.12f : 0.18f, sr);

        auto& rr = rrCounter[(size_t) layer];
        rr = rr % set.numRR + 1;

        auto copies = p.ensemble > 0.02f && ! oneShot ? 2 : 1;
        for (int c = 0; c < copies; ++c)
        {
            auto* reg = set.find (n, vel, c == 0 ? rr : rr % set.numRR + 1);
            if (reg == nullptr) return;

            auto& v = allocate();
            v = {};
            v.reg = reg;
            v.layer = layer;
            v.bus = bus;
            v.note = key;
            v.pitch = n;
            v.keyDown = true;
            v.sus = sus;
            v.oneShot = oneShot;
            v.holdVoice = sus && delaySamples > 0;
            v.active = true;
            v.age = ++ageCounter;

            // The doubled player sits a little later, a few cents off and wider in the stereo field.
            auto cents = (random.nextFloat() - 0.5f) * 6.0f;
            auto spread = pan;
            auto copyGain = 1.0f;
            v.delay = delaySamples;
            if (c == 1)
            {
                cents += (random.nextBool() ? 1.0f : -1.0f) * (4.0f + 5.0f * p.ensemble);
                v.delay += (int) ((0.008 + 0.012 * p.ensemble) * sr);
                spread = juce::jlimit (-1.0f, 1.0f, pan + (pan >= 0 ? 0.3f : -0.3f) * p.ensemble);
                copyGain = 0.75f * p.ensemble;
            }
            else if (copies == 2)
                copyGain = 1.0f - 0.25f * p.ensemble; // keep the total level roughly constant

            v.inc = std::pow (2.0, (n - reg->root + cents / 100.0) / 12.0) * reg->sampleRate / sr;
            v.gain = level * copyGain * reg->gain * 0.5f;
            v.gl = juce::jmin (1.0f, 1.0f - spread);
            v.gr = juce::jmin (1.0f, 1.0f + spread);

            if (sus)
            {
                v.env = 0.0f;
                v.attackStep = 1.0f / (float) juce::jmax (1.0, p.susAttack * sr);
            }
            else
            {
                v.env = 1.0f;
                v.attackStep = 1.0f;
            }
        }
    }

    Voice& allocate()
    {
        for (auto& v : voices)
            if (! v.active) return v;
        Voice* best = nullptr;
        for (auto& v : voices) // oldest releasing voice first, then the oldest of all
            if (v.releasing && (best == nullptr || v.age < best->age)) best = &v;
        if (best == nullptr)
            for (auto& v : voices)
                if (best == nullptr || v.age < best->age) best = &v;
        return *best;
    }

    std::array<Voice, maxVoices> voices;
    std::array<int, numLayers> rrCounter {};
    juce::Random random { 1234 };
    double sr = 44100.0, clock = 0, lastCrash = -1.0e9, lastBoom = -1.0e9;
    bool sustainPedal = false;
    uint32_t ageCounter = 0;
};
} // namespace orch
