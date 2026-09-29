#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// WaveformVisualizerComponent 実装 (-42 〜 0 dBFS スケール)
// ==============================================================================
WaveformVisualizerComponent::WaveformVisualizerComponent()
    : history (static_cast<size_t>(maxHistoryPoints))
{
    setOpaque (true);
}

void WaveformVisualizerComponent::pushData (const VisualDataPoint* points, int numPoints)
{
    if (!guiEnabled || points == nullptr || numPoints <= 0)
        return;

    for (int i = 0; i < numPoints; ++i)
    {
        history[static_cast<size_t>(writeIndex)] = points[i];
        writeIndex = (writeIndex + 1) % maxHistoryPoints;
        if (writeIndex == 0)
            bufferWrapped = true;
    }

    repaint();
}

void WaveformVisualizerComponent::setVisualParams (float targetDb, float rangeDb)
{
    if (std::abs (currentTargetDb - targetDb) > 0.05f || std::abs (currentRangeDb - rangeDb) > 0.05f)
    {
        currentTargetDb = targetDb;
        currentRangeDb  = rangeDb;
        if (guiEnabled)
            repaint();
    }
}

void WaveformVisualizerComponent::clear()
{
    std::fill (history.begin(), history.end(), VisualDataPoint{});
    writeIndex = 0;
    bufferWrapped = false;
    repaint();
}

void WaveformVisualizerComponent::setGuiEnabled (bool enabled)
{
    if (guiEnabled != enabled)
    {
        guiEnabled = enabled;
        repaint();
    }
}

void WaveformVisualizerComponent::resized()
{
    const auto bounds = getLocalBounds().toFloat();
    chartTop    = bounds.getY() + 32.0f;
    chartBottom = bounds.getBottom() - 14.0f;
}

float WaveformVisualizerComponent::getTargetLineY() const
{
    // 中央が 0 dB（ターゲット基準レベル）
    const float height = chartBottom - chartTop;
    return chartBottom - (0.5f * height);
}

