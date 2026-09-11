// ==============================================================================
// PluginEditor.cpp
// Atafuta09 VocaNoise Learnner: 音響プロファイル学習 & 男女別保存 UI v1.3.2
// ==============================================================================
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace AtafutaAudio::VocaNoiseLearnner {

Atafuta09VocaNoiseLearnnerAudioProcessorEditor::Atafuta09VocaNoiseLearnnerAudioProcessorEditor(Atafuta09VocaNoiseLearnnerAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(560, 510);

    // 1. Listen ボタン (大型トグル)
    listenButton.setButtonText("NOISE LISTEN (SOLO)");
    listenButton.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    addAndMakeVisible(listenButton);
    listenAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "listen", listenButton);

    // 2. Sensitivity スライダー (ロータリー)
    sensitivitySlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    sensitivitySlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    sensitivitySlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff06b6d4));
    sensitivitySlider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff2d3748));
    sensitivitySlider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
    sensitivitySlider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    sensitivitySlider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(sensitivitySlider);
    sensitivityAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "sensitivity", sensitivitySlider);

    sensitivityLabel.setText("Sensitivity", juce::dontSendNotification);
    sensitivityLabel.setJustificationType(juce::Justification::centred);
    sensitivityLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    sensitivityLabel.setFont(juce::FontOptions(13.0f).withStyle("Bold"));
    addAndMakeVisible(sensitivityLabel);

    // 3. クラス別トグル
    auto setupToggle = [this](juce::ToggleButton& btn, const juce::String& text, juce::Colour col)
    {
        btn.setButtonText(text);
        btn.setColour(juce::ToggleButton::textColourId, col);
        btn.setColour(juce::ToggleButton::tickColourId, col);
        addAndMakeVisible(btn);
    };

    setupToggle(sibilanceToggle, "Sibilance (s, sh, z)", juce::Colour(0xff06b6d4));
    sibilanceAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "detectSibilance", sibilanceToggle);

    setupToggle(plosiveToggle, "Plosive (k, p, t...)", juce::Colour(0xfff97316));
    plosiveAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "detectPlosive", plosiveToggle);

    setupToggle(breathToggle, "Breath (in/out/quick)", juce::Colour(0xffc084fc));
    breathAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "detectBreath", breathToggle);

    // 4. Learn ボタン群
    learnSibilanceBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0e7490));
    learnSibilanceBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    learnSibilanceBtn.onClick = [this] { audioProcessor.triggerLearnSibilance(); };
    addAndMakeVisible(learnSibilanceBtn);

    learnBreathBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff7e22ce));
    learnBreathBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    learnBreathBtn.onClick = [this] { audioProcessor.triggerLearnBreath(); };
    addAndMakeVisible(learnBreathBtn);

    learnNormalBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff4338ca)); // インディゴ/バイオレット
    learnNormalBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    learnNormalBtn.onClick = [this] { audioProcessor.triggerLearnNormal(); };
    addAndMakeVisible(learnNormalBtn);

    // エクスポートボタン
    exportProfilesBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0284c7)); // シアン/ブルー
    exportProfilesBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    exportProfilesBtn.onClick = [this] {
        int cnt = audioProcessor.exportActiveProfiles("female");
        int curNum = audioProcessor.getExportCount();
        exportProfilesBtn.setButtonText(juce::String(juce::CharPointer_UTF8("\xe2\x9c\x93")) + " SAVED #" + juce::String(curNum));
        exportProfilesBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff059669));
        exportFlashTicks = 45;
        customStatusMessage = "EXPORTED #" + juce::String(curNum) + " (" + juce::String(cnt) + " profiles) to learned_data/female/";
        statusMessageTicks = 60;
    };
    addAndMakeVisible(exportProfilesBtn);

    resetProfilesBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff334155));
    resetProfilesBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffcbd5e1));
    resetProfilesBtn.onClick = [this] { 
        audioProcessor.triggerResetProfiles();
        customStatusMessage = "RESET TO FACTORY TEMPLATES";
        statusMessageTicks = 45;
    };
    addAndMakeVisible(resetProfilesBtn);

    // 5. 確認・承認 (男女別仕分け保存) ボタン
    confirmFemaleBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffdb2777)); // ローズピンク
    confirmFemaleBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    confirmFemaleBtn.onClick = [this] { 
        audioProcessor.confirmPendingAsFemale();
        customStatusMessage = "PROFILE SAVED TO learned_data/female/";
        statusMessageTicks = 60;
    };
    addChildComponent(confirmFemaleBtn);

    confirmMaleBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0284c7)); // スカイブルー
    confirmMaleBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    confirmMaleBtn.onClick = [this] { 
        audioProcessor.confirmPendingAsMale();
        customStatusMessage = "PROFILE SAVED TO learned_data/male/";
        statusMessageTicks = 60;
    };
    addChildComponent(confirmMaleBtn);

    discardBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffdc2626)); // レッド
    discardBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    discardBtn.onClick = [this] { 
        audioProcessor.discardPending();
        customStatusMessage = "PROFILE DISCARDED";
        statusMessageTicks = 45;
    };
    addChildComponent(discardBtn);

    startTimerHz(30);
}

