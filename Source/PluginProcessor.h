#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <vector>
#include <atomic>
#include <cmath>

namespace ParameterIDs
{
    inline constexpr auto inputGain       = "input_gain";
    inline constexpr auto targetLevel     = "target_level";
    inline constexpr auto range           = "range";
    inline constexpr auto speed           = "speed";
    inline constexpr auto outputGain      = "output_gain";
    inline constexpr auto lookaheadEnable = "lookahead_enable";
    inline constexpr auto breathFilter    = "breath_filter";
    inline constexpr auto sibilanceFilter = "sibilance_filter";
    inline constexpr auto detectionMode   = "detection_mode";
    inline constexpr auto timingMode      = "timing_mode";
    inline constexpr auto syncSpeed       = "sync_speed";
    inline constexpr auto meterMode       = "meter_mode";
    inline constexpr auto guiEnable       = "gui_enable";
    inline constexpr auto bypass          = "bypass";
}

/**
 * UI描画用データポイント構造体
 * ダウンサンプリングされた波形RMS値およびゲイン補正量を保持します
 */
struct VisualDataPoint
{
    float inputRms = 0.0f;       // 入力RMSレベル (0.0f ~ 1.0f+)
    float outputRms = 0.0f;      // 出力RMSレベル (0.0f ~ 1.0f+)
    float gainChangeDb = 0.0f;   // ゲイン補正量 (dB単位)
};

/**
 * アタック・リリースの時定数およびUI表示情報
 */
struct TimingInfo
{
    float attackMs = 30.0f;
    float releaseMs = 200.0f;
    juce::String attackLabel = "30 ms";
    juce::String releaseLabel = "200 ms";
    juce::String modeName = "Free";
};

/**
 * アタック・リリースの時定数のみ (オーディオスレッド用、メモリ確保なし)
 */
struct TimingMs
{
    float attackMs;
    float releaseMs;
};

/**
 * プリセットに含めるパラメーターと、ユーザープリセット XML での属性名
 * (属性名は既存の user_presets.xml との互換のため変えない)
 */
struct PresetParam
{
    const char* paramID;
    const char* xmlAttribute;
};

inline constexpr std::array<PresetParam, 11> presetParams {{
    { ParameterIDs::inputGain,       "inGain"    },
    { ParameterIDs::targetLevel,     "target"    },
    { ParameterIDs::range,           "range"     },
    { ParameterIDs::speed,           "speed"     },
    { ParameterIDs::outputGain,      "outGain"   },
    { ParameterIDs::lookaheadEnable, "lookahead" },
    { ParameterIDs::breathFilter,    "breath"    },
    { ParameterIDs::sibilanceFilter, "sibilance" },
    { ParameterIDs::detectionMode,   "det"       }, // 0: RMS, 1: Peak
    { ParameterIDs::timingMode,      "timing"    }, // 0: Free, 1: Sync
    { ParameterIDs::syncSpeed,       "syncSpeed" }, // 0: Fast, 1: Mid, 2: Slow
}};

/**
 * プリセット情報 (values は presetParams と同じ順の実値)
 */
struct Preset
{
    juce::String name;
    std::array<float, presetParams.size()> values;
};

class AutoLevelerAudioProcessor : public juce::AudioProcessor
{
public:
    AutoLevelerAudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override           { return false; }
    bool producesMidi() const override          { return false; }
    bool isMidiEffect() const override          { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }

    // UI連携用ロックフリーFIFOインターフェース
    int readVisualData (VisualDataPoint* destination, int maxPointsToRead);

    // メーター用リアルタイム値取得 (Peak, RMS, VU)
    float getLatestInputPeak()  const noexcept { return latestInputPeak.load (std::memory_order_relaxed); }
    float getLatestOutputPeak() const noexcept { return latestOutputPeak.load (std::memory_order_relaxed); }

    float getLatestInputRms()   const noexcept { return latestInputRms.load (std::memory_order_relaxed); }
    float getLatestOutputRms()  const noexcept { return latestOutputRms.load (std::memory_order_relaxed); }

    float getLatestInputVu()    const noexcept { return latestInputVu.load (std::memory_order_relaxed); }
    float getLatestOutputVu()   const noexcept { return latestOutputVu.load (std::memory_order_relaxed); }

    // ホストBPMの取得
    float getCurrentBpm() const noexcept { return currentBpm.load (std::memory_order_relaxed); }

    // プリセットリスト取得
    const std::vector<Preset>& getPresets() const noexcept { return presets; }

    // ユーザープリセット管理
    bool saveUserPreset (const juce::String& presetName);
    void loadUserPresets();
    void saveUserPresetsToFile();

    // Speed値、Timingモード (Free / BPM Sync)、SyncSpeed (Fast:0, Mid:1, Slow:2) からAttack/Release時定数 (ms) を計算
    // オーディオスレッドから呼ぶため、文字列などのメモリ確保をしない
    static inline TimingMs calculateTimingMs (float speedVal, bool isSyncMode, int syncSpeedIndex, float hostBpm) noexcept
    {
        if (!isSyncMode)
        {
            // Free モード (ミリ秒)
            const float speedNorm = juce::jlimit (0.0f, 1.0f, speedVal * 0.01f);
            return { 150.0f * std::pow (8.0f / 150.0f, speedNorm),
                     800.0f * std::pow (45.0f / 800.0f, speedNorm) };
        }

        const float bpm = (hostBpm >= 20.0f && hostBpm <= 400.0f) ? hostBpm : 120.0f;
        const float quarterNoteMs = (60.0f / bpm) * 1000.0f;

        switch (syncSpeedIndex)
        {
            case 0:  return { quarterNoteMs / 16.0f, quarterNoteMs / 4.0f }; // Fast: 1/64, 1/16
            case 2:  return { quarterNoteMs / 4.0f,  quarterNoteMs };        // Slow: 1/16, 1/4
            default: return { quarterNoteMs / 8.0f,  quarterNoteMs / 2.0f }; // Mid:  1/32, 1/8
        }
    }

