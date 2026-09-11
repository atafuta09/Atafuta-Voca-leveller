#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_basics/juce_gui_basics.h>
 #include <juce_audio_processors/juce_audio_processors.h>
#endif

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
    void clear();
    void setGuiEnabled (bool enabled);

    void paint (juce::Graphics& g) override;
    void resized() override;

    // TARGET LEVEL スライダーと水平同期するためのY座標マッピング取得
    float getTargetLineY() const;
    float getChartTop() const    { return chartTop; }
    float getChartBottom() const { return chartBottom; }

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
    void resized() override;

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

    float getInputMeterLevel() const { return currentInputDb; }

    void setGuiEnabled (bool enabled)
    {
        if (guiEnabled != enabled)
        {
            guiEnabled = enabled;
            repaint();
        }
    }

    bool isGuiEnabled() const { return guiEnabled; }

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
        const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        const float cornerSize = 4.0f;

        if (intensity > 0.08f)
        {
            // 点灯時：TameNoiseボタンと同系統の鮮烈なアンバー・自照式スイッチ発光
            const float alpha = std::min (1.0f, intensity * 1.25f);
            
            juce::ColourGradient bg (
                juce::Colour (0x78, 0x35, 0x0f).withAlpha (alpha * 0.75f), bounds.getX(), bounds.getY(),
                juce::Colour (0x45, 0x1a, 0x03).withAlpha (alpha * 0.95f), bounds.getX(), bounds.getBottom(), false);
            g.setGradientFill (bg);
            g.fillRoundedRectangle (bounds, cornerSize);

            // 発光枠線
            g.setColour (juce::Colour (0xf5, 0x9e, 0x0b).withAlpha (alpha));
            g.drawRoundedRectangle (bounds, cornerSize, 1.2f);

            // インジケータードット (●)
            const float dotX = bounds.getX() + 18.0f;
            const float dotY = bounds.getCentreY();
            const float dotR = 4.5f;

            // ドット外周グロー
            g.setColour (juce::Colour (0xfb, 0xbf, 0x24).withAlpha (alpha * 0.55f));
            g.fillEllipse (dotX - dotR * 1.8f, dotY - dotR * 1.8f, dotR * 3.6f, dotR * 3.6f);

            // ドット本体 (白熱コア)
            g.setColour (juce::Colour (0xfb, 0xbf, 0x24));
            g.fillEllipse (dotX - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);

            // "NOISE" テキスト
            g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
            g.setColour (juce::Colour (0xfe, 0xf3, 0xc7));
            g.drawText ("NOISE", juce::roundToInt (dotX + dotR + 6.0f), 0, juce::roundToInt (bounds.getWidth() - (dotX + dotR + 8.0f)), getHeight(), juce::Justification::centredLeft, false);
        }
        else
        {
            // 消灯時：TameNoiseボタン等と同じシックな実機ダークスレート
            g.setColour (juce::Colour (0x13, 0x16, 0x1f));
            g.fillRoundedRectangle (bounds, cornerSize);

            g.setColour (juce::Colour (0x26, 0x2b, 0x3a));
            g.drawRoundedRectangle (bounds, cornerSize, 1.0f);

            // 消灯ドット
            const float dotX = bounds.getX() + 18.0f;
            const float dotY = bounds.getCentreY();
            const float dotR = 4.0f;

            g.setColour (juce::Colour (0x33, 0x41, 0x55));
            g.fillEllipse (dotX - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);

            // 消灯テキスト
            g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
            g.setColour (juce::Colour (0x64, 0x74, 0x8b));
            g.drawText ("NOISE", juce::roundToInt (dotX + dotR + 6.0f), 0, juce::roundToInt (bounds.getWidth() - (dotX + dotR + 8.0f)), getHeight(), juce::Justification::centredLeft, false);
        }
    }

private:
    float intensity = 0.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TameNoiseLedComponent)
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
    void updateSyncControlState();
    void syncTargetSliderLayout();

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

    // トップヘッダー内ボタン (SC FILTER, BYPASS)
    juce::ToggleButton scFilterButton;
    juce::ToggleButton bypassButton;

    // TAME NOISE セクション (LED, ON/OFF, LISTEN, AMOUNTノブ, RELEASEノブ)
    TameNoiseLedComponent tameNoiseLed;
    juce::ToggleButton    tameNoiseButton;
    juce::ToggleButton    tameListenButton;
    juce::Slider          tameAmountSlider;
    juce::Label           tameAmountLabel;
    juce::Label           tameAmountValueLabel;
    juce::Slider          tameReleaseSlider;
    juce::Label           tameReleaseLabel;
    juce::Label           tameReleaseValueLabel;

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
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   scFilterAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   guiEnableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   tameNoiseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   tameListenAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   tameAmountAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   tameReleaseAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoLevelerAudioProcessorEditor)
};
