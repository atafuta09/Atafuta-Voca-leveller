#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_basics/juce_gui_basics.h>
#endif

// ==============================================================================
/**
 * Modern Dark Studio Hardware LookAndFeel
 * 実機ラック機材の質感、4レイヤー・アイスブルーLEDアーク、先端ジュエルLED、
 * 青白エッジ発光スリット、ハードウェア黒サムを描画するカスタムデザインシステム
 */
class ModernDarkLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ModernDarkLookAndFeel()
    {
        updateColours();
    }

    ~ModernDarkLookAndFeel() override = default;

    void setWhiteMode (bool white)
    {
        if (isWhite != white)
        {
            isWhite = white;
            updateColours();
        }
    }

    bool isWhiteMode() const noexcept { return isWhite; }

    void updateColours()
    {
        if (isWhite)
        {
            setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (0xf1, 0xf5, 0xf9));
            setColour (juce::Label::textColourId,                 juce::Colour (0x0f, 0x17, 0x2a));
            setColour (juce::ComboBox::backgroundColourId,        juce::Colour (0xff, 0xff, 0xff));
            setColour (juce::ComboBox::outlineColourId,           juce::Colour (0x94, 0xa3, 0xb8));
            setColour (juce::ComboBox::textColourId,              juce::Colour (0x0f, 0x17, 0x2a));
            setColour (juce::ComboBox::arrowColourId,             juce::Colour (0x33, 0x41, 0x55));
            setColour (juce::PopupMenu::backgroundColourId,       juce::Colour (0xff, 0xff, 0xff));
            setColour (juce::PopupMenu::textColourId,             juce::Colour (0x0f, 0x17, 0x2a));
            setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0x02, 0x84, 0xc7).withAlpha (0.25f));
            setColour (juce::PopupMenu::highlightedTextColourId,  juce::Colour (0x02, 0x84, 0xc7));
        }
        else
        {
            setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (0x0a, 0x0b, 0x0f));
            setColour (juce::Label::textColourId,                 juce::Colour (0xf8, 0xfa, 0xfc));
            setColour (juce::ComboBox::backgroundColourId,        juce::Colour (0x0e, 0x10, 0x17));
            setColour (juce::ComboBox::outlineColourId,           juce::Colour (0x29, 0x2f, 0x40));
            setColour (juce::ComboBox::textColourId,              juce::Colour (0x94, 0xa3, 0xb8));
            setColour (juce::ComboBox::arrowColourId,             juce::Colour (0x94, 0xa3, 0xb8));
            setColour (juce::PopupMenu::backgroundColourId,       juce::Colour (0x11, 0x13, 0x19));
            setColour (juce::PopupMenu::textColourId,             juce::Colour (0xf8, 0xfa, 0xfc));
            setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0x1d, 0x4e, 0xd8).withAlpha (0.45f));
            setColour (juce::PopupMenu::highlightedTextColourId,  juce::Colours::white);
        }
    }

    // ==============================================================================
    // 1. SPEED / RANGE ロータリーノブ
    // ==============================================================================
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& /*slider*/) override
    {
        const auto bounds = juce::Rectangle<float> (static_cast<float>(x), static_cast<float>(y),
                                                    static_cast<float>(width), static_cast<float>(height));
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 4.0f;
        const auto center = bounds.getCentre();

        if (radius <= 0.0f)
            return;

        const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        // --- A. ノブ外側のベーストラック ---
        const float arcRadius = radius - 2.5f;
        {
            juce::Path bgArc;
            bgArc.addCentredArc (center.x, center.y, arcRadius, arcRadius, 0.0f,
                                 rotaryStartAngle, rotaryEndAngle, true);
            g.setColour (isWhite ? juce::Colour (0xcc, 0xd5, 0xe4) : juce::Colour (0x17, 0x1a, 0x24));
            g.strokePath (bgArc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // --- B. 発光アーク ---
        if (sliderPosProportional > 0.001f)
        {
            juce::Path activeArc;
            activeArc.addCentredArc (center.x, center.y, arcRadius, arcRadius, 0.0f,
                                     rotaryStartAngle, angle, true);

            if (isWhite)
            {
                // ホワイトモード: 鮮やかなロイヤルブルー光彩＋白熱コア
                g.setColour (juce::Colour (0x02, 0x84, 0xc7).withAlpha (0.25f));
                g.strokePath (activeArc, juce::PathStrokeType (8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour (juce::Colour (0x02, 0x84, 0xc7).withAlpha (0.70f));
                g.strokePath (activeArc, juce::PathStrokeType (4.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour (juce::Colour (0x38, 0xbd, 0xf8));
                g.strokePath (activeArc, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour (juce::Colours::white);
                g.strokePath (activeArc, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            else
            {
                // ダークモード: アイスブルー4層LED
                g.setColour (juce::Colour (0x25, 0x63, 0xeb).withAlpha (0.28f));
                g.strokePath (activeArc, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour (juce::Colour (0x3b, 0x82, 0xf6).withAlpha (0.55f));
                g.strokePath (activeArc, juce::PathStrokeType (5.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour (juce::Colour (0x93, 0xc5, 0xfd).withAlpha (0.85f));
                g.strokePath (activeArc, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour (juce::Colour (0xff, 0xff, 0xff).withAlpha (0.95f));
                g.strokePath (activeArc, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
        }

        // --- C. ノブ本体 ---
        const float knobRadius = radius - 8.5f;
        const auto knobBounds = juce::Rectangle<float> (center.x - knobRadius, center.y - knobRadius,
                                                        knobRadius * 2.0f, knobRadius * 2.0f);

        if (isWhite)
        {
            // ホワイトモード: ブラッシュド・アルミシルバー調
            g.setColour (juce::Colours::black.withAlpha (0.16f));
            g.fillEllipse (knobBounds.translated (0.0f, 2.0f));

            g.setGradientFill (juce::ColourGradient (
                juce::Colour (0xf8, 0xfa, 0xfc), knobBounds.getX(), knobBounds.getY(),
                juce::Colour (0xd9, 0xe2, 0xec), knobBounds.getX(), knobBounds.getBottom(),
                false));
            g.fillEllipse (knobBounds);

            g.setColour (juce::Colour (0x64, 0x74, 0x8b));
            g.drawEllipse (knobBounds, 1.2f);
            g.setColour (juce::Colours::white);
            g.drawEllipse (knobBounds.reduced (1.0f), 0.8f);
        }
        else
        {
            // ダークモード: 梨地マットブラック
            g.setColour (juce::Colours::black.withAlpha (0.65f));
            g.fillEllipse (knobBounds.translated (0.0f, 2.5f));

            g.setGradientFill (juce::ColourGradient (
                juce::Colour (0x26, 0x2b, 0x36), knobBounds.getX(), knobBounds.getY(),
                juce::Colour (0x0e, 0x10, 0x15), knobBounds.getX(), knobBounds.getBottom(),
                false));
            g.fillEllipse (knobBounds);

            g.setColour (juce::Colour (0x3b, 0x43, 0x54));
            g.drawEllipse (knobBounds, 1.2f);
            g.setColour (juce::Colour (0x08, 0x09, 0x0e).withAlpha (0.7f));
            g.drawEllipse (knobBounds.reduced (1.2f), 1.0f);
        }

        // --- D. 先端ポインターライン & ジュエルLED ---
        const float cosA = std::cos (angle - juce::MathConstants<float>::halfPi);
        const float sinA = std::sin (angle - juce::MathConstants<float>::halfPi);

        const float innerR = knobRadius * 0.45f;
        const float tipR   = knobRadius - 2.0f;

        const juce::Point<float> pInner (center.x + innerR * cosA, center.y + innerR * sinA);
        const juce::Point<float> pTip   (center.x + tipR   * cosA, center.y + tipR   * sinA);

        if (isWhite)
        {
            g.setColour (juce::Colour (0x0f, 0x17, 0x2a));
            g.drawLine (juce::Line<float> (pInner, pTip), 2.6f);
            g.setColour (juce::Colour (0x02, 0x84, 0xc7));
            g.drawLine (juce::Line<float> (pInner, pTip), 1.4f);
        }
        else
        {
            g.setColour (juce::Colour (0x93, 0xc5, 0xfd).withAlpha (0.6f));
            g.drawLine (juce::Line<float> (pInner, pTip), 2.2f);
            g.setColour (juce::Colours::white);
            g.drawLine (juce::Line<float> (pInner, pTip), 1.0f);
        }
    }

    // ==============================================================================
    // 2. リニアスライダー (IN/OUT GAIN スリット ＆ TARGET LEVEL 縦長スライダー)
    // ==============================================================================
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                           juce::Slider::SliderStyle /*sliderStyle*/, juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<float> (static_cast<float>(x), static_cast<float>(y),
                                                    static_cast<float>(width), static_cast<float>(height));
        const bool isTargetSlider = slider.getComponentID() == "targetSlider";

        if (isTargetSlider)
        {
            // ================= TARGET LEVEL (トラック内メーター統合型) =================
            const float trackW = 20.0f;
            const float trackX = bounds.getCentreX() - (trackW * 0.5f);
            const float trackY = bounds.getY() + 4.0f;
            const float trackH = bounds.getHeight() - 8.0f;
            const auto trackRect = juce::Rectangle<float> (trackX, trackY, trackW, trackH);

            // 1. スリット外周のアンビエント発光
            g.setColour (isWhite ? juce::Colour (0x02, 0x84, 0xc7).withAlpha (0.25f)
                                 : juce::Colour (0x3b, 0x82, 0xf6).withAlpha (0.35f));
            g.drawRoundedRectangle (trackRect.expanded (2.5f), 7.0f, 2.5f);

            // 2. トラック溝内部背景 (光るメーターの視認性を保つスレートブラック)
            g.setColour (isWhite ? juce::Colour (0x11, 0x14, 0x1d) : juce::Colour (0x08, 0x09, 0x0e));
            g.fillRoundedRectangle (trackRect, 6.0f);

            // 3. 溝内部リアルタイム入力メーターバー
            const float inDb = slider.getProperties().getWithDefault ("inputMeterDb", -60.0f);
            const float normLevel = juce::jlimit (0.0f, 1.0f, (inDb + 36.0f) / 36.0f);
            const float meterH = normLevel * trackH;
            if (meterH > 1.0f)
            {
                const float meterTopY = trackRect.getBottom() - meterH;
                const auto fillRect = juce::Rectangle<float> (trackX + 2.0f, meterTopY, trackW - 4.0f, meterH);

                g.setGradientFill (juce::ColourGradient (
                    juce::Colour (0x1d, 0x4e, 0xd8).withAlpha (0.45f), fillRect.getX(), trackRect.getBottom(),
                    juce::Colour (0x38, 0xbd, 0xf8).withAlpha (0.90f), fillRect.getX(), meterTopY + (meterH * 0.3f),
                    false));
                g.fillRoundedRectangle (fillRect, 4.0f);

                const float capH = juce::jmin (6.0f, meterH);
                g.setGradientFill (juce::ColourGradient (
                    juce::Colour (0x93, 0xc5, 0xfd).withAlpha (0.9f), fillRect.getX(), fillRect.getY() + capH,
                    juce::Colours::white, fillRect.getX(), fillRect.getY(),
                    false));
                g.fillRoundedRectangle (fillRect.withHeight (capH), 3.0f);
            }

            // 4. 外枠エッジボーダー (ホワイト時は高いコントラストを確保)
            g.setColour (isWhite ? juce::Colour (0x47, 0x55, 0x69) : juce::Colour (0xe0, 0xe7, 0xff).withAlpha (0.9f));
            g.drawRoundedRectangle (trackRect, 6.0f, 1.4f);

            // 5. スライダーサム (実機ハードウェア質感 + インジケーターライン)
            const float thumbW = 44.0f;
            const float thumbH = 14.0f;
            const auto thumbRect = juce::Rectangle<float> (bounds.getCentreX() - (thumbW * 0.5f),
                                                           sliderPos - (thumbH * 0.5f),
                                                           thumbW, thumbH);

            if (isWhite)
            {
                g.setColour (juce::Colours::black.withAlpha (0.22f));
                g.fillRoundedRectangle (thumbRect.translated (0.0f, 1.5f), 3.0f);

                g.setGradientFill (juce::ColourGradient (
                    juce::Colour (0xf8, 0xfa, 0xfc), thumbRect.getX(), thumbRect.getY(),
                    juce::Colour (0xcb, 0xd5, 0xe1), thumbRect.getX(), thumbRect.getBottom(),
                    false));
                g.fillRoundedRectangle (thumbRect, 3.0f);

                g.setColour (juce::Colour (0x33, 0x41, 0x55));
                g.drawRoundedRectangle (thumbRect, 3.0f, 1.2f);

                const auto innerGroove = thumbRect.reduced (4.0f, 4.0f);
                g.setColour (juce::Colour (0x33, 0x41, 0x55));
                g.fillRoundedRectangle (innerGroove, 2.0f);

                const float lineY = thumbRect.getCentreY();
                g.setColour (juce::Colour (0x02, 0x84, 0xc7));
                g.drawLine (thumbRect.getX() + 6.0f, lineY, thumbRect.getRight() - 6.0f, lineY, 2.5f);
                g.setColour (juce::Colours::white);
                g.drawLine (thumbRect.getX() + 6.0f, lineY, thumbRect.getRight() - 6.0f, lineY, 1.0f);
            }
            else
            {
                g.setColour (juce::Colours::black.withAlpha (0.85f));
                g.fillRoundedRectangle (thumbRect.translated (0.0f, 2.0f), 3.0f);

                g.setGradientFill (juce::ColourGradient (
                    juce::Colour (0x3d, 0x46, 0x57), thumbRect.getX(), thumbRect.getY(),
                    juce::Colour (0x10, 0x13, 0x1a), thumbRect.getX(), thumbRect.getBottom(),
                    false));
                g.fillRoundedRectangle (thumbRect, 3.0f);

                g.setColour (juce::Colour (0x64, 0x74, 0x8b));
                g.drawRoundedRectangle (thumbRect, 3.0f, 1.2f);

                const auto innerGroove = thumbRect.reduced (4.0f, 4.0f);
                g.setColour (juce::Colour (0x08, 0x0a, 0x10));
                g.fillRoundedRectangle (innerGroove, 2.0f);

                const float lineY = thumbRect.getCentreY();
                g.setColour (juce::Colour (0x60, 0xa5, 0xfa).withAlpha (0.7f));
                g.drawLine (thumbRect.getX() + 6.0f, lineY, thumbRect.getRight() - 6.0f, lineY, 3.0f);
                g.setColour (juce::Colours::white);
                g.drawLine (thumbRect.getX() + 6.0f, lineY, thumbRect.getRight() - 6.0f, lineY, 1.4f);
            }
        }
        else
        {
            // ================= IN GAIN / OUT GAIN スリットスライダー =================
            const float trackW = 10.0f;
            const float trackX = bounds.getCentreX() - (trackW * 0.5f);
            const float trackY = bounds.getY() + 4.0f;
            const float trackH = bounds.getHeight() - 8.0f;
            const auto trackRect = juce::Rectangle<float> (trackX, trackY, trackW, trackH);

            if (isWhite)
            {
                g.setColour (juce::Colour (0x02, 0x84, 0xc7).withAlpha (0.20f));
                g.drawRoundedRectangle (trackRect.expanded (1.5f), 5.0f, 1.5f);

                g.setColour (juce::Colour (0xe2, 0xe8, 0xf0));
                g.fillRoundedRectangle (trackRect, 4.5f);

                g.setColour (juce::Colour (0x64, 0x74, 0x8b));
                g.drawLine (bounds.getCentreX(), trackY + 2.0f, bounds.getCentreX(), trackY + trackH - 2.0f, 3.0f);

                g.setColour (juce::Colour (0x94, 0xa3, 0xb8));
                g.drawRoundedRectangle (trackRect, 4.5f, 1.2f);
            }
            else
            {
                g.setColour (juce::Colour (0x3b, 0x82, 0xf6).withAlpha (0.25f));
                g.drawRoundedRectangle (trackRect.expanded (1.5f), 5.0f, 1.5f);

                g.setColour (juce::Colour (0x07, 0x08, 0x0c));
                g.fillRoundedRectangle (trackRect, 4.5f);

                g.setColour (juce::Colour (0x02, 0x02, 0x04));
                g.drawLine (bounds.getCentreX(), trackY + 2.0f, bounds.getCentreX(), trackY + trackH - 2.0f, 3.0f);

                g.setColour (juce::Colour (0xd0, 0xdb, 0xf5).withAlpha (0.85f));
                g.drawRoundedRectangle (trackRect, 4.5f, 1.0f);
            }

            // サム (横幅26px, 縦幅11px)
            const float thumbW = 26.0f;
            const float thumbH = 11.0f;
            const auto thumbRect = juce::Rectangle<float> (bounds.getCentreX() - (thumbW * 0.5f),
                                                           sliderPos - (thumbH * 0.5f),
                                                           thumbW, thumbH);

            if (isWhite)
            {
                g.setColour (juce::Colours::black.withAlpha (0.18f));
                g.fillRoundedRectangle (thumbRect.translated (0.0f, 1.5f), 2.5f);

                g.setGradientFill (juce::ColourGradient (
                    juce::Colour (0xf8, 0xfa, 0xfc), thumbRect.getX(), thumbRect.getY(),
                    juce::Colour (0xcb, 0xd5, 0xe1), thumbRect.getX(), thumbRect.getBottom(),
                    false));
                g.fillRoundedRectangle (thumbRect, 2.5f);

                g.setColour (juce::Colour (0x47, 0x55, 0x69));
                g.drawRoundedRectangle (thumbRect, 2.5f, 1.0f);

                const float lineY = thumbRect.getCentreY();
                g.setColour (juce::Colour (0x02, 0x84, 0xc7));
                g.drawLine (thumbRect.getX() + 4.0f, lineY, thumbRect.getRight() - 4.0f, lineY, 2.2f);
            }
            else
            {
                g.setColour (juce::Colours::black.withAlpha (0.8f));
                g.fillRoundedRectangle (thumbRect.translated (0.0f, 1.5f), 2.5f);

                g.setGradientFill (juce::ColourGradient (
                    juce::Colour (0x35, 0x3d, 0x4c), thumbRect.getX(), thumbRect.getY(),
                    juce::Colour (0x12, 0x15, 0x1c), thumbRect.getX(), thumbRect.getBottom(),
                    false));
                g.fillRoundedRectangle (thumbRect, 2.5f);

                g.setColour (juce::Colour (0x52, 0x60, 0x75));
                g.drawRoundedRectangle (thumbRect, 2.5f, 1.0f);

                const float lineY = thumbRect.getCentreY();
                g.setColour (juce::Colour (0x60, 0xa5, 0xfa).withAlpha (0.6f));
                g.drawLine (thumbRect.getX() + 4.0f, lineY, thumbRect.getRight() - 4.0f, lineY, 2.5f);
                g.setColour (juce::Colours::white);
                g.drawLine (thumbRect.getX() + 4.0f, lineY, thumbRect.getRight() - 4.0f, lineY, 1.0f);
            }
        }
    }

    // ==============================================================================
    // 3. トグルボタン
    // ==============================================================================
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool /*shouldDrawButtonAsHighlighted*/, bool /*shouldDrawButtonAsDown*/) override
    {
        const auto bounds = button.getLocalBounds().toFloat();
        const bool isActive = button.getToggleState();

        const float ledSize = 7.0f;
        const auto ledRect = juce::Rectangle<float> (bounds.getX() + 2.0f, bounds.getCentreY() - (ledSize * 0.5f),
                                                     ledSize, ledSize);

        if (isActive)
        {
            g.setColour (isWhite ? juce::Colour (0x02, 0x84, 0xc7).withAlpha (0.50f)
                                 : juce::Colour (0x3b, 0x82, 0xf6).withAlpha (0.55f));
            g.fillEllipse (ledRect.expanded (3.0f));
            g.setColour (isWhite ? juce::Colour (0x38, 0xbd, 0xf8) : juce::Colour (0x93, 0xc5, 0xfd));
            g.fillEllipse (ledRect.expanded (1.0f));
            g.setColour (juce::Colours::white);
            g.fillEllipse (ledRect);
        }
        else
        {
            g.setColour (isWhite ? juce::Colour (0xe2, 0xe8, 0xf0) : juce::Colour (0x17, 0x1b, 0x24));
            g.fillEllipse (ledRect);
            g.setColour (isWhite ? juce::Colour (0x94, 0xa3, 0xb8) : juce::Colour (0x3b, 0x43, 0x54));
            g.drawEllipse (ledRect, 1.0f);
        }

        // ラベルテキスト（ホワイトモード時は最高コントラストのスレートインディゴ）
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        if (isWhite)
            g.setColour (isActive ? juce::Colour (0x0f, 0x17, 0x2a) : juce::Colour (0x47, 0x55, 0x69));
        else
            g.setColour (isActive ? juce::Colour (0xf0, 0xf9, 0xff) : juce::Colour (0x64, 0x74, 0x8b));

        const auto textRect = bounds.withTrimmedLeft (ledSize + 7.0f);
        g.drawText (button.getButtonText(), textRect, juce::Justification::centredLeft, true);
    }

    // ==============================================================================
    // 4. コンボボックス
    // ==============================================================================
    void drawComboBox (juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override
    {
        const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));

        if (isWhite)
        {
            g.setColour (juce::Colour (0xff, 0xff, 0xff));
            g.fillRoundedRectangle (bounds, 4.0f);

            g.setColour (box.hasKeyboardFocus (true) ? juce::Colour (0x02, 0x84, 0xc7) : juce::Colour (0x94, 0xa3, 0xb8));
            g.drawRoundedRectangle (bounds, 4.0f, 1.2f);
        }
        else
        {
            g.setColour (juce::Colour (0x0b, 0x0d, 0x13));
            g.fillRoundedRectangle (bounds, 4.0f);

            g.setColour (box.hasKeyboardFocus (true) ? juce::Colour (0x3b, 0x82, 0xf6) : juce::Colour (0x24, 0x29, 0x38));
            g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
        }

        // 下向き矢印
        const auto arrowBounds = juce::Rectangle<float> (static_cast<float>(buttonX), static_cast<float>(buttonY),
                                                         static_cast<float>(buttonW), static_cast<float>(buttonH));
        juce::Path arrow;
        const float aW = 6.0f;
        const float aH = 4.0f;
        const auto aCenter = arrowBounds.getCentre();
        arrow.startNewSubPath (aCenter.x - aW * 0.5f, aCenter.y - aH * 0.5f);
        arrow.lineTo (aCenter.x + aW * 0.5f, aCenter.y - aH * 0.5f);
        arrow.lineTo (aCenter.x, aCenter.y + aH * 0.5f);
        arrow.closeSubPath();

        g.setColour (isWhite ? juce::Colour (0x33, 0x41, 0x55) : juce::Colour (0x64, 0x74, 0x8b));
        g.fillPath (arrow);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override
    {
        return juce::FontOptions (10.0f, juce::Font::bold);
    }

    // ==============================================================================
    // 5. テキストボタン
    // ==============================================================================
    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                const juce::Colour& /*backgroundColour*/,
                                bool shouldDrawButtonAsHighlighted,
                                bool shouldDrawButtonAsDown) override
    {
        const auto bounds = button.getLocalBounds().toFloat();
        const float cornerSize = 4.0f;

        if (isWhite)
        {
            if (shouldDrawButtonAsDown)
            {
                g.setColour (juce::Colour (0xd0, 0xdc, 0xeb));
                g.fillRoundedRectangle (bounds, cornerSize);
                g.setColour (juce::Colour (0x03, 0x69, 0xa1));
                g.drawRoundedRectangle (bounds, cornerSize, 1.2f);
            }
            else if (shouldDrawButtonAsHighlighted)
            {
                g.setColour (juce::Colour (0xf0, 0xf9, 0xff));
                g.fillRoundedRectangle (bounds, cornerSize);
                g.setColour (juce::Colour (0x02, 0x84, 0xc7));
                g.drawRoundedRectangle (bounds, cornerSize, 1.2f);
            }
            else
            {
                g.setColour (juce::Colour (0xff, 0xff, 0xff));
                g.fillRoundedRectangle (bounds, cornerSize);
                g.setColour (juce::Colour (0x94, 0xa3, 0xb8));
                g.drawRoundedRectangle (bounds, cornerSize, 1.0f);
            }
        }
        else
        {
            if (shouldDrawButtonAsDown)
            {
                g.setColour (juce::Colour (0x0f, 0x12, 0x1a));
                g.fillRoundedRectangle (bounds, cornerSize);
                g.setColour (juce::Colour (0x60, 0xa5, 0xfa).withAlpha (0.7f));
                g.drawRoundedRectangle (bounds, cornerSize, 1.0f);
            }
            else if (shouldDrawButtonAsHighlighted)
            {
                g.setColour (juce::Colour (0x1e, 0x24, 0x33));
                g.fillRoundedRectangle (bounds, cornerSize);
                g.setColour (juce::Colour (0x3b, 0x82, 0xf6).withAlpha (0.5f));
                g.drawRoundedRectangle (bounds, cornerSize, 1.0f);
            }
            else
            {
                g.setColour (juce::Colour (0x13, 0x16, 0x20));
                g.fillRoundedRectangle (bounds, cornerSize);
                g.setColour (juce::Colour (0x24, 0x2c, 0x3d));
                g.drawRoundedRectangle (bounds, cornerSize, 1.0f);
            }
        }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override
    {
        juce::Colour textCol;
        if (isWhite)
        {
            if (shouldDrawButtonAsDown)
                textCol = juce::Colour (0x03, 0x69, 0xa1);
            else if (shouldDrawButtonAsHighlighted)
                textCol = juce::Colour (0x02, 0x84, 0xc7);
            else
                textCol = juce::Colour (0x1e, 0x29, 0x3b);
        }
        else
        {
            if (shouldDrawButtonAsDown)
                textCol = juce::Colours::white;
            else if (shouldDrawButtonAsHighlighted)
                textCol = juce::Colour (0xf8, 0xfa, 0xfc);
            else
                textCol = juce::Colour (0x94, 0xa3, 0xb8);
        }

        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.setColour (textCol);
        g.drawText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
    }

private:
    bool isWhite = false;
};

