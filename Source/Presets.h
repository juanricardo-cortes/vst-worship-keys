#pragma once
#include <juce_core/juce_core.h>
#include <initializer_list>
#include <utility>
#include <vector>

// Factory presets. Any parameter not listed falls back to its default (see createLayout()).
// Voicings are educated estimates from rig rundowns / worship-keys practice, not measured patches.
struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;
};

inline const std::vector<Preset>& getPresets()
{
    static const std::vector<Preset> presets {
        { "Elevation - Big Room Grand",
          { { "pianoLevel", 0 }, { "pianoRelease", 0.45f }, { "velCurve", 0.85f },
            { "padLevel", -10 }, { "padAttack", 1.2f }, { "padRelease", 4.0f }, { "padCutoff", 4000 }, { "padReso", 0.15f },
            { "padDetune", 0.45f }, { "padMotion", 0.3f },
            { "hpf", 80 }, { "eqLow", 1.0f }, { "eqLowMid", -3 }, { "eqPresence", 3.5f }, { "eqAir", 2.5f },
            { "comp", 0.65f }, { "drive", 0.15f },
            { "chorusMix", 0.1f }, { "chorusRate", 0.3f },
            { "delayMix", 0.14f }, { "delayFeedback", 0.3f }, { "delayTime", 1 },
            { "revMix", 0.32f }, { "revDecay", 3.8f }, { "revSize", 0.8f }, { "revPreDelay", 40 }, { "revTone", 7000 }, { "shimmer", 0.25f },
            { "master", -2 } } },

        { "Hillsong - Bright & Compressed",
          { { "pianoLevel", 0 }, { "pianoRelease", 0.4f }, { "velCurve", 0.8f },
            { "padLevel", -11 }, { "padAttack", 1.5f }, { "padRelease", 4.5f }, { "padCutoff", 3000 }, { "padReso", 0.1f },
            { "padDetune", 0.55f }, { "padMotion", 0.2f },
            { "hpf", 90 }, { "eqLow", 0 }, { "eqLowMid", -3 }, { "eqPresence", 3 }, { "eqAir", 3 },
            { "comp", 0.6f }, { "drive", 0.08f },
            { "chorusMix", 0.25f }, { "chorusRate", 0.45f },
            { "delayMix", 0.1f }, { "delayFeedback", 0.25f }, { "delayTime", 1 },
            { "revMix", 0.3f }, { "revDecay", 3.5f }, { "revSize", 0.75f }, { "revPreDelay", 30 }, { "revTone", 8000 }, { "shimmer", 0.3f },
            { "master", -2 } } },

        { "Bethel - Intimate Felt",
          { { "pianoLevel", 0 }, { "pianoRelease", 0.6f }, { "velCurve", 1.25f },
            { "padLevel", -14 }, { "padAttack", 2.5f }, { "padRelease", 6.0f }, { "padCutoff", 1800 }, { "padReso", 0.05f },
            { "padDetune", 0.3f }, { "padMotion", 0.2f },
            { "hpf", 60 }, { "eqLow", 1.5f }, { "eqLowMid", -1 }, { "eqPresence", -1 }, { "eqAir", -3 },
            { "comp", 0.25f }, { "drive", 0.12f },
            { "chorusMix", 0.1f }, { "chorusRate", 0.25f },
            { "delayMix", 0.06f }, { "delayFeedback", 0.2f }, { "delayTime", 1 },
            { "revMix", 0.38f }, { "revDecay", 5.5f }, { "revSize", 0.9f }, { "revPreDelay", 50 }, { "revTone", 4500 }, { "shimmer", 0.1f },
            { "master", 0 } } },

        { "Nord Deep Grand (FL chain)",
          { { "pianoLevel", 0 }, { "pianoRelease", 0.4f }, { "velCurve", 0.85f },
            { "padLevel", -60 }, { "epLevel", -60 },
            { "hpf", 30 }, { "eqLow", 3.5f }, { "eqLowMid", 2 }, { "eqPresence", 1 }, { "eqAir", -3 },
            { "comp", 0.3f }, { "drive", 0.25f },
            { "chorusMix", 0.25f }, { "chorusRate", 0.3f },
            { "delayMix", 0 },
            { "revMix", 0.25f }, { "revDecay", 2.2f }, { "revSize", 0.65f }, { "revPreDelay", 15 }, { "revTone", 3500 }, { "shimmer", 0 },
            { "master", -1 } } },

        { "Ambient Shimmer Swell",
          { { "pianoLevel", -5 }, { "pianoRelease", 0.8f }, { "velCurve", 1.0f },
            { "padLevel", -4 }, { "padAttack", 3.0f }, { "padRelease", 7.0f }, { "padCutoff", 2200 }, { "padReso", 0.2f },
            { "padDetune", 0.6f }, { "padMotion", 0.5f }, { "padOctave", 1 },
            { "hpf", 100 }, { "eqLowMid", -3 }, { "eqPresence", 1 }, { "eqAir", 2 },
            { "comp", 0.4f }, { "drive", 0.05f },
            { "chorusMix", 0.35f }, { "chorusRate", 0.2f },
            { "delayMix", 0.2f }, { "delayFeedback", 0.45f }, { "delayTime", 1 },
            { "revMix", 0.5f }, { "revDecay", 9.0f }, { "revSize", 1.0f }, { "revPreDelay", 60 }, { "revTone", 6000 }, { "shimmer", 0.6f },
            { "master", -3 } } },

        { "DX Worship EP",
          { { "pianoLevel", -60 }, { "epLevel", 0 }, { "epBell", 0.6f }, { "epBark", 0.35f }, { "epDecay", 1.0f }, { "velCurve", 0.9f },
            { "padLevel", -16 }, { "padAttack", 1.5f }, { "padCutoff", 2500 },
            { "hpf", 70 }, { "eqLowMid", -2 }, { "eqPresence", 1.5f }, { "eqAir", 1.5f },
            { "comp", 0.45f }, { "drive", 0.1f },
            { "chorusMix", 0.45f }, { "chorusRate", 0.6f },
            { "delayMix", 0.12f }, { "delayFeedback", 0.3f }, { "delayTime", 1 },
            { "revMix", 0.28f }, { "revDecay", 2.5f }, { "revSize", 0.6f }, { "revPreDelay", 20 }, { "revTone", 6500 }, { "shimmer", 0.1f },
            { "master", -1 } } },

        { "Grand + EP Layer",
          { { "pianoLevel", 0 }, { "epLevel", -9 }, { "epBell", 0.5f }, { "epBark", 0.2f }, { "velCurve", 0.85f },
            { "padLevel", -14 }, { "padCutoff", 2800 },
            { "hpf", 80 }, { "eqLowMid", -3 }, { "eqPresence", 2.5f }, { "eqAir", 2 },
            { "comp", 0.55f }, { "drive", 0.1f },
            { "chorusMix", 0.2f }, { "chorusRate", 0.4f },
            { "delayMix", 0.08f }, { "delayTime", 1 },
            { "revMix", 0.3f }, { "revDecay", 3.2f }, { "revSize", 0.75f }, { "revPreDelay", 30 }, { "revTone", 7000 }, { "shimmer", 0.15f },
            { "master", -2 } } },

        { "Dry Grand",
          { { "pianoLevel", 0 }, { "epLevel", -60 }, { "padLevel", -60 },
            { "hpf", 20 }, { "eqLow", 0 }, { "eqLowMid", 0 }, { "eqPresence", 0 }, { "eqAir", 0 },
            { "comp", 0 }, { "drive", 0 }, { "chorusMix", 0 }, { "delayMix", 0 }, { "revMix", 0 }, { "shimmer", 0 },
            { "master", 0 } } },
    };
    return presets;
}
