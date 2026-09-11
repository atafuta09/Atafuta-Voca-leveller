cmake_minimum_required(VERSION 3.22)

# ==============================================================================
# Atafuta09Leveler - Vocal Dynamics Rider Plugin (v1.0.5)
# With AI TameNoise Detector & Zero-Coloration Sidechain HPF
# ==============================================================================
project(Atafuta09Leveler VERSION 1.0.5 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ビルド後にDAWのVST3フォルダ (C:\Program Files\Common Files\VST3) へ自動コピーする設定
option(COPY_PLUGIN_AFTER_BUILD "Copy plugin to system VST3 directory after build" TRUE)

# ==============================================================================
# JUCE フレームワークの取得・設定
# 1. プロジェクト内に JUCE サブディレクトリがあればそれを使用
# 2. なければ FetchContent により GitHub から JUCE 8 を自動ダウンロード
# ==============================================================================
if(NOT TARGET juce::juce_core)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
        message(STATUS "Using local JUCE submodule at ${CMAKE_CURRENT_SOURCE_DIR}/JUCE")
        add_subdirectory(JUCE)
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
# プラグインターゲットの定義 (VST3)
# ==============================================================================
juce_add_plugin(AutoLeveler
    COMPANY_NAME "AtafutaAudio"
    PLUGIN_MANUFACTURER_CODE Ataf
    PLUGIN_CODE AtLv
    FORMATS VST3
    PRODUCT_NAME "Atafuta09Leveler"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD ${COPY_PLUGIN_AFTER_BUILD}
)

# ソースコードファイルの指定
target_sources(AutoLeveler PRIVATE
    Source/AudioFingerprint.h
    Source/ModernDarkLookAndFeel.h
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp
)

# 必要なJUCEモジュールのリンク
target_link_libraries(AutoLeveler PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# Windows / MSVC 向けエンコーディング・警告設定
if(MSVC)
    target_compile_options(AutoLeveler PRIVATE /utf-8 /W4)
endif()
