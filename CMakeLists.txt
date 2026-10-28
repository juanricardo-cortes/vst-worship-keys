cmake_minimum_required(VERSION 3.22)
project(WorshipKeys VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Link the MSVC runtime statically so the exe / VST3 run on PCs without the VC++ Redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------- plugin
# iPhone build = standalone app only (configure with -G Xcode -DCMAKE_SYSTEM_NAME=iOS; see .github/workflows/ios.yml).
if (IOS)
    set(WK_FORMATS Standalone)
else()
    set(WK_FORMATS VST3 Standalone)
endif()

juce_add_plugin(WorshipKeys
    COMPANY_NAME "Robert"
    PLUGIN_MANUFACTURER_CODE Rbpa
    PLUGIN_CODE WsKy
    FORMATS ${WK_FORMATS}
    BUNDLE_ID com.robert.worshipkeys
    IPHONE_SCREEN_ORIENTATIONS UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight
    IPAD_SCREEN_ORIENTATIONS UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight
    BACKGROUND_AUDIO_ENABLED TRUE
    REQUIRES_FULL_SCREEN TRUE
    STATUS_BAR_HIDDEN TRUE
    PRODUCT_NAME "Worship Keys"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth)

set(WK_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/SampleBank.cpp)

set(WK_DEFINES
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_USE_FLAC=1
    WK_DEV_SAMPLE_DIR="${CMAKE_SOURCE_DIR}/Samples")

target_sources(WorshipKeys PRIVATE ${WK_SOURCES})
target_compile_definitions(WorshipKeys PUBLIC ${WK_DEFINES})
target_link_libraries(WorshipKeys
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

if (IOS)
    # Ship the piano samples inside the .app; SampleBank finds them next to the executable.
    add_custom_command(TARGET WorshipKeys_Standalone POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Samples" "$<TARGET_BUNDLE_DIR:WorshipKeys_Standalone>/Samples")
    return() # the console tools below are desktop-only
endif()

# ---------------------------------------------------------------- offline renderer
# Console tool: plays a test progression through every preset and writes WAVs to renders/.
# Used to sanity-check levels / stability without a DAW.
juce_add_console_app(WKRender PRODUCT_NAME "WKRender")
target_sources(WKRender PRIVATE Tools/Render.cpp Source/PluginProcessor.cpp Source/SampleBank.cpp Source/PluginEditor.cpp)
target_compile_definitions(WKRender PRIVATE ${WK_DEFINES})
target_link_libraries(WKRender
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------- demo renderer
# Console tool: renders a CSV of note events through one preset (used for the ad / demo tracks in promo/).
juce_add_console_app(WKDemo PRODUCT_NAME "WKDemo")
target_sources(WKDemo PRIVATE Tools/Demo.cpp Source/PluginProcessor.cpp Source/SampleBank.cpp Source/PluginEditor.cpp)
target_compile_definitions(WKDemo PRIVATE ${WK_DEFINES})
target_link_libraries(WKDemo
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