Atafuta09VocaNoiseLearnnerAudioProcessorEditor::~Atafuta09VocaNoiseLearnnerAudioProcessorEditor()
{
    stopTimer();
}

void Atafuta09VocaNoiseLearnnerAudioProcessorEditor::timerCallback()
{
    const float k = 0.35f;
    dispNormal    += k * (audioProcessor.getNormalScore() - dispNormal);
    dispSibilance += k * (audioProcessor.getSibilanceScore() - dispSibilance);
    dispPlosive   += k * (audioProcessor.getPlosiveScore() - dispPlosive);
    dispBreath    += k * (audioProcessor.getBreathScore() - dispBreath);

    const float targetGlow = audioProcessor.isNoiseActive() ? 1.0f : 0.0f;
    triggerGlow += 0.25f * (targetGlow - triggerGlow);

    blinkPhase = (blinkPhase + 1) % 30;

    if (exportFlashTicks > 0)
    {
        exportFlashTicks--;
        if (exportFlashTicks == 0)
        {
            exportProfilesBtn.setButtonText("EXPORT");
            exportProfilesBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0284c7));
        }
    }

    if (statusMessageTicks > 0)
        statusMessageTicks--;

    const bool isPending = audioProcessor.isPendingConfirm();
    learnSibilanceBtn.setVisible(!isPending);
    learnBreathBtn.setVisible(!isPending);
    learnNormalBtn.setVisible(!isPending);
    exportProfilesBtn.setVisible(!isPending);
    resetProfilesBtn.setVisible(!isPending);

    confirmFemaleBtn.setVisible(isPending);
    confirmMaleBtn.setVisible(isPending);
    discardBtn.setVisible(isPending);

    if (isPending)
    {
        const int pClass = audioProcessor.getPendingClass();
        juce::String clsName = "NOISE";
        if (pClass == 1) clsName = "SIBILANCE";
        else if (pClass == 2) clsName = "BREATH";
        else if (pClass == 3) clsName = "NORMAL";

        confirmFemaleBtn.setButtonText(juce::String(juce::CharPointer_UTF8("\xe2\x99\x80 ")) + "SAVE FEMALE " + clsName);
        confirmMaleBtn.setButtonText(juce::String(juce::CharPointer_UTF8("\xe2\x99\x82 ")) + "SAVE MALE " + clsName);
        discardBtn.setButtonText(juce::String(juce::CharPointer_UTF8("\xe2\x9c\x97 ")) + "DISCARD");
    }

    repaint();
}

void Atafuta09VocaNoiseLearnnerAudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient bg(juce::Colour(0xff0f172a), 0, 0, juce::Colour(0xff020617), 0, (float)getHeight(), false);
    g.setGradientFill(bg);
    g.fillAll();

    // ヘッダーバー
    g.setColour(juce::Colour(0xff1e293b));
    g.fillRect(0, 0, getWidth(), 46);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(15.0f).withStyle("Bold"));
    g.drawText("ATAFUTA09 VOCANOISE LEARNNER v1.3.2", 20, 0, 350, 46, juce::Justification::centredLeft);

    const int userLoaded = audioProcessor.getUserProfileCount();
    g.setColour(juce::Colour(0xff38bdf8));
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("87 Base + " + juce::String(userLoaded) + " User Profiles", getWidth() - 260, 0, 240, 46, juce::Justification::centredRight);

    // 4 連メーター
    const int meterY = 56;
    const int meterH = 138;
    const int meterW = (getWidth() - 50) / 4;

    auto drawMeter = [&](int idx, const juce::String& title, float val, juce::Colour col, bool isCustom, bool isLearning)
    {
        const int x = 20 + idx * (meterW + 3);
        
        g.setColour(juce::Colour(0xff1e293b));
        g.fillRoundedRectangle((float)x, (float)meterY, (float)meterW, (float)meterH, 6.0f);

        const int barW = meterW - 16;
        const int barMaxH = meterH - 48;
        const int barH = juce::jlimit(0, barMaxH, (int)(val * barMaxH));
        const int barX = x + 8;
        const int barY = meterY + 8 + (barMaxH - barH);

        g.setColour(juce::Colour(0xff090d16));
        g.fillRoundedRectangle((float)barX, (float)(meterY + 8), (float)barW, (float)barMaxH, 4.0f);

        if (barH > 0)
        {
            g.setColour(col.withAlpha(0.85f));
            g.fillRoundedRectangle((float)barX, (float)barY, (float)barW, (float)barH, 4.0f);
        }

        g.setColour(juce::Colour(0xff94a3b8));
        g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
        g.drawText(title, x, meterY + meterH - 34, meterW, 16, juce::Justification::centred);

        if (isLearning)
        {
            const int pct = juce::roundToInt(audioProcessor.getLearningProgress() * 100.0f);
            const bool blink = (blinkPhase < 15);
            g.setColour(blink ? juce::Colour(0xfff87171) : juce::Colours::white);
            g.drawText("LEARNING (" + juce::String(pct) + "%)", x, meterY + meterH - 18, meterW, 14, juce::Justification::centred);
        }
        else
        {
            g.setColour(isCustom ? juce::Colour(0xff34d399) : juce::Colour(0xff64748b));
            const juce::String sub = isCustom ? "CUSTOM" : "FEMALE";
            g.drawText(sub + " " + juce::String((int)(val * 100)) + "%", x, meterY + meterH - 18, meterW, 14, juce::Justification::centred);
        }
    };

    drawMeter(0, "NORMAL", dispNormal, juce::Colour(0xff10b981), audioProcessor.hasCustomNorm(), audioProcessor.isLearningNorm());
    drawMeter(1, "SIBILANCE", dispSibilance, juce::Colour(0xff06b6d4), audioProcessor.hasCustomSib(), audioProcessor.isLearningSib());
    drawMeter(2, "PLOSIVE", dispPlosive, juce::Colour(0xfff97316), false, false);
    drawMeter(3, "BREATH", dispBreath, juce::Colour(0xffc084fc), audioProcessor.hasCustomBr(), audioProcessor.isLearningBr());

    // Listen ボタン
    const auto lBounds = listenButton.getBounds();
    const bool isListening = (listenButton.getToggleState());

    if (isListening)
    {
        g.setColour(juce::Colour(0xffdc2626));
        g.fillRoundedRectangle(lBounds.toFloat(), 8.0f);
        g.setColour(juce::Colours::white);
        g.drawRoundedRectangle(lBounds.toFloat(), 8.0f, 2.0f);
    }
    else
    {
        g.setColour(juce::Colour(0xff1e293b));
        g.fillRoundedRectangle(lBounds.toFloat(), 8.0f);
        g.setColour(juce::Colour(0xff475569));
        g.drawRoundedRectangle(lBounds.toFloat(), 8.0f, 1.0f);
    }

    if (triggerGlow > 0.05f)
    {
        g.setColour(juce::Colour(0xfff43f5e).withAlpha(triggerGlow * 0.4f));
        g.drawRoundedRectangle(lBounds.toFloat().expanded(2.0f), 10.0f, 3.0f);
    }

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(15.0f).withStyle("Bold"));
    const juce::String lText = isListening 
        ? juce::String(juce::CharPointer_UTF8("\xe2\x97\x8f ")) + "SOLO AUDITION ACTIVE (NOISE ONLY)" 
        : "NOISE LISTEN (SOLO) - AUDITION TRIGGERED NOISE";
    g.drawText(lText, lBounds, juce::Justification::centred);

    // 下部コントロールパネル
    const int panelY = 318;
    g.setColour(juce::Colour(0xff1e293b).withAlpha(0.6f));
    g.fillRoundedRectangle(20.0f, (float)panelY, (float)(getWidth() - 40), 140.0f, 8.0f);

    // フッターステータスバー
    g.setColour(juce::Colour(0xff090d16));
    g.fillRect(0, getHeight() - 32, getWidth(), 32);

    g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
    if (statusMessageTicks > 0)
    {
        g.setColour(juce::Colour(0xff34d399));
        g.drawText(juce::String(juce::CharPointer_UTF8("\xe2\x9c\x93 ")) + customStatusMessage, 20, getHeight() - 32, getWidth() - 40, 32, juce::Justification::centredLeft);
    }
    else if (audioProcessor.isPendingConfirm())
    {
        g.setColour(juce::Colour(0xfffbbf24));
        g.drawText("AUDITION WITH 'NOISE LISTEN'. SELECT [FEMALE] OR [MALE] TO SAVE", 20, getHeight() - 32, getWidth() - 40, 32, juce::Justification::centredLeft);
    }
    else if (audioProcessor.isLearningSib())
    {
        g.setColour(juce::Colour(0xff38bdf8));
        g.drawText("LEARNING SIBILANCE... SING 'S/SH/Z' INTO MICROPHONE", 20, getHeight() - 32, getWidth() - 40, 32, juce::Justification::centredLeft);
    }
    else if (audioProcessor.isLearningBr())
    {
        g.setColour(juce::Colour(0xffc084fc));
        g.drawText("LEARNING BREATH... INHALE OR EXHALE NATURALLY", 20, getHeight() - 32, getWidth() - 40, 32, juce::Justification::centredLeft);
    }
    else if (audioProcessor.isLearningNorm())
    {
        g.setColour(juce::Colour(0xff818cf8));
        g.drawText("LEARNING NORMAL VOCAL... SING SUSTAINED VOWEL (AH/OH)", 20, getHeight() - 32, getWidth() - 40, 32, juce::Justification::centredLeft);
    }
    else
    {
        g.setColour(juce::Colour(0xff64748b));
        g.drawText("READY (87 FEMALE VOCAL DATASETS ACTIVE: 2 NORMAL, 2 SIBILANCE, 3 BREATH)", 20, getHeight() - 32, getWidth() - 40, 32, juce::Justification::centredLeft);
    }
}

