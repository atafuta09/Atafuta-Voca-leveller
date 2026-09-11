// ==============================================================================
// PluginProcessor.h
// Atafuta09 VocaNoise Learnner: 音響プロファイル学習 & 照合 VST3
// ==============================================================================
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "AudioFingerprint.h"
#include <atomic>

namespace AtafutaAudio::VocaNoiseLearnner {

class Atafuta09VocaNoiseLearnnerAudioProcessor : public juce::AudioProcessor {
public:
    Atafuta09VocaNoiseLearnnerAudioProcessor();
    ~Atafuta09VocaNoiseLearnnerAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Atafuta09 VocaNoise Learnner"; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Learn 操作メソッド
    void triggerLearnSibilance() { fingerprintEngine.startLearnSibilance(); }
    void triggerLearnBreath()    { fingerprintEngine.startLearnBreath(); }
    void triggerLearnNormal()    { fingerprintEngine.startLearnNormal(); }
    void triggerResetProfiles()  { fingerprintEngine.resetToDefaults(); }

    // ユーザー確認・承認メソッド (男女別仕分け保存)
    void confirmPendingAsFemale() { fingerprintEngine.confirmPendingProfile(learnedDataDir, "female"); }
    void confirmPendingAsMale()   { fingerprintEngine.confirmPendingProfile(learnedDataDir, "male"); }
    void discardPending()         { fingerprintEngine.discardPendingProfile(); }
    int exportActiveProfiles(const juce::String& gender = "female") { return fingerprintEngine.exportActiveProfiles(learnedDataDir, gender); }

    bool isLearningSib() const noexcept       { return fingerprintEngine.isLearningSibilance(); }
    bool isLearningBr() const noexcept        { return fingerprintEngine.isLearningBreath(); }
    bool isLearningNorm() const noexcept      { return fingerprintEngine.isLearningNormal(); }
    bool isPendingConfirm() const noexcept    { return fingerprintEngine.isPendingConfirmation(); }
    int  getPendingClass() const noexcept     { return fingerprintEngine.getPendingClassType(); }

    bool hasCustomSib() const noexcept        { return fingerprintEngine.hasCustomSibProfile(); }
    bool hasCustomBr() const noexcept         { return fingerprintEngine.hasCustomBrProfile(); }
    bool hasCustomNorm() const noexcept       { return fingerprintEngine.hasCustomNormProfile(); }
    int  getExportCount() const noexcept      { return fingerprintEngine.getExportCount(); }

    // UI メーター用のスレッドセーフな値
    float getNormalScore() const noexcept    { return meterNormal.load(std::memory_order_relaxed); }
    float getSibilanceScore() const noexcept { return meterSibilance.load(std::memory_order_relaxed); }
    float getPlosiveScore() const noexcept   { return meterPlosive.load(std::memory_order_relaxed); }
    float getBreathScore() const noexcept    { return meterBreath.load(std::memory_order_relaxed); }
    bool isNoiseActive() const noexcept      { return meterNoiseTrigger.load(std::memory_order_relaxed); }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    static constexpr double INTERNAL_SR = 16000.0;
    static constexpr int BLOCK_SIZE     = 512;
    static constexpr int HOP_SIZE       = 128;

    AudioFingerprintEngine fingerprintEngine;
    juce::String learnedDataDir;

    // 16kHz リングバッファ
    std::vector<float> ringBuffer16k;
    int ringWritePos = 0;
    int samplesSinceLastAnalysis = 0;

    // リサンプラー用
    double currentDawSampleRate = 44100.0;
    double resampleRatio = 1.0;
    double resamplePhase = 0.0;
    float lastDawSample = 0.0f;

    // 可聴 (Listen) スムージングゲイン
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> auditionGain;

    // UI メーター用 Atomic 変数
    std::atomic<float> meterNormal { 0.0f };
    std::atomic<float> meterSibilance { 0.0f };
    std::atomic<float> meterPlosive { 0.0f };
    std::atomic<float> meterBreath { 0.0f };
    std::atomic<bool>  meterNoiseTrigger { false };

    // パラメータキャッシュ
    std::atomic<float>* listenParam = nullptr;
    std::atomic<float>* sensitivityParam = nullptr;
    std::atomic<float>* detectSibilanceParam = nullptr;
    std::atomic<float>* detectPlosiveParam = nullptr;
    std::atomic<float>* detectBreathParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Atafuta09VocaNoiseLearnnerAudioProcessor)
};

} // namespace AtafutaAudio::VocaNoiseLearnner