    // UI 表示用: 時定数に加えてラベル文字列を組み立てる (メッセージスレッド専用)
    static inline TimingInfo calculateTiming (float speedVal, bool isSyncMode, int syncSpeedIndex, float hostBpm)
    {
        const auto ms = calculateTimingMs (speedVal, isSyncMode, syncSpeedIndex, hostBpm);
        TimingInfo info;
        info.attackMs  = ms.attackMs;
        info.releaseMs = ms.releaseMs;

        const juce::String attackMsText  = juce::String (juce::roundToInt (ms.attackMs));
        const juce::String releaseMsText = juce::String (juce::roundToInt (ms.releaseMs));

        if (!isSyncMode)
        {
            info.attackLabel  = attackMsText + " ms";
            info.releaseLabel = releaseMsText + " ms";
            info.modeName     = "Free";
            return info;
        }

        // Fast(0), Mid(1), Slow(2)。範囲外は calculateTimingMs と同じく Mid 扱い
        const int idx = (syncSpeedIndex == 0 || syncSpeedIndex == 2) ? syncSpeedIndex : 1;
        const char* modeNames[]    = { "Fast", "Mid", "Slow" };
        const char* attackNotes[]  = { "1/64", "1/32", "1/16" };
        const char* releaseNotes[] = { "1/16", "1/8",  "1/4"  };

        info.attackLabel  = juce::String (attackNotes[idx])  + " (" + attackMsText  + "ms)";
        info.releaseLabel = juce::String (releaseNotes[idx]) + " (" + releaseMsText + "ms)";
        info.modeName     = modeNames[idx];
        return info;
    }

    static constexpr float lookaheadMs = 22.5f; // 22.5ms Lookahead (48kHz で 1080 サンプル)

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

    // パラメーター参照ポインタ (オーディオスレッド用、コンストラクタで必ず非 null になる)
    std::atomic<float>* inputGainParam       = nullptr;
    std::atomic<float>* targetLevelParam     = nullptr;
    std::atomic<float>* rangeParam           = nullptr;
    std::atomic<float>* speedParam           = nullptr;
    std::atomic<float>* outputGainParam      = nullptr;
    std::atomic<float>* lookaheadEnableParam = nullptr;
    std::atomic<float>* breathFilterParam    = nullptr;
    std::atomic<float>* sibilanceFilterParam = nullptr;
    std::atomic<float>* detectionModeParam   = nullptr;
    std::atomic<float>* timingModeParam      = nullptr;
    std::atomic<float>* syncSpeedParam       = nullptr;
    std::atomic<float>* guiEnableParam       = nullptr;
    std::atomic<float>* bypassParam          = nullptr;

    // プリセット管理 (先頭 numFactoryPresets 件がファクトリープリセット、以降がユーザープリセット)
    static constexpr size_t numFactoryPresets = 8;
    std::vector<Preset> presets;
    int currentProgram = 0;

    // スムージング用
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedInputGainDb;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedOutputGainDb;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bypassMix; // 0 = 処理音, 1 = ドライ
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> lookaheadMix; // 0 = 遅延なし, 1 = 22.5ms 先読み

    // Lookahead ディレイ (Input Gain 前の原音を遅らせる)
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> lookaheadDelay;
    int lookaheadSamples = 0;

    // ブレス除去 (HPF 150Hz) および 歯擦音除去 (LPF 4000Hz) 用サイドチェーンフィルター
    juce::dsp::IIR::Filter<float> sidechainHPF;
    juce::dsp::IIR::Filter<float> sidechainLPF;

    // ボーカルオートレベラー DSPステート
    float fastEnvelopeRms = 0.0f;
    float fastEnvelopePeak = 0.0f;
    float currentSmoothedGainDb = 0.0f;
    double currentSampleRate = 44100.0;

    // メーター用最新値 (Peak, RMS, VU)
    std::atomic<float> latestInputPeak  { 0.0f };
    std::atomic<float> latestOutputPeak { 0.0f };
    std::atomic<float> latestInputRms   { 0.0f };
    std::atomic<float> latestOutputRms  { 0.0f };
    std::atomic<float> latestInputVu    { 0.0f };
    std::atomic<float> latestOutputVu   { 0.0f };

    // スムージング用RMS / VUバリスティクス
    float smoothedRmsIn  = 0.0f;
    float smoothedRmsOut = 0.0f;
    float smoothedVuIn   = 0.0f;
    float smoothedVuOut  = 0.0f;

    // BPM
    std::atomic<float> currentBpm { 120.0f };

    // リアルタイム波形描画用 FIFO
    static constexpr int fifoCapacity = 4096;
    juce::AbstractFifo visualFifo { fifoCapacity };
    std::vector<VisualDataPoint> visualFifoBuffer;

    // UI用ダウンサンプリング (滑らかな描画のため約100Hzで送信)
    int downsampleInterval = 480;
    int downsampleCounter = 0;
    float inputAccumSumSquares = 0.0f;
    float outputAccumSumSquares = 0.0f;
    float gainChangeAccumDb = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoLevelerAudioProcessor)
};
