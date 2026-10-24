cmake_minimum_required(VERSION 3.22)
project(OmniChoirFX VERSION 0.6.0 LANGUAGES C CXX)

option(OMNICHOIR_NATIVE_ONLY "Build JUCE-free tests without fetching JUCE" OFF)
option(OMNICHOIR_BUILD_TESTS "Build JUCE-free native DSP tests" ${OMNICHOIR_NATIVE_ONLY})
if(NOT OMNICHOIR_NATIVE_ONLY)
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
    Source/StagedChoir.h
    Source/StagedChoir.cpp
    Source/Stage/Common.h
    Source/Stage/Mics.h
    Source/Stage/Room.h
    Source/Stage/Spatial.h
    Source/Stage/Stage.h
    Source/Offline/FullTakeScan.h
    Source/Offline/FullTakeScan.cpp
    Source/Offline/ExternalStemMixer.h
    Source/Offline/ExternalStemMixer.cpp
    Source/Offline/TakeWorker.h
    Source/Offline/TakeWorker.cpp
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

endif() # native tests do not need JUCE
if(OMNICHOIR_BUILD_TESTS)
    enable_testing()
    add_executable(OmniChoirDSPTests tests/dsp_tests.cpp Source/ChoirDSP.cpp)
    add_executable(OmniChoirFXTests tests/fx_tests.cpp Source/ChoirFX.cpp Source/ChoirDSP.cpp)
    target_compile_features(OmniChoirDSPTests PRIVATE cxx_std_17)
    add_test(NAME voice_and_spatial_dsp COMMAND OmniChoirDSPTests)
    target_compile_features(OmniChoirFXTests PRIVATE cxx_std_17)
    add_test(NAME input_fx_and_scan COMMAND OmniChoirFXTests)
    add_executable(OmniChoirTrackerTests tests/tracker_tests.cpp Source/Offline/FullTakeScan.cpp Source/ChoirFX.cpp Source/ChoirDSP.cpp)
    target_compile_features(OmniChoirTrackerTests PRIVATE cxx_std_17)
    add_test(NAME yin_tracker_and_scanner_accuracy COMMAND OmniChoirTrackerTests)
    add_executable(OmniChoirStagedTests tests/staged_tests.cpp Source/StagedChoir.cpp Source/ChoirFX.cpp Source/ChoirDSP.cpp)
    target_compile_features(OmniChoirStagedTests PRIVATE cxx_std_17)
    add_test(NAME faithful_stage_live_path COMMAND OmniChoirStagedTests)
    add_executable(OmniChoirStemMixerTests tests/stem_mixer_tests.cpp Source/Offline/ExternalStemMixer.cpp)
    target_compile_features(OmniChoirStemMixerTests PRIVATE cxx_std_17)
    add_test(NAME external_singer_stems COMMAND OmniChoirStemMixerTests)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME midi_and_python_interop
                 COMMAND ${Python3_EXECUTABLE} -m unittest discover
                         -s ${CMAKE_CURRENT_SOURCE_DIR}/tests -p test_interop_tools.py)
    endif()
    add_executable(OmniChoirCalibrateTrim tests/calibrate_trim.cpp Source/StagedChoir.cpp Source/ChoirFX.cpp Source/ChoirDSP.cpp)
    target_compile_features(OmniChoirCalibrateTrim PRIVATE cxx_std_17)
    add_executable(OmniChoirFullTakeTests tests/fulltake_tests.cpp
        Source/Offline/FullTakeScan.cpp Source/ChoirFX.cpp Source/ChoirDSP.cpp)
    target_compile_features(OmniChoirFullTakeTests PRIVATE cxx_std_17)
    add_test(NAME complete_take_scan_and_score COMMAND OmniChoirFullTakeTests)
endif()