void WaveformVisualizerComponent::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // 1. 実機ダークパネル背景
    g.setGradientFill (juce::ColourGradient (
        juce::Colour (0x11, 0x13, 0x1a), bounds.getX(), bounds.getY(),
        juce::Colour (0x07, 0x08, 0x0d), bounds.getX(), bounds.getBottom(),
        false));
    g.fillRect (bounds);

    g.setColour (juce::Colour (0x1f, 0x23, 0x30));
    g.drawRect (bounds, 1.0f);

    if (!guiEnabled)
    {
        g.setColour (juce::Colour (0x64, 0x74, 0x8b));
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        g.drawText ("DISPLAY PAUSED (CPU Saving Mode)", bounds.toNearestInt(), juce::Justification::centred, true);
        return;
    }

    const float top    = chartTop;
    const float bottom = chartBottom;
    const float height = bottom - top;
    const float width  = bounds.getWidth();

    // 2. ボリューム目盛りスケール (-42 dBFS ~ 0 dBFS)
    constexpr float minDb = -42.0f;
    constexpr float maxDb =   0.0f;

    auto dbToY = [bottom, height, minDb, maxDb](float db) -> float
    {
        const float norm = juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
        return bottom - (norm * height);
    };

    // 3. 水平ボリューム目盛り (dBFS グリッド: 0, -6, -12, -18, -24, -30, -36, -42)
    const float gridDbs[] = { 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -30.0f, -36.0f, -42.0f };
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));

    for (float db : gridDbs)
    {
        const float y = dbToY (db);
        const bool isZero = std::abs (db) < 0.01f;

        g.setColour (isZero ? juce::Colour (0x3b, 0x82, 0xf6).withAlpha (0.4f)
                            : juce::Colour (0x1a, 0x1e, 0x29).withAlpha (0.65f));
        g.drawHorizontalLine (juce::roundToInt (y), bounds.getX() + 6.0f, bounds.getX() + width - 46.0f);

        // dBFS 数値ラベル
        g.setColour (isZero ? juce::Colour (0x93, 0xc5, 0xfd) : juce::Colour (0x4b, 0x55, 0x68));
        const juce::String lbl = juce::String (static_cast<int>(db)) + (isZero ? " dBFS" : " dB");
        g.drawText (lbl, juce::roundToInt (bounds.getX() + width - 44.0f), juce::roundToInt (y - 7.0f), 42, 14, juce::Justification::centredLeft);
    }

    // 4. ターゲットライン ＆ レンジ帯域 (TARGET LEVEL スライダーに完全追従)
    const float targetY = dbToY (currentTargetDb);
    const float clampedRange = juce::jlimit (0.0f, 13.0f, currentRangeDb);
    const float rangeTopY    = dbToY (currentTargetDb + clampedRange);
    const float rangeBottomY = dbToY (currentTargetDb - clampedRange);
    const float bandH        = juce::jmax (2.0f, rangeBottomY - rangeTopY);

    // A. レンジ帯域ハイライト (ターゲットラインを中心に上下 ±Range dB, 最大13dB)
    g.setGradientFill (juce::ColourGradient (
        juce::Colour (0x3b, 0x82, 0xf6).withAlpha (0.09f), bounds.getCentreX(), targetY,
        juce::Colour (0x3b, 0x82, 0xf6).withAlpha (0.015f), bounds.getCentreX(), rangeTopY,
        false));
    g.fillRect (bounds.getX(), rangeTopY, width, bandH);

    // 上下のレンジ境界破線
    float dashPattern[] = { 4.0f, 4.0f };
    g.setColour (juce::Colour (0x60, 0xa5, 0xfa).withAlpha (0.50f));
    g.drawDashedLine (juce::Line<float> (bounds.getX(), rangeTopY, bounds.getX() + width, rangeTopY), dashPattern, 2, 1.2f);
    g.drawDashedLine (juce::Line<float> (bounds.getX(), rangeBottomY, bounds.getX() + width, rangeBottomY), dashPattern, 2, 1.2f);

    // B. ターゲットライン (白熱コア ＋ スカイブルー光彩)
    g.setColour (juce::Colour (0x3b, 0x82, 0xf6).withAlpha (0.50f));
    g.drawLine (bounds.getX() + 4.0f, targetY, bounds.getX() + width - 46.0f, targetY, 2.6f);
    g.setColour (juce::Colours::white);
    g.drawLine (bounds.getX() + 4.0f, targetY, bounds.getX() + width - 46.0f, targetY, 1.2f);

    // 5. 波形データの描画 (Input RMS, Output RMS, GR Ride)
    const int availablePoints = bufferWrapped ? maxHistoryPoints : writeIndex;
    if (availablePoints < 2)
        return;

    const float xStep = width / static_cast<float>(maxHistoryPoints - 1);

    juce::Path inputWavePath;
    juce::Path outputWavePath;
    juce::Path gainRidePath;

    inputWavePath.startNewSubPath (bounds.getX(), bottom);
    outputWavePath.startNewSubPath (bounds.getX(), bottom);

    bool firstGain = true;

    for (int i = 0; i < maxHistoryPoints; ++i)
    {
        const float x = bounds.getX() + (static_cast<float>(i) * xStep);

        int idx = 0;
        if (bufferWrapped)
            idx = (writeIndex + i) % maxHistoryPoints;
        else
        {
            idx = i - (maxHistoryPoints - writeIndex);
            if (idx < 0)
                continue;
        }

        const auto& pt = history[static_cast<size_t>(idx)];

        // 入力・出力波形: ボリューム目盛りにマッピング
        const float inDb  = juce::Decibels::gainToDecibels (pt.inputRms,  -60.0f);
        const float outDb = juce::Decibels::gainToDecibels (pt.outputRms, -60.0f);
        const float inY   = dbToY (inDb);
        const float outY  = dbToY (outDb);

        inputWavePath.lineTo (x, inY);
        outputWavePath.lineTo (x, outY);

        // ゲイン補正線 (GR Ride): ターゲットラインを基準として追従プロット
        const float rideDb = currentTargetDb + pt.gainChangeDb;
        const float grY = juce::jlimit (top, bottom, dbToY (rideDb));
        if (firstGain)
        {
            gainRidePath.startNewSubPath (x, grY);
            firstGain = false;
        }
        else
        {
            gainRidePath.lineTo (x, grY);
        }
    }

    // A. 入力波形: 以前の大きさを保つゴーストフィル ＋ 白熱グレー細線
    inputWavePath.lineTo (bounds.getX() + width, bottom);
    inputWavePath.closeSubPath();
    g.setColour (juce::Colour (0xe2, 0xe8, 0xf0).withAlpha (0.08f));
    g.fillPath (inputWavePath);

    g.setColour (juce::Colour (0xe2, 0xe8, 0xf0).withAlpha (0.20f));
    g.strokePath (inputWavePath, juce::PathStrokeType (2.6f));
    g.setColour (juce::Colour (0xf8, 0xfa, 0xfc).withAlpha (0.75f));
    g.strokePath (inputWavePath, juce::PathStrokeType (0.95f));

    // B. 出力波形: ネオンシアン波形フィル ＆ ストローク
    outputWavePath.lineTo (bounds.getX() + width, bottom);
    outputWavePath.closeSubPath();
    g.setColour (juce::Colour (0x00, 0xe5, 0xff).withAlpha (0.15f));
    g.fillPath (outputWavePath);

    g.setColour (juce::Colour (0x00, 0xe5, 0xff).withAlpha (0.85f));
    g.strokePath (outputWavePath, juce::PathStrokeType (1.2f));

    // C. ゲイン補正線 (GR Ride): 中央0dBを基準に鮮烈に輝くネオンピンク (#ff2a85) レーザー軌跡
    // 外層グロー
    g.setColour (juce::Colour (0xff, 0x2a, 0x85).withAlpha (0.32f));
    g.strokePath (gainRidePath, juce::PathStrokeType (6.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    // 中間光彩
    g.setColour (juce::Colour (0xff, 0x2a, 0x85).withAlpha (0.85f));
    g.strokePath (gainRidePath, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    // 白熱コア
    g.setColour (juce::Colours::white);
    g.strokePath (gainRidePath, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // 7. 上部HUD凡例
    const float hudY = bounds.getY() + 7.0f;
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));

    // Input Wave
    g.setColour (juce::Colour (0xe2, 0xe8, 0xf0));
    g.fillEllipse (bounds.getX() + 10.0f, hudY + 3.0f, 6.0f, 6.0f);
    g.setColour (juce::Colour (0x94, 0xa3, 0xb8));
    g.drawText ("Input Wave", juce::roundToInt (bounds.getX() + 20.0f), juce::roundToInt (hudY), 70, 13, juce::Justification::centredLeft);

    // Output Wave
    g.setColour (juce::Colour (0x00, 0xe5, 0xff));
    g.fillEllipse (bounds.getX() + 98.0f, hudY + 3.0f, 6.0f, 6.0f);
    g.setColour (juce::Colour (0x94, 0xa3, 0xb8));
    g.drawText ("Output Wave", juce::roundToInt (bounds.getX() + 108.0f), juce::roundToInt (hudY), 75, 13, juce::Justification::centredLeft);

    // GR Trajectory
    g.setColour (juce::Colour (0xff, 0x2a, 0x85));
    g.fillEllipse (bounds.getX() + 192.0f, hudY + 3.0f, 6.0f, 6.0f);
    g.setColour (juce::Colour (0x94, 0xa3, 0xb8));
    g.drawText ("GR Ride", juce::roundToInt (bounds.getX() + 202.0f), juce::roundToInt (hudY), 55, 13, juce::Justification::centredLeft);

    // Target & Range (±Range dB 表示)
    g.setColour (juce::Colour (0x93, 0xc5, 0xfd));
    g.drawRect (bounds.getX() + 268.0f, hudY + 3.0f, 8.0f, 6.0f, 1.0f);
    const juce::String rangeStr = "Target " + juce::String (static_cast<int>(currentTargetDb)) + "dB (±" + juce::String (currentRangeDb, 1) + "dB)";
    g.drawText (rangeStr, juce::roundToInt (bounds.getX() + 280.0f), juce::roundToInt (hudY), 155, 13, juce::Justification::centredLeft);
}

// ==============================================================================
// TargetInputMeterComponent 実装 (TARGET LEVEL スライダー右横のInputメーター)
// ==============================================================================
// SlimMeterComponent 実装 (IN / OUT ＋ 精密dB目盛り)
// ==============================================================================
SlimMeterComponent::SlimMeterComponent()
{
    setOpaque (true);
}

void SlimMeterComponent::updateLevels (float inLinear, float outLinear, int mode)
{
    if (!guiEnabled)
        return;

    currentMeterMode = mode;
    const float curInDb  = juce::Decibels::gainToDecibels (inLinear,  -60.0f);
    const float curOutDb = juce::Decibels::gainToDecibels (outLinear, -60.0f);

    constexpr float decayRate = 1.6f;
    inputLevelDb  = juce::jmax (curInDb,  inputLevelDb - decayRate);
    outputLevelDb = juce::jmax (curOutDb, outputLevelDb - decayRate);

    // ピークメーターモード時のピークホールド処理 (約1.5秒ホールド後に減衰)
    if (currentMeterMode == 0)
    {
        if (curInDb >= inputPeakDb)
        {
            inputPeakDb = curInDb;
            inHoldTimer = 90; // 60fps で 90フレーム (約1.5秒)
        }
        else if (inHoldTimer > 0)
        {
            --inHoldTimer;
        }
        else
        {
            inputPeakDb = juce::jmax (curInDb, inputPeakDb - 0.7f);
        }

        if (curOutDb >= outputPeakDb)
        {
            outputPeakDb = curOutDb;
            outHoldTimer = 90;
        }
        else if (outHoldTimer > 0)
        {
            --outHoldTimer;
        }
        else
        {
            outputPeakDb = juce::jmax (curOutDb, outputPeakDb - 0.7f);
        }
    }

    repaint();
}

void SlimMeterComponent::setGuiEnabled (bool enabled)
{
    if (guiEnabled != enabled)
    {
        guiEnabled = enabled;
        repaint();
    }
}

void SlimMeterComponent::resized()
{
}

void SlimMeterComponent::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // パネル背景
    g.setColour (juce::Colour (0x11, 0x13, 0x19));
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (juce::Colour (0x1f, 0x23, 0x30));
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

    const float labelH = 18.0f;
    const float valH   = 18.0f;
    const float meterTop    = bounds.getY() + labelH + 6.0f;
    const float meterBottom = bounds.getBottom() - valH - 6.0f;
    const float meterHeight = meterBottom - meterTop;

    const float trackW = 11.0f;

    // IN は左寄り、OUT は右寄り配置にして中央に目盛り空間を確保
    const float inTrackX  = bounds.getX() + 12.0f;
    const float outTrackX = bounds.getRight() - 12.0f - trackW;

    // A. チャンネルヘッダー (IN / OUT - 十分な幅を確保して見切れ解消)
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.setColour (juce::Colour (0xe0, 0xe7, 0xff));
    const float headerW = 34.0f;
    g.drawText ("IN",  juce::roundToInt (inTrackX + (trackW * 0.5f) - (headerW * 0.5f)),  juce::roundToInt (bounds.getY() + 3.0f), juce::roundToInt (headerW), 14, juce::Justification::centred);
    g.drawText ("OUT", juce::roundToInt (outTrackX + (trackW * 0.5f) - (headerW * 0.5f)), juce::roundToInt (bounds.getY() + 3.0f), juce::roundToInt (headerW), 14, juce::Justification::centred);

    // B. トラック外枠描画関数
    auto drawTrack = [&g, meterTop, meterHeight, trackW](float trackX)
    {
        const auto trackRect = juce::Rectangle<float> (trackX, meterTop, trackW, meterHeight);

        // 青白グロー
        g.setColour (juce::Colour (0x3b, 0x82, 0xf6).withAlpha (0.25f));
        g.drawRoundedRectangle (trackRect.expanded (2.5f), 6.0f, 2.5f);
        g.setColour (juce::Colour (0x93, 0xc5, 0xfd).withAlpha (0.45f));
        g.drawRoundedRectangle (trackRect.expanded (1.2f), 6.0f, 1.2f);

        // トラック内部背景
        g.setColour (juce::Colour (0x08, 0x09, 0x0e));
        g.fillRoundedRectangle (trackRect, 5.0f);

        // インナーグロー
        g.setColour (juce::Colour (0x3b, 0x82, 0xf6).withAlpha (0.30f));
        g.drawRoundedRectangle (trackRect.reduced (0.8f), 4.5f, 1.0f);

        // 外枠エッジボーダー
        g.setColour (juce::Colour (0xe0, 0xe7, 0xff).withAlpha (0.9f));
        g.drawRoundedRectangle (trackRect, 5.0f, 1.2f);
    };

    drawTrack (inTrackX);
    drawTrack (outTrackX);

    // C. メーターバー描画
    auto dbToY = [meterTop, meterBottom, meterHeight](float db) -> float
    {
        const float norm = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
        return meterBottom - (norm * meterHeight);
    };

    const bool isVuMode = (currentMeterMode == 2);
    const float zeroVuY = dbToY (-18.0f); // 0 VU = -18 dBFS

    auto drawBarFill = [&g, meterBottom, trackW, isVuMode, zeroVuY](float trackX, float levelY)
    {
        const float fillH = meterBottom - levelY;
        if (fillH > 1.0f)
        {
            const auto fillRect = juce::Rectangle<float> (trackX + 1.0f, levelY, trackW - 2.0f, fillH);

            if (isVuMode && levelY < zeroVuY)
            {
                // VU モードで 0 VU を超えた場合はレッドゾーン演出
                g.setGradientFill (juce::ColourGradient (
                    juce::Colour (0x1d, 0x4e, 0xd8).withAlpha (0.4f), fillRect.getX(), fillRect.getBottom(),
                    juce::Colour (0xef, 0x44, 0x44).withAlpha (0.95f), fillRect.getX(), levelY,
                    false));
                g.fillRoundedRectangle (fillRect, 3.5f);

                const float capH = juce::jmin (5.0f, fillH);
                g.setColour (juce::Colour (0xfc, 0xa5, 0xa5));
                g.fillRoundedRectangle (fillRect.withHeight (capH), 2.5f);
            }
            else
            {
                // 通常のエレクトリックブルー〜白熱コア
                g.setGradientFill (juce::ColourGradient (
                    juce::Colour (0x1d, 0x4e, 0xd8).withAlpha (0.35f), fillRect.getX(), fillRect.getBottom(),
                    juce::Colour (0x38, 0xbd, 0xf8).withAlpha (0.85f), fillRect.getX(), fillRect.getY() + fillH * 0.4f,
                    false));
                g.fillRoundedRectangle (fillRect, 3.5f);

                const float capH = juce::jmin (5.0f, fillH);
                g.setGradientFill (juce::ColourGradient (
                    juce::Colour (0x93, 0xc5, 0xfd).withAlpha (0.9f), fillRect.getX(), fillRect.getY() + capH,
                    juce::Colours::white, fillRect.getX(), fillRect.getY(),
                    false));
                g.fillRoundedRectangle (fillRect.withHeight (capH), 2.5f);
            }
        }
    };

    const float inY  = dbToY (inputLevelDb);
    const float outY = dbToY (outputLevelDb);

    drawBarFill (inTrackX, inY);
    drawBarFill (outTrackX, outY);

    // ピークメーターモード時のピークホールドライン描画
    if (currentMeterMode == 0)
    {
        auto drawPeakHoldLine = [&g, trackW, dbToY](float trackX, float peakDb)
        {
            if (peakDb > -46.0f)
            {
                const float py = dbToY (peakDb);
                g.setColour (juce::Colour (0x38, 0xbd, 0xf8).withAlpha (0.7f));
                g.drawHorizontalLine (juce::roundToInt (py), trackX + 1.0f, trackX + trackW - 1.0f);
                g.setColour (juce::Colours::white);
                g.drawHorizontalLine (juce::roundToInt (py), trackX + 2.0f, trackX + trackW - 2.0f);
            }
        };
        drawPeakHoldLine (inTrackX, inputPeakDb);
        drawPeakHoldLine (outTrackX, outputPeakDb);
    }

    // D. 中央目盛り (VU-18 モード vs Peak/RMS モードの切り替え)
    const float valLabelW = 42.0f;

    if (isVuMode)
    {
        // VU-18 専用目盛り (0 VU = -18 dBFS, +3 ~ -20 VU)
        struct VuTick { float db; const char* label; bool isRed; bool isZero; };
        const VuTick vuTicks[] = {
            { -15.0f, "+3",  true,  false },
            { -17.0f, "+1",  true,  false },
            { -18.0f,  "0",  false, true  },
            { -21.0f, "-3",  false, false },
            { -23.0f, "-5",  false, false },
            { -28.0f, "-10", false, false },
            { -38.0f, "-20", false, false }
        };

        g.setFont (juce::FontOptions (8.0f, juce::Font::bold));

        for (const auto& tick : vuTicks)
        {
            const float ty = dbToY (tick.db);

            juce::Colour tickCol;
            if (tick.isRed)
                tickCol = juce::Colour (0xf8, 0x71, 0x71); // レッドゾーン
            else if (tick.isZero)
                tickCol = juce::Colour (0x93, 0xc5, 0xfd); // 0 VU 基準線
            else
                tickCol = juce::Colour (0x47, 0x55, 0x69).withAlpha (0.7f);

            g.setColour (tickCol);
            g.drawHorizontalLine (juce::roundToInt (ty), inTrackX + trackW + 1.5f, inTrackX + trackW + (tick.isZero ? 5.5f : 3.5f));
            g.drawHorizontalLine (juce::roundToInt (ty), outTrackX - (tick.isZero ? 5.5f : 3.5f), outTrackX - 1.5f);

            // 中央ラベル
            g.setColour (tick.isRed ? juce::Colour (0xf8, 0x71, 0x71) : (tick.isZero ? juce::Colour (0x93, 0xc5, 0xfd) : juce::Colour (0x64, 0x74, 0x8b)));
            g.drawText (tick.label, juce::roundToInt (bounds.getX()), juce::roundToInt (ty - 5.0f),
                        juce::roundToInt (bounds.getWidth()), 10, juce::Justification::centred);
        }

        // E. 下部数値テキスト (VU値表記)
        g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
        const float inVu  = inputLevelDb  + 18.0f;
        const float outVu = outputLevelDb + 18.0f;

        const juce::String inStr  = (inVu > 0.0f ? "+" : "") + juce::String (inVu, 1) + " VU";
        const juce::String outStr = (outVu > 0.0f ? "+" : "") + juce::String (outVu, 1) + " VU";

        g.setColour (inVu > 0.0f ? juce::Colour (0xf8, 0x71, 0x71) : juce::Colours::white);
        g.drawText (inStr,  juce::roundToInt (inTrackX + (trackW * 0.5f) - (valLabelW * 0.5f)),  juce::roundToInt (bounds.getBottom() - valH), juce::roundToInt (valLabelW), 16, juce::Justification::centred);

        g.setColour (outVu > 0.0f ? juce::Colour (0xf8, 0x71, 0x71) : juce::Colours::white);
        g.drawText (outStr, juce::roundToInt (outTrackX + (trackW * 0.5f) - (valLabelW * 0.5f)), juce::roundToInt (bounds.getBottom() - valH), juce::roundToInt (valLabelW), 16, juce::Justification::centred);
    }
    else
    {
        // Peak / RMS モード (dBFS 目盛り: 0, -6, -12, -18, -24, -36, -48)
        const float meterTicks[] = { 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -36.0f, -48.0f };
        g.setFont (juce::FontOptions (8.0f, juce::Font::bold));

        for (float tDb : meterTicks)
        {
            const float ty = dbToY (tDb);
            const bool isTop = std::abs (tDb) < 0.01f;

            g.setColour (isTop ? juce::Colour (0x93, 0xc5, 0xfd).withAlpha (0.8f)
                               : juce::Colour (0x47, 0x55, 0x69).withAlpha (0.7f));
            g.drawHorizontalLine (juce::roundToInt (ty), inTrackX + trackW + 1.5f, inTrackX + trackW + 5.0f);
            g.drawHorizontalLine (juce::roundToInt (ty), outTrackX - 5.0f, outTrackX - 1.5f);

            // 中央のdB数値
            g.setColour (isTop ? juce::Colour (0x93, 0xc5, 0xfd) : juce::Colour (0x64, 0x74, 0x8b));
            const juce::String s = juce::String (static_cast<int>(tDb));
            g.drawText (s, juce::roundToInt (bounds.getX()), juce::roundToInt (ty - 5.0f),
                        juce::roundToInt (bounds.getWidth()), 10, juce::Justification::centred);
        }

        // E. 下部数値テキスト (dBFS 表記)
        g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
        g.setColour (juce::Colours::white);

        const juce::String inStr  = juce::String (inputLevelDb, 1) + " dB";
        const juce::String outStr = juce::String (outputLevelDb, 1) + " dB";

        g.drawText (inStr,  juce::roundToInt (inTrackX + (trackW * 0.5f) - (valLabelW * 0.5f)),  juce::roundToInt (bounds.getBottom() - valH), juce::roundToInt (valLabelW), 16, juce::Justification::centred);
        g.drawText (outStr, juce::roundToInt (outTrackX + (trackW * 0.5f) - (valLabelW * 0.5f)), juce::roundToInt (bounds.getBottom() - valH), juce::roundToInt (valLabelW), 16, juce::Justification::centred);
    }
}

