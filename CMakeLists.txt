cmake_minimum_required(VERSION 3.22)
project(OmniChoir VERSION 0.1.0 LANGUAGES C CXX)

include(FetchContent)
if(DEFINED JUCE_PATH AND EXISTS "${JUCE_PATH}/CMakeLists.txt")
    add_subdirectory("${JUCE_PATH}" JUCE)
else()
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.12
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(OmniChoir
    COMPANY_NAME "Skell Audio"
    BUNDLE_ID "audio.skell.omnichoir"
    PLUGIN_MANUFACTURER_CODE SkAu
    PLUGIN_CODE OmC1
    FORMATS VST3
    PRODUCT_NAME "OmniChoir"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(OmniChoir)
target_sources(OmniChoir PRIVATE
    Source/ChoirDSP.h
    Source/ChoirDSP.cpp
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp)
target_compile_features(OmniChoir PRIVATE cxx_std_17)
target_compile_definitions(OmniChoir PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)
target_compile_definitions(OmniChoir PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(OmniChoir PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

option(OMNICHOIR_BUILD_TESTS "Build JUCE-free native DSP tests" OFF)
if(OMNICHOIR_BUILD_TESTS)
    enable_testing()
    add_executable(OmniChoirDSPTests tests/dsp_tests.cpp Source/ChoirDSP.cpp)
    target_compile_features(OmniChoirDSPTests PRIVATE cxx_std_17)
    add_test(NAME voice_and_spatial_dsp COMMAND OmniChoirDSPTests)
endif()
