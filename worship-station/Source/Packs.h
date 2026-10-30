#pragma once
// Worship Station hosts the Worship Keys and Anthem Synth processors unchanged, straight from their own
// source folders. Both projects define global names like Preset / getPresets() / createPluginFilter(),
// so each one is compiled inside its own namespace (keyspack::, synthpack::). Every system header they
// use is included here first, so their own #includes are no-ops inside the namespace.
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>
#include "../../worship-keys/Source/HardwareLook.h"

namespace keyspack
{
#include "../../worship-keys/Source/PluginProcessor.h"
}

namespace synthpack
{
#include "../../anthem-synth/Source/PluginProcessor.h"
}
