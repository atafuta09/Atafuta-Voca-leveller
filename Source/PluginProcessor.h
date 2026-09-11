#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_audio_processors/juce_audio_processors.h>
 #include <juce_dsp/juce_dsp.h>
#endif

#include "AudioFingerprint.h"
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
    inline constexpr auto detectionMode   = "detection_mode";
    inline constexpr auto timingMode      = "timing_mode";
    inline constexpr auto syncSpeed       = "sync_speed";
    inline constexpr auto meterMode       = "meter_mode";
    inline constexpr auto guiEnable       = "gui_enable";
    inline constexpr auto bypass          = "bypass";

    // TameNoise & サイドチェインフィルター新パラメータ
    inline constexpr auto tameNoiseEnable = "tame_noise_enable";
    inline constexpr auto tameNoiseListen = "tame_noise_listen";
    inline constexpr auto tameNoiseAmount  = "tame_noise_amount";
    inline constexpr auto tameNoiseRelease = "tame_noise_release";
    inline constexpr auto scFilterEnable  = "sc_filter_enable";
}

/**
 * UI描画用データポイント構造体
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
 * ファクトリープリセット情報
 */
struct Preset
{
    juce::String name;
    float inGain = 0.0f;
    float targetLevel = -12.0f;
    float range = 6.0f;
    float speed = 50.0f;
    float outGain = 0.0f;
    bool  lookahead = true;
    bool  tameNoise = true;
    float tameAmount = 30.0f;
    float tameRelease = 50.0f;
    bool  scFilter = true;
    int   detectionMode = 0; // 0: RMS, 1: Peak
    int   timingMode = 0;    // 0: Free, 1: Sync
    int   syncSpeed = 1;     // 0: Fast, 1: Mid, 2: Slow
};

