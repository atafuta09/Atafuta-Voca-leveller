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
        auto updatePeakHold = [] (float curDb, float& peakDb, int& holdTimer)
        {
            if (curDb >= peakDb)
            {
                peakDb = curDb;
                holdTimer = 90; // 60fps で 90フレーム (約1.5秒)
            }
            else if (holdTimer > 0)
                --holdTimer;
            else
                peakDb = juce::jmax (curDb, peakDb - 0.7f);
        };
        updatePeakHold (curInDb,  inputPeakDb,  inHoldTimer);
        updatePeakHold (curOutDb, outputPeakDb, outHoldTimer);
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

    // ラベル共通設定 (文字色は updateThemeColours() で設定する)
    auto setupLabel = [this] (juce::Label& label, const juce::String& text, float fontSize)
    {
        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::FontOptions (fontSize, juce::Font::bold));
        addAndMakeVisible (label);
    };

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

    setupLabel (speedLabel, "SPEED", 11.0f);
    setupLabel (attackReleaseLabel, {}, 10.0f); // Attack / Release の両方の値を表示するバッジ

    speedAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.getAPVTS(), ParameterIDs::speed, speedSlider);

    // --- 2. RANGE ノブ (0 〜 13 dB、範囲はパラメーター定義からアタッチメントが設定する) ---
    rangeSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    rangeSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (rangeSlider);

    setupLabel (rangeLabel, "RANGE", 11.0f);
    setupLabel (rangeValueLabel, {}, 12.0f);

    rangeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.getAPVTS(), ParameterIDs::range, rangeSlider);

    // --- 3. IN GAIN スライダー (スリットエッジ発光) ---
    inputGainSlider.setSliderStyle (juce::Slider::LinearVertical);
    inputGainSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    inputGainSlider.setComponentID ("inGainSlider");
    addAndMakeVisible (inputGainSlider);

    setupLabel (inputGainLabel, "IN GAIN", 10.0f);
    setupLabel (inputGainValueLabel, {}, 11.0f);

    inputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.getAPVTS(), ParameterIDs::inputGain, inputGainSlider);

    // --- 4. OUT GAIN スライダー (スリットエッジ発光) ---
    outputGainSlider.setSliderStyle (juce::Slider::LinearVertical);
    outputGainSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    outputGainSlider.setComponentID ("outGainSlider");
    addAndMakeVisible (outputGainSlider);

    setupLabel (outputGainLabel, "OUT GAIN", 10.0f);
    setupLabel (outputGainValueLabel, {}, 11.0f);

    outputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.getAPVTS(), ParameterIDs::outputGain, outputGainSlider);

    // --- 5. TARGET LEVEL 縦長スライダー (中央黒溝＋青白エッジ発光) ---
    targetLevelSlider.setSliderStyle (juce::Slider::LinearVertical);
    targetLevelSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    targetLevelSlider.setComponentID ("targetSlider");
    addAndMakeVisible (targetLevelSlider);

    setupLabel (targetLevelLabel, "TARGET\nLEVEL", 10.0f);
    setupLabel (targetLevelValueLabel, {}, 12.0f);

    targetLevelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.getAPVTS(), ParameterIDs::targetLevel, targetLevelSlider);

    // --- 6. セレクター (DETECTOR / TIMING) ---
    detectionModeBox.addItem ("RMS", 1);
    detectionModeBox.addItem ("Peak", 2);
    detectionModeBox.setSelectedId (1);
    addAndMakeVisible (detectionModeBox);

    setupLabel (detectionModeLabel, "DETECTOR", 9.0f);

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

    setupLabel (timingModeLabel, "TIMING", 9.0f);

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
    lookaheadButton.setButtonText ("LOOKAHEAD (22.5ms)");
    addAndMakeVisible (lookaheadButton);

    lookaheadAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.getAPVTS(), ParameterIDs::lookaheadEnable, lookaheadButton);

    guiEnableButton.setButtonText ("GUI RENDER");
    guiEnableButton.setToggleState (true, juce::dontSendNotification);
    addAndMakeVisible (guiEnableButton);
    guiEnableButton.onClick = [this]
    {
        updateTimerState();
    };

    guiEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.getAPVTS(), ParameterIDs::guiEnable, guiEnableButton);

    // --- 8. トップヘッダー内ボタン (SC FILTER / BYPASS) ---
    scFilterButton.setButtonText ("SC FILTER");
    scFilterButton.setTooltip ("Zero-coloration 100Hz HPF Sidechain Filter (Prevents vocal plosives & low-end pumping)");
    addAndMakeVisible (scFilterButton);

    scFilterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.getAPVTS(), ParameterIDs::scFilterEnable, scFilterButton);

    bypassButton.setButtonText ("BYPASS");
    addAndMakeVisible (bypassButton);

    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.getAPVTS(), ParameterIDs::bypass, bypassButton);

    // --- TAME NOISE セクション (ON/OFF, LISTEN, AMOUNTノブ) ---
    tameNoiseButton.setButtonText ("TAME NOISE");
    tameNoiseButton.setTooltip ("Enable AI Vocal Noise Suppression (Sibilance, Breath, Plosives)");
    addAndMakeVisible (tameNoiseButton);

    tameNoiseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.getAPVTS(), ParameterIDs::tameNoiseEnable, tameNoiseButton);

    tameListenButton.setButtonText ("LISTEN");
    tameListenButton.setTooltip ("Solo monitor detected noise artifacts");
    addAndMakeVisible (tameListenButton);

    tameListenAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.getAPVTS(), ParameterIDs::tameNoiseListen, tameListenButton);

    tameAmountSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    tameAmountSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    tameAmountSlider.setRange (0.0, 100.0, 0.5);
    addAndMakeVisible (tameAmountSlider);

    tameAmountLabel.setText ("TAME SENS", juce::dontSendNotification);
    tameAmountLabel.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    tameAmountLabel.setJustificationType (juce::Justification::centredLeft);
    tameAmountLabel.setColour (juce::Label::textColourId, juce::Colour (0xe0, 0xe7, 0xff));
    tameAmountLabel.setTooltip ("Detection Sensitivity (0-100%, Threshold: 0.85 - 0.20)");
    tameAmountSlider.setTooltip ("Detection Sensitivity (0-100%, Threshold: 0.85 - 0.20)");
    addAndMakeVisible (tameAmountLabel);

    tameAmountValueLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    tameAmountValueLabel.setJustificationType (juce::Justification::centredLeft);
    tameAmountValueLabel.setColour (juce::Label::textColourId, juce::Colour (0x38, 0xbd, 0xf8));
    addAndMakeVisible (tameAmountValueLabel);

    tameAmountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.getAPVTS(), ParameterIDs::tameNoiseAmount, tameAmountSlider);

    // TAME RELEASE ノブ (10ms ~ 500ms 調整ノブ ＆ ms数値表示)

    // TameNoise 検出専用 LED インジケーター
    addAndMakeVisible (tameNoiseLed);

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
    presetBox.setTextWhenNothingSelected ("(No Presets)");
    refreshPresetBox();
    presetBox.onChange = [this]
    {
        const int pIdx = presetBox.getSelectedId() - 1;
        if (pIdx >= 0 && pIdx < static_cast<int>(audioProcessor.getPresets().size()))
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
            // 名前の trim と空チェックは saveUserPreset 側で行う
            if (result == 1 && audioProcessor.saveUserPreset (alert->getTextEditorContents ("presetName")))
                refreshPresetBox();
        }), true);
    };
    addAndMakeVisible (savePresetBtn);

    // ズームコントロール (50% ~ 130%, 10%ステップ)
    setupLabel (zoomLabel, "100%", 10.0f);

    auto applyZoom = [this] (float delta)
    {
        currentUiScale = juce::jlimit (0.5f, 1.3f, std::round ((currentUiScale + delta) * 10.0f) / 10.0f);
        setScaleFactor (currentUiScale);
        zoomLabel.setText (juce::String (juce::roundToInt (currentUiScale * 100.0f)) + "%", juce::dontSendNotification);
    };

    zoomOutBtn.setButtonText ("-");
    zoomOutBtn.setTooltip ("Zoom Out (Min 50%)");
    zoomOutBtn.onClick = [applyZoom] { applyZoom (-0.1f); };
    addAndMakeVisible (zoomOutBtn);

    zoomInBtn.setButtonText ("+");
    zoomInBtn.setTooltip ("Zoom In (Max 130%)");
    zoomInBtn.onClick = [applyZoom] { applyZoom (0.1f); };
    addAndMakeVisible (zoomInBtn);

    // ラベルの文字色を初期テーマ (ダーク) で設定
    updateThemeColours();

    // カラー変更ボタン (Dark / White トグル切り替え。初期表示は COLOR)
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

    // 起動時は確実に GUI RENDER を ON に設定
    if (auto* param = audioProcessor.getAPVTS().getParameter (ParameterIDs::guiEnable))
        param->setValueNotifyingHost (1.0f);
    guiEnableButton.setToggleState (true, juce::dontSendNotification);
    updateTimerState();
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
    if (presetsList.empty())
    {
        presetBox.setTextWhenNothingSelected ("(No Presets)");
        presetBox.setSelectedId (0, juce::dontSendNotification);
        return;
    }

    for (size_t i = 0; i < presetsList.size(); ++i)
        presetBox.addItem (presetsList[i].name, static_cast<int>(i + 1));

    const int curProg = audioProcessor.getCurrentProgram();
    if (curProg >= 0 && curProg < static_cast<int>(presetsList.size()))
        presetBox.setSelectedId (curProg + 1, juce::dontSendNotification);
    else
        presetBox.setSelectedId (1, juce::dontSendNotification);
}

