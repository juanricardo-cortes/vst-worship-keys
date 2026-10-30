#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

// Virtual-analog voice engine:
//   OSC A (7-saw supersaw / saw / square / triangle) + OSC B + sub sine + noise
//   -> 4-pole ladder low-pass per channel (stereo, so the supersaw spread survives the filter)
//   -> amp envelope. Global LFO, glide, poly / mono / legato, sustain pedal, mod-wheel "swell".
namespace syn
{
enum class WaveA { supersaw, saw, square, triangle };
enum class WaveB { saw, square, triangle, sine };
enum class VoiceMode { poly, mono, legato };

struct Params
{
    WaveA waveA = WaveA::supersaw;
    int octA = 0;
    float detune = 0.35f, levelA = 0.8f;
    WaveB waveB = WaveB::saw;
    int octB = 0, semiB = 0;
    float fineB = 7, levelB = 0;
    float pw = 0, sub = 0, noise = 0, width = 0.7f;

    float cutoff = 4000, reso = 0.15f, fltEnv = 0.3f, keyTrack = 0.5f, fltVel = 0.2f, drive = 0.1f;
    float fA = 0.005f, fD = 0.6f, fS = 0.4f, fR = 0.5f;
    float aA = 0.005f, aD = 0.5f, aS = 0.9f, aR = 0.4f, velSens = 0.5f;