// ==============================================================================
// AutoLevelerAudioProcessorEditor 実装
// ==============================================================================
AutoLevelerAudioProcessorEditor::AutoLevelerAudioProcessorEditor (AutoLevelerAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // カスタム LookAndFeel 適用
    setLookAndFeel (&modernDarkLookAndFeel);

    // --- 1. SPEED ノブ (BPMモード時3段階切替対応) ---
    speedSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    speedSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    speedSlider.onValueChange = [this]
    {
        const bool isSync = (timingModeBox.getSelectedId() == 2);
        if (isSync)
        {
            // BPM モード時: ノブ位置 (0: Slow, 1: Mid, 2: Fast) を syncSpeedBox の項目 (0: Fast, 1: Mid, 2: Slow) に伝達
            const int knobStep = juce::jlimit (0, 2, juce::roundToInt (speedSlider.getValue()));
            syncSpeedBox.setSelectedItemIndex (2 - knobStep, juce::sendNotificationSync);
        }
    };
    addAndMakeVisible (speedSlider);

    speedLabel.setText ("SPEED", juce::dontSendNotification);
    speedLabel.setJustificationType (juce::Justification::centred);
    speedLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    speedLabel.setColour (juce::Label::textColourId, juce::Colour (0xe0, 0xe7, 0xff));
    addAndMakeVisible (speedLabel);

    // 視認性を高めた Attack / Release バッジ (横幅を広げ、両方の値を明記)
    attackReleaseLabel.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    attackReleaseLabel.setJustificationType (juce::Justification::centred);
    attackReleaseLabel.setColour (juce::Label::textColourId, juce::Colour (0x93, 0xc5, 0xfd));
    addAndMakeVisible (attackReleaseLabel);

    speedAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.getAPVTS(), ParameterIDs::speed, speedSlider);

    // --- 2. RANGE ノブ (0 〜 13 dB、範囲はパラメーター定義からアタッチメントが設定する) ---
    rangeSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    rangeSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (rangeSlider);

    rangeLabel.setText ("RANGE", juce::dontSendNotification);
    rangeLabel.setJustificationType (juce::Justification::centred);
    rangeLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    rangeLabel.setColour (juce::Label::textColourId, juce::Colour (0xe0, 0xe7, 0xff));
    addAndMakeVisible (rangeLabel);

    // 視認性を高めた Range 数値表示 (12.0px Bold 発光アイスホワイト)
    rangeValueLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    rangeValueLabel.setJustificationType (juce::Justification::centred);
    rangeValueLabel.setColour (juce::Label::textColourId, juce::Colour (0xf0, 0xf9, 0xff));
    addAndMakeVisible (rangeValueLabel);

    rangeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.getAPVTS(), ParameterIDs::range, rangeSlider);

    // --- 3. IN GAIN スライダー (スリットエッジ発光) ---
    inputGainSlider.setSliderStyle (juce::Slider::LinearVertical);
    inputGainSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    inputGainSlider.setComponentID ("inGainSlider");
    addAndMakeVisible (inputGainSlider);

    inputGainLabel.setText ("IN GAIN", juce::dontSendNotification);
    inputGainLabel.setJustificationType (juce::Justification::centred);
    inputGainLabel.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    inputGainLabel.setColour (juce::Label::textColourId, juce::Colour (0x94, 0xa3, 0xb8));
    addAndMakeVisible (inputGainLabel);

    inputGainValueLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    inputGainValueLabel.setJustificationType (juce::Justification::centred);
    inputGainValueLabel.setColour (juce::Label::textColourId, juce::Colour (0xe2, 0xe8, 0xf0));
    addAndMakeVisible (inputGainValueLabel);

    inputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.getAPVTS(), ParameterIDs::inputGain, inputGainSlider);

    // --- 4. OUT GAIN スライダー (スリットエッジ発光) ---
    outputGainSlider.setSliderStyle (juce::Slider::LinearVertical);
    outputGainSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    outputGainSlider.setComponentID ("outGainSlider");
    addAndMakeVisible (outputGainSlider);

    outputGainLabel.setText ("OUT GAIN", juce::dontSendNotification);
    outputGainLabel.setJustificationType (juce::Justification::centred);
    outputGainLabel.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    outputGainLabel.setColour (juce::Label::textColourId, juce::Colour (0x94, 0xa3, 0xb8));
    addAndMakeVisible (outputGainLabel);

    outputGainValueLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    outputGainValueLabel.setJustificationType (juce::Justification::centred);
    outputGainValueLabel.setColour (juce::Label::textColourId, juce::Colour (0xe2, 0xe8, 0xf0));
    addAndMakeVisible (outputGainValueLabel);

    outputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.getAPVTS(), ParameterIDs::outputGain, outputGainSlider);

    // --- 5. TARGET LEVEL 縦長スライダー (中央黒溝＋青白エッジ発光) ---
    targetLevelSlider.setSliderStyle (juce::Slider::LinearVertical);
    targetLevelSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    targetLevelSlider.setComponentID ("targetSlider");
    addAndMakeVisible (targetLevelSlider);

    targetLevelLabel.setText ("TARGET\nLEVEL", juce::dontSendNotification);
    targetLevelLabel.setJustificationType (juce::Justification::centred);
    targetLevelLabel.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    targetLevelLabel.setColour (juce::Label::textColourId, juce::Colour (0xe0, 0xe7, 0xff));
    addAndMakeVisible (targetLevelLabel);

    targetLevelValueLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    targetLevelValueLabel.setJustificationType (juce::Justification::centred);
    targetLevelValueLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (targetLevelValueLabel);

    targetLevelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.getAPVTS(), ParameterIDs::targetLevel, targetLevelSlider);

    // --- 6. セレクター (DETECTOR / TIMING) ---
    detectionModeBox.addItem ("RMS", 1);
    detectionModeBox.addItem ("Peak", 2);
    detectionModeBox.setSelectedId (1);
    addAndMakeVisible (detectionModeBox);

    detectionModeLabel.setText ("DETECTOR", juce::dontSendNotification);
    detectionModeLabel.setFont (juce::FontOptions (9.0f, juce::Font::bold));
    detectionModeLabel.setColour (juce::Label::textColourId, juce::Colour (0x94, 0xa3, 0xb8));
    detectionModeLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (detectionModeLabel);

    detectionModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        audioProcessor.getAPVTS(), ParameterIDs::detectionMode, detectionModeBox);

    timingModeBox.addItem ("Free (ms)", 1);
    timingModeBox.addItem ("Sync (BPM)", 2);
    timingModeBox.setSelectedId (1);
    timingModeBox.onChange = [this]
    {
        updateSyncControlState();
    };
    addAndMakeVisible (timingModeBox);

    timingModeLabel.setText ("TIMING", juce::dontSendNotification);
    timingModeLabel.setFont (juce::FontOptions (9.0f, juce::Font::bold));
    timingModeLabel.setColour (juce::Label::textColourId, juce::Colour (0x94, 0xa3, 0xb8));
    timingModeLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (timingModeLabel);

    timingModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        audioProcessor.getAPVTS(), ParameterIDs::timingMode, timingModeBox);

    // ComboBoxAttachment は項目の並び順でパラメーターと対応するため、パラメーターと同じ Fast / Mid / Slow の順に並べる
    syncSpeedBox.addItem ("Fast", 1);
    syncSpeedBox.addItem ("Mid",  2);
    syncSpeedBox.addItem ("Slow", 3);
    syncSpeedBox.setSelectedId (2);
    syncSpeedBox.onChange = [this]
    {
        const bool isSync = (timingModeBox.getSelectedId() == 2);
        if (isSync)
        {
            const double currentSyncVal = static_cast<double>(2 - syncSpeedBox.getSelectedItemIndex());
            if (std::abs (speedSlider.getValue() - currentSyncVal) > 0.01)
                speedSlider.setValue (currentSyncVal, juce::dontSendNotification);
        }
    };
    addChildComponent (syncSpeedBox);

    syncSpeedAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        audioProcessor.getAPVTS(), ParameterIDs::syncSpeed, syncSpeedBox);

    // --- 7. 下部トグル (LOOKAHEAD / GUI RENDER) ---
    lookaheadButton.setButtonText ("LOOKAHEAD (5ms)");
    addAndMakeVisible (lookaheadButton);

    lookaheadAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.getAPVTS(), ParameterIDs::lookaheadEnable, lookaheadButton);

    guiEnableButton.setButtonText ("GUI RENDER");
    addAndMakeVisible (guiEnableButton);
    guiEnableButton.onClick = [this]
    {
        updateTimerState();
    };

    guiEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.getAPVTS(), ParameterIDs::guiEnable, guiEnableButton);

    // --- 8. トップヘッダー内ボタン (BREATH / SIBILANCE / BYPASS) ---
    breathFilterButton.setButtonText ("BREATH");
    addAndMakeVisible (breathFilterButton);

    breathFilterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.getAPVTS(), ParameterIDs::breathFilter, breathFilterButton);

    sibilanceFilterButton.setButtonText ("SIBILANCE");
    addAndMakeVisible (sibilanceFilterButton);

    sibilanceFilterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.getAPVTS(), ParameterIDs::sibilanceFilter, sibilanceFilterButton);

    bypassButton.setButtonText ("BYPASS");
    addAndMakeVisible (bypassButton);

    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.getAPVTS(), ParameterIDs::bypass, bypassButton);

    // --- 9. 右端 METERS パネル ＆ メーターモード切替 ---
    meterModeBox.addItem ("Peak",  1);
    meterModeBox.addItem ("RMS",   2);
    meterModeBox.addItem ("VU-18", 3);
    meterModeBox.setSelectedId (1);
    addAndMakeVisible (meterModeBox);

    meterModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        audioProcessor.getAPVTS(), ParameterIDs::meterMode, meterModeBox);

    // --- 10. トップヘッダー新設コントロール (プリセット, 保存, ズーム, カラー, X / YT) ---
    // プリセットセレクター
    refreshPresetBox();
    presetBox.onChange = [this]
    {
        const int pIdx = presetBox.getSelectedId() - 1;
        if (pIdx >= 0)
        {
            audioProcessor.setCurrentProgram (pIdx);
            updateSyncControlState();
        }
    };
    addAndMakeVisible (presetBox);

    // プリセット保存ボタン (SAVE)
    savePresetBtn.setButtonText ("SAVE");
    savePresetBtn.setTooltip ("Save current settings as a new user preset");
    savePresetBtn.onClick = [this]
    {
        auto* alert = new juce::AlertWindow ("Save User Preset", "Enter a name for the new preset:", juce::AlertWindow::NoIcon);
        alert->addTextEditor ("presetName", "My Preset", "Preset Name:");
        alert->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        alert->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        alert->enterModalState (true, juce::ModalCallbackFunction::create ([this, alert] (int result)
        {
            if (result == 1)
            {
                const auto name = alert->getTextEditorContents ("presetName");
                if (name.isNotEmpty())
                {
                    if (audioProcessor.saveUserPreset (name))
                    {
                        refreshPresetBox();
                    }
                }
            }
        }), true);
    };
    addAndMakeVisible (savePresetBtn);

    // ズームコントロール (50% ~ 130%, 10%ステップ)
    zoomLabel.setText ("100%", juce::dontSendNotification);
    zoomLabel.setJustificationType (juce::Justification::centred);
    zoomLabel.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    zoomLabel.setColour (juce::Label::textColourId, juce::Colour (0x94, 0xa3, 0xb8));
    addAndMakeVisible (zoomLabel);

    zoomOutBtn.setButtonText ("-");
    zoomOutBtn.setTooltip ("Zoom Out (Min 50%)");
    zoomOutBtn.onClick = [this]
    {
        if (currentUiScale > 0.55f)
        {
            currentUiScale = juce::jlimit (0.5f, 1.3f, std::round ((currentUiScale - 0.1f) * 10.0f) / 10.0f);
            setScaleFactor (currentUiScale);
            zoomLabel.setText (juce::String (juce::roundToInt (currentUiScale * 100.0f)) + "%", juce::dontSendNotification);
        }
    };
    addAndMakeVisible (zoomOutBtn);

    zoomInBtn.setButtonText ("+");
    zoomInBtn.setTooltip ("Zoom In (Max 130%)");
    zoomInBtn.onClick = [this]
    {
        if (currentUiScale < 1.25f)
        {
            currentUiScale = juce::jlimit (0.5f, 1.3f, std::round ((currentUiScale + 0.1f) * 10.0f) / 10.0f);
            setScaleFactor (currentUiScale);
            zoomLabel.setText (juce::String (juce::roundToInt (currentUiScale * 100.0f)) + "%", juce::dontSendNotification);
        }
    };
    addAndMakeVisible (zoomInBtn);

    // カラー変更ボタン (Dark / White トグル切り替え)
    colorThemeBtn.setButtonText ("COLOR");
    colorThemeBtn.setTooltip ("Switch between Dark Mode and White Mode");
    colorThemeBtn.onClick = [this]
    {
        isWhiteMode = !isWhiteMode;
        updateThemeColours();
    };
    addAndMakeVisible (colorThemeBtn);

    // 公式SNSリンクボタン (X & YouTube / YT - 主張しすぎない馴染むデザイン)
    xLinkBtn.setButtonText ("X");
    xLinkBtn.setTooltip ("Open @atafuta09 on X");
    xLinkBtn.onClick = []
    {
        juce::URL ("https://x.com/atafuta09").launchInDefaultBrowser();
    };
    addAndMakeVisible (xLinkBtn);

    ytLinkBtn.setButtonText ("YT");
    ytLinkBtn.setTooltip ("Open @atafuta09 on YouTube");
    ytLinkBtn.onClick = []
    {
        juce::URL ("https://www.youtube.com/@atafuta09").launchInDefaultBrowser();
    };
    addAndMakeVisible (ytLinkBtn);

    addAndMakeVisible (waveformComponent);
    addAndMakeVisible (slimMeterComponent);

    // ウィンドウサイズ設定 (1000 × 580 px)
    setSize (1000, 580);

    updateSyncControlState();
    updateTimerState();

    // 初回から即座に滑らかに描画を開始
    startTimerHz (60);
}

