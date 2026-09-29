cmake_minimum_required(VERSION 3.22)

# ==============================================================================
# Atafuta09Leveler - Vocal Dynamics Rider Plugin
# ==============================================================================
# macOS: Apple Silicon / Intel 両対応のユニバーサルバイナリ、対応 OS は macOS 12 以降（現行 Xcode の下限）
# （project() より前に設定する必要がある。-D で上書き可能）
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS build architectures")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")
endif()

project(Atafuta09Leveler VERSION 1.0.4 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ビルド後にシステムのプラグインフォルダへ自動コピーする設定
#   Windows: C:\Program Files\Common Files\VST3
#   macOS  : ~/Library/Audio/Plug-Ins/VST3 および ~/Library/Audio/Plug-Ins/Components (AU)
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
# プラグインターゲットの定義 (Windows: VST3 / macOS: VST3 + AU)
# ==============================================================================
if(APPLE)
    set(ATAFUTA_PLUGIN_FORMATS VST3 AU)
else()
    set(ATAFUTA_PLUGIN_FORMATS VST3)
endif()

juce_add_plugin(AutoLeveler
    COMPANY_NAME "Atafuta09"
    PLUGIN_MANUFACTURER_CODE Ataf
    PLUGIN_CODE AtLv
    FORMATS ${ATAFUTA_PLUGIN_FORMATS}
    BUNDLE_ID "com.atafutaaudio.atafuta09leveler"
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Dynamics
    PRODUCT_NAME "Atafuta09Leveler"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD ${COPY_PLUGIN_AFTER_BUILD}
)

# ソースコードファイルの指定
target_sources(AutoLeveler PRIVATE
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