void AutoLevelerAudioProcessorEditor::updateThemeColours()
{
    modernDarkLookAndFeel.setWhiteMode (isWhiteMode);
    colorThemeBtn.setButtonText (isWhiteMode ? "WHITE" : "DARK");

    // { ラベル, ダーク時の色, ホワイト時の色 }
    const juce::Colour darkHeading (0xe0, 0xe7, 0xff), darkMuted (0x94, 0xa3, 0xb8), darkValue (0xe2, 0xe8, 0xf0);
    const juce::Colour whiteHeading (0x0f, 0x17, 0x2a), whiteMuted (0x47, 0x55, 0x69), whiteAccent (0x02, 0x84, 0xc7);
    const std::tuple<juce::Label&, juce::Colour, juce::Colour> labelColours[] = {
        { speedLabel,            darkHeading,                   whiteHeading },
        { rangeLabel,            darkHeading,                   whiteHeading },
        { targetLevelLabel,      darkHeading,                   whiteHeading },
        { attackReleaseLabel,    juce::Colour (0x93, 0xc5, 0xfd), whiteAccent },
        { rangeValueLabel,       juce::Colour (0xf0, 0xf9, 0xff), whiteAccent },
        { targetLevelValueLabel, juce::Colours::white,          whiteAccent },
        { inputGainLabel,        darkMuted,                     whiteMuted },
        { outputGainLabel,       darkMuted,                     whiteMuted },
        { inputGainValueLabel,   darkValue,                     whiteHeading },
        { outputGainValueLabel,  darkValue,                     whiteHeading },
        { detectionModeLabel,    darkMuted,                     whiteMuted },
        { timingModeLabel,       darkMuted,                     whiteMuted },
        { zoomLabel,             darkMuted,                     juce::Colour (0x33, 0x41, 0x55) },
    };
    for (auto& [label, dark, white] : labelColours)
        label.setColour (juce::Label::textColourId, isWhiteMode ? white : dark);

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
    repaint();
}