AutoLevelerAudioProcessorEditor::~AutoLevelerAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void AutoLevelerAudioProcessorEditor::refreshPresetBox()
{
    presetBox.clear (juce::dontSendNotification);
    const auto& presetsList = audioProcessor.getPresets();
    for (size_t i = 0; i < presetsList.size(); ++i)
        presetBox.addItem (presetsList[i].name, static_cast<int>(i + 1));
    presetBox.setSelectedId (audioProcessor.getCurrentProgram() + 1, juce::dontSendNotification);
}

void AutoLevelerAudioProcessorEditor::updateThemeColours()
{
    modernDarkLookAndFeel.setWhiteMode (isWhiteMode);
    colorThemeBtn.setButtonText (isWhiteMode ? "WHITE" : "DARK");

    if (isWhiteMode)
    {
        // 高コントラスト・ホワイトモード用テキストカラー
        speedLabel.setColour (juce::Label::textColourId, juce::Colour (0x0f, 0x17, 0x2a));
        rangeLabel.setColour (juce::Label::textColourId, juce::Colour (0x0f, 0x17, 0x2a));
        targetLevelLabel.setColour (juce::Label::textColourId, juce::Colour (0x0f, 0x17, 0x2a));

        attackReleaseLabel.setColour (juce::Label::textColourId, juce::Colour (0x02, 0x84, 0xc7));
        rangeValueLabel.setColour (juce::Label::textColourId, juce::Colour (0x02, 0x84, 0xc7));
        targetLevelValueLabel.setColour (juce::Label::textColourId, juce::Colour (0x02, 0x84, 0xc7));

        inputGainLabel.setColour (juce::Label::textColourId, juce::Colour (0x47, 0x55, 0x69));
        outputGainLabel.setColour (juce::Label::textColourId, juce::Colour (0x47, 0x55, 0x69));
        inputGainValueLabel.setColour (juce::Label::textColourId, juce::Colour (0x0f, 0x17, 0x2a));
        outputGainValueLabel.setColour (juce::Label::textColourId, juce::Colour (0x0f, 0x17, 0x2a));

        detectionModeLabel.setColour (juce::Label::textColourId, juce::Colour (0x47, 0x55, 0x69));
        timingModeLabel.setColour (juce::Label::textColourId, juce::Colour (0x47, 0x55, 0x69));
        zoomLabel.setColour (juce::Label::textColourId, juce::Colour (0x33, 0x41, 0x55));
    }
    else
    {
        // シック・ダークモード用テキストカラー
        speedLabel.setColour (juce::Label::textColourId, juce::Colour (0xe0, 0xe7, 0xff));
        rangeLabel.setColour (juce::Label::textColourId, juce::Colour (0xe0, 0xe7, 0xff));
        targetLevelLabel.setColour (juce::Label::textColourId, juce::Colour (0xe0, 0xe7, 0xff));

        attackReleaseLabel.setColour (juce::Label::textColourId, juce::Colour (0x93, 0xc5, 0xfd));
        rangeValueLabel.setColour (juce::Label::textColourId, juce::Colour (0xf0, 0xf9, 0xff));
        targetLevelValueLabel.setColour (juce::Label::textColourId, juce::Colours::white);

        inputGainLabel.setColour (juce::Label::textColourId, juce::Colour (0x94, 0xa3, 0xb8));
        outputGainLabel.setColour (juce::Label::textColourId, juce::Colour (0x94, 0xa3, 0xb8));
        inputGainValueLabel.setColour (juce::Label::textColourId, juce::Colour (0xe2, 0xe8, 0xf0));
        outputGainValueLabel.setColour (juce::Label::textColourId, juce::Colour (0xe2, 0xe8, 0xf0));

        detectionModeLabel.setColour (juce::Label::textColourId, juce::Colour (0x94, 0xa3, 0xb8));
        timingModeLabel.setColour (juce::Label::textColourId, juce::Colour (0x94, 0xa3, 0xb8));
        zoomLabel.setColour (juce::Label::textColourId, juce::Colour (0x94, 0xa3, 0xb8));
    }

    sendLookAndFeelChange();
    repaint();
}

