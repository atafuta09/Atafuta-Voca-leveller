// ==============================================================================
// PluginEditor.h
// Atafuta09 VocaNoise Learnner: 音響プロファイル学習 & 男女別保存 UI v1.3.1
// ==============================================================================
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace AtafutaAudio::VocaNoiseLearnner {

class Atafuta09VocaNoiseLearnnerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                                      private juce::Timer
{
public:
    explicit Atafuta09VocaNoiseLearnnerAudioProcessorEditor(Atafuta09VocaNoiseLearnnerAudioProcessor&);
    ~Atafuta09VocaNoiseLearnnerAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    Atafuta09VocaNoiseLearnnerAudioProcessor& audioProcessor;

    // APVTS アタッチメント
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    // UI コンポーネント
    juce::ToggleButton listenButton;
    std::unique_ptr<ButtonAttachment> listenAttachment;

    juce::Slider sensitivitySlider;
    juce::Label sensitivityLabel;
    std::unique_ptr<SliderAttachment> sensitivityAttachment;

    juce::ToggleButton sibilanceToggle;
    std::unique_ptr<ButtonAttachment> sibilanceAttachment;

    juce::ToggleButton plosiveToggle;
    std::unique_ptr<ButtonAttachment> plosiveAttachment;

    juce::ToggleButton breathToggle;
    std::unique_ptr<ButtonAttachment> breathAttachment;

    // Learn 操作ボタン
    juce::TextButton learnSibilanceBtn { "LEARN SIBILANCE" };
    juce::TextButton learnBreathBtn    { "LEARN BREATH" };
    juce::TextButton learnNormalBtn    { "LEARN NORMAL" };
    juce::TextButton exportProfilesBtn { "EXPORT" };
    juce::TextButton resetProfilesBtn  { "RESET" };

    // 確認・承認 (男女別仕分け保存) ボタン
    juce::TextButton confirmFemaleBtn { "♀ SAVE AS FEMALE" };
    juce::TextButton confirmMaleBtn   { "♂ SAVE AS MALE" };
    juce::TextButton discardBtn       { "✗ DISCARD" };

    // スムーズ描画用補間値
    float dispNormal = 0.0f;
    float dispSibilance = 0.0f;
    float dispPlosive = 0.0f;
    float dispBreath = 0.0f;
    float triggerGlow = 0.0f;

    int blinkPhase = 0;
    int exportFlashTicks = 0;
    juce::String customStatusMessage;
    int statusMessageTicks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Atafuta09VocaNoiseLearnnerAudioProcessorEditor)
};

} // namespace AtafutaAudio::VocaNoiseLearnner
