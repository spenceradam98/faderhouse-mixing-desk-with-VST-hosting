cmake_minimum_required(VERSION 3.22)

# macOS: one universal binary that runs on macOS 11 Intel Macs and on Apple
# Silicon. No AVX/AVX2 flags anywhere: x86_64 builds stay on the SSE baseline.
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
if(NOT DEFINED CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "macOS architectures")
endif()

# Windows: static C++ runtime, so no Visual C++ Redistributable is needed.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(FADERHOUSE VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# Brand placeholders: change once the brand name is decided.
set(BRAND_NAME "YourBrand")
set(PRODUCT_NAME "Faderhouse")

set(FH_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND FH_FORMATS AU)
endif()

juce_add_plugin(Faderhouse
    COMPANY_NAME "${BRAND_NAME}"
    PLUGIN_MANUFACTURER_CODE Ybrd
    PLUGIN_CODE Fdh1
    PRODUCT_NAME "${PRODUCT_NAME}"
    FORMATS ${FH_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Mixing
    AU_MAIN_TYPE kAudioUnitType_MusicEffect)

set(FH_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/engine/Params.cpp
    Source/engine/ChannelStrip.cpp
    Source/engine/BusCompressor.cpp
    Source/hosting/HostedChain.cpp
    Source/ui/Controls.cpp
    Source/ui/ChainUI.cpp
    Source/ui/StripView.cpp
    Source/ui/FocusPanel.cpp
    Source/ui/MasterSection.cpp)

set(FH_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=1
    JUCE_PLUGINHOST_LADSPA=0
    JUCE_PLUGINHOST_LV2=0
    FH_BRAND_NAME="${BRAND_NAME}"
    FH_PRODUCT_NAME="${PRODUCT_NAME}")

target_sources(Faderhouse PRIVATE ${FH_SOURCES})
target_include_directories(Faderhouse PRIVATE Source)
target_compile_definitions(Faderhouse PUBLIC ${FH_DEFS})
target_link_libraries(Faderhouse
    PRIVATE juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# Tests and developer tools
option(FH_BUILD_TOOLS "Build tests, test plugin and screenshot renderer" ON)
if(FH_BUILD_TOOLS)
    enable_testing()

    juce_add_plugin(TestHalfGain
        COMPANY_NAME "${BRAND_NAME}" PLUGIN_MANUFACTURER_CODE Ybrd PLUGIN_CODE Thg2
        PRODUCT_NAME "TestHalfGain" FORMATS VST3 IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE)
    target_sources(TestHalfGain PRIVATE tools/TestHalfGain.cpp)
    target_compile_definitions(TestHalfGain PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries(TestHalfGain PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)

    juce_add_plugin(TestSineSynth
        COMPANY_NAME "${BRAND_NAME}" PLUGIN_MANUFACTURER_CODE Ybrd PLUGIN_CODE Tss1
        PRODUCT_NAME "TestSineSynth" FORMATS VST3 IS_SYNTH TRUE NEEDS_MIDI_INPUT TRUE)
    target_sources(TestSineSynth PRIVATE tools/TestSineSynth.cpp)
    target_compile_definitions(TestSineSynth PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries(TestSineSynth PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)

    foreach(tool EngineTests Snapshots)
        juce_add_console_app(${tool} PRODUCT_NAME "${tool}")
        target_sources(${tool} PRIVATE tools/${tool}.cpp ${FH_SOURCES})
        target_include_directories(${tool} PRIVATE Source)
        target_compile_definitions(${tool} PRIVATE ${FH_DEFS} JUCE_MODAL_LOOPS_PERMITTED=1
            JucePlugin_Name="${PRODUCT_NAME}")
        target_link_libraries(${tool} PRIVATE juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp
            PUBLIC juce::juce_recommended_config_flags)
    endforeach()
    add_dependencies(EngineTests TestHalfGain_VST3 TestSineSynth_VST3)

    add_test(NAME EngineTests COMMAND EngineTests
        $<TARGET_FILE_DIR:TestHalfGain>/VST3/TestHalfGain.vst3
        $<TARGET_FILE_DIR:TestSineSynth>/VST3/TestSineSynth.vst3)
endif()
