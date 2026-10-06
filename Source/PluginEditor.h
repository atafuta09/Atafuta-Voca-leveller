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

    float currentTargetDb = -24.0f;
    float currentRangeDb  = 7.0f;

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

// ==============================================================================
/**
 * TameNoise リアルタイム検出 LED インジケーター Component
 */
class TameNoiseLedComponent : public juce::Component
{
public:
    TameNoiseLedComponent() { setOpaque (false); }
    ~TameNoiseLedComponent() override = default;

    void setIntensity (float val)
    {
        val = juce::jlimit (0.0f, 1.0f, val);
        if (std::abs (intensity - val) > 0.02f)
        {
            intensity = val;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const float cx = bounds.getCentreX();
        const float cy = bounds.getCentreY();
        const float radius = 7.0f; // LEDレンズの基本半径 (直径14px)

        // 1. 発光時の多層グラデーション光彩・ブルーム演出 (Lookahead等と同系統の鮮烈なエレクトリックブルー)
        if (intensity > 0.04f)
        {
            const float actIntensity = std::min (1.0f, intensity * 1.3f);

            // Layer A: 外周の広域アンビエントブルーム (柔らかな光の漏れ・環境光)
            const float bloomRadius = radius * 3.0f;
            juce::ColourGradient bloomGrad (
                juce::Colour (0x3b, 0x82, 0xf6).withAlpha (actIntensity * 0.38f), cx, cy,
                juce::Colour (0x3b, 0x82, 0xf6).withAlpha (0.0f), cx + bloomRadius, cy, true);
            g.setGradientFill (bloomGrad);
            g.fillEllipse (cx - bloomRadius, cy - bloomRadius, bloomRadius * 2.0f, bloomRadius * 2.0f);

            // Layer B: 中間オーラグロー (高彩度シアンの集中光)
            const float auraRadius = radius * 1.85f;
            juce::ColourGradient auraGrad (
                juce::Colour (0x38, 0xbd, 0xf8).withAlpha (actIntensity * 0.72f), cx, cy,
                juce::Colour (0x38, 0xbd, 0xf8).withAlpha (0.0f), cx + auraRadius, cy, true);
            g.setGradientFill (auraGrad);
            g.fillEllipse (cx - auraRadius, cy - auraRadius, auraRadius * 2.0f, auraRadius * 2.0f);
        }

        // 2. 金属ベゼルリング (パネルマウント部: 立体的なダークチタン外枠)
        const float bezelRadius = radius + 2.4f;
        juce::ColourGradient bezelGrad (
            juce::Colour (0x3b, 0x43, 0x54), cx, cy - bezelRadius,
            juce::Colour (0x17, 0x1b, 0x24), cx, cy + bezelRadius, false);
        g.setGradientFill (bezelGrad);
        g.fillEllipse (cx - bezelRadius, cy - bezelRadius, bezelRadius * 2.0f, bezelRadius * 2.0f);

        // ベゼル内側の彫り込みシャドウ (凹み感の演出)
        g.setColour (juce::Colour (0x0f, 0x11, 0x18));
        g.drawEllipse (cx - radius - 0.8f, cy - radius - 0.8f, (radius + 0.8f) * 2.0f, (radius + 0.8f) * 2.0f, 1.0f);

        // 3. LEDレンズ本体 & コアの発光グラデーション描画
        if (intensity > 0.04f)
        {
            const float actIntensity = std::min (1.0f, intensity * 1.3f);

            // レンズ内部のラジアル発光グラデーション (中心: シアン白熱 -> 外周: ディープサファイア)
            juce::ColourGradient lensLit (
                juce::Colour (0xba, 0xe6, 0xfd).interpolatedWith (juce::Colours::white, actIntensity * 0.7f), cx, cy - 0.5f,
                juce::Colour (0x1d, 0x4e, 0xd8), cx + radius, cy + radius, true);
            g.setGradientFill (lensLit);
            g.fillEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

            // 白熱超高輝度ホットスポット (ダイオード中心部)
            const float coreRadius = radius * 0.45f;
            juce::ColourGradient coreGrad (
                juce::Colours::white.withAlpha (std::min (1.0f, actIntensity * 1.5f)), cx, cy - 0.5f,
                juce::Colour (0x93, 0xc5, 0xfd).withAlpha (0.0f), cx + coreRadius * 1.5f, cy, true);
            g.setGradientFill (coreGrad);
            g.fillEllipse (cx - coreRadius * 1.5f, cy - coreRadius * 1.5f - 0.5f, coreRadius * 3.0f, coreRadius * 3.0f);

            // 球面ガラスレンズ特有のスペキュラ反射光 (斜め上方のハイライト)
            g.setColour (juce::Colours::white.withAlpha (std::min (0.90f, 0.45f + actIntensity * 0.5f)));
            g.fillEllipse (cx - radius * 0.45f, cy - radius * 0.50f, radius * 0.42f, radius * 0.28f);
        }
        else
        {
            // 消灯時: 深いダークスレートレンズ (Lookahead消灯時と同色調)
            juce::ColourGradient lensOff (
                juce::Colour (0x1f, 0x26, 0x36), cx, cy - radius * 0.5f,
                juce::Colour (0x11, 0x14, 0x1d), cx, cy + radius * 0.8f, false);
            g.setGradientFill (lensOff);
            g.fillEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

            // 微弱なガラス反射ハイライト
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.drawEllipse (cx - radius + 1.0f, cy - radius + 1.0f, (radius - 1.0f) * 2.0f, (radius - 1.0f) * 2.0f, 0.8f);
            g.fillEllipse (cx - radius * 0.45f, cy - radius * 0.50f, radius * 0.35f, radius * 0.22f);
        }
    }

private:
    float intensity = 0.0f;
};

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

    // セレクター (DETECTOR)
    juce::ComboBox detectionModeBox;
    juce::Label    detectionModeLabel;

    // 下部トグル (LOOKAHEAD, GUI RENDER)
    juce::ToggleButton lookaheadButton;
    juce::ToggleButton guiEnableButton;

    // トップヘッダー内ボタン (SC FILTER, BYPASS)
    juce::ToggleButton scFilterButton;
    juce::ToggleButton bypassButton;

    // TAME NOISE セクション (LED, ON/OFF, LISTEN, AMOUNTノブ)
    TameNoiseLedComponent tameNoiseLed;
    juce::ToggleButton    tameNoiseButton;
    juce::ToggleButton    tameListenButton;
    juce::Slider          tameAmountSlider;
    juce::Label           tameAmountLabel;
    juce::Label           tameAmountValueLabel;

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
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> meterModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   lookaheadAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   scFilterAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   guiEnableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   tameNoiseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   tameListenAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   tameAmountAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoLevelerAudioProcessorEditor)
};
