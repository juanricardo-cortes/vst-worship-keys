#pragma once
#include <juce_core/juce_core.h>
#include <utility>
#include <vector>

// Factory presets. Any parameter not listed falls back to its default (see createLayout()).
// "bpm" and "master" are never touched by presets.
// Voicings are educated estimates of each band's synth sound (from live videos and records),
// not copies of their actual patches.
//
// Choice indices:
//   oscAWave 0 Supersaw 1 Saw 2 Square 3 Triangle     oscBWave 0 Saw 1 Square 2 Triangle 3 Sine
//   voiceMode 0 Poly 1 Mono 2 Legato
//   motion 0 Off 1 Pump 1/4 2 Pump 1/8 3 Gate 1/8 4 Gate 1/16 5 Trance Gate
//   arpMode 0 Off 1 Up 2 Down 3 Up/Down 4 Random 5 Chord     arpRate 0 1/4 1 1/8 2 1/8 dot 3 1/16 4 1/32
//   delayTime 0 1/4 1 1/8 dot 2 1/8 3 1/4 dot 4 1/2 5 1/16
struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;
};

inline const std::vector<Preset>& getPresets()
{
    static const std::vector<Preset> presets {
        //---------------------------------------------------------------- Elevation Worship
        // Warm wide supersaw bed with an octave layer, slow swell, huge shimmer hall.
        { "Elevation - Ambient Pad",
          { { "level", -1 }, { "oscAWave", 0 }, { "detune", 0.22f }, { "oscALevel", 0.8f },
            { "oscBWave", 0 }, { "oscBOct", 1 }, { "oscBFine", 5 }, { "oscBLevel", 0.22f }, { "width", 0.95f },
            { "cutoff", 1400 }, { "reso", 0.12f }, { "fltEnv", 0.15f }, { "keyTrack", 0.4f }, { "fltVel", 0.1f },
            { "fAtk", 1.6f }, { "fDec", 3.5f }, { "fSus", 0.7f }, { "fRel", 3.0f },
            { "aAtk", 1.2f }, { "aDec", 2.0f }, { "aSus", 1.0f }, { "aRel", 3.5f }, { "velSens", 0.2f },
            { "lfoRate", 0.2f }, { "lfoFilter", 0.08f },
            { "lowCut", 120 }, { "bright", 0.5f },
            { "chorusMix", 0.4f }, { "chorusRate", 0.35f }, { "chorusDepth", 0.55f },
            { "delayMix", 0.12f }, { "delayFeedback", 0.4f }, { "delayTime", 1 },
            { "revMix", 0.45f }, { "revDecay", 7.0f }, { "revSize", 0.95f }, { "revTone", 6500 }, { "shimmer", 0.35f } } },

        // The pulsing 16th-note pad that drives Elevation's builds; delay fills the gaps.
        { "Elevation - Pulse Pad",
          { { "oscAWave", 0 }, { "detune", 0.28f }, { "oscALevel", 0.8f },
            { "oscBWave", 1 }, { "oscBOct", -1 }, { "oscBLevel", 0.3f }, { "pw", 0.3f }, { "width", 0.85f },
            { "cutoff", 2200 }, { "reso", 0.15f }, { "fltEnv", 0.1f }, { "keyTrack", 0.4f },
            { "fAtk", 0.5f }, { "fDec", 2.0f }, { "fSus", 0.6f }, { "fRel", 1.5f },
            { "aAtk", 0.25f }, { "aDec", 1.0f }, { "aSus", 1.0f }, { "aRel", 1.6f }, { "velSens", 0.25f },
            { "motion", 4 }, { "motionDepth", 0.85f },
            { "lowCut", 110 }, { "bright", 1.0f },
            { "chorusMix", 0.3f }, { "chorusRate", 0.4f }, { "chorusDepth", 0.5f },
            { "delayMix", 0.2f }, { "delayFeedback", 0.3f }, { "delayTime", 1 },
            { "revMix", 0.35f }, { "revDecay", 4.5f }, { "revSize", 0.85f }, { "revTone", 7000 }, { "shimmer", 0.15f } } },

        // Singing supersaw lead with a sub-octave square, legato glide and delayed vibrato-style LFO.
        { "Elevation - Soaring Lead",
          { { "level", 3 }, { "oscAWave", 0 }, { "detune", 0.3f }, { "oscALevel", 0.85f },
            { "oscBWave", 1 }, { "oscBOct", -1 }, { "oscBLevel", 0.3f }, { "width", 0.6f },
            { "cutoff", 3200 }, { "reso", 0.2f }, { "fltEnv", 0.25f }, { "keyTrack", 0.6f }, { "fltVel", 0.25f },
            { "fAtk", 0.01f }, { "fDec", 0.8f }, { "fSus", 0.5f }, { "fRel", 0.6f },
            { "aAtk", 0.02f }, { "aDec", 0.5f }, { "aSus", 1.0f }, { "aRel", 0.6f }, { "velSens", 0.35f },
            { "lfoRate", 5.5f }, { "lfoPitch", 0.22f },
            { "voiceMode", 2 }, { "glide", 0.08f },
            { "lowCut", 90 }, { "bright", 1.5f },
            { "chorusMix", 0.15f },
            { "delayMix", 0.25f }, { "delayFeedback", 0.45f }, { "delayTime", 1 },
            { "revMix", 0.3f }, { "revDecay", 4.0f }, { "revSize", 0.8f }, { "revTone", 7500 }, { "shimmer", 0.1f } } },

        // Moog-style mono bass: round, a little growl on the attack, very short glide.
        { "Elevation - Moog Bass",
          { { "oscAWave", 1 }, { "oscAOct", -1 }, { "oscALevel", 0.8f },
            { "oscBWave", 1 }, { "oscBOct", -1 }, { "oscBFine", -6 }, { "oscBLevel", 0.5f }, { "subLevel", 0.6f }, { "width", 0.0f },
            { "cutoff", 380 }, { "reso", 0.3f }, { "fltEnv", 0.45f }, { "keyTrack", 0.3f }, { "fltVel", 0.3f }, { "fltDrive", 0.4f },
            { "fAtk", 0.002f }, { "fDec", 0.35f }, { "fSus", 0.3f }, { "fRel", 0.2f },
            { "aAtk", 0.002f }, { "aDec", 0.4f }, { "aSus", 1.0f }, { "aRel", 0.12f }, { "velSens", 0.4f },
            { "voiceMode", 1 }, { "glide", 0.03f },
            { "lowCut", 25 }, { "chorusMix", 0.0f }, { "delayMix", 0.0f },
            { "revMix", 0.05f }, { "revDecay", 1.2f }, { "revSize", 0.4f } } },

        //---------------------------------------------------------------- Hillsong
        // Jupiter-style two-saw "Analog V" pad: slow filter bloom, rich chorus.
        { "Hillsong - Analog V Pad",
          { { "level", -2 }, { "oscAWave", 1 }, { "oscALevel", 0.75f },
            { "oscBWave", 0 }, { "oscBFine", 9 }, { "oscBLevel", 0.75f }, { "width", 0.8f },
            { "cutoff", 900 }, { "reso", 0.25f }, { "fltEnv", 0.35f }, { "keyTrack", 0.5f }, { "fltVel", 0.15f },
            { "fAtk", 2.5f }, { "fDec", 4.0f }, { "fSus", 0.5f }, { "fRel", 3.0f },
            { "aAtk", 0.9f }, { "aDec", 2.0f }, { "aSus", 1.0f }, { "aRel", 3.0f }, { "velSens", 0.25f },
            { "lowCut", 100 },
            { "chorusMix", 0.6f }, { "chorusRate", 0.5f }, { "chorusDepth", 0.6f },
            { "delayMix", 0.1f }, { "delayFeedback", 0.35f }, { "delayTime", 1 },
            { "revMix", 0.4f }, { "revDecay", 6.0f }, { "revSize", 0.9f }, { "revTone", 6000 }, { "shimmer", 0.1f } } },

        // United-style plucks that bloom into dotted-eighth echoes.
        { "Hillsong - United Pluck",
          { { "level", 10 }, { "oscAWave", 0 }, { "detune", 0.18f }, { "oscALevel", 0.85f }, { "width", 0.7f },
            { "cutoff", 500 }, { "reso", 0.25f }, { "fltEnv", 0.7f }, { "keyTrack", 0.6f }, { "fltVel", 0.35f },
            { "fAtk", 0.001f }, { "fDec", 0.28f }, { "fSus", 0.0f }, { "fRel", 0.25f },
            { "aAtk", 0.001f }, { "aDec", 0.6f }, { "aSus", 0.0f }, { "aRel", 0.35f }, { "velSens", 0.6f },
            { "lowCut", 120 }, { "bright", 1.0f },
            { "chorusMix", 0.2f },
            { "delayMix", 0.35f }, { "delayFeedback", 0.5f }, { "delayTime", 1 },
            { "revMix", 0.3f }, { "revDecay", 4.0f }, { "revSize", 0.85f }, { "revTone", 7500 }, { "shimmer", 0.05f } } },

        // Young & Free: bright pop supersaw chords pumping on the quarter note.
        { "Hillsong - Young & Free Pump",
          { { "oscAWave", 0 }, { "detune", 0.35f }, { "oscALevel", 0.8f },
            { "oscBWave", 0 }, { "oscBOct", 1 }, { "oscBLevel", 0.3f }, { "width", 0.9f },
            { "cutoff", 5000 }, { "reso", 0.1f }, { "fltEnv", 0.1f }, { "keyTrack", 0.4f },
            { "aAtk", 0.01f }, { "aDec", 0.5f }, { "aSus", 1.0f }, { "aRel", 0.5f }, { "velSens", 0.3f },
            { "motion", 1 }, { "motionDepth", 0.8f },
            { "lowCut", 130 }, { "bright", 2.0f },
            { "chorusMix", 0.25f },
            { "delayMix", 0.1f }, { "delayFeedback", 0.3f }, { "delayTime", 2 },
            { "revMix", 0.3f }, { "revDecay", 3.0f }, { "revSize", 0.8f }, { "revTone", 8000 } } },

        // Soft string-machine swell for spontaneous moments; long shimmer tail.
        { "Hillsong - Oceans Swell",
          { { "oscAWave", 1 }, { "oscALevel", 0.6f },
            { "oscBWave", 1 }, { "oscBFine", -7 }, { "oscBLevel", 0.5f }, { "pw", 0.4f }, { "lfoPwm", 0.3f }, { "width", 0.9f },
            { "cutoff", 1800 }, { "reso", 0.1f }, { "fltEnv", 0.0f }, { "keyTrack", 0.5f },
            { "aAtk", 2.5f }, { "aDec", 2.0f }, { "aSus", 1.0f }, { "aRel", 4.0f }, { "velSens", 0.15f },
            { "lfoRate", 0.5f },
            { "lowCut", 140 },
            { "chorusMix", 0.5f }, { "chorusRate", 0.3f }, { "chorusDepth", 0.6f },
            { "delayMix", 0.12f }, { "delayFeedback", 0.45f }, { "delayTime", 3 },
            { "revMix", 0.55f }, { "revDecay", 9.0f }, { "revSize", 1.0f }, { "revTone", 6000 }, { "shimmer", 0.45f } } },

        //---------------------------------------------------------------- Planetshakers
        // "This Is Our Time": huge stacked supersaw chords, sub underneath, pumping on every beat.
        { "Planetshakers - Our Time Anthem",
          { { "oscAWave", 0 }, { "detune", 0.45f }, { "oscALevel", 0.85f },
            { "oscBWave", 0 }, { "oscBOct", -1 }, { "oscBLevel", 0.45f }, { "subLevel", 0.15f }, { "width", 1.0f },
            { "cutoff", 7000 }, { "reso", 0.05f }, { "fltEnv", 0.1f }, { "keyTrack", 0.3f },
            { "aAtk", 0.005f }, { "aDec", 0.5f }, { "aSus", 1.0f }, { "aRel", 0.35f }, { "velSens", 0.25f },
            { "motion", 1 }, { "motionDepth", 0.9f },
            { "lowCut", 60 }, { "bright", 3.0f },
            { "chorusMix", 0.2f },
            { "delayMix", 0.12f }, { "delayFeedback", 0.3f }, { "delayTime", 2 },
            { "revMix", 0.25f }, { "revDecay", 2.5f }, { "revSize", 0.8f }, { "revTone", 9000 } } },

        // "This Is Our Time": bright supersaw pluck for the hook line, 1/8 echoes.
        { "Planetshakers - Our Time Pluck",
          { { "level", 9 }, { "oscAWave", 0 }, { "detune", 0.3f }, { "oscALevel", 0.85f },
            { "oscBWave", 1 }, { "oscBOct", 1 }, { "oscBLevel", 0.2f }, { "width", 0.8f },
            { "cutoff", 900 }, { "reso", 0.2f }, { "fltEnv", 0.65f }, { "keyTrack", 0.6f }, { "fltVel", 0.3f },
            { "fAtk", 0.001f }, { "fDec", 0.22f }, { "fSus", 0.15f }, { "fRel", 0.2f },
            { "aAtk", 0.001f }, { "aDec", 0.35f }, { "aSus", 0.35f }, { "aRel", 0.2f }, { "velSens", 0.4f },
            { "lowCut", 150 }, { "bright", 2.5f },
            { "chorusMix", 0.15f },
            { "delayMix", 0.28f }, { "delayFeedback", 0.35f }, { "delayTime", 2 },
            { "revMix", 0.22f }, { "revDecay", 2.2f }, { "revSize", 0.7f }, { "revTone", 9000 } } },

        // "Turn It Up": aggressive detuned saw lead, octave layer, drive, quick glide.
        { "Planetshakers - Turn It Up Lead",
          { { "level", 1 }, { "oscAWave", 0 }, { "detune", 0.4f }, { "oscALevel", 0.85f },
            { "oscBWave", 0 }, { "oscBOct", 1 }, { "oscBFine", -8 }, { "oscBLevel", 0.4f }, { "width", 0.7f },
            { "cutoff", 4500 }, { "reso", 0.22f }, { "fltEnv", 0.3f }, { "keyTrack", 0.5f }, { "fltVel", 0.2f }, { "fltDrive", 0.55f },
            { "fAtk", 0.001f }, { "fDec", 0.3f }, { "fSus", 0.6f }, { "fRel", 0.3f },
            { "aAtk", 0.003f }, { "aDec", 0.3f }, { "aSus", 1.0f }, { "aRel", 0.25f }, { "velSens", 0.2f },
            { "lfoRate", 6.0f }, { "lfoPitch", 0.1f },
            { "voiceMode", 2 }, { "glide", 0.05f },
            { "lowCut", 140 }, { "bright", 2.5f },
            { "chorusMix", 0.1f },
            { "delayMix", 0.2f }, { "delayFeedback", 0.3f }, { "delayTime", 1 },
            { "revMix", 0.2f }, { "revDecay", 2.0f }, { "revSize", 0.7f }, { "revTone", 9000 } } },

        // "Turn It Up": rhythmic chord stabs. Hold a chord and the arp chops it in 1/8 notes.
        { "Planetshakers - Turn It Up Stabs",
          { { "level", 7 }, { "oscAWave", 0 }, { "detune", 0.32f }, { "oscALevel", 0.85f },
            { "oscBWave", 1 }, { "oscBOct", 0 }, { "oscBLevel", 0.25f }, { "width", 0.9f },
            { "cutoff", 2400 }, { "reso", 0.18f }, { "fltEnv", 0.4f }, { "keyTrack", 0.4f }, { "fltVel", 0.2f },
            { "fAtk", 0.001f }, { "fDec", 0.2f }, { "fSus", 0.3f }, { "fRel", 0.15f },
            { "aAtk", 0.002f }, { "aDec", 0.3f }, { "aSus", 0.5f }, { "aRel", 0.12f }, { "velSens", 0.3f },
            { "arpMode", 5 }, { "arpRate", 1 }, { "arpGate", 0.45f },
            { "motion", 1 }, { "motionDepth", 0.5f },
            { "lowCut", 150 }, { "bright", 2.0f },
            { "chorusMix", 0.15f },
            { "delayMix", 0.12f }, { "delayFeedback", 0.25f }, { "delayTime", 1 },
            { "revMix", 0.2f }, { "revDecay", 2.0f }, { "revSize", 0.75f }, { "revTone", 9000 } } },

        // Festival-style 1/16 arp across two octaves, pumping gently.
        { "Planetshakers - Festival Arp",
          { { "level", 11 }, { "oscAWave", 0 }, { "detune", 0.2f }, { "oscALevel", 0.85f }, { "width", 0.7f },
            { "cutoff", 1200 }, { "reso", 0.3f }, { "fltEnv", 0.5f }, { "keyTrack", 0.5f }, { "fltVel", 0.2f },
            { "fAtk", 0.001f }, { "fDec", 0.18f }, { "fSus", 0.1f }, { "fRel", 0.15f },
            { "aAtk", 0.001f }, { "aDec", 0.25f }, { "aSus", 0.3f }, { "aRel", 0.15f }, { "velSens", 0.3f },
            { "arpMode", 1 }, { "arpRate", 3 }, { "arpOct", 2 }, { "arpGate", 0.5f },
            { "motion", 1 }, { "motionDepth", 0.5f },
            { "lowCut", 160 }, { "bright", 2.0f },
            { "chorusMix", 0.15f },
            { "delayMix", 0.25f }, { "delayFeedback", 0.35f }, { "delayTime", 1 },
            { "revMix", 0.2f }, { "revDecay", 2.5f }, { "revSize", 0.75f }, { "revTone", 9000 } } },

        // Driving mono saw bass that ducks with the kick.
        { "Planetshakers - Drive Bass",
          { { "oscAWave", 1 }, { "oscAOct", -1 }, { "oscALevel", 0.8f },
            { "oscBWave", 1 }, { "oscBOct", -1 }, { "oscBFine", 8 }, { "oscBLevel", 0.6f }, { "subLevel", 0.5f }, { "width", 0.1f },
            { "cutoff", 700 }, { "reso", 0.25f }, { "fltEnv", 0.5f }, { "keyTrack", 0.3f }, { "fltDrive", 0.7f },
            { "fAtk", 0.001f }, { "fDec", 0.2f }, { "fSus", 0.35f }, { "fRel", 0.15f },
            { "aAtk", 0.002f }, { "aDec", 0.3f }, { "aSus", 1.0f }, { "aRel", 0.1f }, { "velSens", 0.3f },
            { "voiceMode", 1 }, { "glide", 0.02f },
            { "motion", 1 }, { "motionDepth", 0.6f },
            { "lowCut", 28 }, { "chorusMix", 0.0f }, { "delayMix", 0.0f },
            { "revMix", 0.04f }, { "revDecay", 1.0f }, { "revSize", 0.4f } } },

        //---------------------------------------------------------------- trance / orchestral
        // Big euphoric supersaw "string section": octave stack, slow-ish bow, ensemble chorus, huge hall.
        { "Trance Anthem Strings",
          { { "level", 0 }, { "oscAWave", 0 }, { "detune", 0.3f }, { "oscALevel", 0.8f },
            { "oscBWave", 0 }, { "oscBOct", 1 }, { "oscBFine", 6 }, { "oscBLevel", 0.32f }, { "subLevel", 0.15f }, { "width", 1.0f },
            { "cutoff", 2200 }, { "reso", 0.1f }, { "fltEnv", 0.25f }, { "keyTrack", 0.5f }, { "fltVel", 0.25f },
            { "fAtk", 0.45f }, { "fDec", 2.0f }, { "fSus", 0.6f }, { "fRel", 1.5f },
            { "aAtk", 0.22f }, { "aDec", 1.0f }, { "aSus", 1.0f }, { "aRel", 1.8f }, { "velSens", 0.3f },
            { "lfoRate", 5.2f }, { "lfoPitch", 0.03f },
            { "lowCut", 150 }, { "bright", 1.5f },
            { "chorusMix", 0.45f }, { "chorusRate", 0.4f }, { "chorusDepth", 0.6f },
            { "delayMix", 0.15f }, { "delayFeedback", 0.4f }, { "delayTime", 1 },
            { "revMix", 0.4f }, { "revDecay", 5.0f }, { "revSize", 0.95f }, { "revTone", 8000 }, { "shimmer", 0.1f } } },

        // The same wall of saws chopped by the trance gate. Hold chords; it locks to the song tempo.
        { "Trance Gate Chords",
          { { "level", 2 }, { "oscAWave", 0 }, { "detune", 0.32f }, { "oscALevel", 0.8f },
            { "oscBWave", 0 }, { "oscBOct", 1 }, { "oscBFine", 6 }, { "oscBLevel", 0.3f }, { "subLevel", 0.15f }, { "width", 1.0f },
            { "cutoff", 2800 }, { "reso", 0.12f }, { "fltEnv", 0.15f }, { "keyTrack", 0.5f }, { "fltVel", 0.2f },
            { "fAtk", 0.01f }, { "fDec", 1.0f }, { "fSus", 0.8f }, { "fRel", 0.6f },
            { "aAtk", 0.005f }, { "aDec", 0.5f }, { "aSus", 1.0f }, { "aRel", 0.5f }, { "velSens", 0.2f },
            { "motion", 5 }, { "motionDepth", 0.9f },
            { "lowCut", 160 }, { "bright", 2.0f },
            { "chorusMix", 0.35f }, { "chorusRate", 0.4f }, { "chorusDepth", 0.55f },
            { "delayMix", 0.22f }, { "delayFeedback", 0.35f }, { "delayTime", 1 },
            { "revMix", 0.32f }, { "revDecay", 3.5f }, { "revSize", 0.9f }, { "revTone", 9000 }, { "shimmer", 0.05f } } },

        // Soaring hands-in-the-air lead: wide detuned saws an octave apart, vibrato, dotted-eighth echoes.
        { "Euphoric Supersaw Lead",
          { { "level", 0 }, { "oscAWave", 0 }, { "detune", 0.45f }, { "oscALevel", 0.85f },
            { "oscBWave", 0 }, { "oscBOct", 1 }, { "oscBFine", -6 }, { "oscBLevel", 0.45f }, { "width", 0.85f },
            { "cutoff", 5000 }, { "reso", 0.2f }, { "fltEnv", 0.2f }, { "keyTrack", 0.5f }, { "fltVel", 0.2f }, { "fltDrive", 0.4f },
            { "fAtk", 0.001f }, { "fDec", 0.6f }, { "fSus", 0.7f }, { "fRel", 0.4f },
            { "aAtk", 0.01f }, { "aDec", 0.4f }, { "aSus", 1.0f }, { "aRel", 0.4f }, { "velSens", 0.2f },
            { "lfoRate", 5.5f }, { "lfoPitch", 0.08f },
            { "voiceMode", 2 }, { "glide", 0.06f },
            { "lowCut", 160 }, { "bright", 2.0f },
            { "chorusMix", 0.15f },
            { "delayMix", 0.3f }, { "delayFeedback", 0.42f }, { "delayTime", 1 },
            { "revMix", 0.32f }, { "revDecay", 3.5f }, { "revSize", 0.85f }, { "revTone", 9000 }, { "shimmer", 0.08f } } },

        // Synth brass section: the filter "blows" open on each note, harder playing = brassier.
        { "Epic Brass Saw",
          { { "level", -4 }, { "oscAWave", 1 }, { "oscALevel", 0.8f },
            { "oscBWave", 0 }, { "oscBFine", 9 }, { "oscBLevel", 0.7f }, { "subLevel", 0.2f }, { "width", 0.6f },
            { "cutoff", 650 }, { "reso", 0.15f }, { "fltEnv", 0.55f }, { "keyTrack", 0.6f }, { "fltVel", 0.5f }, { "fltDrive", 0.35f },
            { "fAtk", 0.12f }, { "fDec", 0.7f }, { "fSus", 0.55f }, { "fRel", 0.4f },
            { "aAtk", 0.03f }, { "aDec", 0.5f }, { "aSus", 1.0f }, { "aRel", 0.4f }, { "velSens", 0.5f },
            { "lfoRate", 5.0f }, { "lfoPitch", 0.02f },
            { "lowCut", 90 }, { "bright", 1.0f },
            { "chorusMix", 0.25f }, { "chorusRate", 0.5f }, { "chorusDepth", 0.5f },
            { "delayMix", 0.08f }, { "delayFeedback", 0.25f }, { "delayTime", 1 },
            { "revMix", 0.35f }, { "revDecay", 3.5f }, { "revSize", 0.9f }, { "revTone", 7000 } } },

        // Orchestral-hit style synth stab: saws plus a square an octave down, short and huge.
        { "Hybrid Orchestral Stabs",
          { { "level", 7 }, { "oscAWave", 0 }, { "detune", 0.3f }, { "oscALevel", 0.85f },
            { "oscBWave", 1 }, { "oscBOct", -1 }, { "oscBLevel", 0.35f }, { "subLevel", 0.2f }, { "width", 0.95f },
            { "cutoff", 900 }, { "reso", 0.2f }, { "fltEnv", 0.6f }, { "keyTrack", 0.5f }, { "fltVel", 0.35f },
            { "fAtk", 0.001f }, { "fDec", 0.35f }, { "fSus", 0.0f }, { "fRel", 0.3f },
            { "aAtk", 0.002f }, { "aDec", 0.55f }, { "aSus", 0.0f }, { "aRel", 0.35f }, { "velSens", 0.4f },
            { "lowCut", 70 }, { "bright", 1.5f },
            { "chorusMix", 0.2f },
            { "delayMix", 0.15f }, { "delayFeedback", 0.3f }, { "delayTime", 1 },
            { "revMix", 0.45f }, { "revDecay", 4.0f }, { "revSize", 0.95f }, { "revTone", 8000 }, { "shimmer", 0.05f } } },

        // Classic uplifting 1/16 arp over two octaves. Hold a chord.
        { "Uplifting Trance Arp",
          { { "level", 9 }, { "oscAWave", 0 }, { "detune", 0.18f }, { "oscALevel", 0.85f },
            { "oscBWave", 1 }, { "oscBOct", 1 }, { "oscBLevel", 0.15f }, { "width", 0.8f },
            { "cutoff", 1500 }, { "reso", 0.25f }, { "fltEnv", 0.5f }, { "keyTrack", 0.5f }, { "fltVel", 0.2f },
            { "fAtk", 0.001f }, { "fDec", 0.2f }, { "fSus", 0.15f }, { "fRel", 0.15f },
            { "aAtk", 0.001f }, { "aDec", 0.25f }, { "aSus", 0.4f }, { "aRel", 0.18f }, { "velSens", 0.3f },
            { "arpMode", 1 }, { "arpRate", 3 }, { "arpOct", 2 }, { "arpGate", 0.55f },
            { "lowCut", 170 }, { "bright", 2.0f },
            { "chorusMix", 0.2f },
            { "delayMix", 0.3f }, { "delayFeedback", 0.4f }, { "delayTime", 1 },
            { "revMix", 0.32f }, { "revDecay", 3.0f }, { "revSize", 0.85f }, { "revTone", 9000 } } },

        //---------------------------------------------------------------- utility
        // Hold a chord for 8+ bars: noise and saws open up slowly into a build.
        { "Build-Up Riser",
          { { "level", 6 }, { "oscAWave", 0 }, { "detune", 0.5f }, { "oscALevel", 0.6f }, { "noise", 0.7f }, { "width", 1.0f },
            { "cutoff", 250 }, { "reso", 0.35f }, { "fltEnv", 0.85f }, { "keyTrack", 0.2f },
            { "fAtk", 8.0f }, { "fDec", 1.0f }, { "fSus", 1.0f }, { "fRel", 0.8f },
            { "aAtk", 3.0f }, { "aDec", 1.0f }, { "aSus", 1.0f }, { "aRel", 1.2f }, { "velSens", 0.0f },
            { "motion", 4 }, { "motionDepth", 0.35f },
            { "lowCut", 200 }, { "bright", 2.0f },
            { "chorusMix", 0.3f },
            { "delayMix", 0.15f }, { "delayFeedback", 0.4f }, { "delayTime", 1 },
            { "revMix", 0.45f }, { "revDecay", 5.0f }, { "revSize", 0.95f }, { "revTone", 9000 }, { "shimmer", 0.2f } } },

        // Prayer / altar moments: soft triangle + sine cloud drowned in shimmer.
        { "Prayer - Shimmer Cloud",
          { { "level", -2 }, { "oscAWave", 3 }, { "oscALevel", 0.8f },
            { "oscBWave", 3 }, { "oscBOct", 1 }, { "oscBFine", 4 }, { "oscBLevel", 0.4f }, { "width", 1.0f },
            { "cutoff", 3000 }, { "reso", 0.05f }, { "fltEnv", 0.0f },
            { "aAtk", 3.5f }, { "aDec", 2.0f }, { "aSus", 1.0f }, { "aRel", 6.0f }, { "velSens", 0.1f },
            { "lfoRate", 0.3f }, { "lfoPitch", 0.04f },
            { "lowCut", 120 },
            { "chorusMix", 0.5f }, { "chorusRate", 0.3f }, { "chorusDepth", 0.7f },
            { "delayMix", 0.2f }, { "delayFeedback", 0.5f }, { "delayTime", 3 },
            { "revMix", 0.7f }, { "revDecay", 14.0f }, { "revSize", 1.0f }, { "revTone", 6500 }, { "shimmer", 0.65f } } },

        // Starting point: one saw, open filter, no effects.
        { "Init - Basic Saw",
          { { "oscAWave", 1 }, { "oscBLevel", 0 }, { "cutoff", 8000 }, { "reso", 0 }, { "fltEnv", 0 },
            { "chorusMix", 0 }, { "delayMix", 0 }, { "revMix", 0 }, { "shimmer", 0 } } },
    };
    return presets;
}
