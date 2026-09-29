#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "ModernDarkLookAndFeel.h"
#include <vector>

// ==============================================================================
/**
 * リアルタイム波形・ゲイン補正・レンジ幅描画カスタムComponent
 * (Modern Dark Studio Hardware デザイン)
 */
class WaveformVisualizerComponent : public juce::Component
{
public:
    WaveformVisualizerComponent();
    ~WaveformVisualizerComponent() override = default;

    void pushData (const VisualDataPoint* points, int numPoints);
    void setVisualParams (float targetDb, float rangeDb);
    void setGuiEnabled (bool enabled);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    static constexpr int maxHistoryPoints = 400; // 描画履歴ポイント数
    std::vector<VisualDataPoint> history;
    int writeIndex = 0;
    bool bufferWrapped = false;
    bool guiEnabled = true;

    float currentTargetDb = -12.0f;
    float currentRangeDb  = 6.0f;

    float chartTop    = 36.0f;
    float chartBottom = 480.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformVisualizerComponent)
};

// ==============================================================================
/**
 * 右端に配置するスリムなステレオメーターComponent (IN / OUT)
 * (TARGET LEVEL 同調青白エッジ発光 ＆ エレクトリックブルー〜白熱コアバー)
 */
class SlimMeterComponent : public juce::Component
{
public:
    SlimMeterComponent();
    ~SlimMeterComponent() override = default;

    void updateLevels (float inLinear, float outLinear, int mode);
    void setGuiEnabled (bool enabled);

    void paint (juce::Graphics& g) override;

private:
    int   currentMeterMode = 0; // 0: Peak, 1: RMS, 2: VU
    float inputLevelDb     = -60.0f;
    float outputLevelDb    = -60.0f;
    float inputPeakDb      = -60.0f;
    float outputPeakDb     = -60.0f;
    int   inHoldTimer      = 0;
    int   outHoldTimer     = 0;
    bool  guiEnabled       = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlimMeterComponent)
};

// ==============================================================================
/**
 * トラック内メーター統合型 TARGET LEVEL 縦長フェーダー
 * トラック溝内部にリアルタイムの Input レベル (-36 dBFS ~ 0 dBFS) が
 * エレクトリックブルー〜白熱コアのバーとして直接光り上がるカスタムスライダー
 */
class TargetLevelFaderSlider : public juce::Slider
{
public:
    TargetLevelFaderSlider()
    {
        getProperties().set ("inputMeterDb", -60.0f);
    }
    ~TargetLevelFaderSlider() override = default;

    void setInputMeterLevel (float levelDb)
    {
        if (std::abs (currentInputDb - levelDb) > 0.1f)
        {
            currentInputDb = levelDb;
            getProperties().set ("inputMeterDb", currentInputDb);
            repaint();
        }
    }

    void setGuiEnabled (bool enabled)
    {
        if (guiEnabled != enabled)
        {
            guiEnabled = enabled;
            repaint();
        }
    }

private:
    float currentInputDb = -60.0f;
    bool  guiEnabled     = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TargetLevelFaderSlider)
};

// ==============================================================================
/**
 * AutoLeveler プラグインエディター (GUI)
 * 1000 x 580 px Modern Dark Studio Hardware UI
 */
class AutoLevelerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       public juce::Timer
{
public:
    explicit AutoLevelerAudioProcessorEditor (AutoLevelerAudioProcessor&);
    ~AutoLevelerAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void timerCallback() override;
    void visibilityChanged() override;

private:
    void updateTimerState();
    void updateSyncControlState();

    AutoLevelerAudioProcessor& audioProcessor;

    // カスタム LookAndFeel (実機ラック機材質感＆発光システム)
    ModernDarkLookAndFeel modernDarkLookAndFeel;

    // 1. メイン描画エリア（中央波形 ＆ 右端スリムメーター ＆ メーターモード切替）
    WaveformVisualizerComponent waveformComponent;
    SlimMeterComponent          slimMeterComponent;
    juce::ComboBox              meterModeBox;

    // 2. 左側 CONTROL PANEL コンポーネント
    // SPEED ノブ
    juce::Slider speedSlider;
    juce::Label  speedLabel;
    juce::Label  attackReleaseLabel;

    // RANGE ノブ
    juce::Slider rangeSlider;
    juce::Label  rangeLabel;
    juce::Label  rangeValueLabel;

    // IN GAIN スライダー
    juce::Slider inputGainSlider;
    juce::Label  inputGainLabel;
    juce::Label  inputGainValueLabel;

    // OUT GAIN スライダー
    juce::Slider outputGainSlider;
    juce::Label  outputGainLabel;
    juce::Label  outputGainValueLabel;

    // TARGET LEVEL 縦長フェーダー (トラック内メーター統合型)
    TargetLevelFaderSlider targetLevelSlider;
    juce::Label            targetLevelLabel;
    juce::Label            targetLevelValueLabel;

    // セレクター (DETECTOR, TIMING, BPM SPEED)
    juce::ComboBox detectionModeBox;
    juce::Label    detectionModeLabel;

    juce::ComboBox timingModeBox;
    juce::Label    timingModeLabel;

    juce::ComboBox syncSpeedBox;
    juce::Label    syncSpeedLabel;

    // 下部トグル (LOOKAHEAD, GUI RENDER)
    juce::ToggleButton lookaheadButton;
    juce::ToggleButton guiEnableButton;

    // トップヘッダー内ボタン (BREATH, SIBILANCE, BYPASS)
    juce::ToggleButton breathFilterButton;
    juce::ToggleButton sibilanceFilterButton;
    juce::ToggleButton bypassButton;

    // トップヘッダー新設コンポーネント (プリセット、保存、ズーム、カラー、SNSリンク)
    juce::ComboBox   presetBox;
    juce::TextButton savePresetBtn;
    void refreshPresetBox();

    juce::TextButton zoomOutBtn;
    juce::TextButton zoomInBtn;
    juce::Label      zoomLabel;
    float            currentUiScale = 1.0f;

    juce::TextButton colorThemeBtn;
    bool             isWhiteMode = false;
    void updateThemeColours();

    juce::TextButton xLinkBtn;
    juce::TextButton ytLinkBtn;

    // APVTS アタッチメント
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   inputGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   targetLevelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   rangeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   speedAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   outputGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> detectionModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> timingModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> syncSpeedAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> meterModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   lookaheadAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   breathFilterAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   sibilanceFilterAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   guiEnableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoLevelerAudioProcessorEditor)
};
