cmake_minimum_required(VERSION 3.22)
project(WorshipStation VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static MSVC runtime: the exe / VST3 run on PCs without the VC++ Redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Reuses the JUCE checkout from worship-keys if it's there (configure with -DFETCHCONTENT_SOURCE_DIR_JUCE=...).
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

# The Keys and Synth packs are compiled straight from their own projects (see Source/Packs.h).
set(KEYS_DIR "${CMAKE_SOURCE_DIR}/../worship-keys")
set(SYNTH_DIR "${CMAKE_SOURCE_DIR}/../anthem-synth")

# iPhone build = standalone app only (configure with -G Xcode -DCMAKE_SYSTEM_NAME=iOS; see .github/workflows/ios.yml).
if (IOS)
    set(WS_FORMATS Standalone)
else()
    set(WS_FORMATS VST3 Standalone)
endif()

juce_add_plugin(WorshipStation
    COMPANY_NAME "Robert"
    PLUGIN_MANUFACTURER_CODE Rbpa
    PLUGIN_CODE WsSt
    FORMATS ${WS_FORMATS}
    BUNDLE_ID com.robert.worshipstation
    IPHONE_SCREEN_ORIENTATIONS UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight
    IPAD_SCREEN_ORIENTATIONS UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight
    BACKGROUND_AUDIO_ENABLED TRUE
    REQUIRES_FULL_SCREEN TRUE
    STATUS_BAR_HIDDEN TRUE
    PRODUCT_NAME "Worship Station"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth)

set(WS_CORE_SOURCES
    Source/Packs/KeysProcessor.cpp
    Source/Packs/KeysEditor.cpp
    Source/Packs/KeysSampleBank.cpp
    Source/Packs/SynthProcessor.cpp
    Source/Packs/SynthEditor.cpp
    Source/Orchestra/OrchSampleBank.cpp
    Source/Orchestra/OrchProcessor.cpp
    Source/Orchestra/OrchEditor.cpp
    Source/StationProcessor.cpp
    Source/StationEditor.cpp)

set(WS_DEFINES
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_USE_FLAC=1
    WK_DEV_SAMPLE_DIR="${KEYS_DIR}/Samples"
    WS_DEV_SAMPLE_DIR="${CMAKE_SOURCE_DIR}/Samples")

target_sources(WorshipStation PRIVATE ${WS_CORE_SOURCES})
target_compile_definitions(WorshipStation PUBLIC ${WS_DEFINES})
target_compile_options(WorshipStation PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
target_link_libraries(WorshipStation
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

if (IOS)
    # Ship the piano + orchestra samples inside the .app: Samples/ (piano) and Samples/Orchestra. The sample banks
    # look next to the executable.
    add_custom_command(TARGET WorshipStation_Standalone POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${KEYS_DIR}/Samples" "$<TARGET_BUNDLE_DIR:WorshipStation_Standalone>/Samples"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Samples/Orchestra" "$<TARGET_BUNDLE_DIR:WorshipStation_Standalone>/Samples/Orchestra")
    return() # the console tools below are desktop-only
endif()

# ---------------------------------------------------------------- test / render tool
# WSRender: renders every orchestra preset to renders/, tests pack switching + routing, and snapshots the UI.
juce_add_console_app(WSRender PRODUCT_NAME "WSRender")
target_sources(WSRender PRIVATE Tools/Render.cpp ${WS_CORE_SOURCES})
target_compile_definitions(WSRender PRIVATE ${WS_DEFINES} JUCE_MODAL_LOOPS_PERMITTED=1)
target_compile_options(WSRender PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
target_link_libraries(WSRender
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
