#pragma once
#include "../Packs.h"

namespace orch
{
// Factory presets. Anything not listed falls back to its default (see createLayout()).
// Generic names only - voiced for up-tempo praise: punchy stabs, big unison hits, swelling held chords.
struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;
};

inline const std::vector<Preset>& getPresets()
{
    static const std::vector<Preset> presets {
        { "Praise Stabs",
          { { "artic", 2 }, { "holdTime", 0.2f }, { "susAttack", 0.3f }, { "release", 0.6f },
            { "strings", 0 }, { "brass", -1.5f }, { "lowEnd", -4 }, { "timpani", -6 }, { "hits", -8 },
            { "octUp", 0.25f }, { "octDown", 0.7f }, { "split", 55 }, { "accent", 118 }, { "ensemble", 0.55f },
            { "comp", 0.45f }, { "revMix", 0.24f }, { "revDecay", 2.6f }, { "eqPresence", 1.0f } } },

        { "Brass Hits",
          { { "artic", 0 }, { "strings", -60 }, { "brass", 0 }, { "lowEnd", -3 }, { "timpani", -5 }, { "hits", -7 },
            { "octUp", 0 }, { "octDown", 1.0f }, { "split", 55 }, { "accent", 116 }, { "ensemble", 0.7f },
            { "comp", 0.5f }, { "drive", 0.18f }, { "revMix", 0.2f }, { "revDecay", 2.2f }, { "eqPresence", 1.5f } } },

        { "String Stabs",
          { { "artic", 0 }, { "strings", 0 }, { "brass", -60 }, { "lowEnd", -4 }, { "timpani", -60 }, { "hits", -60 },
            { "octUp", 0.5f }, { "octDown", 0.8f }, { "split", 50 }, { "ensemble", 0.6f },
            { "comp", 0.4f }, { "revMix", 0.26f }, { "revDecay", 2.4f } } },

        { "Full Orchestra Hit",
          { { "artic", 3 }, { "susAttack", 0.03f }, { "release", 0.5f },
            { "strings", 0 }, { "brass", 0 }, { "lowEnd", -2 }, { "timpani", -3 }, { "hits", -5 },
            { "octUp", 0.5f }, { "octDown", 1.0f }, { "split", 55 }, { "accent", 100 }, { "ensemble", 0.8f },
            { "comp", 0.6f }, { "drive", 0.2f }, { "revMix", 0.28f }, { "revDecay", 3.0f }, { "eqLow", 1.5f }, { "eqPresence", 1.0f },
            { "master", -2 } } },

        { "Celebration Hits",
          { { "artic", 0 }, { "strings", -1 }, { "brass", 0 }, { "lowEnd", -3 }, { "timpani", -4 }, { "hits", -3 },
            { "octUp", 0.4f }, { "octDown", 0.9f }, { "split", 55 }, { "accent", 104 }, { "ensemble", 0.7f },
            { "comp", 0.55f }, { "drive", 0.15f }, { "revMix", 0.22f }, { "revDecay", 2.4f }, { "eqPresence", 1.5f } } },

        { "Big Marcato",
          { { "artic", 3 }, { "susAttack", 0.05f }, { "release", 0.8f },
            { "strings", 0 }, { "brass", -1 }, { "lowEnd", -3 }, { "timpani", -5 }, { "hits", -8 },
            { "octUp", 0.6f }, { "octDown", 1.0f }, { "split", 55 }, { "accent", 120 }, { "ensemble", 1.0f },
            { "comp", 0.5f }, { "revMix", 0.3f }, { "revDecay", 3.2f } } },

        { "Anthem Swell",
          { { "artic", 1 }, { "susAttack", 0.6f }, { "release", 1.2f },
            { "strings", 0 }, { "brass", -3 }, { "lowEnd", -4 }, { "timpani", -60 }, { "hits", -60 },
            { "octUp", 0.4f }, { "octDown", 0.8f }, { "split", 52 }, { "ensemble", 0.8f },
            { "comp", 0.35f }, { "revMix", 0.34f }, { "revDecay", 4.0f }, { "revSize", 0.85f } } },

        { "Strings Pad",
          { { "artic", 1 }, { "susAttack", 0.45f }, { "release", 1.5f },
            { "strings", 0 }, { "brass", -60 }, { "lowEnd", -6 }, { "timpani", -60 }, { "hits", -60 },
            { "octUp", 0.3f }, { "octDown", 0.7f }, { "split", 48 }, { "ensemble", 0.7f },
            { "comp", 0.3f }, { "revMix", 0.36f }, { "revDecay", 4.5f }, { "revSize", 0.85f }, { "eqAir", 1.0f } } },

        { "Brass Section Swell",
          { { "artic", 1 }, { "susAttack", 0.25f }, { "release", 0.7f },
            { "strings", -60 }, { "brass", 0 }, { "lowEnd", -4 }, { "timpani", -60 }, { "hits", -60 },
            { "octDown", 0.9f }, { "split", 52 }, { "ensemble", 0.7f },
            { "comp", 0.4f }, { "revMix", 0.28f }, { "revDecay", 3.0f } } },

        { "Timpani + Low Strings",
          { { "artic", 2 }, { "holdTime", 0.25f }, { "susAttack", 0.35f },
            { "strings", -2 }, { "brass", -8 }, { "lowEnd", 0 }, { "timpani", 0 }, { "hits", -6 },
            { "octUp", 0 }, { "octDown", 0.8f }, { "split", 60 }, { "accent", 112 }, { "ensemble", 0.6f },
            { "comp", 0.45f }, { "revMix", 0.28f }, { "revDecay", 3.0f }, { "eqLow", 2.0f } } },
    };
    return presets;
}
} // namespace orch
