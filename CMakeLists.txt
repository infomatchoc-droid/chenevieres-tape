cmake_minimum_required(VERSION 3.22)

project(ChenevieresTape VERSION 0.3.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

if(MSVC)
    add_compile_options(/utf-8)
endif()

# JUCE : copie locale si JUCE_DIR est donné, sinon téléchargement
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

set(PLUGIN_FORMATS VST3)
if(APPLE)
    list(APPEND PLUGIN_FORMATS AU)
endif()

option(CHNV_AAX "Build the AAX plugin (Pro Tools)" ON)
if(CHNV_AAX AND (APPLE OR WIN32))
    list(APPEND PLUGIN_FORMATS AAX)
endif()

option(CHNV_STANDALONE "Build the standalone app too" OFF)
if(CHNV_STANDALONE)
    list(APPEND PLUGIN_FORMATS Standalone)
endif()

juce_add_plugin(ChenevieresTape
    COMPANY_NAME "Studio des Chenevieres"
    BUNDLE_ID "fr.chenevieres.tape"
    PLUGIN_MANUFACTURER_CODE Chnv
    PLUGIN_CODE Chtp
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Chenevieres Tape"
    DESCRIPTION "Emulation de magnetophone a bandes"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Distortion
    AU_MAIN_TYPE kAudioUnitType_Effect
    AAX_CATEGORY Harmonic
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(ChenevieresTape)

juce_add_binary_data(ChenevieresTapeData SOURCES
    Resources/wordmark_tape.png
    Resources/logo_beagle.png
    Resources/fonts/RobotoCondensed-500.ttf
    Resources/fonts/RobotoCondensed-600.ttf
    Resources/fonts/RobotoCondensed-700.ttf)

target_sources(ChenevieresTape PRIVATE
    Source/TapeEngine.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(ChenevieresTape PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(ChenevieresTape
    PRIVATE
        ChenevieresTapeData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(NOT MSVC)
    target_compile_options(ChenevieresTape PRIVATE -Wno-float-equal)
endif()

# Tests et mesures du moteur (sans JUCE) : ctest
enable_testing()
add_executable(EngineTest tests/EngineTest.cpp Source/TapeEngine.cpp)
add_test(NAME EngineTest COMMAND EngineTest)
add_executable(TapeMeasure tests/Measure.cpp Source/TapeEngine.cpp)

# Capture de l'interface (docs/apercu.png) : -DCHNV_TESTS=ON
option(CHNV_TESTS "Build the snapshot tool" OFF)
if(CHNV_TESTS)
    add_executable(TapeSnapshot tests/Snapshot.cpp)
    target_link_libraries(TapeSnapshot PRIVATE ChenevieresTape)
    target_include_directories(TapeSnapshot PRIVATE $<TARGET_PROPERTY:ChenevieresTape,INCLUDE_DIRECTORIES>)
    target_compile_definitions(TapeSnapshot PRIVATE $<TARGET_PROPERTY:ChenevieresTape,COMPILE_DEFINITIONS>)
endif()
if(CHNV_TESTS)
    add_executable(TapeProcTest tests/ProcTest.cpp)
    target_link_libraries(TapeProcTest PRIVATE ChenevieresTape)
    target_include_directories(TapeProcTest PRIVATE $<TARGET_PROPERTY:ChenevieresTape,INCLUDE_DIRECTORIES>)
    target_compile_definitions(TapeProcTest PRIVATE $<TARGET_PROPERTY:ChenevieresTape,COMPILE_DEFINITIONS>)
endif()
if(CHNV_TESTS)
    add_executable(TapeRender tests/Render.cpp)
    target_link_libraries(TapeRender PRIVATE ChenevieresTape)
    target_include_directories(TapeRender PRIVATE $<TARGET_PROPERTY:ChenevieresTape,INCLUDE_DIRECTORIES>)
    target_compile_definitions(TapeRender PRIVATE $<TARGET_PROPERTY:ChenevieresTape,COMPILE_DEFINITIONS>)
endif()