void AutoLevelerAudioProcessorEditor::updateSyncControlState()
{
    const bool isSyncMode = (timingModeBox.getSelectedId() == 2);

    if (isSyncMode)
    {
        // BPM モード時: SPEED ノブを 3段階ステップ (0: Slow, 1: Mid, 2: Fast) に切り替え
        speedAttachment.reset();
        speedSlider.setRange (0.0, 2.0, 1.0);
        speedSlider.setValue (static_cast<double>(2 - syncSpeedBox.getSelectedItemIndex()), juce::dontSendNotification);
    }
    else
    {
        // Free モード時: SPEED ノブを通常の連続可変 (0〜100%) に復帰
        speedSlider.setRange (0.0, 100.0, 0.1);
        speedAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            audioProcessor.getAPVTS(), ParameterIDs::speed, speedSlider);
    }
}

void AutoLevelerAudioProcessorEditor::updateTimerState()
{
    const bool isGuiOn = guiEnableButton.getToggleState();

    if (isGuiOn)
    {
        if (!isTimerRunning())
            startTimerHz (60);
    }
    else
    {
        if (isTimerRunning())
            stopTimer();
    }

    waveformComponent.setGuiEnabled (isGuiOn);
    slimMeterComponent.setGuiEnabled (isGuiOn);
    targetLevelSlider.setGuiEnabled (isGuiOn);
}

