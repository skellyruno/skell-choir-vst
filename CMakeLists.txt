cmake_minimum_required(VERSION 3.22)
project(OmniChoirFX VERSION 0.9.0 LANGUAGES C CXX)

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

# Keep SkellCore's hubless WorldLink independent of SkellyHub.
FetchContent_Declare(SkellCoreShared
    GIT_REPOSITORY https://github.com/skellyruno/skell-vst-template.git
    GIT_TAG 4c499c58cb3b2791eed2d5dcef6d8d543fcaea8d
    SOURCE_SUBDIR SkellCore)
FetchContent_MakeAvailable(SkellCoreShared)

juce_add_plugin(OmniChoirFX
    COMPANY_NAME "Skell Audio"
    BUNDLE_ID "audio.skell.omnichoirfx"
    PLUGIN_MANUFACTURER_CODE SkAu
    PLUGIN_CODE OmF2
    FORMATS VST3
    PRODUCT_NAME "SkellyChoir"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE      # v0.7: MIDI Chord mode. Still an audio effect (IS_SYNTH FALSE); DAWs that do not route MIDI to effects simply never send any
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(OmniChoirFX)
juce_add_binary_data(OmniChoirFXArt SOURCES Assets/cathedral_stage.jpg Assets/Scenes/scene_concert_hall.jpg Assets/Scenes/scene_studio_booth.jpg Assets/Scenes/scene_chapel.jpg Assets/Scenes/scene_gospel_stage.jpg Assets/Scenes/scene_stadium_anthem.jpg Assets/Scenes/scene_opera_house.jpg Assets/Scenes/scene_graveyard.jpg Assets/Scenes/scene_crypt.jpg Assets/Scenes/scene_cave.jpg Assets/Scenes/scene_bathroom.jpg Assets/Scenes/scene_parking_garage.jpg Assets/Scenes/scene_marble_dome.jpg Assets/Scenes/scene_stairwell.jpg Assets/Rooms/room_cathedral.wav Assets/Rooms/room_chapel.wav Assets/Rooms/room_small_hall.wav)
target_link_libraries(OmniChoirFX PRIVATE OmniChoirFXArt)
target_sources(OmniChoirFX PRIVATE
    Source/ChoirFX.h
    Source/ChoirFX.cpp
    Source/StagedChoir.h
    Source/StagedChoir.cpp
    Source/FormantChoir.h
    Source/FormantChoir.cpp
    Source/StagePresets.h
    Source/Stage/Convolver.h
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
    Source/Branding.h
    Source/Scenes.h
    Source/Theme.h
    Source/StageGeometry.h
    Source/UI/SkullShape.h
    Source/UI/TitleLayout.h
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp
    Source/UI/OmniLookAndFeel.h
    Source/StylePresets.h
    Source/MidiChord.h
    Source/Scales.h
    Source/Consonants.h
    Source/StyleFile.h
    Source/Offline/ListenSession.h)
target_compile_features(OmniChoirFX PRIVATE cxx_std_17)
target_compile_definitions(OmniChoirFX PRIVATE SKELLYCHOIR_WITH_SKELLCORE=1)
target_compile_definitions(OmniChoirFX PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)
target_compile_definitions(OmniChoirFX PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(OmniChoirFX PRIVATE
    SkellCore::DSP
    SkellCore::UI
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# JUCE embeds __DATE__ and __TIME__ in juce_core_CompilationTime.cpp. Pin those
# values to the source commit and ask MSVC's linker for reproducible PE output,
# otherwise identical source builds produce different VST3 hashes on every run.
if(MSVC)
    if(NOT DEFINED ENV{SOURCE_DATE_EPOCH})
        find_package(Git REQUIRED)
        set(OMNICHOIR_EPOCH_COMMIT "$ENV{SOURCE_COMMIT}")
        if(NOT OMNICHOIR_EPOCH_COMMIT)
            set(OMNICHOIR_EPOCH_COMMIT "HEAD")
        endif()
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${CMAKE_SOURCE_DIR}" show -s --format=%ct "${OMNICHOIR_EPOCH_COMMIT}"
            OUTPUT_VARIABLE OMNICHOIR_SOURCE_DATE_EPOCH
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE OMNICHOIR_GIT_TIMESTAMP_RESULT)
        if(NOT OMNICHOIR_GIT_TIMESTAMP_RESULT EQUAL 0 OR
           NOT OMNICHOIR_SOURCE_DATE_EPOCH MATCHES "^[0-9]+$")
            message(FATAL_ERROR
                "Reproducible MSVC builds require SOURCE_DATE_EPOCH or a valid Git commit timestamp")
        endif()
        set(ENV{SOURCE_DATE_EPOCH} "${OMNICHOIR_SOURCE_DATE_EPOCH}")
    endif()

    string(TIMESTAMP OMNICHOIR_REPRO_DATE "%b %d %Y" UTC)
    string(TIMESTAMP OMNICHOIR_REPRO_TIME "%H:%M:%S" UTC)
    function(omnichoir_apply_reproducible_compile_metadata target_name)
        if(TARGET ${target_name})
            target_compile_options(${target_name} PRIVATE /wd4005)
            target_compile_definitions(${target_name} PRIVATE
                "__DATE__=\"${OMNICHOIR_REPRO_DATE}\""
                "__TIME__=\"${OMNICHOIR_REPRO_TIME}\"")
        endif()
    endfunction()

    omnichoir_apply_reproducible_compile_metadata(OmniChoirFX)
    omnichoir_apply_reproducible_compile_metadata(OmniChoirFX_VST3)
    if(TARGET OmniChoirFX_VST3)
        target_link_options(OmniChoirFX_VST3 PRIVATE /Brepro)
    endif()
endif()

# Optional: a headless tool that paints every tab to PNG (tools/screenshots.cpp). Off by default; the CI "screenshots" job turns it on.
option(OMNICHOIR_BUILD_SCREENSHOTS "Also build the headless screenshot tool" OFF)
if(OMNICHOIR_BUILD_SCREENSHOTS)
    juce_add_console_app(OmniChoirScreenshots PRODUCT_NAME "OmniChoirScreenshots")
    juce_generate_juce_header(OmniChoirScreenshots)
    target_sources(OmniChoirScreenshots PRIVATE
        tools/screenshots.cpp
        Source/ChoirFX.cpp Source/StagedChoir.cpp Source/FormantChoir.cpp Source/ChoirDSP.cpp
        Source/Offline/FullTakeScan.cpp Source/Offline/ExternalStemMixer.cpp Source/Offline/TakeWorker.cpp
        Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_include_directories(OmniChoirScreenshots PRIVATE Source)
    target_compile_features(OmniChoirScreenshots PRIVATE cxx_std_17)
    target_compile_definitions(OmniChoirScreenshots PRIVATE SKELLYCHOIR_WITH_SKELLCORE=1)
    target_compile_definitions(OmniChoirScreenshots PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(OmniChoirScreenshots PRIVATE
        OmniChoirFXArt
        SkellCore::DSP
        SkellCore::UI
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_extra
        juce::juce_recommended_config_flags)
    if(MSVC)
        omnichoir_apply_reproducible_compile_metadata(OmniChoirScreenshots)
    endif()
endif()


# Local Quick4: a real JUCE AudioProcessorEditor renderer for the shared
# Skelly UI router. This is opt-in and separate from the exhaustive CI tour.
option(OMNICHOIR_BUILD_QUICK4 "Build the local SkellyChoir Quick4 editor renderer" OFF)
if(OMNICHOIR_BUILD_QUICK4)
    juce_add_console_app(SkellyChoirQuick4 PRODUCT_NAME "SkellyChoirQuick4")
    juce_generate_juce_header(SkellyChoirQuick4)
    target_sources(SkellyChoirQuick4 PRIVATE
        tools/quick4.cpp
        Source/ChoirFX.cpp Source/StagedChoir.cpp Source/FormantChoir.cpp Source/ChoirDSP.cpp
        Source/Offline/FullTakeScan.cpp Source/Offline/ExternalStemMixer.cpp Source/Offline/TakeWorker.cpp
        Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_include_directories(SkellyChoirQuick4 PRIVATE Source)
    target_compile_features(SkellyChoirQuick4 PRIVATE cxx_std_17)
    target_compile_definitions(SkellyChoirQuick4 PRIVATE
        SKELLYCHOIR_WITH_SKELLCORE=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(SkellyChoirQuick4 PRIVATE
        OmniChoirFXArt
        juce::juce_cryptography
        SkellCore::DSP
        SkellCore::UI
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
    if(MSVC)
        omnichoir_apply_reproducible_compile_metadata(SkellyChoirQuick4)
    endif()
endif()

endif() # native tests do not need JUCE
if(OMNICHOIR_BUILD_TESTS)
    enable_testing()
    add_executable(OmniChoirDSPTests tests/dsp_tests.cpp Source/ChoirDSP.cpp)
    add_executable(OmniChoirFXTests tests/fx_tests.cpp Source/ChoirFX.cpp Source/FormantChoir.cpp Source/ChoirDSP.cpp)
    target_compile_features(OmniChoirDSPTests PRIVATE cxx_std_17)
    add_test(NAME voice_and_spatial_dsp COMMAND OmniChoirDSPTests)
    target_compile_features(OmniChoirFXTests PRIVATE cxx_std_17)
    add_test(NAME input_fx_and_scan COMMAND OmniChoirFXTests)
    add_executable(OmniChoirTrackerTests tests/tracker_tests.cpp Source/Offline/FullTakeScan.cpp Source/ChoirFX.cpp Source/FormantChoir.cpp Source/ChoirDSP.cpp)
    target_compile_features(OmniChoirTrackerTests PRIVATE cxx_std_17)
    add_test(NAME yin_tracker_and_scanner_accuracy COMMAND OmniChoirTrackerTests)
    add_executable(OmniChoirStagedTests tests/staged_tests.cpp Source/StagedChoir.cpp Source/ChoirFX.cpp Source/FormantChoir.cpp Source/ChoirDSP.cpp)
    target_compile_features(OmniChoirStagedTests PRIVATE cxx_std_17)
    add_test(NAME faithful_stage_live_path COMMAND OmniChoirStagedTests)
    add_executable(OmniChoirFormantTests tests/formant_tests.cpp Source/ChoirFX.cpp Source/FormantChoir.cpp Source/ChoirDSP.cpp)
    target_compile_features(OmniChoirFormantTests PRIVATE cxx_std_17)
    add_test(NAME formant_choir_voice_engine COMMAND OmniChoirFormantTests)
    add_executable(OmniChoirConvolverTests tests/convolver_tests.cpp)
    target_compile_features(OmniChoirConvolverTests PRIVATE cxx_std_17)
    add_test(NAME impulse_response_convolver COMMAND OmniChoirConvolverTests)
    add_executable(OmniChoirFeatureTests tests/features_tests.cpp Source/StagedChoir.cpp Source/ChoirFX.cpp Source/FormantChoir.cpp Source/ChoirDSP.cpp)
    target_compile_features(OmniChoirFeatureTests PRIVATE cxx_std_17)
    add_test(NAME stems_meters_ab_ir_formant_chain COMMAND OmniChoirFeatureTests)
    add_executable(OmniChoirPresetTests tests/preset_tests.cpp Source/StagedChoir.cpp Source/ChoirFX.cpp Source/FormantChoir.cpp Source/ChoirDSP.cpp)
    target_compile_features(OmniChoirPresetTests PRIVATE cxx_std_17)
    add_test(NAME stage_presets COMMAND OmniChoirPresetTests)
    add_executable(OmniChoirPageRulesTests tests/page_rules_tests.cpp)
    target_compile_features(OmniChoirPageRulesTests PRIVATE cxx_std_17)
    add_test(NAME page_rules COMMAND OmniChoirPageRulesTests ${CMAKE_SOURCE_DIR})
    add_executable(OmniChoirRoomApplyTests tests/room_apply_tests.cpp Source/StagedChoir.cpp Source/ChoirFX.cpp Source/FormantChoir.cpp Source/ChoirDSP.cpp)
    target_compile_features(OmniChoirRoomApplyTests PRIVATE cxx_std_17)
    add_test(NAME room_apply COMMAND OmniChoirRoomApplyTests ${CMAKE_SOURCE_DIR})
    add_executable(OmniChoirKeyPresetUiTests tests/key_preset_ui_tests.cpp)
    target_compile_features(OmniChoirKeyPresetUiTests PRIVATE cxx_std_17)
    add_test(NAME key_preset_ui COMMAND OmniChoirKeyPresetUiTests ${CMAKE_SOURCE_DIR})
    add_executable(OmniChoirPositionTests tests/position_tests.cpp Source/StagedChoir.cpp Source/ChoirFX.cpp Source/FormantChoir.cpp Source/ChoirDSP.cpp)
    target_include_directories(OmniChoirPositionTests PRIVATE Source)
    add_test(NAME position COMMAND OmniChoirPositionTests)
    add_executable(OmniChoirRoomSwitchTests tests/room_switch_tests.cpp Source/StagedChoir.cpp Source/ChoirFX.cpp Source/FormantChoir.cpp Source/ChoirDSP.cpp)
    target_compile_features(OmniChoirRoomSwitchTests PRIVATE cxx_std_17)
    add_test(NAME room_switch COMMAND OmniChoirRoomSwitchTests)
    add_executable(OmniChoirMicDistanceTests tests/mic_distance_tests.cpp Source/StagedChoir.cpp Source/ChoirFX.cpp Source/FormantChoir.cpp Source/ChoirDSP.cpp)
    target_compile_features(OmniChoirMicDistanceTests PRIVATE cxx_std_17)
    add_test(NAME mic_distance COMMAND OmniChoirMicDistanceTests)
    add_executable(OmniChoirStageFuzzTests tests/stage_fuzz_tests.cpp Source/StagedChoir.cpp Source/ChoirFX.cpp Source/FormantChoir.cpp Source/ChoirDSP.cpp)
    target_compile_features(OmniChoirStageFuzzTests PRIVATE cxx_std_17)
    add_test(NAME stage_fuzz COMMAND OmniChoirStageFuzzTests)
    add_executable(OmniChoirStemMixerTests tests/stem_mixer_tests.cpp Source/Offline/ExternalStemMixer.cpp)
    target_compile_features(OmniChoirStemMixerTests PRIVATE cxx_std_17)
    add_test(NAME external_singer_stems COMMAND OmniChoirStemMixerTests)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME midi_and_python_interop
                 COMMAND ${Python3_EXECUTABLE} -m unittest discover
                         -s ${CMAKE_CURRENT_SOURCE_DIR}/tests -p test_interop_tools.py)
    endif()
    add_executable(OmniChoirBalanceTests tests/balance_tests.cpp Source/StagedChoir.cpp Source/ChoirFX.cpp Source/FormantChoir.cpp Source/ChoirDSP.cpp)
    target_compile_features(OmniChoirBalanceTests PRIVATE cxx_std_17)
    add_test(NAME level_and_balance_goals COMMAND OmniChoirBalanceTests ${CMAKE_CURRENT_SOURCE_DIR})
    add_executable(OmniChoirRealVocalTests tests/real_vocal_tests.cpp Source/StagedChoir.cpp Source/ChoirFX.cpp Source/FormantChoir.cpp Source/ChoirDSP.cpp)
    target_compile_features(OmniChoirRealVocalTests PRIVATE cxx_std_17)
    add_test(NAME real_vocal_tracker_and_balance COMMAND OmniChoirRealVocalTests ${CMAKE_CURRENT_SOURCE_DIR}/tests/data)
    add_executable(OmniChoirMidiChordTests tests/midi_chord_tests.cpp Source/StagedChoir.cpp Source/ChoirFX.cpp Source/FormantChoir.cpp Source/ChoirDSP.cpp)
    target_compile_features(OmniChoirMidiChordTests PRIVATE cxx_std_17)
    add_test(NAME midi_chord_mode COMMAND OmniChoirMidiChordTests)
    add_executable(OmniChoirStyleTests tests/style_tests.cpp Source/StagedChoir.cpp Source/ChoirFX.cpp Source/FormantChoir.cpp Source/ChoirDSP.cpp)
    target_compile_features(OmniChoirStyleTests PRIVATE cxx_std_17)
    add_test(NAME styles_size_and_surprise COMMAND OmniChoirStyleTests ${CMAKE_CURRENT_SOURCE_DIR})
    add_executable(OmniChoirRoomIrTests tests/room_ir_tests.cpp Source/StagedChoir.cpp Source/ChoirFX.cpp Source/FormantChoir.cpp Source/ChoirDSP.cpp)
    target_compile_features(OmniChoirRoomIrTests PRIVATE cxx_std_17)
    add_test(NAME built_in_rooms COMMAND OmniChoirRoomIrTests ${CMAKE_CURRENT_SOURCE_DIR})
    find_package(Threads REQUIRED)
    set(OMNICHOIR_ENGINE Source/StagedChoir.cpp Source/ChoirFX.cpp Source/FormantChoir.cpp Source/ChoirDSP.cpp)
    add_executable(OmniChoirKeyScaleTests tests/key_scale_tests.cpp ${OMNICHOIR_ENGINE})
    target_compile_features(OmniChoirKeyScaleTests PRIVATE cxx_std_17)
    add_test(NAME key_and_scale COMMAND OmniChoirKeyScaleTests)
    add_executable(OmniChoirSwellPulseTests tests/swell_pulse_tests.cpp ${OMNICHOIR_ENGINE})
    target_compile_features(OmniChoirSwellPulseTests PRIVATE cxx_std_17)
    add_test(NAME swell_and_pulse COMMAND OmniChoirSwellPulseTests)
    add_executable(OmniChoirConsonantTests tests/consonant_tests.cpp ${OMNICHOIR_ENGINE})
    target_compile_features(OmniChoirConsonantTests PRIVATE cxx_std_17)
    add_test(NAME consonants COMMAND OmniChoirConsonantTests ${CMAKE_CURRENT_SOURCE_DIR})
    add_executable(OmniChoirStyleFileTests tests/style_file_tests.cpp ${OMNICHOIR_ENGINE})
    target_compile_features(OmniChoirStyleFileTests PRIVATE cxx_std_17)
    add_test(NAME style_files COMMAND OmniChoirStyleFileTests ${CMAKE_CURRENT_SOURCE_DIR})
    add_executable(OmniChoirListenTests tests/listen_tests.cpp Source/Offline/FullTakeScan.cpp ${OMNICHOIR_ENGINE})
    target_compile_features(OmniChoirListenTests PRIVATE cxx_std_17)
    target_link_libraries(OmniChoirListenTests PRIVATE Threads::Threads)
    add_test(NAME listen_mode COMMAND OmniChoirListenTests ${CMAKE_CURRENT_SOURCE_DIR})
    add_executable(OmniChoirListenRaceTests tests/listen_race_tests.cpp Source/Offline/FullTakeScan.cpp ${OMNICHOIR_ENGINE})
    target_compile_features(OmniChoirListenRaceTests PRIVATE cxx_std_17)
    target_link_libraries(OmniChoirListenRaceTests PRIVATE Threads::Threads)
    add_test(NAME listen_capture_race COMMAND OmniChoirListenRaceTests)
    add_executable(OmniChoirBrandingTests tests/branding_tests.cpp)
    target_compile_features(OmniChoirBrandingTests PRIVATE cxx_std_17)
    add_test(NAME branding COMMAND OmniChoirBrandingTests ${CMAKE_CURRENT_SOURCE_DIR})
    add_executable(OmniChoirSceneTests tests/scene_tests.cpp)
    target_compile_features(OmniChoirSceneTests PRIVATE cxx_std_17)
    add_test(NAME scenes COMMAND OmniChoirSceneTests ${CMAKE_CURRENT_SOURCE_DIR})
    add_executable(OmniChoirTitleLayoutTests tests/title_layout_tests.cpp)
    target_compile_features(OmniChoirTitleLayoutTests PRIVATE cxx_std_17)
    add_test(NAME title_layout COMMAND OmniChoirTitleLayoutTests)
    add_executable(OmniChoirCurtainTests tests/curtain_tests.cpp)
    target_compile_features(OmniChoirCurtainTests PRIVATE cxx_std_17)
    add_test(NAME curtain COMMAND OmniChoirCurtainTests)
    add_executable(OmniChoirGrainLockTests tests/grain_lock_tests.cpp Source/ChoirFX.cpp Source/FormantChoir.cpp Source/ChoirDSP.cpp)
    target_compile_features(OmniChoirGrainLockTests PRIVATE cxx_std_17)
    add_test(NAME grain_lock COMMAND OmniChoirGrainLockTests)
    add_executable(OmniChoirSkullShapeTests tests/skull_shape_tests.cpp)
    target_compile_features(OmniChoirSkullShapeTests PRIVATE cxx_std_17)
    add_test(NAME skull_shape COMMAND OmniChoirSkullShapeTests)
    add_executable(OmniChoirThemeTests tests/theme_tests.cpp)
    target_compile_features(OmniChoirThemeTests PRIVATE cxx_std_17)
    add_test(NAME themes COMMAND OmniChoirThemeTests ${CMAKE_CURRENT_SOURCE_DIR})
    add_executable(OmniChoirStageGeometryTests tests/stage_geometry_tests.cpp)
    target_compile_features(OmniChoirStageGeometryTests PRIVATE cxx_std_17)
    add_test(NAME stage_geometry COMMAND OmniChoirStageGeometryTests ${CMAKE_CURRENT_SOURCE_DIR})
    add_executable(OmniChoirStageControlLayoutTests tests/stage_control_layout_tests.cpp)
    target_compile_features(OmniChoirStageControlLayoutTests PRIVATE cxx_std_17)
    add_test(NAME stage_control_layout COMMAND OmniChoirStageControlLayoutTests)
    add_executable(OmniChoirStageRealtimeAllocationTests tests/stage_realtime_allocation_tests.cpp)
    target_compile_features(OmniChoirStageRealtimeAllocationTests PRIVATE cxx_std_17)
    add_test(NAME stage_realtime_allocations COMMAND OmniChoirStageRealtimeAllocationTests)
    add_executable(OmniChoirCalibrateTrim tests/calibrate_trim.cpp Source/StagedChoir.cpp Source/ChoirFX.cpp Source/FormantChoir.cpp Source/ChoirDSP.cpp)
    target_compile_features(OmniChoirCalibrateTrim PRIVATE cxx_std_17)
    add_executable(OmniChoirFullTakeTests tests/fulltake_tests.cpp
        Source/Offline/FullTakeScan.cpp Source/ChoirFX.cpp Source/FormantChoir.cpp Source/ChoirDSP.cpp)
    target_compile_features(OmniChoirFullTakeTests PRIVATE cxx_std_17)
    add_test(NAME complete_take_scan_and_score COMMAND OmniChoirFullTakeTests)
    add_executable(OmniChoirSyncShiftTests tests/sync_shift_tests.cpp Source/ChoirFX.cpp Source/FormantChoir.cpp Source/ChoirDSP.cpp)
    target_compile_features(OmniChoirSyncShiftTests PRIVATE cxx_std_17)
    add_test(NAME sync_shift COMMAND OmniChoirSyncShiftTests ${CMAKE_CURRENT_SOURCE_DIR}/tests/data)
    add_executable(OmniChoirVibratoStackTests tests/vibrato_stack_tests.cpp Source/ChoirFX.cpp Source/FormantChoir.cpp Source/ChoirDSP.cpp)
    target_compile_features(OmniChoirVibratoStackTests PRIVATE cxx_std_17)
    add_test(NAME vibrato_stack COMMAND OmniChoirVibratoStackTests ${CMAKE_CURRENT_SOURCE_DIR})
    add_executable(OmniChoirStageSpreadTests tests/stage_spread_tests.cpp ${OMNICHOIR_ENGINE})
    target_compile_features(OmniChoirStageSpreadTests PRIVATE cxx_std_17)
    add_test(NAME stage_spread COMMAND OmniChoirStageSpreadTests ${CMAKE_CURRENT_SOURCE_DIR})
    add_executable(OmniChoirSectionToneTests tests/section_tone_tests.cpp ${OMNICHOIR_ENGINE})
    target_compile_features(OmniChoirSectionToneTests PRIVATE cxx_std_17)
    add_test(NAME section_tone COMMAND OmniChoirSectionToneTests ${CMAKE_CURRENT_SOURCE_DIR})
    add_executable(OmniChoirPhraseBreathTests tests/phrase_breath_tests.cpp ${OMNICHOIR_ENGINE})
    target_compile_features(OmniChoirPhraseBreathTests PRIVATE cxx_std_17)
    add_test(NAME phrase_breath COMMAND OmniChoirPhraseBreathTests ${CMAKE_CURRENT_SOURCE_DIR})
    if(Python3_Interpreter_FOUND)
        add_test(NAME processor_wiring COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${CMAKE_CURRENT_SOURCE_DIR}/tests -p test_processor_wiring.py)
    endif()

endif()