void Atafuta09VocaNoiseLearnnerAudioProcessorEditor::resized()
{
    // 通常の Learn ボタン群
    learnSibilanceBtn.setBounds(20, 208, 125, 34);
    learnBreathBtn.setBounds(149, 208, 110, 34);
    learnNormalBtn.setBounds(263, 208, 110, 34);
    exportProfilesBtn.setBounds(377, 208, 115, 34);
    resetProfilesBtn.setBounds(496, 208, 44, 34);

    // 確認・承認 (男女別仕分け保存) ボタン群
    confirmFemaleBtn.setBounds(20, 208, 175, 34);
    confirmMaleBtn.setBounds(200, 208, 175, 34);
    discardBtn.setBounds(380, 208, 160, 34);

    // Listen ボタンのクリッカブル領域
    listenButton.setBounds(20, 252, getWidth() - 40, 54);
    listenButton.setAlpha(0.0f);

    // 下部コントロールパネル
    const int panelY = 326;
    sensitivitySlider.setBounds(35, panelY + 12, 100, 100);
    sensitivityLabel.setBounds(35, panelY + 112, 100, 18);

    const int toggleX = 170;
    sibilanceToggle.setBounds(toggleX, panelY + 20, 340, 26);
    plosiveToggle.setBounds(toggleX, panelY + 54, 340, 26);
    breathToggle.setBounds(toggleX, panelY + 88, 340, 26);
}

} // namespace AtafutaAudio::VocaNoiseLearnner