class AutoLevelerAudioProcessor : public juce::AudioProcessor
{
public:
    AutoLevelerAudioProcessor();
    ~AutoLevelerAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }

    // UI波形描画用FIFOインターフェース
    int readVisualData (VisualDataPoint* destination, int maxPointsToRead);

    // メーター用リアルタイム値取得 (Peak, RMS, VU)
    float getLatestInputPeak()  const noexcept { return latestInputPeak.load (std::memory_order_relaxed); }
    float getLatestOutputPeak() const noexcept { return latestOutputPeak.load (std::memory_order_relaxed); }

    float getLatestInputRms()   const noexcept { return latestInputRms.load (std::memory_order_relaxed); }
    float getLatestOutputRms()  const noexcept { return latestOutputRms.load (std::memory_order_relaxed); }

    float getLatestInputVu()    const noexcept { return latestInputVu.load (std::memory_order_relaxed); }
    float getLatestOutputVu()   const noexcept { return latestOutputVu.load (std::memory_order_relaxed); }

    // TameNoise リアルタイム値取得
    float getTameSibilanceScore() const noexcept { return tameNoiseSibilanceScore.load (std::memory_order_relaxed); }
    float getTameBreathScore()    const noexcept { return tameNoiseBreathScore.load (std::memory_order_relaxed); }
    float getTameNormalScore()    const noexcept { return tameNoiseNormalScore.load (std::memory_order_relaxed); }
    bool  isTameNoiseTriggered()   const noexcept { return tameNoiseTrigger.load (std::memory_order_relaxed); }
    float getTameNoiseLedIntensity() const noexcept { return tameNoiseLedIntensity.load (std::memory_order_relaxed); }

    // ホストBPMの取得
    float getCurrentBpm() const noexcept { return currentBpm.load (std::memory_order_relaxed); }

    // プリセットリスト取得
    const std::vector<Preset>& getPresets() const noexcept { return presets; }

    // ユーザープリセット管理
    bool saveUserPreset (const juce::String& presetName);
    void loadUserPresets();
    void saveUserPresetsToFile();

    // Speed値、Timingモード (Free / BPM Sync)、SyncSpeed (Fast:0, Mid:1, Slow:2) からAttack/Release時定数計算
    static inline TimingInfo calculateTiming (float speedVal, bool isSyncMode, int syncSpeedIndex, float hostBpm) noexcept
    {
        TimingInfo info;
        const float bpm = (hostBpm >= 20.0f && hostBpm <= 400.0f) ? hostBpm : 120.0f;
        const float quarterNoteMs = (60.0f / bpm) * 1000.0f;

        if (!isSyncMode)
        {
            // Free モード (ミリ秒)
            const float speedNorm = juce::jlimit (0.0f, 1.0f, speedVal * 0.01f);
            info.attackMs  = 150.0f * std::pow (8.0f / 150.0f, speedNorm);
            info.releaseMs = 800.0f * std::pow (45.0f / 800.0f, speedNorm);
            info.attackLabel  = juce::String (juce::roundToInt (info.attackMs)) + " ms";
            info.releaseLabel = juce::String (juce::roundToInt (info.releaseMs)) + " ms";
            info.modeName     = "Free";
        }
        else
        {
            // BPM Sync モード: Fast(0), Mid(1), Slow(2)
            switch (syncSpeedIndex)
            {
                case 0:
                    info.attackMs  = quarterNoteMs * (1.0f / 16.0f);
                    info.releaseMs = quarterNoteMs * (1.0f / 4.0f);
                    info.attackLabel  = "1/64 (" + juce::String (juce::roundToInt (info.attackMs)) + "ms)";
                    info.releaseLabel = "1/16 (" + juce::String (juce::roundToInt (info.releaseMs)) + "ms)";
                    info.modeName     = "Fast";
                    break;

                case 2:
                    info.attackMs  = quarterNoteMs * (1.0f / 4.0f);
                    info.releaseMs = quarterNoteMs * 1.0f;
                    info.attackLabel  = "1/16 (" + juce::String (juce::roundToInt (info.attackMs)) + "ms)";
                    info.releaseLabel = "1/4 (" + juce::String (juce::roundToInt (info.releaseMs)) + "ms)";
                    info.modeName     = "Slow";
                    break;

                case 1:
                default:
                    info.attackMs  = quarterNoteMs * (1.0f / 8.0f);
                    info.releaseMs = quarterNoteMs * 0.5f;
                    info.attackLabel  = "1/32 (" + juce::String (juce::roundToInt (info.attackMs)) + "ms)";
                    info.releaseLabel = "1/8 (" + juce::String (juce::roundToInt (info.releaseMs)) + "ms)";
                    info.modeName     = "Mid";
                    break;
            }
        }
        return info;
    }

    static constexpr float lookaheadMs = 5.0f; // 5ms Lookahead

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

    // パラメータポインタ
    std::atomic<float>* inputGainParam       = nullptr;
    std::atomic<float>* targetLevelParam     = nullptr;
    std::atomic<float>* rangeParam           = nullptr;
    std::atomic<float>* speedParam           = nullptr;
    std::atomic<float>* outputGainParam      = nullptr;
    std::atomic<float>* lookaheadEnableParam = nullptr;
    std::atomic<float>* detectionModeParam   = nullptr;
    std::atomic<float>* timingModeParam      = nullptr;
    std::atomic<float>* syncSpeedParam       = nullptr;
    std::atomic<float>* meterModeParam       = nullptr;
    std::atomic<float>* guiEnableParam       = nullptr;
    std::atomic<float>* bypassParam          = nullptr;

    std::atomic<float>* tameNoiseEnableParam = nullptr;
    std::atomic<float>* tameNoiseListenParam = nullptr;
    std::atomic<float>* tameNoiseAmountParam = nullptr;
    std::atomic<float>* tameNoiseReleaseParam = nullptr;
    std::atomic<float>* scFilterEnableParam  = nullptr;

    // プリセット管理
    std::vector<Preset> presets;
    int currentProgram = 0;

    // スムージング用
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedInputGainDb;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedOutputGainDb;

    // Lookahead ディレイバッファ (原音スルー用)
    juce::AudioBuffer<float> delayBuffer;
    int delayBufferSize = 0;
    int delayBufferWritePos = 0;
    int lookaheadSamples = 0;

    // 音質変化ゼロのサイドチェイン検出専用 HPF (100Hz)
    juce::dsp::IIR::Filter<float> sidechainDetectorHPF;

    // TameNoise AI 検知エンジン (87データ事前学習済み)
    AtafutaAudio::VocaNoiseLearnner::AudioFingerprintEngine tameNoiseEngine;

    // 16kHz リサンプラー用
    static constexpr double TAME_INTERNAL_SR = 16000.0;
    static constexpr int TAME_BLOCK_SIZE     = 512;
    static constexpr int TAME_HOP_SIZE       = 128;
    std::vector<float> tameRingBuffer16k;
    int tameRingWritePos = 0;
    int tameSamplesSinceLastAnalysis = 0;
    double tameResampleRatio = 1.0;
    double tameResamplePhase = 0.0;
    float tameLastDawSample = 0.0f;

    // TameNoise ソロ試聴用スムージングゲイン
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> tameAuditionGain;

    // TameNoise UI 表示用 Atomic
    std::atomic<float> tameNoiseSibilanceScore { 0.0f };
    std::atomic<float> tameNoiseBreathScore    { 0.0f };
    std::atomic<float> tameNoiseNormalScore    { 0.0f };
    std::atomic<bool>  tameNoiseTrigger        { false };
    std::atomic<float> tameNoiseLedIntensity     { 0.0f };

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

    // UI用ダウンサンプリング
    int downsampleInterval = 480;
    int downsampleCounter = 0;
    float inputAccumSumSquares = 0.0f;
    float outputAccumSumSquares = 0.0f;
    float gainChangeAccumDb = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoLevelerAudioProcessor)
};
