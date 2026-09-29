#include "PluginProcessor.h"
#include "PluginEditor.h"

AutoLevelerAudioProcessor::AutoLevelerAudioProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout()),
      visualFifoBuffer (static_cast<size_t>(fifoCapacity))
{
    inputGainParam       = apvts.getRawParameterValue (ParameterIDs::inputGain);
    targetLevelParam     = apvts.getRawParameterValue (ParameterIDs::targetLevel);
    rangeParam           = apvts.getRawParameterValue (ParameterIDs::range);
    speedParam           = apvts.getRawParameterValue (ParameterIDs::speed);
    outputGainParam      = apvts.getRawParameterValue (ParameterIDs::outputGain);
    lookaheadEnableParam = apvts.getRawParameterValue (ParameterIDs::lookaheadEnable);
    breathFilterParam    = apvts.getRawParameterValue (ParameterIDs::breathFilter);
    sibilanceFilterParam = apvts.getRawParameterValue (ParameterIDs::sibilanceFilter);
    detectionModeParam   = apvts.getRawParameterValue (ParameterIDs::detectionMode);
    timingModeParam      = apvts.getRawParameterValue (ParameterIDs::timingMode);
    syncSpeedParam       = apvts.getRawParameterValue (ParameterIDs::syncSpeed);
    guiEnableParam       = apvts.getRawParameterValue (ParameterIDs::guiEnable);
    bypassParam          = apvts.getRawParameterValue (ParameterIDs::bypass);

    for (auto* param : { inputGainParam, targetLevelParam, rangeParam, speedParam, outputGainParam,
                         lookaheadEnableParam, breathFilterParam, sibilanceFilterParam, detectionModeParam,
                         timingModeParam, syncSpeedParam, guiEnableParam, bypassParam })
        jassertquiet (param != nullptr);

    // 8つの実践的ファクトリープリセット
    // 値の順: InGain, Target, Range, Speed, OutGain, Lookahead, Breath, Sibilance, Det(0:RMS/1:Peak), Timing(0:Free/1:Sync), SyncSpeed(0:Fast/1:Mid/2:Slow)
    presets = {
        { "Default",             { 0.0f, -24.0f,  7.0f, 93.6f, 0.0f, 1, 1, 1, 0, 0, 1 } }, // 標準 (パラメーター初期値と同じ)
        { "Synth Vocal",         { 0.0f, -14.0f,  6.0f, 75.0f, 0.0f, 1, 1, 1, 0, 0, 1 } }, // 合成音声・打ち込みボーカル特化
        { "Gentle Vocal Ride",   { 0.0f, -14.0f,  4.0f, 35.0f, 0.0f, 1, 1, 0, 0, 0, 1 } }, // 自然な音量均一化
        { "Aggressive Leveler",  { 0.0f, -10.0f, 10.0f, 75.0f, 0.0f, 1, 1, 1, 0, 0, 1 } }, // ロック・激しいボーカル向け
        { "Fast Peak Tamer",     { 0.0f,  -8.0f,  6.0f, 90.0f, 0.0f, 1, 0, 1, 1, 0, 1 } }, // ピーク抑制
        { "BPM Sync 1/4 (Mid)",  { 0.0f, -12.0f,  6.0f, 50.0f, 0.0f, 1, 0, 0, 0, 1, 1 } }, // テンポ同期ノーマル
        { "Podcast / Voiceover", { 0.0f, -16.0f,  8.0f, 45.0f, 0.0f, 1, 1, 1, 0, 0, 1 } }, // 配信・ナレーション
        { "Ballad Vocal",        { 0.0f, -15.0f,  5.0f, 25.0f, 0.0f, 1, 1, 0, 0, 0, 1 } }, // バラード向けスロー
    };
    jassert (presets.size() == numFactoryPresets);

    // ユーザー保存プリセットの読み込み
    loadUserPresets();
}

const juce::String AutoLevelerAudioProcessor::getName() const
{
    return "Atafuta09Leveler";
}

int AutoLevelerAudioProcessor::getNumPrograms()
{
    return static_cast<int>(presets.size());
}

int AutoLevelerAudioProcessor::getCurrentProgram()
{
    return currentProgram;
}

void AutoLevelerAudioProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= static_cast<int>(presets.size()))
        return;

    currentProgram = index;
    const auto& p = presets[static_cast<size_t>(index)];

    for (size_t i = 0; i < presetParams.size(); ++i)
        if (auto* param = apvts.getParameter (presetParams[i].paramID))
            param->setValueNotifyingHost (param->convertTo0to1 (p.values[i]));
}

const juce::String AutoLevelerAudioProcessor::getProgramName (int index)
{
    if (index >= 0 && index < static_cast<int>(presets.size()))
        return presets[static_cast<size_t>(index)].name;
    return {};
}

void AutoLevelerAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    // ファクトリープリセットは保存対象外のため改名させない (再起動で元に戻ってしまうため)
    if (index >= static_cast<int>(numFactoryPresets) && index < static_cast<int>(presets.size()))
    {
        presets[static_cast<size_t>(index)].name = newName;
        saveUserPresetsToFile();
    }
}

static juce::File getUserPresetFile()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Atafuta09")
                   .getChildFile ("Atafuta09Leveler");
    if (!dir.exists())
        dir.createDirectory();
    return dir.getChildFile ("user_presets.xml");
}

void AutoLevelerAudioProcessor::loadUserPresets()
{
    const auto file = getUserPresetFile();
    if (!file.existsAsFile())
        return;

    auto xml = juce::parseXML (file);
    if (xml == nullptr || !xml->hasTagName ("UserPresets"))
        return;

    // 属性が欠けていたら Default プリセットの値で補う
    const auto& defaults = presets.front().values;

    for (auto* child : xml->getChildWithTagNameIterator ("Preset"))
    {
        Preset p { child->getStringAttribute ("name", "User Preset"), defaults };
        for (size_t i = 0; i < presetParams.size(); ++i)
            p.values[i] = static_cast<float>(child->getDoubleAttribute (presetParams[i].xmlAttribute, defaults[i]));
        presets.push_back (p);
    }
}

void AutoLevelerAudioProcessor::saveUserPresetsToFile()
{
    juce::XmlElement rootXml ("UserPresets");

    // ファクトリープリセットの後ろがユーザー追加プリセット
    for (size_t i = numFactoryPresets; i < presets.size(); ++i)
    {
        auto* child = rootXml.createNewChildElement ("Preset");
        child->setAttribute ("name", presets[i].name);
        for (size_t j = 0; j < presetParams.size(); ++j)
            child->setAttribute (presetParams[j].xmlAttribute, static_cast<double>(presets[i].values[j]));
    }

    rootXml.writeTo (getUserPresetFile(), {});
}

bool AutoLevelerAudioProcessor::saveUserPreset (const juce::String& presetName)
{
    if (presetName.trim().isEmpty())
        return false;

    Preset newPreset { presetName.trim(), {} };
    for (size_t i = 0; i < presetParams.size(); ++i)
        newPreset.values[i] = apvts.getRawParameterValue (presetParams[i].paramID)->load();

    presets.push_back (newPreset);
    currentProgram = static_cast<int>(presets.size()) - 1;

    saveUserPresetsToFile();
    return true;
}

bool AutoLevelerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void AutoLevelerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = (sampleRate > 0.0) ? sampleRate : 44100.0;

    // 1. Lookaheadディレイ（22.5ms）のサンプル数計算
    lookaheadSamples = juce::jmax (1, juce::roundToInt (currentSampleRate * (lookaheadMs * 0.001f)));
    const bool isLookaheadOn = lookaheadEnableParam->load() > 0.5f;
    setLatencySamples (isLookaheadOn ? lookaheadSamples : 0);

    lookaheadDelay.prepare ({ currentSampleRate, static_cast<juce::uint32>(juce::jmax (1, samplesPerBlock)),
                              static_cast<juce::uint32>(juce::jmax (1, getTotalNumInputChannels())) });
    lookaheadDelay.setMaximumDelayInSamples (lookaheadSamples);
    lookaheadDelay.setDelay (static_cast<float>(lookaheadSamples));

    // 2. ゲインスムージング初期化
    smoothedInputGainDb.reset (currentSampleRate, 0.02);
    smoothedInputGainDb.setCurrentAndTargetValue (inputGainParam->load());

    smoothedOutputGainDb.reset (currentSampleRate, 0.02);
    smoothedOutputGainDb.setCurrentAndTargetValue (outputGainParam->load());

    lookaheadMix.reset (currentSampleRate, 0.02);
    lookaheadMix.setCurrentAndTargetValue (isLookaheadOn ? 1.0f : 0.0f);

    bypassMix.reset (currentSampleRate, 0.02);
    bypassMix.setCurrentAndTargetValue (bypassParam->load() > 0.5f ? 1.0f : 0.0f);

    // 3. 歯擦音・ブレス用サイドチェーンバンドパスフィルターの初期化
    sidechainHPF.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, 150.0f);
    sidechainLPF.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate, 4000.0f);
    sidechainHPF.reset();
    sidechainLPF.reset();

    fastEnvelopeRms = 0.0f;
    fastEnvelopePeak = 0.0f;
    currentSmoothedGainDb = 0.0f;
    smoothedRmsIn = 0.0f;
    smoothedRmsOut = 0.0f;
    smoothedVuIn = 0.0f;
    smoothedVuOut = 0.0f;

    // 4. UI波形送信用ダウンサンプラーの初期化（約100Hz）
    downsampleInterval = juce::jmax (1, juce::roundToInt (currentSampleRate / 100.0));
    downsampleCounter = 0;
    inputAccumSumSquares = 0.0f;
    outputAccumSumSquares = 0.0f;
    gainChangeAccumDb = 0.0f;

    visualFifo.reset();
}

void AutoLevelerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    if (numSamples == 0 || totalNumInputChannels == 0)
        return;

    // -------------------------------------------------------------
    // 1. ホストBPMの取得 (BPM Sync用)
    // -------------------------------------------------------------
    if (auto* currentPlayHead = getPlayHead())
    {
        if (auto pos = currentPlayHead->getPosition())
        {
            if (pos->getBpm().hasValue())
                currentBpm.store (static_cast<float>(*pos->getBpm()), std::memory_order_relaxed);
        }
    }

    // -------------------------------------------------------------
    // 2. パラメーター取得 (Lock-Free)
    // -------------------------------------------------------------
    constexpr auto relaxed = std::memory_order_relaxed;
    const float inGainDb           = inputGainParam->load (relaxed);
    const float targetLevelDb      = targetLevelParam->load (relaxed);
    const float rangeDb            = rangeParam->load (relaxed);
    const float speedVal           = speedParam->load (relaxed);
    const float outGainDb          = outputGainParam->load (relaxed);
    const bool isLookaheadOn       = lookaheadEnableParam->load (relaxed) > 0.5f;
    const bool isBreathFilterOn    = breathFilterParam->load (relaxed)    > 0.5f;
    const bool isSibilanceFilterOn = sibilanceFilterParam->load (relaxed) > 0.5f;
    const bool isPeakDetection     = detectionModeParam->load (relaxed)   > 0.5f;
    const bool isSyncMode          = timingModeParam->load (relaxed)      > 0.5f;
    const int syncSpeedChoice      = juce::roundToInt (syncSpeedParam->load (relaxed));
    const bool isGuiEnabled        = guiEnableParam->load (relaxed)       > 0.5f;
    const bool isBypassed          = bypassParam->load (relaxed)          > 0.5f;

    // Lookahead切り替えに伴うDAWレイテンシー報告（バイパス時もドライを同じだけ遅らせるので固定）
    const int targetLatency = isLookaheadOn ? lookaheadSamples : 0;
    if (getLatencySamples() != targetLatency)
        setLatencySamples (targetLatency);

    smoothedInputGainDb.setTargetValue (inGainDb);
    smoothedOutputGainDb.setTargetValue (outGainDb);
    bypassMix.setTargetValue (isBypassed ? 1.0f : 0.0f);
    lookaheadMix.setTargetValue (isLookaheadOn ? 1.0f : 0.0f);

    // -------------------------------------------------------------
    // 3. アタック / リリースの計算 (Free vs BPM Sync 3段階: Fast, Mid, Slow)
    // -------------------------------------------------------------
    const auto timing = calculateTimingMs (speedVal, isSyncMode, syncSpeedChoice, currentBpm.load (std::memory_order_relaxed));
    const float attackCoeff  = 1.0f - std::exp (-1.0f / static_cast<float>(currentSampleRate * (timing.attackMs * 0.001f)));
    const float releaseCoeff = 1.0f - std::exp (-1.0f / static_cast<float>(currentSampleRate * (timing.releaseMs * 0.001f)));

    // エンベロープ検出時定数 (RMS平滑化用 ~15ms, Peak高速追従用 ~1.5ms)
    const float rmsEnvCoeff  = 1.0f - std::exp (-1.0f / static_cast<float>(currentSampleRate * 0.015f));
    const float peakAttCoeff = 1.0f - std::exp (-1.0f / static_cast<float>(currentSampleRate * 0.0015f));
    const float peakRelCoeff = 1.0f - std::exp (-1.0f / static_cast<float>(currentSampleRate * 0.040f));

    // チャンネルポインタ
    const float* inL = buffer.getReadPointer (0);
    const float* inR = (totalNumInputChannels > 1) ? buffer.getReadPointer (1) : inL;
    float* outL = buffer.getWritePointer (0);
    float* outR = (totalNumOutputChannels > 1) ? buffer.getWritePointer (1) : outL;

    float blockPeakIn  = 0.0f;
    float blockPeakOut = 0.0f;
    float blockSumSqIn = 0.0f;
    float blockSumSqOut = 0.0f;

    // -------------------------------------------------------------
    // 4. サンプル単位のオーディオ処理ループ
    // -------------------------------------------------------------
    for (int n = 0; n < numSamples; ++n)
    {
        // A. Input Gain 適用
        const float curInGainLinear = juce::Decibels::decibelsToGain (smoothedInputGainDb.getNextValue());
        const float gainedSampleL = inL[n] * curInGainLinear;
        const float gainedSampleR = inR[n] * curInGainLinear;

        // B. Lookahead ディレイ（バイパス用ドライと共用するため Input Gain 前の原音を遅らせる）
        lookaheadDelay.pushSample (0, inL[n]);
        const float lookedL = lookaheadDelay.popSample (0);
        float lookedR = lookedL;
        if (totalNumInputChannels > 1)
        {
            lookaheadDelay.pushSample (1, inR[n]);
            lookedR = lookaheadDelay.popSample (1);
        }

        // C. 22.5ms 前と現在サンプルを 20ms クロスフェード（Lookahead 切替時のクリック防止）
        const float laMix = lookaheadMix.getNextValue();
        const float delayedSampleL = inL[n] + laMix * (lookedL - inL[n]);
        const float delayedSampleR = inR[n] + laMix * (lookedR - inR[n]);

        // D. サイドチェーン入力の準備
        const float monoDetectorInput = 0.5f * (gainedSampleL + gainedSampleR);
        float filteredDetector = monoDetectorInput;

        // ブレス除去 (HPF 150Hz) ＆ 歯擦音除去 (LPF 4000Hz)
        if (isBreathFilterOn)
            filteredDetector = sidechainHPF.processSample (filteredDetector);

        if (isSibilanceFilterOn)
            filteredDetector = sidechainLPF.processSample (filteredDetector);

        // E. 駆動方式 (RMS vs Peak) の判定とレベル検出
        float detectedDb = -100.0f;
        if (!isPeakDetection)
        {
            // RMSモード (デフォルト)
            fastEnvelopeRms += rmsEnvCoeff * ((filteredDetector * filteredDetector) - fastEnvelopeRms);
            const float linLevel = std::sqrt (std::max (0.0f, fastEnvelopeRms));
            detectedDb = juce::Decibels::gainToDecibels (linLevel + 1e-6f, -100.0f);
        }
        else
        {
            // Peakモード
            const float detAbs = std::abs (filteredDetector);
            fastEnvelopePeak += (detAbs > fastEnvelopePeak ? peakAttCoeff : peakRelCoeff) * (detAbs - fastEnvelopePeak);
            detectedDb = juce::Decibels::gainToDecibels (fastEnvelopePeak + 1e-6f, -100.0f);
        }

        // F. ブレス音検知・安全ゲート
        float activityWeight = 1.0f;
        if (isBreathFilterOn)
        {
            // ブレス検知ON: 低フォルマント息継ぎ区間（-48dB以下）を滑らかにスルー
            constexpr float breathFloorDb = -48.0f;
            constexpr float breathCeilDb  = -36.0f;
            activityWeight = juce::jlimit (0.0f, 1.0f, (detectedDb - breathFloorDb) / (breathCeilDb - breathFloorDb));
        }
        else
        {
            // ブレス検知OFF: 完全無音時のみ保護
            constexpr float digitalZeroFloorDb = -72.0f;
            activityWeight = juce::jlimit (0.0f, 1.0f, (detectedDb - digitalZeroFloorDb) / 6.0f);
        }

        // G. 目的ゲインの算出と Range パラメーターによるクランプ ([-Range, +Range])
        const float targetDifferenceDb = (targetLevelDb - detectedDb) * activityWeight;
        const float clampedTargetCorrectionDb = juce::jlimit (-rangeDb, rangeDb, targetDifferenceDb);

        // H. 動的バリスティクス平滑化 (dB空間)
        if (clampedTargetCorrectionDb < currentSmoothedGainDb)
            currentSmoothedGainDb += attackCoeff * (clampedTargetCorrectionDb - currentSmoothedGainDb);
        else
            currentSmoothedGainDb += releaseCoeff * (clampedTargetCorrectionDb - currentSmoothedGainDb);

        // I. 総合ゲイン乗算と Output Gain の適用
        const float curOutGainLinear = juce::Decibels::decibelsToGain (smoothedOutputGainDb.getNextValue());
        const float levelingGainLinear = juce::Decibels::decibelsToGain (currentSmoothedGainDb);
        const float totalGain = curInGainLinear * levelingGainLinear * curOutGainLinear;

        // バイパスはレイテンシー補償済みのドライへ 20ms クロスフェード（切替時のクリック防止）
        const float dryMix = bypassMix.getNextValue();
        const float finalOutL = delayedSampleL * (totalGain + dryMix * (1.0f - totalGain));
        const float finalOutR = delayedSampleR * (totalGain + dryMix * (1.0f - totalGain));

        outL[n] = juce::jlimit (-4.0f, 4.0f, finalOutL);
        if (totalNumOutputChannels > 1)
            outR[n] = juce::jlimit (-4.0f, 4.0f, finalOutR);

        // ピークトラッキング (左右最大値) & 2乗和集計 (RMS / VU用)
        const float peakSampleIn  = std::max (std::abs (gainedSampleL), std::abs (gainedSampleR));
        const float peakSampleOut = std::max (std::abs (outL[n]), totalNumOutputChannels > 1 ? std::abs (outR[n]) : std::abs (outL[n]));
        blockPeakIn  = std::max (blockPeakIn, peakSampleIn);
        blockPeakOut = std::max (blockPeakOut, peakSampleOut);

        blockSumSqIn  += 0.5f * (gainedSampleL * gainedSampleL + gainedSampleR * gainedSampleR);
        blockSumSqOut += 0.5f * (outL[n] * outL[n] + (totalNumOutputChannels > 1 ? outR[n] * outR[n] : outL[n] * outL[n]));

        // J. UI波形描画用データの集計とFIFO送信
        if (isGuiEnabled)
        {
            const float monoInSample  = 0.5f * (gainedSampleL + gainedSampleR);
            const float monoOutSample = 0.5f * (outL[n] + (totalNumOutputChannels > 1 ? outR[n] : outL[n]));

            inputAccumSumSquares  += monoInSample * monoInSample;
            outputAccumSumSquares += monoOutSample * monoOutSample;
            gainChangeAccumDb     += (isBypassed ? 0.0f : currentSmoothedGainDb);

            if (++downsampleCounter >= downsampleInterval)
            {
                const float countF = static_cast<float>(downsampleCounter);
                VisualDataPoint point;
                point.inputRms     = std::sqrt (inputAccumSumSquares / countF);
                point.outputRms    = std::sqrt (outputAccumSumSquares / countF);
                point.gainChangeDb = gainChangeAccumDb / countF;

                inputAccumSumSquares  = 0.0f;
                outputAccumSumSquares = 0.0f;
                gainChangeAccumDb     = 0.0f;
                downsampleCounter     = 0;

                visualFifo.write (1).forEach ([&] (int index) { visualFifoBuffer[static_cast<size_t>(index)] = point; });
            }
        }
    }

    // ブロック全体のRMS & VUバリスティクス算出
    const float blockRmsIn  = std::sqrt (std::max (0.0f, blockSumSqIn  / static_cast<float>(numSamples)));
    const float blockRmsOut = std::sqrt (std::max (0.0f, blockSumSqOut / static_cast<float>(numSamples)));

    // RMSメーター平滑化 (時定数約 50ms)
    const float rmsMeterCoeff = 1.0f - std::exp (-static_cast<float>(numSamples) / static_cast<float>(currentSampleRate * 0.050f));
    smoothedRmsIn  += rmsMeterCoeff * (blockRmsIn  - smoothedRmsIn);
    smoothedRmsOut += rmsMeterCoeff * (blockRmsOut - smoothedRmsOut);

    // VUメーター平滑化 (時定数約 160ms = 300msで99%到達の標準VUバリスティクス)
    const float vuMeterCoeff = 1.0f - std::exp (-static_cast<float>(numSamples) / static_cast<float>(currentSampleRate * 0.160f));
    smoothedVuIn  += vuMeterCoeff * (blockRmsIn  - smoothedVuIn);
    smoothedVuOut += vuMeterCoeff * (blockRmsOut - smoothedVuOut);

    // 各メーター用アトミック値の更新
    latestInputPeak.store  (blockPeakIn,     std::memory_order_relaxed);
    latestOutputPeak.store (blockPeakOut,    std::memory_order_relaxed);
    latestInputRms.store   (smoothedRmsIn,   std::memory_order_relaxed);
    latestOutputRms.store  (smoothedRmsOut,  std::memory_order_relaxed);
    latestInputVu.store    (smoothedVuIn,    std::memory_order_relaxed);
    latestOutputVu.store   (smoothedVuOut,   std::memory_order_relaxed);
}