void AutoLevelerAudioProcessorEditor::visibilityChanged()
{
    updateTimerState();
}

void AutoLevelerAudioProcessorEditor::timerCallback()
{
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

    const float curTameAmount = static_cast<float>(tameAmountSlider.getValue());
    tameAmountValueLabel.setText (juce::String (juce::roundToInt (curTameAmount)) + "%", juce::dontSendNotification);


    // 専用 LED ライトのリアルタイム点灯更新
    tameNoiseLed.setIntensity (audioProcessor.getTameNoiseLedIntensity());

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
    // シャーシ背景 (ホワイト: スタジオシルバー / ダーク: 梨地ブラック)
    g.fillAll (isWhiteMode ? juce::Colour (0xf1, 0xf5, 0xf9) : juce::Colour (0x0a, 0x0b, 0x0f));

    // トップブランドヘッダーバー (42px)
    g.setColour (isWhiteMode ? juce::Colours::white : juce::Colour (0x0e, 0x10, 0x17));
    g.fillRect (getLocalBounds().removeFromTop (42));

    g.setColour (isWhiteMode ? juce::Colour (0xcb, 0xd5, 0xe1) : juce::Colour (0x1a, 0x1e, 0x2a));
    g.drawHorizontalLine (42, 0.0f, static_cast<float>(getWidth()));

    // プラグイン名ロゴ (Atafuta09Leveler)
    const juce::String title ("Atafuta09Leveler");
    const auto titleRect = juce::Rectangle<float> (18.0f, 0.0f, 155.0f, 42.0f);
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));

    if (isWhiteMode)
    {
        // ディープスレートシャドウ
        g.setColour (juce::Colour (0x02, 0x84, 0xc7).withAlpha (0.22f));
        g.drawText (title, titleRect.translated (1.0f, 1.0f), juce::Justification::centredLeft);

        g.setColour (juce::Colour (0x0f, 0x17, 0x2a));
        g.drawText (title, titleRect, juce::Justification::centredLeft);
    }
    else
    {
        // 控えめなソフトアイスブルーの微細グロー (周囲 1px オフセット, alpha 0.35)
        g.setColour (juce::Colour (0x93, 0xc5, 0xfd).withAlpha (0.35f));
        for (int dx = -1; dx <= 1; ++dx)
        {
            for (int dy = -1; dy <= 1; ++dy)
            {
                if (dx != 0 || dy != 0)
                    g.drawText (title, titleRect.translated (static_cast<float>(dx), static_cast<float>(dy)), juce::Justification::centredLeft);
            }
        }

        // 上品な白熱コア (純白よりほんの少し優しいソフトホワイト)
        g.setColour (juce::Colour (0xf8, 0xfa, 0xfc));
        g.drawText (title, titleRect, juce::Justification::centredLeft);
    }

    // バージョン表記 (CMakeLists.txt の project(VERSION) と連動)
    g.setColour (juce::Colour (0x64, 0x74, 0x8b));
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText (juce::String ("Ver ") + JucePlugin_VersionString, 175, 0, 60, 42, juce::Justification::centredLeft);
}

void AutoLevelerAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // 1. トップヘッダー (高さ42px: ロゴ、バージョン、X/YT、COLOR、プリセット+SAVE、ズーム、フィルター、BYPASS)
    auto headerArea = bounds.removeFromTop (42);

    // 右端: BYPASS, SC FILTER
    bypassButton.setBounds   (headerArea.removeFromRight (82).reduced (4, 9));
    scFilterButton.setBounds (headerArea.removeFromRight (96).reduced (4, 9));

    // 左側: ロゴ(0~170px) + バージョン表記(175~235px) の直後
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

    // ユーザー要望：LOOKAHEAD, GUI RENDER と TAME NOISE 等を完全に同じ高さに合わせた最下部ストリップ (高さ 44px)
    auto bottomRow = mainBody.removeFromBottom (44);
    mainBody.removeFromBottom (8); // 下部マージン

    // --- A. 左側 CONTROL PANEL (幅285px) ---
    auto ctrlPanel = mainBody.removeFromLeft (285);

    // 最下部ストリップの左側エリア (幅 285px): LOOKAHEAD と GUI RENDER を美しく配置
    auto leftFooter = bottomRow.removeFromLeft (285);
    lookaheadButton.setBounds (leftFooter.removeFromLeft (142).reduced (2, 6));
    guiEnableButton.setBounds (leftFooter.reduced (2, 6));

    // 上部コントロールエリア (左右2分割)
    // 右サブ列: TARGET LEVEL フェーダー (幅70px: トラック内メーター統合型)
    const int targetColW = 70;
    auto targetCol = ctrlPanel.removeFromRight (targetColW);
    ctrlPanel.removeFromRight (10); // 列間ギャップ

    // [左サブ列 (幅 205px)]
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

    // 3段目: DETECTOR & TIMING セレクターセクション (残り高さを美しく充填)
    auto selRow = ctrlPanel;
    const int selRowH = 34;
    selRow.removeFromTop (10);

    auto detSec = selRow.removeFromTop (selRowH).reduced (2, 1);
    detectionModeLabel.setBounds (detSec.removeFromLeft (70));
    detectionModeBox.setBounds   (detSec);

    selRow.removeFromTop (8); // セレクター行間

    auto timingSec = selRow.removeFromTop (selRowH).reduced (2, 1);
    timingModeLabel.setBounds (timingSec.removeFromLeft (70));
    timingModeBox.setBounds   (timingSec);

    // [右サブ列: TARGET LEVEL 縦長フェーダー]
    targetLevelLabel.setBounds      (targetCol.removeFromTop (24));
    targetLevelValueLabel.setBounds (targetCol.removeFromTop (18));
    targetCol.removeFromTop (6);
    targetLevelSlider.setBounds     (targetCol);

    // --- B. 右側 METERS パネル (幅88px) ---
    mainBody.removeFromLeft (10); // ギャップ
    bottomRow.removeFromLeft (10); // ギャップ
    auto meterPanel = mainBody.removeFromRight (88);

    // メーターモードセレクター (上部)
    meterModeBox.setBounds (meterPanel.removeFromTop (24).reduced (4, 1));
    meterPanel.removeFromTop (6);
    slimMeterComponent.setBounds (meterPanel);

    // --- C. 中央 WAVEFORM DISPLAY (残りメインエリア) ---
    mainBody.removeFromRight (10); // ギャップ
    bottomRow.removeFromRight (98); // 右側メーター幅+ギャップ分
    waveformComponent.setBounds (mainBody);

    // 最下部ストリップの中央エリア: TAME NOISE コントロール
    // [円形 NOISE LED (28px)] [TAME NOISE (102px)] [LISTEN (76px)] --- [AMOUNT (125px)] --- [RELEASE (135px)]
    auto tameLedArea = bottomRow.removeFromLeft (28);
    tameNoiseLed.setBounds (tameLedArea);

    bottomRow.removeFromLeft (6); // LEDとボタン間の程よいスペース

    auto tameNoiseBtnArea = bottomRow.removeFromLeft (102).reduced (2, 6);
    tameNoiseButton.setBounds (tameNoiseBtnArea);

    auto tameListenBtnArea = bottomRow.removeFromLeft (76).reduced (2, 6);
    tameListenButton.setBounds (tameListenBtnArea);

    bottomRow.removeFromLeft (12); // スペーサー

    // TAME AMOUNT ノブ & ラベル
    auto amountArea = bottomRow.removeFromLeft (125);
    auto knobBox = amountArea.removeFromLeft (38).reduced (0, 2);
    tameAmountSlider.setBounds (knobBox);

    auto amountLabels = amountArea.reduced (4, 0);
    tameAmountLabel.setBounds      (amountLabels.removeFromTop (18));
    tameAmountValueLabel.setBounds (amountLabels.removeFromTop (18));
}
