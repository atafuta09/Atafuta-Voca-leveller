cmake_minimum_required(VERSION 3.22)

# ==============================================================================
# Atafuta09 VocaNoise Learnner
# AI Vocal Noise & Acoustic Profile Learning VST3 Plugin
# ==============================================================================
project(Atafuta09VocaNoiseLearnner VERSION 1.3.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(COPY_PLUGIN_AFTER_BUILD "Copy plugin to system VST3 directory after build" TRUE)

# ==============================================================================
# JUCE Framework Configuration
# ==============================================================================
if(NOT TARGET juce::juce_core)
    set(LOCAL_JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../build/_deps/juce-src")
    if(EXISTS "${LOCAL_JUCE_PATH}/CMakeLists.txt")
        message(STATUS "Using cached JUCE at ${LOCAL_JUCE_PATH}")
        add_subdirectory("${LOCAL_JUCE_PATH}" "${CMAKE_CURRENT_BINARY_DIR}/juce")
    else()
        include(FetchContent)
        message(STATUS "Fetching JUCE 8 from GitHub...")
        FetchContent_Declare(
            JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 8.0.4
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(JUCE)
    endif()
endif()

# ==============================================================================
# Plugin Target Definition (VST3)
# ==============================================================================
juce_add_plugin(Atafuta09VocaNoiseLearnner
    COMPANY_NAME "AtafutaAudio"
    PLUGIN_MANUFACTURER_CODE Ataf
    PLUGIN_CODE AtVn
    FORMATS VST3
    PRODUCT_NAME "Atafuta09 VocaNoise Learnner"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD ${COPY_PLUGIN_AFTER_BUILD}
)

# Source Files
target_sources(Atafuta09VocaNoiseLearnner PRIVATE
    Source/AudioFingerprint.h
    Source/ModelWeights.h
    Source/MelSpectrogram.h
    Source/InferenceEngine.h
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp
)

# Linked JUCE Modules
target_link_libraries(Atafuta09VocaNoiseLearnner PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# MSVC UTF-8
if(MSVC)
    target_compile_options(Atafuta09VocaNoiseLearnner PRIVATE /utf-8 /W3)
endif()
