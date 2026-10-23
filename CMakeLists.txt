cmake_minimum_required(VERSION 3.22)
project(OmniChoirFX VERSION 0.2.0 LANGUAGES C CXX)

include(FetchContent)
if(DEFINED JUCE_PATH AND EXISTS "${JUCE_PATH}/CMakeLists.txt")
    add_subdirectory("${JUCE_PATH}" JUCE)
else()
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.16-security
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(OmniChoirFX
    COMPANY_NAME "Skell Audio"
    BUNDLE_ID "audio.skell.omnichoirfx"
    PLUGIN_MANUFACTURER_CODE SkAu
    PLUGIN_CODE OmF2
    FORMATS VST3
    PRODUCT_NAME "OmniChoir FX"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(OmniChoirFX)
juce_add_binary_data(OmniChoirFXArt SOURCES Assets/cathedral_stage.jpg)
target_link_libraries(OmniChoirFX PRIVATE OmniChoirFXArt)
target_sources(OmniChoirFX PRIVATE
    Source/ChoirFX.h
    Source/ChoirFX.cpp
    Source/ChoirDSP.h
    Source/ChoirDSP.cpp
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp)
target_compile_features(OmniChoirFX PRIVATE cxx_std_17)
target_compile_definitions(OmniChoirFX PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)
target_compile_definitions(OmniChoirFX PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(OmniChoirFX PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

option(OMNICHOIR_BUILD_TESTS "Build JUCE-free native DSP tests" OFF)
if(OMNICHOIR_BUILD_TESTS)
    enable_testing()
    add_executable(OmniChoirDSPTests tests/dsp_tests.cpp Source/ChoirDSP.cpp)
    add_executable(OmniChoirFXTests tests/fx_tests.cpp Source/ChoirFX.cpp Source/ChoirDSP.cpp)
    target_compile_features(OmniChoirDSPTests PRIVATE cxx_std_17)
    add_test(NAME voice_and_spatial_dsp COMMAND OmniChoirDSPTests)
    target_compile_features(OmniChoirFXTests PRIVATE cxx_std_17)
    add_test(NAME input_fx_and_scan COMMAND OmniChoirFXTests)
endif()