    float lfoRate = 5, lfoPitch = 0, lfoFilter = 0, lfoPwm = 0;
    VoiceMode mode = VoiceMode::poly;
    float glide = 0;
    int bendRange = 2;
    float swell = 0;
};

inline float fastTanh (float x)
{
    x = juce::jlimit (-3.0f, 3.0f, x);
    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

// polyBLEP residual for a discontinuity at phase 0 (t in [0,1), dt = phase increment)
inline float blep (float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

inline float wrap (float p) { return p >= 1.0f ? p - 1.0f : p; }

//==============================================================================
// Exponential ADSR. Times are roughly "how long it sounds", not time-constants.
struct Envelope
{
    enum Stage { idle, attack, decay, release };
    Stage stage = idle;
    float value = 0, aCoef = 0, dCoef = 0, rCoef = 0, sustain = 1;

    void set (double sr, float a, float d, float s, float r)
    {
        // attack aims at 1.3 so it reaches 1.0 in ~a seconds with an analog-ish curve
        aCoef = 1.0f - std::exp (-1.466f / (juce::jmax (0.0005f, a) * (float) sr));
        dCoef = std::exp (-5.0f / (juce::jmax (0.001f, d) * (float) sr));
        rCoef = std::exp (-5.0f / (juce::jmax (0.001f, r) * (float) sr));
        sustain = s;
    }
    void gateOn() { stage = attack; }   // retriggers from the current level (no click)
    void gateOff() { if (stage != idle) stage = release; }

    float next()
    {
        switch (stage)
        {
            case attack:
                value += (1.3f - value) * aCoef;
                if (value >= 1.0f) { value = 1.0f; stage = decay; }
                break;
            case decay:   value = sustain + (value - sustain) * dCoef; break;
            case release:
                value *= rCoef;
                if (value < 1.0e-4f) { value = 0; stage = idle; }
                break;
            case idle: break;
        }
        return value;
    }
};

//==============================================================================
// Zero-delay-feedback 4-pole ladder (Zavalishin TPT form) with a saturating input stage.
struct Ladder
{
    float s[4] {};
    void reset() { s[0] = s[1] = s[2] = s[3] = 0; }

    float process (float x, float G, float k)
    {
        auto b = 1.0f - G;
        auto G2 = G * G;
        auto S = b * (G2 * G * s[0] + G2 * s[1] + G * s[2] + s[3]);
        auto u = fastTanh ((x - k * S) / (1.0f + k * G2 * G2));
        for (int i = 0; i < 4; ++i)
        {
            auto v = (u - s[i]) * G;
            auto y = v + s[i];
            s[i] = y + v;
            u = y;
        }
        return u;
    }
};

//==============================================================================
struct Voice
{
    static constexpr int numSaws = 7;

    int note = -1, age = 0;
    float vel = 1, pitch = 60, targetPitch = 60, drift = 0;
    bool held = false, sustained = false;
    float sawPh[numSaws] {}, phB = 0, phSub = 0;
    float panL[numSaws] {}, panR[numSaws] {};
    Ladder fl, fr;
    Envelope amp, flt;
    uint32_t rng = 1;
    // control-rate values
    float inc[numSaws] {}, incB = 0, incSub = 0, G = 0.5f, k = 0, pwA = 0.5f, pwB = 0.5f;

    bool active() const { return amp.stage != Envelope::idle; }

    float noise()
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (float) (int32_t) rng * (1.0f / 2147483648.0f);
    }
};

//==============================================================================
class Engine
{
public:
    static constexpr int maxVoices = 16;
    static constexpr int controlRate = 16;

    void prepare (double newSr)
    {
        sr = newSr;
        juce::Random r (0x5eed);
        for (int v = 0; v < maxVoices; ++v)
        {
            auto& vo = voices[(size_t) v];
            vo = Voice();
            vo.rng = 0x9e3779b9u * (uint32_t) (v + 1);
            for (auto& p : vo.sawPh) p = r.nextFloat();
            vo.phB = r.nextFloat();
        }
        lfoPhase = 0;
        bend = bendTarget = 0;
        heldStack.clear();
        sustainDown = false;
        wheel = 0;
        counter = 0;
    }

    //------------------------------------------------------------------ MIDI
    void noteOn (int note, float velocity, const Params& p)
    {
        lastParams = p;
        if (p.mode == VoiceMode::poly)
        {
            auto& v = allocate (note);
            start (v, note, velocity, p, true);
            lastNote = note;
            return;
        }

        // mono / legato: one voice with a note stack (last-note priority)
        heldStack.erase (std::remove (heldStack.begin(), heldStack.end(), note), heldStack.end());
        heldStack.push_back (note);
        auto& v = voices[0];
        bool legato = p.mode == VoiceMode::legato && v.held && v.active();
        start (v, note, velocity, p, ! legato);
        lastNote = note;
    }

    void noteOff (int note, const Params& p)
    {
        if (p.mode == VoiceMode::poly)
        {
            for (auto& v : voices)
                if (v.note == note && v.held)
                    release (v);
            return;
        }
        heldStack.erase (std::remove (heldStack.begin(), heldStack.end(), note), heldStack.end());
        auto& v = voices[0];
        if (v.note != note)
            return;
        if (! heldStack.empty())
        {
            // fall back to the previous held key, gliding, without retriggering
            v.note = heldStack.back();
            v.targetPitch = (float) v.note;
            if (p.glide <= 0.0005f) v.pitch = v.targetPitch;
        }
        else
            release (v);
    }

    void setSustain (bool down)
    {
        sustainDown = down;
        if (! down)
            for (auto& v : voices)
                if (v.sustained)
                {
                    v.sustained = false;
                    if (! v.held) { v.amp.gateOff(); v.flt.gateOff(); }
                }
    }

    void allNotesOff()
    {
        heldStack.clear();
        for (auto& v : voices) { v.held = v.sustained = false; v.amp.gateOff(); v.flt.gateOff(); }
    }

    void setPitchBend (float normalised) { bendTarget = normalised; }   // -1 .. 1
    void setModWheel (float value) { wheel = value; }                   //  0 .. 1
    int activeVoices() const { int n = 0; for (auto& v : voices) n += v.active() ? 1 : 0; return n; }

    //------------------------------------------------------------------ audio
    // Adds into L/R.
    void render (float* L, float* R, int n, const Params& p)
    {
        lastParams = p;
        for (auto& v : voices)
        {
            if (! v.active()) continue;
            v.amp.set (sr, p.aA, p.aD, p.aS, p.aR);
            v.flt.set (sr, p.fA, p.fD, p.fS, p.fR);
        }

        auto lfoInc = (float) (p.lfoRate / sr);
        auto glideCoef = p.glide > 0.0005f ? std::exp (-(float) controlRate / (p.glide * 0.35f * (float) sr)) : 0.0f;
        auto bendCoef = std::exp (-(float) controlRate / (0.004f * (float) sr));
        auto supersaw = p.waveA == WaveA::supersaw;
        auto gainA = p.levelA * (supersaw ? 0.85f : 1.0f);
        auto voiceGain = 0.28f;

        // supersaw detune: JP-8000-style spread, curved so the low end of the knob is fine-grained
        static constexpr float offsets[Voice::numSaws] = { -0.110f, -0.063f, -0.0195f, 0.0f, 0.0199f, 0.062f, 0.107f };
        static constexpr float panPos[Voice::numSaws] = { -1.0f, 0.66f, -0.33f, 0.0f, 0.33f, -0.66f, 1.0f };
        auto det = p.detune * p.detune * 0.55f + p.detune * 0.06f;
        float sawGain[Voice::numSaws];
        for (int s = 0; s < Voice::numSaws; ++s)
            sawGain[s] = supersaw ? (s == 3 ? 0.5f : 0.3f) : (s == 3 ? 1.0f : 0.0f);

        for (int i = 0; i < n; ++i)
        {
            if (counter <= 0)
            {
                counter = controlRate;
                bend = bendTarget + bendCoef * (bend - bendTarget);
                auto lfo = std::sin (juce::MathConstants<float>::twoPi * lfoPhase);
                lfoNow = lfo;
                auto swell = juce::jmax (p.swell, wheel);
                for (auto& v : voices)
                    if (v.active())
                        updateControl (v, p, lfo, swell, glideCoef, det, offsets, panPos);
            }
            --counter;
            lfoPhase += lfoInc;
            if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;

            float outL = 0, outR = 0;
            for (auto& v : voices)
            {
                if (! v.active()) continue;
                auto a = v.amp.next();
                auto fe = v.flt.next();
                (void) fe;

                // ---- OSC A
                float aL = 0, aR = 0;
                if (supersaw)
                {
                    for (int s = 0; s < Voice::numSaws; ++s)
                    {
                        auto ph = v.sawPh[s], dt = v.inc[s];
                        auto x = (2.0f * ph - 1.0f - blep (ph, dt)) * sawGain[s];
                        aL += x * v.panL[s];
                        aR += x * v.panR[s];
                        v.sawPh[s] = wrap (ph + dt);
                    }
                }
                else
                {
                    auto ph = v.sawPh[3], dt = v.inc[3];
                    float x;
                    if (p.waveA == WaveA::saw) x = 2.0f * ph - 1.0f - blep (ph, dt);
                    else if (p.waveA == WaveA::square) x = squareWave (ph, dt, v.pwA);
                    else x = 4.0f * std::abs (ph - 0.5f) - 1.0f;
                    aL = aR = x * 0.7071f;
                    v.sawPh[3] = wrap (ph + dt);
                }

                // ---- OSC B, sub, noise (centre)
                float mono = 0;
                if (p.levelB > 0.001f)
                {
                    auto ph = v.phB, dt = v.incB;
                    float x;
                    switch (p.waveB)
                    {
                        case WaveB::saw:      x = 2.0f * ph - 1.0f - blep (ph, dt); break;
                        case WaveB::square:   x = squareWave (ph, dt, v.pwB); break;
                        case WaveB::triangle: x = 4.0f * std::abs (ph - 0.5f) - 1.0f; break;
                        default:              x = std::sin (juce::MathConstants<float>::twoPi * ph); break;
                    }
                    mono += x * p.levelB;
                    v.phB = wrap (ph + dt);
                }
                if (p.sub > 0.001f)
                {
                    mono += std::sin (juce::MathConstants<float>::twoPi * v.phSub) * p.sub;
                    v.phSub = wrap (v.phSub + v.incSub);
                }
                if (p.noise > 0.001f)
                    mono += v.noise() * p.noise * 0.5f;

                auto drive = 1.0f + 3.0f * p.drive;
                auto inL = (aL * gainA + mono * 0.7071f) * drive * 0.6f;
                auto inR = (aR * gainA + mono * 0.7071f) * drive * 0.6f;
                auto comp = (1.0f + 0.6f * v.k) / (0.6f * (1.0f + 1.2f * p.drive));
                auto g = a * a * voiceGain * (1.0f - p.velSens * (1.0f - v.vel)) * comp;   // squared env ~ more natural fades
                outL += v.fl.process (inL, v.G, v.k) * g;
                outR += v.fr.process (inR, v.G, v.k) * g;
            }
            L[i] += outL;
            R[i] += outR;
        }
    }

    float getLfo() const { return lfoNow; }

private:
    static float squareWave (float ph, float dt, float pw)
    {
        auto x = ph < pw ? 1.0f : -1.0f;
        x += blep (ph, dt);
        auto t = ph - pw;
        if (t < 0.0f) t += 1.0f;
        x -= blep (t, dt);
        return x * 0.8f;
    }

    void updateControl (Voice& v, const Params& p, float lfo, float swell, float glideCoef, float det,
                        const float* offsets, const float* panPos)
    {
        v.pitch = v.targetPitch + glideCoef * (v.pitch - v.targetPitch);
        auto pitch = v.pitch + bend * (float) p.bendRange + v.drift + lfo * p.lfoPitch * 0.5f;
        auto baseA = 440.0f * std::exp2 ((pitch - 69.0f) / 12.0f + (float) p.octA);
        auto nyq = 0.45f * (float) sr;
        for (int s = 0; s < Voice::numSaws; ++s)
        {
            v.inc[s] = juce::jmin (nyq, baseA * (1.0f + offsets[s] * det)) / (float) sr;
            auto pan = panPos[s] * p.width;
            auto angle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
            v.panL[s] = std::cos (angle) * 1.4142f * 0.7071f;
            v.panR[s] = std::sin (angle) * 1.4142f * 0.7071f;
        }
        auto fB = 440.0f * std::exp2 ((pitch - 69.0f + (float) p.semiB + p.fineB * 0.01f) / 12.0f + (float) p.octB);
        v.incB = juce::jmin (nyq, fB) / (float) sr;
        v.incSub = baseA * 0.5f / (float) sr;
        auto pw = 0.5f + 0.45f * juce::jlimit (0.0f, 1.0f, p.pw + p.lfoPwm * 0.5f * (lfo + 1.0f) * 0.9f);
        v.pwA = v.pwB = pw;

        // cutoff in octaves around the knob value
        auto oct = p.fltEnv * v.flt.value * 7.0f
                 + p.keyTrack * (v.pitch - 60.0f) / 12.0f
                 + p.lfoFilter * lfo * 2.0f
                 + swell * 3.0f
                 + p.fltVel * (v.vel - 0.7f) * 2.5f;
        auto fc = juce::jlimit (20.0f, 0.42f * (float) sr, p.cutoff * std::exp2 (oct));
        auto gg = std::tan (juce::MathConstants<float>::pi * fc / (float) sr);
        v.G = gg / (1.0f + gg);
        v.k = p.reso * 3.85f;
        ++v.age;
    }

    Voice& allocate (int note)
    {
        // same key re-struck: reuse its voice (like a real poly retriggering)
        for (auto& v : voices)
            if (v.note == note && v.active()) return v;
        for (auto& v : voices)
            if (! v.active()) return v;
        // steal: prefer released voices, then the oldest
        Voice* best = nullptr;
        for (auto& v : voices)
            if (! v.held && ! v.sustained && (best == nullptr || v.amp.value < best->amp.value)) best = &v;
        if (best != nullptr) return *best;
        for (auto& v : voices)
            if (best == nullptr || v.age > best->age) best = &v;
        return *best;
    }

    void start (Voice& v, int note, float velocity, const Params& p, bool retrigger)
    {
        auto wasActive = v.active();
        v.note = note;
        v.vel = velocity;
        v.held = true;
        v.sustained = false;
        v.targetPitch = (float) note;
        auto from = p.mode == VoiceMode::poly ? (lastNote >= 0 ? (float) lastNote : (float) note) : v.pitch;
        v.pitch = (p.glide > 0.0005f && (wasActive || p.mode == VoiceMode::poly) && lastNote >= 0) ? from : (float) note;
        v.drift = (((float) (v.noise() + 1.0f)) * 0.5f - 0.5f) * 0.05f;   // +-2.5 cents of analog drift
        v.age = 0;
        if (! wasActive) { v.fl.reset(); v.fr.reset(); v.amp.value = 0; v.flt.value = 0; }
        v.amp.set (sr, p.aA, p.aD, p.aS, p.aR);
        v.flt.set (sr, p.fA, p.fD, p.fS, p.fR);
        if (retrigger || ! wasActive) { v.amp.gateOn(); v.flt.gateOn(); }
        counter = 0;   // refresh control values right away
    }

    void release (Voice& v)
    {
        v.held = false;
        if (sustainDown) { v.sustained = true; return; }
        v.amp.gateOff();
        v.flt.gateOff();
    }

    double sr = 44100;
    std::array<Voice, maxVoices> voices;
    std::vector<int> heldStack;
    Params lastParams;
    float lfoPhase = 0, lfoNow = 0, bend = 0, bendTarget = 0, wheel = 0;
    bool sustainDown = false;
    int lastNote = -1, counter = 0;
};

//==============================================================================
// Arpeggiator: collects held keys (the sustain pedal latches them) and plays them on a beat grid.
class Arpeggiator
{
public:
    enum class Mode { off, up, down, upDown, random, chord };

    struct Event { bool on; int note; float vel; };

    void reset() { held.clear(); playing.clear(); lastStep = -1; stepCount = 0; offBeat = -1; }
    bool hasNotes() const { return ! held.empty(); }

    void noteOn (int note, float vel)
    {
        for (auto& h : held) if (h.note == note) { h.vel = vel; h.down = true; return; }
        held.push_back ({ note, vel, true });
        std::sort (held.begin(), held.end(), [] (const Key& a, const Key& b) { return a.note < b.note; });
        if (held.size() == 1) { lastStep = -1; stepCount = 0; }   // first key: play immediately
    }

    void noteOff (int note)
    {
        for (size_t i = 0; i < held.size(); ++i)
            if (held[i].note == note)
            {
                if (sustain) held[i].down = false;
                else held.erase (held.begin() + (long) i);
                return;
            }
    }

    void setSustain (bool down)
    {
        sustain = down;
        if (! down)
            held.erase (std::remove_if (held.begin(), held.end(), [] (const Key& k) { return ! k.down; }), held.end());
    }

    // Called once per control chunk with the beat position at the chunk start.
    template <typename Fn>
    void tick (double beat, Mode mode, double stepBeats, int octaves, float gate, Fn&& emit)
    {
        if (offBeat >= 0 && beat >= offBeat)
        {
            for (auto& n : playing) emit (Event { false, n, 0 });
            playing.clear();
            offBeat = -1;
        }
        if (held.empty() || mode == Mode::off)
        {
            for (auto& n : playing) emit (Event { false, n, 0 });
            playing.clear();
            lastStep = -1;
            return;
        }
        auto step = (long long) std::floor (beat / stepBeats);
        if (step == lastStep && lastStep >= 0)
            return;
        lastStep = step;

        for (auto& n : playing) emit (Event { false, n, 0 });
        playing.clear();

        if (mode == Mode::chord)
        {
            for (auto& h : held) { emit (Event { true, h.note, h.vel }); playing.push_back (h.note); }
        }
        else
        {
            std::vector<Key> seq;
            for (int o = 0; o < octaves; ++o)
                for (auto& h : held) if (h.note + 12 * o < 128) seq.push_back ({ h.note + 12 * o, h.vel, true });
            auto len = (int) seq.size();
            int idx = 0;
            switch (mode)
            {
                case Mode::up:   idx = stepCount % len; break;
                case Mode::down: idx = len - 1 - stepCount % len; break;
                case Mode::upDown:
                {
                    auto cycle = juce::jmax (1, 2 * len - 2);
                    auto c = stepCount % cycle;
                    idx = c < len ? c : cycle - c;
                    break;
                }
                default: idx = rng.nextInt (len); break;
            }
            emit (Event { true, seq[(size_t) idx].note, seq[(size_t) idx].vel });
            playing.push_back (seq[(size_t) idx].note);
        }
        ++stepCount;
        offBeat = beat + stepBeats * juce::jlimit (0.05, 1.0, (double) gate) - 1.0e-6;
    }

    template <typename Fn>
    void flush (Fn&& emit)
    {
        for (auto& n : playing) emit (Event { false, n, 0 });
        playing.clear();
        offBeat = -1;
    }

private:
    struct Key { int note; float vel; bool down; };
    std::vector<Key> held;
    std::vector<int> playing;
    long long lastStep = -1;
    int stepCount = 0;
    double offBeat = -1;
    bool sustain = false;
    juce::Random rng { 0xa4b };
};

//==============================================================================
// Tempo-synced "motion": sidechain-style pump or rhythmic gate. Returns a per-sample gain.
class Motion
{
public:
    enum class Type { off, pump4, pump8, gate8, gate16, trance };

    void prepare (double newSr) { sr = newSr; smooth = 1; }

    float next (double beat, Type type, float depth)
    {
        float target = 1;
        switch (type)
        {
            case Type::off: break;
            case Type::pump4:
            case Type::pump8:
            {
                auto len = type == Type::pump4 ? 1.0 : 0.5;
                auto ph = (float) (std::fmod (beat, len) / len);
                // quick duck at the beat, smooth recovery by ~60% of the period
                auto shape = ph < 0.03f ? 1.0f - ph / 0.03f : std::pow (juce::jmin (1.0f, (ph - 0.03f) / 0.6f), 1.5f);
                target = 1.0f - depth * (1.0f - shape);
                break;
            }
            case Type::gate8:
            case Type::gate16:
            case Type::trance:
            {
                static constexpr bool pattern[16] = { 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0 };
                auto len = type == Type::gate8 ? 0.5 : 0.25;
                auto stepIdx = (long long) std::floor (beat / len);
                auto ph = std::fmod (beat, len) / len;
                auto open = ph < 0.6 && (type != Type::trance || pattern[stepIdx & 15]);
                target = open ? 1.0f : 1.0f - depth;
                break;
            }
        }
        // ~1 ms attack, ~12 ms release for the gate; keeps edges click-free
        auto coef = target > smooth ? attackCoef() : releaseCoef();
        smooth = target + coef * (smooth - target);
        return smooth;
    }

private:
    float attackCoef() const { return std::exp (-1.0f / (0.0015f * (float) sr)); }
    float releaseCoef() const { return std::exp (-1.0f / (0.012f * (float) sr)); }
    double sr = 44100;
    float smooth = 1;
};
} // namespace syn
