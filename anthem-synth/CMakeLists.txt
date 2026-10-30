cmake_minimum_required(VERSION 3.22)
project(AnthemSynth VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Tip: configure with -DFETCHCONTENT_SOURCE_DIR_JUCE=<path to an existing JUCE checkout> to skip the download.
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------- plugin
juce_add_plugin(AnthemSynth
    COMPANY_NAME "Robert"
    PLUGIN_MANUFACTURER_CODE Rbpa
    PLUGIN_CODE AnSy
    FORMATS VST3 Standalone
    PRODUCT_NAME "Anthem Synth"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth)

set(AS_DEFINES
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_sources(AnthemSynth PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(AnthemSynth PUBLIC ${AS_DEFINES})
target_link_libraries(AnthemSynth
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------- offline renderer / test / demo
juce_add_console_app(AnthemRender PRODUCT_NAME "AnthemRender")
target_sources(AnthemRender PRIVATE Tools/Render.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(AnthemRender PRIVATE ${AS_DEFINES})
target_link_libraries(AnthemRender
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