void AutoLevelerAudioProcessorEditor::visibilityChanged()
{
    updateTimerState();
}

void AutoLevelerAudioProcessorEditor::timerCallback()
{
    if (!guiEnableButton.getToggleState())
        return;

    // 1. ProcessorのFIFOから波形データを取得
    constexpr int maxReadPoints = 64;
    VisualDataPoint points[maxReadPoints];
    const int numRead = audioProcessor.readVisualData (points, maxReadPoints);
    if (numRead > 0)
    {
        waveformComponent.pushData (points, numRead);
    }

    // 2. ターゲットレベルとレンジ幅の視覚化更新
    const float curTarget = static_cast<float>(targetLevelSlider.getValue());
    const float curRange  = static_cast<float>(rangeSlider.getValue());
    waveformComponent.setVisualParams (curTarget, curRange);

    // 3. 数値ラベルのリアルタイム更新
    targetLevelValueLabel.setText (juce::String (curTarget, 1) + " dB", juce::dontSendNotification);
    rangeValueLabel.setText (juce::String (curRange, 1) + " dB", juce::dontSendNotification);

    const float curInGain = static_cast<float>(inputGainSlider.getValue());
    inputGainValueLabel.setText ((curInGain > 0.0f ? "+" : "") + juce::String (curInGain, 1) + " dB", juce::dontSendNotification);

    const float curOutGain = static_cast<float>(outputGainSlider.getValue());
    outputGainValueLabel.setText ((curOutGain > 0.0f ? "+" : "") + juce::String (curOutGain, 1) + " dB", juce::dontSendNotification);

    // 4. Attack / Release 値の両方を表示 (ユーザー要望対応)
    const bool isSyncMode = (timingModeBox.getSelectedId() == 2);
    if (isSyncMode)
    {
        // ノブ位置 (0: Slow, 2: Fast) をパラメーターの添字 (0: Fast, 2: Slow) に変換
        const int knobStep = juce::jlimit (0, 2, juce::roundToInt (speedSlider.getValue()));
        const auto timing = AutoLevelerAudioProcessor::calculateTiming (
            50.0f, true, 2 - knobStep, audioProcessor.getCurrentBpm());

        attackReleaseLabel.setText (timing.modeName + ": Att " + timing.attackLabel + " | Rel " + timing.releaseLabel, juce::dontSendNotification);
    }
    else
    {
        const auto timing = AutoLevelerAudioProcessor::calculateTiming (
            static_cast<float>(speedSlider.getValue()), false, 1, audioProcessor.getCurrentBpm());

        attackReleaseLabel.setText ("Att " + timing.attackLabel + " | Rel " + timing.releaseLabel, juce::dontSendNotification);
    }

    // 5. TARGET LEVEL フェーダー内部のリアルタイム入力メーター更新 (トラック内メーター統合型)
    const float inLinear = audioProcessor.getLatestInputRms();
    const float inDb     = inLinear > 0.0001f ? juce::Decibels::gainToDecibels (inLinear, -60.0f) : -60.0f;
    targetLevelSlider.setInputMeterLevel (inDb);

    // 6. 右端スリムメーターの更新 (Peak / RMS / VU)
    const int activeMeterMode = meterModeBox.getSelectedId() - 1;
    float inVal  = 0.0f;
    float outVal = 0.0f;
    if (activeMeterMode == 1) // RMS
    {
        inVal  = audioProcessor.getLatestInputRms();
        outVal = audioProcessor.getLatestOutputRms();
    }
    else if (activeMeterMode == 2) // VU
    {
        inVal  = audioProcessor.getLatestInputVu();
        outVal = audioProcessor.getLatestOutputVu();
    }
    else // Peak (0)
    {
        inVal  = audioProcessor.getLatestInputPeak();
        outVal = audioProcessor.getLatestOutputPeak();
    }
    slimMeterComponent.updateLevels (inVal, outVal, activeMeterMode);
}