int AutoLevelerAudioProcessor::readVisualData (VisualDataPoint* destination, int maxPointsToRead)
{
    int numRead = 0;
    visualFifo.read (maxPointsToRead).forEach ([&] (int index) { destination[numRead++] = visualFifoBuffer[static_cast<size_t>(index)]; });
    return numRead;
}

bool AutoLevelerAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* AutoLevelerAudioProcessor::createEditor()
{
    return new AutoLevelerAudioProcessorEditor (*this);
}

void AutoLevelerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void AutoLevelerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
    {
        auto state = juce::ValueTree::fromXml (*xmlState);
        apvts.replaceState (state);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout AutoLevelerAudioProcessor::createParameterLayout()
{
    using Float  = juce::AudioParameterFloat;
    using Bool   = juce::AudioParameterBool;
    using Choice = juce::AudioParameterChoice;
    const auto dB = juce::AudioParameterFloatAttributes().withLabel ("dB");
    const auto id = [] (const char* paramID) { return juce::ParameterID { paramID, 1 }; };

    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<Float>  (id (ParameterIDs::inputGain),       "Input Gain",       juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f),  0.0f, dB),
                std::make_unique<Float>  (id (ParameterIDs::targetLevel),     "Target Level",     juce::NormalisableRange<float> (-36.0f,  0.0f, 0.1f), -24.0f, dB),
                std::make_unique<Float>  (id (ParameterIDs::range),           "Range",            juce::NormalisableRange<float> (  0.0f, 13.0f, 0.1f),  7.0f, dB),
                std::make_unique<Float>  (id (ParameterIDs::speed),           "Speed",            juce::NormalisableRange<float> (  0.0f, 100.0f, 0.1f), 93.6f,
                                          juce::AudioParameterFloatAttributes().withLabel ("%")),
                std::make_unique<Float>  (id (ParameterIDs::outputGain),      "Output Gain",      juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f),  0.0f, dB),
                std::make_unique<Bool>   (id (ParameterIDs::lookaheadEnable), "Lookahead",        true),
                std::make_unique<Bool>   (id (ParameterIDs::breathFilter),    "Breath Filter",    true),
                std::make_unique<Bool>   (id (ParameterIDs::sibilanceFilter), "Sibilance Filter", true),
                std::make_unique<Choice> (id (ParameterIDs::detectionMode),   "Detection Mode",   juce::StringArray { "RMS", "Peak" }, 0),
                std::make_unique<Choice> (id (ParameterIDs::timingMode),      "Timing Mode",      juce::StringArray { "Free (ms)", "Sync (BPM)" }, 0),
                std::make_unique<Choice> (id (ParameterIDs::syncSpeed),       "Sync Speed",       juce::StringArray { "Fast", "Mid", "Slow" }, 1),
                std::make_unique<Choice> (id (ParameterIDs::meterMode),       "Meter Mode",       juce::StringArray { "Peak", "RMS", "VU-18" }, 0),
                std::make_unique<Bool>   (id (ParameterIDs::guiEnable),       "GUI Enable",       true),
                std::make_unique<Bool>   (id (ParameterIDs::bypass),          "Bypass",           false));
    return layout;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AutoLevelerAudioProcessor();
}