void AutoLevelerAudioProcessorEditor::paint (juce::Graphics& g)
{
    if (isWhiteMode)
    {
        // 高品位スタジオシルバー＆ホワイトシャーシ
        g.fillAll (juce::Colour (0xf1, 0xf5, 0xf9));

        // トップブランドヘッダーバー (42px)
        const auto headerBounds = getLocalBounds().removeFromTop (42);
        g.setColour (juce::Colour (0xff, 0xff, 0xff));
        g.fillRect (headerBounds);

        g.setColour (juce::Colour (0xcb, 0xd5, 0xe1));
        g.drawHorizontalLine (42, 0.0f, static_cast<float>(getWidth()));

        // プラグイン名ロゴ (Atafuta09Leveler)
        const auto titleRect = juce::Rectangle<float> (18.0f, 0.0f, 155.0f, 42.0f);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));

        // ディープスレートシャドウ
        g.setColour (juce::Colour (0x02, 0x84, 0xc7).withAlpha (0.22f));
        g.drawText ("Atafuta09Leveler", titleRect.translated (1.0f, 1.0f), juce::Justification::centredLeft);

        g.setColour (juce::Colour (0x0f, 0x17, 0x2a));
        g.drawText ("Atafuta09Leveler", titleRect, juce::Justification::centredLeft);

        // バージョン表記 (Ver 1.04)
        g.setColour (juce::Colour (0x64, 0x74, 0x8b));
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText ("Ver 1.04", 175, 0, 60, 42, juce::Justification::centredLeft);
    }
    else
    {
        // 実機梨地ダークシャーシ背景
        g.fillAll (juce::Colour (0x0a, 0x0b, 0x0f));

        // トップブランドヘッダーバー (42px)
        const auto headerBounds = getLocalBounds().removeFromTop (42);
        g.setColour (juce::Colour (0x0e, 0x10, 0x17));
        g.fillRect (headerBounds);

        g.setColour (juce::Colour (0x1a, 0x1e, 0x2a));
        g.drawHorizontalLine (42, 0.0f, static_cast<float>(getWidth()));

        // プラグイン名ロゴ (Atafuta09Leveler)
        const auto titleRect = juce::Rectangle<float> (18.0f, 0.0f, 155.0f, 42.0f);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));

        // 控えめなソフトアイスブルーの微細グロー (1px オフセット, alpha 0.35)
        g.setColour (juce::Colour (0x93, 0xc5, 0xfd).withAlpha (0.35f));
        for (int dx = -1; dx <= 1; ++dx)
        {
            for (int dy = -1; dy <= 1; ++dy)
            {
                if (dx != 0 || dy != 0)
                    g.drawText ("Atafuta09Leveler", titleRect.translated (static_cast<float>(dx), static_cast<float>(dy)), juce::Justification::centredLeft);
            }
        }

        // 上品な白熱コア (純白よりほんの少し優しいソフトホワイト)
        g.setColour (juce::Colour (0xf8, 0xfa, 0xfc));
        g.drawText ("Atafuta09Leveler", titleRect, juce::Justification::centredLeft);

        // バージョン表記 (Ver 1.04)
        g.setColour (juce::Colour (0x64, 0x74, 0x8b));
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText ("Ver 1.04", 175, 0, 60, 42, juce::Justification::centredLeft);
    }
}

void AutoLevelerAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // 1. トップヘッダー (高さ42px: ロゴ、バージョン、X/YT、COLOR、プリセット+SAVE、ズーム、フィルター、BYPASS)
    auto headerArea = bounds.removeFromTop (42);

    // 右端: BYPASS, SIBILANCE, BREATH
    bypassButton.setBounds          (headerArea.removeFromRight (82).reduced (4, 9));
    sibilanceFilterButton.setBounds (headerArea.removeFromRight (98).reduced (4, 9));
    breathFilterButton.setBounds    (headerArea.removeFromRight (90).reduced (4, 9));

    // 左側: ロゴ(0~170px) + Ver 1.04(175~235px) の直後
    auto leftHeader = headerArea.removeFromLeft (375);
    leftHeader.removeFromLeft (240); // ロゴ & バージョン領域スキップ

    // X と YT ボタン (主張しすぎない実機調コンパクトボタン)
    xLinkBtn.setBounds  (leftHeader.removeFromLeft (28).reduced (1, 9));
    ytLinkBtn.setBounds (leftHeader.removeFromLeft (32).reduced (1, 9));
    leftHeader.removeFromLeft (6); // スペーサー
    colorThemeBtn.setBounds (leftHeader.removeFromLeft (52).reduced (1, 9));

    // 中央〜右寄り: ZOOM コントロール & プリセットドロップダウン + SAVEボタン
    headerArea.removeFromRight (14); // フィルターとの間隔
    auto zoomArea = headerArea.removeFromRight (82).reduced (0, 9);
    zoomOutBtn.setBounds (zoomArea.removeFromLeft (20));
    zoomLabel.setBounds  (zoomArea.removeFromLeft (40));
    zoomInBtn.setBounds  (zoomArea);

    headerArea.removeFromRight (14); // ズームとの間隔
    savePresetBtn.setBounds (headerArea.removeFromRight (46).reduced (0, 9));
    headerArea.removeFromRight (4); // SAVEとPRESETの間隔
    presetBox.setBounds (headerArea.removeFromRight (130).reduced (0, 9));

    // 2. メインボディ (パディング10px)
    auto mainBody = bounds.reduced (10);

    // --- A. 左側 CONTROL PANEL (幅285px: 上下バランスの黄金比率レイアウト) ---
    auto ctrlPanel = mainBody.removeFromLeft (285);

    // 下部フッタートグル (高さ 28px)
    auto footerToggles = ctrlPanel.removeFromBottom (28);
    lookaheadButton.setBounds (footerToggles.removeFromLeft (145).reduced (2, 2));
    guiEnableButton.setBounds (footerToggles.reduced (2, 2));

    ctrlPanel.removeFromBottom (12); // フッター上の調和マージン (残り高さ 478px)

    // 上部コントロールエリア (左右2分割)
    // 右サブ列: TARGET LEVEL フェーダー (幅70px: トラック内メーター統合型)
    const int targetColW = 70;
    auto targetCol = ctrlPanel.removeFromRight (targetColW);
    ctrlPanel.removeFromRight (10); // 列間ギャップ

    // [左サブ列 (幅 205px, 有効高さ 478px)]
    // 1段目: SPEED & RANGE ノブセクション (高さ152px)
    auto knobRow = ctrlPanel.removeFromTop (152);
    const int halfW = knobRow.getWidth() / 2;

    auto speedCol = knobRow.removeFromLeft (halfW).reduced (2, 0);
    speedLabel.setBounds         (speedCol.removeFromTop (18));
    attackReleaseLabel.setBounds (speedCol.removeFromBottom (18));
    speedSlider.setBounds        (speedCol);

    auto rangeCol = knobRow.reduced (2, 0);
    rangeLabel.setBounds      (rangeCol.removeFromTop (18));
    rangeValueLabel.setBounds (rangeCol.removeFromBottom (18));
    rangeSlider.setBounds     (rangeCol);

    ctrlPanel.removeFromTop (14); // セクション間マージン

    // 2段目: IN GAIN & OUT GAIN スライダースリット (高さ190px)
    auto gainRow = ctrlPanel.removeFromTop (190);
    const int halfGainW = gainRow.getWidth() / 2;

    auto inGainCol = gainRow.removeFromLeft (halfGainW).reduced (4, 0);
    inputGainLabel.setBounds      (inGainCol.removeFromTop (18));
    inputGainValueLabel.setBounds (inGainCol.removeFromBottom (18));
    inputGainSlider.setBounds     (inGainCol);

    auto outGainCol = gainRow.reduced (4, 0);
    outputGainLabel.setBounds      (outGainCol.removeFromTop (18));
    outputGainValueLabel.setBounds (outGainCol.removeFromBottom (18));
    outputGainSlider.setBounds     (outGainCol);

    ctrlPanel.removeFromTop (14); // セクション間マージン

    // 3段目: DETECTOR & TIMING セレクターセクション (残り高さ 108px を美しく充填)
    auto selRow = ctrlPanel;
    const int selRowH = 34;
    selRow.removeFromTop (13); // 上部均等パディング

    auto detSec = selRow.removeFromTop (selRowH).reduced (2, 1);
    detectionModeLabel.setBounds (detSec.removeFromLeft (70));
    detectionModeBox.setBounds   (detSec);

    selRow.removeFromTop (8); // セレクター行間

    auto timingSec = selRow.removeFromTop (selRowH).reduced (2, 1);
    timingModeLabel.setBounds (timingSec.removeFromLeft (70));
    timingModeBox.setBounds   (timingSec);

    // [右サブ列: TARGET LEVEL 縦長フェーダー (下端ラインを左サブ列と完全に整列)]
    targetLevelLabel.setBounds      (targetCol.removeFromTop (24));
    targetLevelValueLabel.setBounds (targetCol.removeFromTop (18));
    targetCol.removeFromTop (6);
    targetLevelSlider.setBounds     (targetCol);

    // --- B. 右側 METERS パネル (幅88px) ---
    mainBody.removeFromLeft (10); // ギャップ
    auto meterPanel = mainBody.removeFromRight (88);

    // メーターモードセレクター (上部)
    meterModeBox.setBounds (meterPanel.removeFromTop (24).reduced (4, 1));
    meterPanel.removeFromTop (6);
    slimMeterComponent.setBounds (meterPanel);

    // --- C. 中央 WAVEFORM DISPLAY (残り幅) ---
    mainBody.removeFromRight (10); // ギャップ
    waveformComponent.setBounds (mainBody);

    syncTargetSliderLayout();
}

void AutoLevelerAudioProcessorEditor::syncTargetSliderLayout()
{
}
